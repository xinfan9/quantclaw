#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "config.h"
#include "core/MemoryEngine.h"
#include "gateway/GatewayClient.h"
#include "gateway/GatewayServer.h"
#include "mcp/MCPClient.h"
#include "mcp/MCPTool.h"
#include "plugins/SidecarManager.h"
#include "providers/LLMProvider.h"
#include "providers/ProviderFactory.h"
#include "security/PermissionManager.h"
#include "session/ChatHistory.h"
#include "tools/CalculatorTool.h"
#include "tools/ToolRegistry.h"
#include "web/WebServer.h"

namespace {

void SummarizeMessages(const std::vector<quantclaw::providers::Message>& messages,
                       const std::string& label) {
  spdlog::info("[messages] {}: count={}", label, messages.size());
  for (size_t i = 0; i < messages.size(); ++i) {
    const auto& m = messages[i];
    spdlog::debug("  [{}] role={} content_len={} tool_call_id={} tool_calls={}",
                  i, m.role, m.content.size(),
                  m.tool_call_id.empty() ? "<none>" : m.tool_call_id,
                  m.tool_calls.size());
    spdlog::debug("    content: {}", m.content);
    for (const auto& tc : m.tool_calls) {
      spdlog::debug("    tool_call: id={} name={} args={}",
                    tc.id, tc.name, tc.arguments.dump());
    }
  }
}

void SummarizeResponse(const quantclaw::providers::ChatResponse& response) {
  if (response.isToolCall()) {
    spdlog::info("[llm response] type=tool_call content_len={}", response.content.size());
    for (const auto& tc : response.tool_calls) {
      spdlog::info("  tool_call: id={} name={} args={}",
                   tc.id, tc.name, tc.arguments.dump());
    }
  } else {
    spdlog::info("[llm response] type=content content_len={}", response.content.size());
    spdlog::debug("  content: {}", response.content);
  }
}

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
    spdlog::info("MCP tools registered from: {}", cmd);
  } catch (const std::exception& e) {
    spdlog::error("Failed to initialize MCP server: {}", e.what());
  }
}

void RegisterSidecarTools(quantclaw::tools::ToolRegistry& tools) {
  quantclaw::plugins::SidecarManager sidecar;
  sidecar.RegisterTools(tools);
}

// ---- M9：通过网关发送请求 ----
int RunAsClient(const std::string& user_message) {
  try {
    quantclaw::gateway::GatewayClient client;
    client.Connect();
    std::string reply = client.Chat(user_message);
    spdlog::info("Assistant: {}", reply);
    return 0;
  } catch (const std::exception& e) {
    spdlog::error("Gateway client error: {}", e.what());
    return 1;
  }
}

// ---- M9：启动网关服务器 ----
int RunAsServer(const quantclaw::Config& cfg) {
  try {
    quantclaw::gateway::GatewayServer server(cfg);
    server.Run();
    return 0;
  } catch (const std::exception& e) {
    spdlog::error("Gateway server error: {}", e.what());
    return 1;
  }
}

// ---- M11：启动 Web UI 服务器 ----
int RunWebUI(const quantclaw::Config& cfg) {
  try {
    quantclaw::web::WebServer server(cfg);
    server.Run();
    return 0;
  } catch (const std::exception& e) {
    spdlog::error("Web UI server error: {}", e.what());
    return 1;
  }
}

} // namespace

