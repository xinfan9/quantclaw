#pragma once

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

namespace quantclaw::tools {
class ToolRegistry;
}

namespace quantclaw::mcp {

// 基于 stdio 的 MCP Server：将 my_claw ToolRegistry 导出为 MCP tools
class MCPServer {
 public:
  explicit MCPServer(const tools::ToolRegistry& tools);

  // 从标准输入读取一行请求，处理并写入标准输出
  void Run(std::istream& in = std::cin, std::ostream& out = std::cout);

  void Stop();

  // 核心 JSON-RPC 处理方法；声明为 public 以便单元测试直接调用。
  nlohmann::json HandleInitialize(const nlohmann::json& params);
  nlohmann::json HandleToolsList(const nlohmann::json& params);
  nlohmann::json HandleToolsCall(const nlohmann::json& params);

  nlohmann::json MakeResponse(const nlohmann::json& id,
                              const nlohmann::json& result);
  nlohmann::json MakeError(const nlohmann::json& id, int code,
                           const std::string& message);

 private:
  const tools::ToolRegistry& tools_;
  std::atomic<bool> running_{false};
};

}  // namespace quantclaw::mcp
