#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "tools/Tool.h"
#include "tools/ToolRegistry.h"

namespace quantclaw::tools {

// 测试用的简单工具：返回两个数之和。
class AddTool : public Tool {
 public:
  AddTool() {
    name = "add";
    description = "Add two numbers";
    parameters = nlohmann::json::object({
        {"type", "object"},
        {"properties",
         nlohmann::json::object({
             {"a", nlohmann::json::object({{"type", "number"}})},
             {"b", nlohmann::json::object({{"type", "number"}})},
         })},
        {"required", nlohmann::json::array({"a", "b"})},
    });
  }

  std::string Execute(const nlohmann::json& args) const override {
    int a = args.value("a", 0);
    int b = args.value("b", 0);
    return std::to_string(a + b);
  }
};

TEST(ToolRegistryTest, RegisterAndExecute) {
  ToolRegistry registry;
  EXPECT_TRUE(registry.Empty());

  registry.Register(std::make_unique<AddTool>());
  EXPECT_FALSE(registry.Empty());
  EXPECT_TRUE(registry.Has("add"));

  auto result = registry.Execute("add", nlohmann::json::object({{"a", 2}, {"b", 3}}));
  EXPECT_EQ(result, "5");
}

TEST(ToolRegistryTest, GetDefinitions) {
  ToolRegistry registry;
  registry.Register(std::make_unique<AddTool>());

  auto defs = registry.GetDefinitions();
  ASSERT_EQ(defs.size(), 1);
  EXPECT_EQ(defs[0]["function"]["name"], "add");
}

TEST(ToolRegistryTest, ExecuteUnknownToolThrows) {
  ToolRegistry registry;
  EXPECT_THROW(registry.Execute("unknown", nlohmann::json::object()),
               std::runtime_error);
}

}  // namespace quantclaw::tools
