//
// 由 xinfang 创建于 2026/9/7.
//

#include "MCPClient.h"

#include "StdioTransport.h"
#include "spdlog/spdlog.h"

namespace quantclaw::mcp {

struct MCPClient::Impl {
  std::unique_ptr<StdioTransport> transport;
  int request_id = 0;
};

MCPClient::MCPClient(const std::string& command, const std::vector<std::string>& args) : _impl(std::make_unique<Impl>()) {
  _impl->transport = std::make_unique<StdioTransport>(command, args);

  nlohmann::json init_params = {
    {"protocolVersion", "2024-11-05"},
    {"capabilities", nlohmann::json::object()},
    {"clientInfo", {
      {"name", "quantclaw"},
      {"version", "0.1.0"}
    }}
  };

  SendRequest("initialize", init_params);

  nlohmann::json notification = {
      {"jsonrpc", "2.0"},
      {"method", "notifications/initialized"},
  };

  _impl->transport->Send(notification.dump());
  spdlog::info("Sent initialization notification, {}", command);
}

MCPClient::~MCPClient() = default;

nlohmann::json MCPClient::SendRequest(const std::string& method, const nlohmann::json& params) {
  ++_impl->request_id;
  nlohmann::json request = {
      {"jsonrpc", "2.0"},
      {"id", _impl->request_id},
      {"method", method},
      {"params", params},
  };

  _impl->transport->Send(request.dump());
  std::string response_str = _impl->transport->Receive();

  auto response = nlohmann::json::parse(response_str, nullptr, false);
  if (response.is_discarded()) throw std::runtime_error("Invalid JSON response:" + response_str);

  if (response.contains("error")) throw std::runtime_error("MCP returned an error: " + response["error"].dump());

  return response.value("result", nlohmann::json::object());
}

std::vector<MCPToolDef> MCPClient::ListTools() {
  auto result = SendRequest("tools/list", nlohmann::json::object());

  std::vector<MCPToolDef> tools;

  if (result.contains("tools") && result["tools"].is_array()) {
    for (const auto& t : result["tools"]) {
      MCPToolDef def;
      def.name = t.value("name", "");
      def.description = t.value("description", "");
      def.input_schema = t.value("inputSchema", nlohmann::json::object());
      tools.push_back(def);
    }
  }

  spdlog::info("Listed {} tools", tools.size());
  return tools;
}

std::string MCPClient::CallTool(const std::string& name, const nlohmann::json& arguments) {
  nlohmann::json params = {
    {"name", name},
    {"arguments", arguments}
  };

  auto result = SendRequest("tools/call", params);
  if (result.contains("structuredContent")) return result["structuredContent"].dump();
  if (result.contains("content") && result["content"].is_array()) {
    std::string text;
    for (const auto& item : result["content"]) {
      if (item.value("type", "") == "text") {
        if (!text.empty()) text += "\n";
        text += item.value("text", "");
      }
    }
    return text;
  }
  return result.dump();
}



}
