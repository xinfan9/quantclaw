#pragma once
#include "MCPClient.h"
#include "tools/Tool.h"

namespace quantclaw::mcp {
class MCPTool : public quantclaw::tools::Tool {
public:
  MCPTool(std::shared_ptr<MCPClient> client, MCPToolDef def);
  std::string Execute(const nlohmann::json& args) const override;

private:
  std::shared_ptr<MCPClient> _client;
  MCPToolDef _def;
};
}
