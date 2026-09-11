#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include <spdlog/spdlog.h>

#include "cli/CliManager.h"
#include "config.h"
#include "core/ContextEngine.h"
#include "core/MemoryEngine.h"
#include "core/SubagentManager.h"
#include "gateway/GatewayClient.h"
#include "gateway/GatewayServer.h"
#include "platform/service.hpp"
#include "mcp/MCPClient.h"
#include "mcp/MCPServer.hpp"
#include "mcp/MCPTool.h"
#include "mcp/MCPToolManager.hpp"
#include "plugins/SidecarManager.h"
#include "platform.h"
#include "providers/EmbeddingProvider.h"
#include "providers/FailoverResolver.h"
#include "providers/LLMProvider.h"
#include "providers/OpenAIEmbeddingProvider.h"
#include "providers/ProviderRegistry.h"
#include "security/ExecApprovalManager.h"
#include "security/ToolPermissionChecker.h"
#include "session/ChatHistory.h"
#include "tools/CalculatorTool.h"
#include "tools/SubagentTool.h"
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
    spdlog::info("[llm response] type=tool_call content_len={}",
                 response.content.size());
    for (const auto& tc : response.tool_calls) {
      spdlog::info("  tool_call: id={} name={} args={}",
                   tc.id, tc.name, tc.arguments.dump());
    }
  } else {
    spdlog::info("[llm response] type=content content_len={}",
                 response.content.size());
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
  // 统一使用 MCPToolManager 管理外部 MCP 服务器，支持单服务器与多服务器配置。
  quantclaw::mcp::MCPToolManager manager;

  // 优先读取多服务器环境变量 QUANTCLAW_MCP_SERVERS（逗号分隔命令行）。
  const char* mcp_servers_env = std::getenv("QUANTCLAW_MCP_SERVERS");
  if (mcp_servers_env) {
    std::string servers_str(mcp_servers_env);
    std::vector<std::string> server_commands;
    std::string current;
    // 简单的逗号分隔解析，暂不支持命令内部含逗号。
    for (char c : servers_str) {
      if (c == ',') {
        if (!current.empty()) {
          server_commands.push_back(current);
          current.clear();
        }
      } else {
        current += c;
      }
    }
    if (!current.empty()) server_commands.push_back(current);

    int index = 0;
    for (const auto& cmd_line : server_commands) {
      auto parts = SplitCommand(cmd_line);
      if (parts.empty()) continue;
      std::string id = "mcp" + std::to_string(index++);
      manager.AddServer(id, parts[0],
                        std::vector<std::string>(parts.begin() + 1, parts.end()));
    }
  }

  // 兼容旧版单服务器环境变量 QUANTCLAW_MCP_SERVER。
  const char* mcp_server = std::getenv("QUANTCLAW_MCP_SERVER");
  if (mcp_server) {
    std::string server_cmd(mcp_server);
    auto parts = SplitCommand(server_cmd);
    if (!parts.empty()) {
      manager.AddServer("mcp", parts[0],
                        std::vector<std::string>(parts.begin() + 1, parts.end()));
    }
  }

  try {
    int registered = 0;
    // 通过 manager 保持所有 client 存活，并把工具注册到 ToolRegistry。
    auto manager_ptr = std::make_shared<quantclaw::mcp::MCPToolManager>(std::move(manager));
    for (const auto& entry : manager_ptr->ListAllToolEntries()) {
      tools.Register(std::make_unique<quantclaw::mcp::MCPTool>(manager_ptr,
                                                                entry.server_id,
                                                                entry.def));
      ++registered;
    }
    spdlog::info("MCP tools registered: {}", registered);
  } catch (const std::exception& e) {
    spdlog::error("Failed to initialize MCP server: {}", e.what());
  }
}

void RegisterSidecarTools(quantclaw::tools::ToolRegistry& tools) {
  quantclaw::plugins::SidecarManager sidecar;
  sidecar.Start();  // 尝试启动本地 TypeScript sidecar
  sidecar.RegisterTools(tools);
}

