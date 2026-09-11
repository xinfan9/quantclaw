#include <gtest/gtest.h>

#include <sstream>

#include "mcp/MCPServer.hpp"
#include "tools/CalculatorTool.h"
#include "tools/ToolRegistry.h"

namespace quantclaw::mcp {

// 从输出中提取 initialize 结果中的 protocolVersion。
std::string ExtractProtocolVersion(const std::string& output) {
  auto j = nlohmann::json::parse(output);
  return j.value("result", nlohmann::json::object())
      .value("protocolVersion", "");
}

TEST(MCPServerTest, InitializeRequest) {
  tools::ToolRegistry registry;
  registry.Register(std::make_unique<tools::CalculatorTool>());

  MCPServer server(registry);

  std::stringstream input;
  input << R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})" << "\n";
  std::stringstream output;

  // Run 会阻塞读取直到 EOF，因此只发送一条请求后关闭输入。
  server.Stop();
  // 注意：Stop 设置 running_ = false，但 Run 可能已经开始循环。
  // 更可靠的做法是在单独线程运行并在发送后关闭输入流。这里简化处理。

  // 由于 MCPServer::Run 设计为持续运行，单元测试中直接调用较为困难，
  // 因此仅验证 HandleInitialize 的返回值。
  auto result = server.HandleInitialize(nlohmann::json::object());
  EXPECT_EQ(result["protocolVersion"], "2024-11-05");
}

TEST(MCPServerTest, ToolsListIncludesCalculatorAndChat) {
  tools::ToolRegistry registry;
  registry.Register(std::make_unique<tools::CalculatorTool>());

  MCPServer server(registry);
  auto result = server.HandleToolsList(nlohmann::json::object());

  ASSERT_TRUE(result.contains("tools"));
  auto tools = result["tools"];

  bool found_calculator = false;
  bool found_chat = false;
  for (const auto& tool : tools) {
    std::string name = tool.value("name", "");
    if (name == "calculator") found_calculator = true;
    if (name == "quantclaw_chat") found_chat = true;
  }
  EXPECT_TRUE(found_calculator);
  EXPECT_TRUE(found_chat);
}

TEST(MCPServerTest, ToolsCallCalculator) {
  tools::ToolRegistry registry;
  registry.Register(std::make_unique<tools::CalculatorTool>());

  MCPServer server(registry);
  nlohmann::json params;
  params["name"] = "calculator";
  params["arguments"] = nlohmann::json::object({{"expression", "1 + 2"}});

  auto result = server.HandleToolsCall(params);
  ASSERT_TRUE(result.contains("content"));
  EXPECT_NE(result["content"][0]["text"].get<std::string>().find("3"),
            std::string::npos);
}

}  // namespace quantclaw::mcp
