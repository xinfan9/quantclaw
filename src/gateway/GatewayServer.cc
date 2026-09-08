//
// Created by xinfang on 2026/9/8.
//

#include "GatewayServer.h"

#include <utility>

#include "JsonRpcMessage.h"
#include "core/MemoryEngine.h"
#include "providers/ProviderFactory.h"
#include "session/ChatHistory.h"
#include "spdlog/spdlog.h"
#include "tools/CalculatorTool.h"

namespace quantclaw::gateway {

GatewayServer::GatewayServer(Config  cfg, int port, std::string  host)
  : _cfg(std::move(cfg)), _port(port), _host(std::move(host)) {

  _tools = std::make_unique<tools::ToolRegistry>();
  _tools->Register(std::make_unique<tools::CalculatorTool>());

  _permission = std::make_unique<security::PermissionManager>(security::PermissionManager::Mode::kAlwaysAsk);

  _provider = providers::CreateProvider(_cfg);
}

void GatewayServer::Run() {
  ix::WebSocketServer server(_port, _host);

  server.setOnConnectionCallback(
    // [this] 捕获表示这个 lambda 可以访问 GatewayServer 的成员变量和成员函数，比如 _tools、_provider、_permission 等。
    //     | 捕获方式 | 说明 |
    // | --- | --- |
    // | [] | 不捕获 |
    // | [=] | 按值捕获所有用到的变量 |
    // | [&] | 按引用捕获所有用到的变量 |
    // | [x] | 按值捕获 x |
    // | [&x] | 按引用捕获 x |
    // | [=, &y] | 默认按值，y 按引用 |
    // | [&, x] | 默认按引用，x 按值 |
    // | [this] | 捕获当前对象指针 |
    // | [*this] | 按值捕获当前对象副本（C++17） |
    // | [x = expr] | 初始化捕获（C++14），允许你在 lambda 捕获列表里直接定义并初始化一个新变量, x = a + b |
    // | [...args = std::forward(args)] | 包展开捕获（C++20） |
    [this](std::weak_ptr<ix::WebSocket> webSocket, // 为什么 webSocket 是 weak_ptr？
      // 因为 ixwebsocket 内部用 shared_ptr 管理 WebSocket 生命周期，但回调里给你 weak_ptr 是为了避免循环引用：
      // 回调里不直接持有强引用，需要使用时再 lock() 升级。
      // weak_ptr：不拥有对象，只观察对象是否还活着，主要用来避免循环引用。
      std::shared_ptr<ix::ConnectionState> connectionState) {
      // .lock() 尝试把弱指针提升为 std::shared_ptr<WebSocket>（强指针）
      // 如果对象还活着 → 返回有效的 shared_ptr，引用计数 +1，保证在 ws 作用域内对象不会被销毁
      // 如果对象已被销毁 → 返回空的 shared_ptr（等价于 nullptr）
      auto ws = webSocket.lock();
      if (!ws) return;
      ws->setOnMessageCallback(
        [this, ws](const ix::WebSocketMessagePtr& msg) {
          if (msg->type == ix::WebSocketMessageType::Message) {
            try {
              auto request = JsonRpcMessage::FromString(msg->str);

              if (request.method == "chat") {
                HandleChat(ws, request.id, request.params);
              } else {
                JsonRpcMessage response;
                response.id = request.id;
                response.error = {
                  {"code", -32601},
                  {"message", "Method not found"}
                };
                ws->send(response.ToJson().dump());
              }
            } catch (const std::exception& e) {
              spdlog::error("Failed to handle gateway message: {}", e.what());
              JsonRpcMessage response;
              response.error = {
                {"code", -32600},
                {"message", e.what()}
              };
              ws->send(response.ToJson().dump());
            }
          }
        }
      );
    }
  );

  auto res = server.listen();
  if (!res.first) {
    throw std::runtime_error("Failed to start gateway server:" + res.second);
  }

  spdlog::info("Gateway server listening on {}:{}", _host, _port);
  _running = true;
  server.start();

  while (_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  server.stop();
}

void GatewayServer::Stop() {
  _running = false;
}

void GatewayServer::HandleChat(const std::shared_ptr<ix::WebSocket>& websocket, int request_id, const nlohmann::json& params) {
  std::string user_message = params.value("message", "");

  // 加载记忆
  session::ChatHistory history;
  auto messages = history.Load();
  constexpr size_t kMaxHistory = 20;
  if (messages.size() > kMaxHistory) messages = std::vector(messages.begin(), messages.begin() + kMaxHistory);

  // 搜索记忆
  core::MemoryEngine memory(messages);
  auto relevant = memory.Search(user_message, 3);
  std::string memory_context = core::MemoryEngine::FormatContext(relevant);

  // 系统提示
  if (messages.empty()) {
    std::string system_prompt =
    "You are a helpful assistant. Use tools when they can help answer "
    "the user's question.";
    if (!memory_context.empty()) {
      system_prompt += "\n\n" + memory_context;
    }

    messages.push_back({ "system", system_prompt, "", {}});
  }

  messages.push_back({ "user", user_message, "", {}});

  spdlog::info("[gateway] User: {}", user_message);

  auto result = ExecuteToolLoop(messages);

  history.Save(messages);

  JsonRpcMessage response;
  response.id = request_id;
  response.result = {{"reply", result}};
  websocket->send(response.ToJson().dump());
}

nlohmann::json GatewayServer::ExecuteToolLoop(const std::vector<providers::Message>& messages) {
  auto current_messages = messages;
  std::string final_reply;

  for (int iteration = 0; iteration < 5; ++iteration) {
    auto response = _provider->Chat(current_messages, *_tools);
    if (!response.isToolCall()) {
      final_reply = response.content;
      current_messages.push_back({ "assistant", response.content, "", {}});
      break;
    }

    std::string tool_names;
    for (size_t i = 0; i < response.tool_calls.size(); ++i) {
      if (i > 0) tool_names += ", ";
      tool_names += response.tool_calls[i].name;
    }
    spdlog::info("call {}", tool_names);

    providers::Message assistant_msg;
    assistant_msg.role = "assistant";
    assistant_msg.content = response.content;
    assistant_msg.tool_calls = response.tool_calls;
    current_messages.push_back(assistant_msg);

    for (const auto& tc : response.tool_calls) {
      if (!_permission->RequestPermission(tc.name, tc.arguments.dump())) {
        spdlog::info("[gateway] Tool {}", tc.name);
        current_messages.push_back({"tool", "user denied permission", tc.id, {}});
        continue;
      }

      std::string result = _tools->Execute(tc.name, tc.arguments);
      spdlog::info("[gateway] Tool {} result: {}", tc.name, result);
      current_messages.push_back({"tool", result, tc.id, {}});
    }
  }

  spdlog::info("[gateway] Final reply: {}", final_reply);
  return final_reply;
}


}
