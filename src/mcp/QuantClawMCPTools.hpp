#pragma once

#include <nlohmann/json.hpp>

namespace quantclaw::mcp {

// QuantClaw 内置 MCP 工具集合（通过 stdio 暴露给外部 MCP client）
// 实际逻辑在 MCPServer 中实现；这里仅声明额外工具 schema

nlohmann::json QuantClawChatToolSchema();

}  // namespace quantclaw::mcp