quantclaw::providers::ProviderRegistry BuildProviderRegistry(
    const quantclaw::Config& cfg) {
  quantclaw::providers::ProviderRegistry registry;

  // 添加每个 Provider 的配置条目。
  for (const auto& [id, pc] : cfg.providers) {
    registry.AddProvider({id, pc.api_key, pc.base_url});
  }

  // 添加模型别名映射。
  for (const auto& [alias, target] : cfg.aliases) {
    registry.AddAlias(alias, target);
  }

  return registry;
}

std::unique_ptr<quantclaw::providers::FailoverResolver> BuildFailoverResolver(
    quantclaw::providers::ProviderRegistry& registry,
    const quantclaw::Config& cfg) {
  auto resolver = std::make_unique<quantclaw::providers::FailoverResolver>(
      &registry, cfg);

  resolver->SetFallbackChain(cfg.fallback_chain);

  for (const auto& [id, pc] : cfg.providers) {
    if (pc.profiles.empty()) continue;
    std::vector<quantclaw::providers::AuthProfile> profiles;
    for (const auto& p : pc.profiles) {
      profiles.push_back({p.id, p.api_key, 0});
    }
    resolver->SetProfiles(id, profiles);
  }

  return resolver;
}

std::shared_ptr<quantclaw::providers::EmbeddingProvider> BuildEmbeddingProvider(
    const quantclaw::Config& cfg) {
  if (cfg.embedding.api_key.empty() || cfg.embedding.base_url.empty()) {
    return nullptr;
  }
  return std::make_shared<quantclaw::providers::OpenAIEmbeddingProvider>(
      cfg.embedding.api_key,
      cfg.embedding.model.empty() ? "text-embedding-3-small"
                                  : cfg.embedding.model,
      cfg.embedding.base_url);
}

