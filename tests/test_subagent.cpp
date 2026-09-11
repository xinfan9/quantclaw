#include <gtest/gtest.h>

#include "core/SubagentManager.h"

namespace quantclaw::core {

TEST(SubagentManagerTest, RunWithoutRunnerFails) {
  SubagentManager manager;
  auto result = manager.Run("test task");
  EXPECT_FALSE(result.success);
  EXPECT_NE(result.error.find("未设置"), std::string::npos);
}

TEST(SubagentManagerTest, RunWithRunnerSuccess) {
  SubagentManager manager;
  manager.SetRunner([](const std::string& task) -> std::string {
    return "完成: " + task;
  });

  auto result = manager.Run("子任务");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.output, "完成: 子任务");
  EXPECT_EQ(manager.TotalSpawned(), 1);
}

TEST(SubagentManagerTest, EmptyTaskFails) {
  SubagentManager manager;
  manager.SetRunner([](const std::string&) -> std::string { return "ok"; });

  auto result = manager.Run("");
  EXPECT_FALSE(result.success);
}

TEST(SubagentManagerTest, DepthLimit) {
  SubagentConfig cfg;
  cfg.max_depth = 1;
  SubagentManager manager(cfg);
  manager.SetRunner([](const std::string& task) -> std::string { return task; });

  EXPECT_TRUE(manager.Run("depth=0").success);
  EXPECT_FALSE(manager.Run("depth=1", "", 2).success);
}

}  // namespace quantclaw::core
