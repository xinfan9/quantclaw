#pragma once
#include "MCPClient.h"
#include "MCPToolManager.hpp"
#include "tools/Tool.h"

namespace quantclaw::mcp {

// 包装外部 MCP 工具，使其可以作为 quantclaw::tools::Tool 被 LLM 调用。
// 支持两种工作模式：
//   1. 直接持有 MCPClient，调用时直接转发；
//   2. 持有 MCPToolManager 与 server_id，由 manager 按服务器分发调用。
class MCPTool : public quantclaw::tools::Tool {
 public:
  // 直接持有 MCPClient 的传统模式
  MCPTool(std::shared_ptr<MCPClient> client, MCPToolDef def);
  // 通过 MCPToolManager 分发的模式，支持多服务器管理
  MCPTool(std::shared_ptr<MCPToolManager> manager, const std::string& server_id,
          MCPToolDef def);

  std::string Execute(const nlohmann::json& args) const override;

 private:
  std::shared_ptr<MCPClient> _client;
  std::shared_ptr<MCPToolManager> _manager;
  std::string _server_id;
  MCPToolDef _def;
};

}  // namespace quantclaw::mcp
