#include "QuantClawMCPTools.hpp"

namespace quantclaw::mcp {

nlohmann::json QuantClawChatToolSchema() {
  // 显式使用 nlohmann::json::object 避免 initializer_list 被误解析为数组。
  nlohmann::json input_schema = nlohmann::json::object();
  input_schema["type"] = "object";

  nlohmann::json properties = nlohmann::json::object();
  properties["message"] = nlohmann::json::object({
      {"type", "string"},
      {"description", "User message"},
  });
  properties["session_key"] = nlohmann::json::object({
      {"type", "string"},
      {"description", "Session key"},
      {"default", "default"},
  });
  input_schema["properties"] = properties;

  input_schema["required"] = nlohmann::json::array({"message"});

  return nlohmann::json::object({
      {"name", "quantclaw_chat"},
      {"description", "Send a message to the local QuantClaw agent"},
      {"inputSchema", input_schema},
  });
}

}  // namespace quantclaw::mcp
