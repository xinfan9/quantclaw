#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "MCPClient.h"

namespace quantclaw::mcp {

// 管理多个 MCP client/server 连接，统一对外提供工具列表与调用入口。
class MCPToolManager {
 public:
  // 单个服务器上的工具定义，包含原始工具与所属服务器 ID。
  struct ToolEntry {
    std::string server_id;
    MCPToolDef def;
  };

  // 添加一个 stdio MCP 服务器
  void AddServer(const std::string& id, const std::string& command,
                 const std::vector<std::string>& args);

  // 列出所有可用 MCP 工具（工具名已加上 server_id/ 前缀）
  std::vector<MCPToolDef> ListAllTools() const;

  // 列出所有可用 MCP 工具及其所属服务器 ID
  std::vector<ToolEntry> ListAllToolEntries() const;

  // 调用指定服务器上的工具
  std::string CallTool(const std::string& server_id,
                       const std::string& tool_name,
                       const nlohmann::json& arguments);

  // 获取服务器 ID 列表
  std::vector<std::string> ServerIds() const;

 private:
  struct ServerEntry {
    std::string id;
    std::shared_ptr<MCPClient> client;
  };
  std::vector<ServerEntry> servers_;
};

}  // namespace quantclaw::mcp
