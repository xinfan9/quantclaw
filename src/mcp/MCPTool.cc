//
// 由 xinfang 创建于 2026/9/7.
//

#include "MCPTool.h"

namespace quantclaw::mcp {

// 传统模式：直接绑定到单个 MCPClient。
MCPTool::MCPTool(std::shared_ptr<MCPClient> client, MCPToolDef def)
    : _client(std::move(client)),
      _manager(nullptr),
      _server_id(),
      _def(std::move(def)) {
  name = _def.name;
  description = _def.description;
  parameters = _def.input_schema;
}

// Manager 模式：通过 MCPToolManager 按 server_id 分发，支持多服务器。
MCPTool::MCPTool(std::shared_ptr<MCPToolManager> manager,
                 const std::string& server_id, MCPToolDef def)
    : _client(nullptr),
      _manager(std::move(manager)),
      _server_id(server_id),
      _def(std::move(def)) {
  name = _def.name;
  description = _def.description;
  parameters = _def.input_schema;
}

std::string MCPTool::Execute(const nlohmann::json& args) const {
  // 优先使用 manager 模式，支持多服务器场景下的统一调度。
  if (_manager) {
    // _def.name 可能已被 MCPToolManager 加上 server_id/ 前缀，调用前需剥离前缀。
    std::string tool_name = _def.name;
    size_t slash = tool_name.find('/');
    if (slash != std::string::npos) {
      tool_name = tool_name.substr(slash + 1);
    }
    return _manager->CallTool(_server_id, tool_name, args);
  }

  // 回退到传统模式：直接由绑定的 MCPClient 调用。
  if (_client) {
    return _client->CallTool(_def.name, args);
  }

  throw std::runtime_error("MCPTool has no valid client or manager");
}

}  // namespace quantclaw::mcp
