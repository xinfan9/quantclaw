#include "WebServer.h"

#include <spdlog/spdlog.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "../core/MemoryEngine.h"
#include "../mcp/MCPClient.h"
#include "../mcp/MCPTool.h"
#include "../plugins/SidecarManager.h"
#include "../providers/LLMProvider.h"
#include "../security/ExecApprovalManager.h"
#include "../security/ToolPermissionChecker.h"
#include "../session/ChatHistory.h"
#include "../tools/CalculatorTool.h"
#include "../tools/ToolRegistry.h"
#include "providers/ProviderRegistry.h"

namespace quantclaw::web {

namespace {

constexpr const char* kIndexHtml = R"html(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>QuantClaw Web UI</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: #f5f5f5;
      height: 100vh;
      display: flex;
      flex-direction: column;
    }
    header {
      background: #1a1a2e;
      color: #fff;
      padding: 1rem 2rem;
      font-size: 1.25rem;
      font-weight: 600;
    }
    #chat {
      flex: 1;
      overflow-y: auto;
      padding: 2rem;
      display: flex;
      flex-direction: column;
      gap: 1rem;
    }
    .message {
      max-width: 70%;
      padding: 0.75rem 1rem;
      border-radius: 1rem;
      line-height: 1.5;
      white-space: pre-wrap;
    }
    .user {
      align-self: flex-end;
      background: #007aff;
      color: #fff;
      border-bottom-right-radius: 0.25rem;
    }
    .assistant {
      align-self: flex-start;
      background: #fff;
      color: #333;
      border-bottom-left-radius: 0.25rem;
      box-shadow: 0 1px 2px rgba(0,0,0,0.1);
    }
    #input-area {
      background: #fff;
      padding: 1rem 2rem;
      border-top: 1px solid #e0e0e0;
      display: flex;
      gap: 0.75rem;
    }
    #message-input {
      flex: 1;
      padding: 0.75rem 1rem;
      border: 1px solid #d0d0d0;
      border-radius: 0.5rem;
      font-size: 1rem;
    }
    #send-btn {
      padding: 0.75rem 1.5rem;
      background: #007aff;
      color: #fff;
      border: none;
      border-radius: 0.5rem;
      cursor: pointer;
      font-size: 1rem;
    }
    #send-btn:hover { background: #0051d5; }
    #send-btn:disabled { background: #999; }
    .loading { color: #666; font-style: italic; }
  </style>
</head>
<body>
  <header>QuantClaw Web UI</header>
  <div id="chat"></div>
  <div id="input-area">
    <input id="message-input" type="text" placeholder="输入消息..." autocomplete="off">
    <button id="send-btn">发送</button>
  </div>
  <script>
    const chat = document.getElementById('chat');
    const input = document.getElementById('message-input');
    const sendBtn = document.getElementById('send-btn');

    function appendMessage(role, text) {
      const div = document.createElement('div');
      div.className = 'message ' + role;
      div.textContent = text;
      chat.appendChild(div);
      chat.scrollTop = chat.scrollHeight;
    }

    async function sendMessage() {
      const text = input.value.trim();
      if (!text) return;
      appendMessage('user', text);
      input.value = '';
      sendBtn.disabled = true;

      const loading = document.createElement('div');
      loading.className = 'message assistant loading';
      loading.textContent = '思考中...';
      chat.appendChild(loading);
      chat.scrollTop = chat.scrollHeight;

      try {
        const res = await fetch('/api/chat', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ message: text })
        });
        const data = await res.json();
        loading.remove();
        appendMessage('assistant', data.reply || '（无回复）');
      } catch (e) {
        loading.remove();
        appendMessage('assistant', '错误：' + e.message);
      } finally {
        sendBtn.disabled = false;
        input.focus();
      }
    }

    sendBtn.addEventListener('click', sendMessage);
    input.addEventListener('keydown', e => { if (e.key === 'Enter') sendMessage(); });
    input.focus();
  </script>
</body>
</html>
)html";

