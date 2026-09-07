//
// Created by xinfang on 2026/9/7.
//

#include "MCPTool.h"

namespace quantclaw::mcp {
MCPTool::MCPTool(std::shared_ptr<MCPClient> client, MCPToolDef def) : _client(std::move(client)), _def(std::move(def)) {
  name = _def.name;
  description = _def.description;
  parameters = _def.input_schema;
}

std::string MCPTool::Execute(const nlohmann::json& args) const {
  return _client->CallTool(_def.name, args);
}

}