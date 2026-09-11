#include "SubagentManager.h"

#include <spdlog/spdlog.h>

namespace quantclaw::core {

SubagentManager::SubagentManager(SubagentConfig cfg) : config_(std::move(cfg)) {}

void SubagentManager::SetRunner(SubagentRunner runner) {
  runner_ = std::move(runner);
}

SubagentResult SubagentManager::Run(const std::string& task,
                                    const std::string& /*model*/, int depth) {
  SubagentResult result;

  if (task.empty()) {
    result.error = "子任务描述不能为空";
    return result;
  }

  // 检查递归深度，防止无限 spawn。
  if (depth > config_.max_depth) {
    result.error = "子任务递归深度超过限制: " + std::to_string(config_.max_depth);
    return result;
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    if (active_count_ >= config_.max_children) {
      result.error = "并发子任务数量超过限制: " + std::to_string(config_.max_children);
      return result;
    }
    ++active_count_;
    ++total_spawned_;
  }

  spdlog::info("[subagent] 启动子任务 depth={} task={}", depth, task);

  try {
    if (!runner_) {
      throw std::runtime_error("SubagentRunner 未设置");
    }
    result.output = runner_(task);
    result.success = true;
    spdlog::info("[subagent] 子任务完成 depth={}", depth);
  } catch (const std::exception& e) {
    result.error = e.what();
    spdlog::error("[subagent] 子任务失败 depth={}: {}", depth, e.what());
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    --active_count_;
  }

  return result;
}

int SubagentManager::ActiveCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return active_count_;
}

int SubagentManager::TotalSpawned() const {
  std::lock_guard<std::mutex> lock(mu_);
  return total_spawned_;
}

}  // namespace quantclaw::core