std::vector<std::string> SplitCommand(const std::string& cmd) {
  std::vector<std::string> parts;
  std::string current;
  for (char c : cmd) {
    if (c == ' ') {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) parts.push_back(current);
  return parts;
}

void RegisterMcpTools(quantclaw::tools::ToolRegistry& tools) {
  const char* mcp_server = std::getenv("QUANTCLAW_MCP_SERVER");
  if (!mcp_server) return;

  try {
    std::string server_cmd(mcp_server);
    auto parts = SplitCommand(server_cmd);
    if (parts.empty()) return;

    std::string cmd = parts[0];
    std::vector<std::string> args(parts.begin() + 1, parts.end());

    auto mcp_client = std::make_shared<quantclaw::mcp::MCPClient>(cmd, args);
    for (auto& def : mcp_client->ListTools()) {
      tools.Register(std::make_unique<quantclaw::mcp::MCPTool>(mcp_client, def));
    }
    spdlog::info("[web] MCP tools registered from: {}", cmd);
  } catch (const std::exception& e) {
    spdlog::error("[web] Failed to initialize MCP server: {}", e.what());
  }
}

void RegisterSidecarTools(quantclaw::tools::ToolRegistry& tools) {
  quantclaw::plugins::SidecarManager sidecar;
  sidecar.RegisterTools(tools);
}

std::string ChatWithLLM(const Config& cfg, const std::string& user_message) {
  providers::ProviderRegistry registry;
  for (const auto& [id, pc] : cfg.providers) {
    registry.AddProvider({id, pc.api_key, pc.base_url});
  }
  for (const auto& [alias, target] : cfg.aliases) {
    registry.AddAlias(alias, target);
  }
  auto provider = registry.CreateProvider(cfg);

  tools::ToolRegistry tools;
  tools.Register(std::make_unique<tools::CalculatorTool>());

  // ---- M8：注册 MCP 工具 ----
  RegisterMcpTools(tools);

  // ---- M11：注册 Node.js Sidecar 插件工具 ----
  RegisterSidecarTools(tools);

  // Web UI 后台无法交互式询问，因此关闭执行级审批；但仍保留工具级 allow/deny 检查
  security::ToolPermissionChecker permission_checker(
      cfg.tool_permissions.allow, cfg.tool_permissions.deny);

  security::ExecApprovalConfig exec_cfg;
  exec_cfg.mode = security::AskMode::kOff; // 后台服务无法进行交互式确认
  exec_cfg.allowlist = cfg.exec_approval.allowlist;
  security::ExecApprovalManager approval_manager(std::move(exec_cfg));

  session::ChatHistory history;
  auto messages = history.Load();

  constexpr size_t kMaxHistory = 20;
  if (messages.size() > kMaxHistory) {
    messages = std::vector(messages.end() - kMaxHistory, messages.end());
  }

  core::MemoryEngine memory(messages);
  auto relevant = memory.Search(user_message, 3);
  std::string memory_context = core::MemoryEngine::FormatContext(relevant);

  if (messages.empty()) {
    std::string sys_prompt = "You are a helpful assistant. Use tools when they can help answer the user's question.";
    if (!memory_context.empty()) {
      sys_prompt += "\n\n" + memory_context;
    }
    messages.push_back({"system", sys_prompt, "", {}});
  }

  messages.push_back({"user", user_message, "", {}});

  std::string final_reply;
  for (int iteration = 0; iteration < 5; ++iteration) {
    auto response = provider->Chat(messages, tools);

    if (!response.isToolCall()) {
      final_reply = response.content;
      messages.push_back({"assistant", final_reply, "", {}});
      break;
    }

    providers::Message assistant_msg;
    assistant_msg.role = "assistant";
    assistant_msg.content = response.content;
    assistant_msg.tool_calls = response.tool_calls;
    messages.push_back(assistant_msg);

    for (const auto& tool_call : response.tool_calls) {
      // 工具级 allow/deny 检查
      if (!permission_checker.IsAllowed(tool_call.name)) {
        messages.push_back({"tool", "Tool permission denied", tool_call.id, {}});
        continue;
      }

      // 执行级审批（Web 后台已关闭交互式确认，仅通过白名单判断）
      std::string command_summary = tool_call.name + " " + tool_call.arguments.dump();
      if (!approval_manager.RequestApproval(command_summary)) {
        messages.push_back({"tool", "Execution approval denied", tool_call.id, {}});
        continue;
      }

      std::string result = tools.Execute(tool_call.name, tool_call.arguments);
      messages.push_back({"tool", result, tool_call.id, {}});
    }
  }

  history.Save(messages);
  return final_reply;
}

}  // namespace

WebServer::WebServer(const Config& cfg, int port, const std::string& host)
    : _cfg(cfg), _port(port), _host(host) {}

void WebServer::Run() {
  httplib::Server server;

  server.Get("/", [](const httplib::Request&, httplib::Response& res) {
    res.set_content(kIndexHtml, "text/html");
  });

  server.Post("/api/chat", [this](const httplib::Request& req,
                                  httplib::Response& res) {
    try {
      auto body = nlohmann::json::parse(req.body);
      std::string user_message = body.value("message", "");
      if (user_message.empty()) {
        res.status = 400;
        res.set_content(R"({"error":"Missing 'message' field"})",
                        "application/json");
        return;
      }

      spdlog::info("[web] chat request: {}", user_message);
      std::string reply = ChatWithLLM(_cfg, user_message);

      nlohmann::json response;
      response["reply"] = reply;
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
      spdlog::error("[web] chat error: {}", e.what());
      res.status = 500;
      nlohmann::json error;
      error["error"] = e.what();
      res.set_content(error.dump(), "application/json");
    }
  });

  _running = true;
  spdlog::info("[web] server listening on http://{}:{}", _host, _port);

  std::thread server_thread([&server, this]() { server.listen(_host.c_str(), _port); });

  while (_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  server.stop();
  server_thread.join();
}

void WebServer::Stop() { _running = false; }

}  // namespace quantclaw::web
