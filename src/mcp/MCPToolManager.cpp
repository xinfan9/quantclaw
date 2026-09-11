#include "MCPToolManager.hpp"

namespace quantclaw::mcp {

void MCPToolManager::AddServer(const std::string& id,
                               const std::string& command,
                               const std::vector<std::string>& args) {
  servers_.push_back({id, std::make_shared<MCPClient>(command, args)});
}

std::vector<MCPToolDef> MCPToolManager::ListAllTools() const {
  std::vector<MCPToolDef> all;
  for (const auto& entry : servers_) {
    auto tools = entry.client->ListTools();
    for (auto& tool : tools) {
      tool.name = entry.id + "/" + tool.name;
    }
    all.insert(all.end(), tools.begin(), tools.end());
  }
  return all;
}

std::vector<MCPToolManager::ToolEntry> MCPToolManager::ListAllToolEntries() const {
  std::vector<ToolEntry> all;
  for (const auto& entry : servers_) {
    auto tools = entry.client->ListTools();
    for (auto& tool : tools) {
      tool.name = entry.id + "/" + tool.name;
      all.push_back({entry.id, tool});
    }
  }
  return all;
}

std::string MCPToolManager::CallTool(const std::string& server_id,
                                     const std::string& tool_name,
                                     const nlohmann::json& arguments) {
  for (const auto& entry : servers_) {
    if (entry.id != server_id) continue;
    return entry.client->CallTool(tool_name, arguments);
  }
  throw std::runtime_error("MCP server not found: " + server_id);
}

std::vector<std::string> MCPToolManager::ServerIds() const {
  std::vector<std::string> ids;
  for (const auto& entry : servers_) ids.push_back(entry.id);
  return ids;
}

}  // namespace quantclaw::mcp
