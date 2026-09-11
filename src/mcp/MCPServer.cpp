#include "MCPServer.hpp"

#include <iostream>
#include <spdlog/spdlog.h>

#include "../tools/ToolRegistry.h"
#include "QuantClawMCPTools.hpp"

namespace quantclaw::mcp {

MCPServer::MCPServer(const tools::ToolRegistry& tools) : tools_(tools) {}

void MCPServer::Stop() { running_ = false; }

void MCPServer::Run(std::istream& in, std::ostream& out) {
  running_ = true;

  while (running_ && !in.eof()) {
    std::string line;
    if (!std::getline(in, line)) break;
    if (line.empty()) continue;

    try {
      auto request = nlohmann::json::parse(line);
      auto id = request.value("id", nlohmann::json(nullptr));
      std::string method = request.value("method", "");
      auto params = request.value("params", nlohmann::json::object());

      nlohmann::json response;
      if (method == "initialize") {
        response = MakeResponse(id, HandleInitialize(params));
      } else if (method == "notifications/initialized") {
        continue;  // 无需响应
      } else if (method == "tools/list") {
        response = MakeResponse(id, HandleToolsList(params));
      } else if (method == "tools/call") {
        response = MakeResponse(id, HandleToolsCall(params));
      } else {
        response = MakeError(id, -32601, "Method not found");
      }

      out << response.dump() << "\n";
      out.flush();
    } catch (const std::exception& e) {
      spdlog::error("MCP server error: {}", e.what());
      auto err = MakeError(nullptr, -32700, e.what());
      out << err.dump() << "\n";
      out.flush();
    }
  }
}

nlohmann::json MCPServer::HandleInitialize(const nlohmann::json& /*params*/) {
  return {
      {"protocolVersion", "2024-11-05"},
      {"capabilities", {{"tools", nlohmann::json::object()}}},
      {"serverInfo", {{"name", "my_claw"}, {"version", "0.3.0"}}},
  };
}

nlohmann::json MCPServer::HandleToolsList(const nlohmann::json& /*params*/) {
  nlohmann::json tools = nlohmann::json::array();
  for (const auto& name : tools_.Names()) {
    auto schema = tools_.GetSchema(name);
    tools.push_back({{"name", name},
                     {"description", tools_.GetDescription(name)},
                     {"inputSchema", schema.is_null() ? nlohmann::json::object() : schema}});
  }
  // 额外暴露 QuantClaw 内置 chat 工具，使外部 MCP client 可以向本地 agent 发消息。
  tools.push_back(QuantClawChatToolSchema());
  return {{"tools", tools}};
}

nlohmann::json MCPServer::HandleToolsCall(const nlohmann::json& params) {
  std::string name = params.value("name", "");
  auto arguments = params.value("arguments", nlohmann::json::object());

  if (name.empty()) {
    return {{"content", {{"type", "text"}, {"text", "Missing tool name"}}},
            {"isError", true}};
  }

  // 处理 QuantClaw 内置 chat 工具：将外部消息转发给本地 agent。
  if (name == "quantclaw_chat") {
    std::string message = arguments.value("message", "");
    if (message.empty()) {
      return {{"content",
               nlohmann::json::array({
                   {{"type", "text"}, {"text", "Missing message"}},
               })},
              {"isError", true}};
    }
    // 当前简化实现：直接返回确认信息。后续可通过 GatewayClient 或本地 IPC 转发给运行中的 agent。
    std::string reply = "[quantclaw_chat] 已收到消息: " + message;
    return {{"content",
             nlohmann::json::array({
                 {{"type", "text"}, {"text", reply}},
             })}};
  }

  try {
    std::string result = tools_.Execute(name, arguments);
    return {{"content",
             nlohmann::json::array({
                 {{"type", "text"}, {"text", result}},
             })}};
  } catch (const std::exception& e) {
    return {{"content",
             nlohmann::json::array({
                 {{"type", "text"}, {"text", e.what()}},
             })},
            {"isError", true}};
  }
}

nlohmann::json MCPServer::MakeResponse(const nlohmann::json& id,
                                       const nlohmann::json& result) {
  return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

nlohmann::json MCPServer::MakeError(const nlohmann::json& id, int code,
                                    const std::string& message) {
  return {{"jsonrpc", "2.0"},
          {"id", id},
          {"error", {{"code", code}, {"message", message}}}};
}

}  // namespace quantclaw::mcp
