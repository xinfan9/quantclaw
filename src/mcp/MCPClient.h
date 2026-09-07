#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace quantclaw::mcp {

struct MCPToolDef {
  std::string name;
  std::string description;
  nlohmann::json input_schema;
};

class MCPClient {
public:
  explicit MCPClient(const std::string& command, const std::vector<std::string>& args = {});
  ~MCPClient();

  std::vector<MCPToolDef> ListTools();
  std::string CallTool(const std::string& name, const nlohmann::json& arguments);

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
  nlohmann::json SendRequest(const std::string& method, const nlohmann::json& params);
};


}