// ---- 默认对话命令 ----
int ChatCommand(int argc, char** argv) {
  std::string user_message;
  for (int i = 1; i < argc; ++i) {
    if (!user_message.empty()) user_message += " ";
    user_message += argv[i];
  }
  if (user_message.empty()) {
    std::cerr << "Usage: quantclaw <user message>\n";
    return 1;
  }
  spdlog::info("[input] user_message: {}", user_message);

  try {
    auto cfg = quantclaw::Config::Load();
    spdlog::info("[config] model={} base_url={}", cfg.model, cfg.base_url);

    // 如果环境变量要求走网关，则通过 GatewayClient 发送请求
    const char* use_gateway = std::getenv("QUANTCLAW_USE_GATEWAY");
    if (use_gateway && std::string(use_gateway) == "1") {
      std::string url = "ws://127.0.0.1:" +
                        std::to_string(cfg.gateway_port > 0
                                           ? cfg.gateway_port
                                           : quantclaw::platform::kDefaultGatewayPort);
      quantclaw::gateway::GatewayClient client(url, cfg.gateway_auth_token);
      client.Connect(5);
      std::string reply = client.Chat(user_message, 120);
      std::cout << reply << "\n";
      return 0;
    }

    auto registry = BuildProviderRegistry(cfg);
    auto resolver = BuildFailoverResolver(registry, cfg);
    auto resolved = resolver->Resolve(cfg.model);
    if (!resolved || !resolved->provider) {
      spdlog::error("Failed to resolve provider for model: {}", cfg.model);
      return 1;
    }
    spdlog::info("[provider] resolved {} profile={} fallback={}", cfg.model,
                 resolved->profile_id.empty() ? "<default>"
                                              : resolved->profile_id,
                 resolved->is_fallback);

    auto provider = resolved->provider;

    auto embedder = BuildEmbeddingProvider(cfg);
    if (embedder) {
      spdlog::info("[embedding] enabled provider={} model={}",
                   embedder->Name(), cfg.embedding.model);
    } else {
      spdlog::info("[embedding] disabled");
    }

    quantclaw::tools::ToolRegistry tools;
    tools.Register(std::make_unique<quantclaw::tools::CalculatorTool>());
    spdlog::info("[tools] registered: calculator");

    // 子 agent 工具：让 LLM 可以 spawn 子任务。
    auto subagent_manager = std::make_shared<quantclaw::core::SubagentManager>();
    subagent_manager->SetRunner([&provider](const std::string& task) -> std::string {
      // 子任务使用简洁的系统提示，不包含历史记忆，避免上下文膨胀。
      std::vector<quantclaw::providers::Message> sub_messages = {
          {"system",
           "You are a helpful sub-agent. Solve the given task concisely.", "", {}},
          {"user", task, "", {}}};
      auto sub_response = provider->Chat(sub_messages, quantclaw::tools::ToolRegistry());
      return sub_response.content;
    });
    tools.Register(std::make_unique<quantclaw::tools::SubagentTool>(subagent_manager));
    spdlog::info("[tools] registered: spawn_subagent");

    RegisterMcpTools(tools);
    RegisterSidecarTools(tools);

    quantclaw::security::ToolPermissionChecker permission_checker(
        cfg.tool_permissions.allow, cfg.tool_permissions.deny);

    quantclaw::security::ExecApprovalConfig exec_cfg;
    exec_cfg.mode =
        quantclaw::security::ParseAskMode(cfg.exec_approval.mode);
    exec_cfg.allowlist = cfg.exec_approval.allowlist;
    exec_cfg.timeout_seconds = cfg.exec_approval.timeout_seconds;
    quantclaw::security::ExecApprovalManager approval_manager(exec_cfg);

    quantclaw::session::ChatHistory history;
    auto messages = history.Load();
    spdlog::info("[history] loaded {} messages", messages.size());
    SummarizeMessages(messages, "after load");

    quantclaw::core::MemoryEngine memory(messages, embedder);
    auto relevant = memory.Search(user_message, 3);
    std::string memory_context =
        quantclaw::core::MemoryEngine::FormatContext(relevant);
    spdlog::debug("[memory] formatted context:\n{}", memory_context);

    std::string system_prompt =
        "You are a helpful assistant. Use tools when they can help answer "
        "the user's question.";
    if (!memory_context.empty()) {
      system_prompt += "\n\n" + memory_context;
    }

    if (messages.empty() || messages[0].role != "system") {
      messages.insert(messages.begin(),
                      {"system", system_prompt, "", {}});
      spdlog::info("[history] added system message");
    } else {
      messages[0].content = system_prompt;
      spdlog::info("[history] updated system message");
    }

    int context_window = cfg.context_window > 0 ? cfg.context_window : 8192;
    int max_tokens = cfg.max_tokens > 0 ? cfg.max_tokens : 4096;

    quantclaw::core::DefaultContextEngine context_engine;
    auto assembled = context_engine.Assemble(messages, user_message,
                                             context_window, max_tokens);
    auto context = assembled.messages;
    SummarizeMessages(context, "before llm");

    // 保存完整历史（而非压缩后的上下文）到文件。
    quantclaw::providers::Message user_msg{"user", user_message, "", {}};
    messages.push_back(user_msg);

    std::string final_reply;
    for (int iteration = 0; iteration < 5; ++iteration) {
      spdlog::info("[llm call] iteration={}", iteration + 1);
      auto response = provider->Chat(context, tools);
      resolver->RecordSuccess(resolved->provider_id, resolved->profile_id);
      SummarizeResponse(response);

      if (!response.isToolCall()) {
        final_reply = response.content;
        messages.push_back({"assistant", final_reply, "", {}});
        context.push_back({"assistant", final_reply, "", {}});
        spdlog::info("[llm] final reply received");
        break;
      }

      quantclaw::providers::Message assistant_msg;
      assistant_msg.role = "assistant";
      assistant_msg.content = response.content;
      assistant_msg.tool_calls = response.tool_calls;
      messages.push_back(assistant_msg);
      context.push_back(assistant_msg);

      for (const auto& tool_call : response.tool_calls) {
        if (!permission_checker.IsAllowed(tool_call.name)) {
          spdlog::info("[tool permission] denied {}", tool_call.name);
          messages.push_back({"tool", "Permission denied", tool_call.id, {}});
          context.push_back({"tool", "Permission denied", tool_call.id, {}});
          continue;
        }

        std::string command_summary = tool_call.name + " " + tool_call.arguments.dump();
        if (!approval_manager.RequestApproval(command_summary)) {
          spdlog::info("[tool approval] denied {}", tool_call.name);
          messages.push_back({"tool", "Execution approval denied", tool_call.id, {}});
          context.push_back({"tool", "Execution approval denied", tool_call.id, {}});
          continue;
        }

        spdlog::info("[tool execute] name={} args={}",
                     tool_call.name, tool_call.arguments.dump());
        std::string result = tools.Execute(tool_call.name, tool_call.arguments);
        spdlog::info("[tool result] name={} result={}", tool_call.name, result);
        messages.push_back({"tool", result, tool_call.id, {}});
        context.push_back({"tool", result, tool_call.id, {}});
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

// ---- 网关服务器命令 ----
int GatewayCommand(int argc, char** argv) {
  try {
    bool background = false;
    for (int i = 1; i < argc; ++i) {
      if (std::string(argv[i]) == "--background") background = true;
    }

    auto cfg = quantclaw::Config::Load();
    quantclaw::gateway::GatewayServer server(cfg);

    if (background) {
      quantclaw::platform::ServiceManager daemon;
      daemon.WritePid(getpid());
      spdlog::info("Gateway running in background, pid={}", getpid());
    }

    server.Run();
    return 0;
  } catch (const std::exception& e) {
    spdlog::error("Gateway server error: {}", e.what());
    return 1;
  }
}

// ---- 网关守护进程管理命令 ----
int GatewayDaemonCommand(int argc, char** argv) {
  quantclaw::platform::ServiceManager daemon;

  std::string sub = "status";
  if (argc >= 3) sub = argv[2];

  if (sub == "install") {
    int port = quantclaw::platform::kDefaultGatewayPort;
    if (argc >= 4) port = std::stoi(argv[3]);
    return daemon.Install(port);
  }
  if (sub == "uninstall") return daemon.Uninstall();
  if (sub == "start") return daemon.Start();
  if (sub == "stop") return daemon.Stop();
  if (sub == "restart") return daemon.Restart();
  if (sub == "status") return daemon.Status();

  std::cerr << "Usage: quantclaw gateway {install|uninstall|start|stop|restart|status} [port]\n";
  return 1;
}

// ---- Web UI 命令 ----
int WebCommand(int /*argc*/, char** /*argv*/) {
  try {
    auto cfg = quantclaw::Config::Load();
    quantclaw::web::WebServer server(cfg);
    server.Run();
    return 0;
  } catch (const std::exception& e) {
    spdlog::error("Web UI server error: {}", e.what());
    return 1;
  }
}

// ---- MCP Server 命令 ----
// 以 stdio MCP server 模式运行，把当前 QuantClaw 工具集导出为 MCP tools。
int MCPServerCommand(int /*argc*/, char** /*argv*/) {
  try {
    quantclaw::tools::ToolRegistry tools;
    tools.Register(std::make_unique<quantclaw::tools::CalculatorTool>());
    spdlog::info("[mcp-server] registered calculator");

    // 可同时导出通过 MCP 接入的外部工具，实现工具链的二次暴露。
    RegisterMcpTools(tools);

    quantclaw::mcp::MCPServer server(tools);
    server.Run();
    return 0;
  } catch (const std::exception& e) {
    spdlog::error("MCP server error: {}", e.what());
    return 1;
  }
}

// ---- 清空历史命令 ----
int ClearCommand(int /*argc*/, char** /*argv*/) {
  quantclaw::session::ChatHistory().Clear();
  spdlog::info("History cleared.");
  return 0;
}

// ---- 模型命令 ----
int ModelsCommand(int argc, char** argv) {
  auto cfg = quantclaw::Config::Load();
  auto registry = BuildProviderRegistry(cfg);

  if (argc >= 3 && std::string(argv[2]) == "set") {
    if (argc < 4) {
      std::cerr << "Usage: quantclaw models set <model>\n";
      return 1;
    }
    std::cerr << "Model set is not yet persisted; use CLAW_MODEL env var.\n";
    return 1;
  }

  std::cout << "Current model: " << cfg.model << "\n";
  std::cout << "Providers:\n";
  for (const auto& id : registry.ProviderIds()) {
    std::cout << "  " << id << "\n";
  }
  auto aliases = registry.Aliases();
  if (!aliases.empty()) {
    std::cout << "Aliases:\n";
    for (const auto& [alias, target] : aliases) {
      std::cout << "  " << alias << " -> " << target << "\n";
    }
  }
  if (!cfg.fallback_chain.empty()) {
    std::cout << "Fallback chain:\n";
    for (const auto& m : cfg.fallback_chain) {
      std::cout << "  " << m << "\n";
    }
  }
  return 0;
}

// ---- 配置命令 ----
int ConfigCommand(int /*argc*/, char** /*argv*/) {
  auto cfg = quantclaw::Config::Load();
  std::cout << "Config path: " << quantclaw::Config::DefaultPath() << "\n";
  std::cout << "model: " << cfg.model << "\n";
  std::cout << "base_url: " << cfg.base_url << "\n";
  std::cout << "context_window: " << cfg.context_window << "\n";
  std::cout << "max_tokens: " << cfg.max_tokens << "\n";
  std::cout << "providers: " << cfg.providers.size() << "\n";
  std::cout << "aliases: " << cfg.aliases.size() << "\n";
  std::cout << "fallback_chain: " << cfg.fallback_chain.size() << "\n";
  std::cout << "embedding enabled: "
            << (cfg.embedding.api_key.empty() ? "no" : "yes") << "\n";
  if (!cfg.embedding.model.empty()) {
    std::cout << "embedding model: " << cfg.embedding.model << "\n";
  }
  std::cout << "tool_permissions allow: " << cfg.tool_permissions.allow.size()
            << " deny: " << cfg.tool_permissions.deny.size() << "\n";
  std::cout << "exec_approval mode: " << cfg.exec_approval.mode << "\n";
  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::debug);

  quantclaw::platform::ensure_dir(quantclaw::platform::base_dir());

  quantclaw::cli::CliManager cli;

  // 默认行为：裸消息映射为 chat 命令。
  cli.AddCommand({"chat",
                  "Send a message to the agent",
                  {},
                  ChatCommand});

  cli.AddCommand({"--gateway",
                  "Run the WebSocket Gateway server",
                  {"gateway"},
                  GatewayCommand});

  cli.AddCommand({"gateway",
                  "Manage gateway daemon (start/stop/restart/status/install/uninstall)",
                  {},
                  GatewayDaemonCommand});

  cli.AddCommand({"--web",
                  "Run the Web UI server",
                  {"web"},
                  WebCommand});

  cli.AddCommand({"--mcp-server",
                  "Run the stdio MCP server",
                  {"mcp-server"},
                  MCPServerCommand});

  cli.AddCommand({"--clear",
                  "Clear chat history",
                  {"clear"},
                  ClearCommand});

  cli.AddCommand({"models",
                  "Show current model, providers and aliases",
                  {"m"},
                  ModelsCommand});

  cli.AddCommand({"config",
                  "Show current configuration",
                  {"c"},
                  ConfigCommand});

  return cli.Run(argc, argv);
}
