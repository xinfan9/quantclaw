#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "config.h"
#include "core/MemoryEngine.h"
#include "mcp/MCPClient.h"
#include "mcp/MCPTool.h"
#include "providers/LLMProvider.h"
#include "providers/ProviderFactory.h"
#include "security/PermissionManager.h"
#include "session/ChatHistory.h"
#include "tools/CalculatorTool.h"
#include "tools/ToolRegistry.h"

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

} // namespace

// TIP 要<b>Run</b>代码，请按 <shortcut actionId="Run"/> 或点击装订区域中的 <icon src="AllIcons.Actions.Execute"/> 图标。
int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::debug);

  if (argc < 2) {
    spdlog::error("Usage: quantclaw <user_message>");
    return 1;
  }

  // 汇集参数
  std::string user_message;
  for (int i = 1; i < argc; ++i) {
    if (!user_message.empty()) user_message += " ";
    user_message += argv[i];
  }
  spdlog::info("[input] user_message: {}", user_message);

  try {
    auto cfg = quantclaw::Config::Load();
    spdlog::info("[config] model={} base_url={}", cfg.model, cfg.base_url);

    auto provider = quantclaw::providers::CreateProvider(cfg);
    spdlog::info("[provider] created {}", cfg.model.rfind("anthropic", 0) == 0 ? "AnthropicProvider" : "OpenAIProvider");

    quantclaw::tools::ToolRegistry tools;
    tools.Register(std::make_unique<quantclaw::tools::CalculatorTool>());
    spdlog::info("[tools] registered: calculator");

    std::shared_ptr<quantclaw::mcp::MCPClient> mcp_client;
    const char* mcp_server = std::getenv("QUANTCLAW_MCP_SERVER");
    if (mcp_server) {
      try {
        std::string server_cmd(mcp_server);
        std::vector<std::string> parts;
        std::string current;
        for (char c : server_cmd) {
          if (c == ' ') {
            if (!current.empty()) {
              parts.push_back(current);
              current.clear();
            }
          } else current += c;
        }

        if (!current.empty()) parts.push_back(current);

        if (!parts.empty()) {
          std::string cmd = parts[0];
          std::vector args(parts.begin() + 1, parts.end());

          mcp_client = std::make_shared<quantclaw::mcp::MCPClient>(cmd, args);

          for (auto& def : mcp_client->ListTools()) {
            tools.Register(std::make_unique<quantclaw::mcp::MCPTool>(mcp_client, def));
          }

          spdlog::info("MCP tools registered from: {}", cmd);
        }
      } catch (const std::exception& e) {
        spdlog::error("Failed to initialize MCP server: {}", e.what());
      }


    }

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