int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::debug);

  // ---- M9：网关模式 ----
  if (argc >= 2 && std::string(argv[1]) == "--gateway") {
    auto cfg = quantclaw::Config::Load();
    return RunAsServer(cfg);
  }

  // ---- M11：Web UI 模式 ----
  if (argc >= 2 && std::string(argv[1]) == "--web") {
    auto cfg = quantclaw::Config::Load();
    return RunWebUI(cfg);
  }

  if (argc < 2) {
    spdlog::error("Usage: quantclaw <user_message>");
    spdlog::error("       quantclaw --clear");
    spdlog::error("       quantclaw clear");
    spdlog::error("       quantclaw --gateway");
    spdlog::error("       quantclaw --web");
    return 1;
  }

  // ---- M2：清空历史 ----
  if (argc == 2 && (std::string(argv[1]) == "--clear" || std::string(argv[1]) == "clear")) {
    quantclaw::session::ChatHistory().Clear();
    spdlog::info("History cleared.");
    return 0;
  }

  // 汇集参数
  std::string user_message;
  for (int i = 1; i < argc; ++i) {
    if (!user_message.empty()) user_message += " ";
    user_message += argv[i];
  }
  spdlog::info("[input] user_message: {}", user_message);

  // ---- M9：如果配置了 USE_GATEWAY，通过网关发送请求 ----
  const char* use_gateway = std::getenv("QUANTCLAW_USE_GATEWAY");
  if (use_gateway && std::string(use_gateway) == "1") {
    return RunAsClient(user_message);
  }

  try {
    auto cfg = quantclaw::Config::Load();
    spdlog::info("[config] model={} base_url={}", cfg.model, cfg.base_url);

    auto provider = quantclaw::providers::CreateProvider(cfg);
    spdlog::info("[provider] created {}", cfg.model.rfind("anthropic", 0) == 0 ? "AnthropicProvider" : "OpenAIProvider");

    quantclaw::tools::ToolRegistry tools;
    tools.Register(std::make_unique<quantclaw::tools::CalculatorTool>());
    spdlog::info("[tools] registered: calculator");

    // ---- M8：注册 MCP 工具 ----
    RegisterMcpTools(tools);

    // ---- M11：注册 Node.js Sidecar 插件工具 ----
    RegisterSidecarTools(tools);

    quantclaw::security::PermissionManager permisson(quantclaw::security::PermissionManager::Mode::kAlwaysAsk);

    quantclaw::session::ChatHistory history;
    auto messages = history.Load();
    spdlog::info("[history] loaded {} messages", messages.size());
    SummarizeMessages(messages, "after load");

    constexpr size_t kMaxHistory = 20;
    if (messages.size() > kMaxHistory) {
      messages = std::vector(messages.end() - kMaxHistory, messages.end());
      spdlog::info("[history] truncated to last {} messages", kMaxHistory);
    }

    quantclaw::core::MemoryEngine memory(messages);
    auto relevant = memory.Search(user_message, 3);
    std::string memory_context = quantclaw::core::MemoryEngine::FormatContext(relevant);
    spdlog::debug("[memory] formatted context:\n{}", memory_context);
    if (messages.empty()) {
      std::string sys_prompt = "You are a helpful assistant. Use tools when they can help answer the user's question.";
      if (!memory_context.empty()) {
        sys_prompt += "\n\n" + memory_context;
      }
      messages.push_back({"system", sys_prompt, "", {}});
      spdlog::info("[history] added default system message");

    }

    messages.push_back({"user", user_message, "", {}});
    SummarizeMessages(messages, "before llm");

    std::string final_reply;
    for (int iteration = 0; iteration < 5; ++iteration) {
      spdlog::info("[llm call] iteration={}", iteration + 1);
      auto response = provider->Chat(messages, tools);
      SummarizeResponse(response);

      if (!response.isToolCall()) {
        final_reply = response.content;
        messages.push_back({"assistant", final_reply, "", {}});
        spdlog::info("[llm] final reply received");
        break;
      }

      quantclaw::providers::Message assistant_msg;
      assistant_msg.role = "assistant";
      assistant_msg.content = response.content;
      assistant_msg.tool_calls = response.tool_calls;
      messages.push_back(assistant_msg);

      for (const auto& tool_call : response.tool_calls) {
        if (!permisson.RequestPermission(tool_call.name, tool_call.arguments.dump())) {
          spdlog::info("[tool permission] denied {}", tool_call.name);
          messages.push_back({"tool", "Permission denied", tool_call.id, {}});
          continue;
        }
        spdlog::info("[tool execute] name={} args={}",
                     tool_call.name, tool_call.arguments.dump());
        std::string result = tools.Execute(tool_call.name, tool_call.arguments);
        spdlog::info("[tool result] name={} result={}", tool_call.name, result);
        messages.push_back({"tool", result, tool_call.id, {}});
      }

      SummarizeMessages(messages, "after tool results");
    }

    spdlog::info("[output] final_reply: {}", final_reply);

    history.Save(messages);
    spdlog::info("[history] saved {} messages", messages.size());

    return 0;
  } catch (const std::exception& e) {
    spdlog::error("Error: {}", e.what());
    return 1;
  }
}
