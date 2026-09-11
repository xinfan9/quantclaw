#pragma once

#include <functional>
#include <mutex>
#include <string>

namespace quantclaw::core {

// 子 agent 执行配置：限制递归深度与并发数量，防止无限 spawn。
struct SubagentConfig {
  int max_depth = 3;      // 最大递归深度
  int max_children = 5;   // 同一层级最多同时存在的子任务数
};

// 子任务执行结果
struct SubagentResult {
  bool success = false;   // 是否成功完成
  std::string output;     // 成功时的输出摘要
  std::string error;      // 失败时的错误信息
};

// 子任务执行器签名：接收任务描述，返回执行结果字符串。
using SubagentRunner = std::function<std::string(const std::string& task)>;

// 子 agent 管理器：负责子任务的调度、深度限制与并发控制。
// 当前为同步实现，子任务会阻塞等待执行完成。
class SubagentManager {
 public:
  explicit SubagentManager(SubagentConfig cfg = {});

  // 设置实际执行子任务的回调函数；未设置时 Run 直接返回错误。
  void SetRunner(SubagentRunner runner);

  // 执行一个子任务。depth 用于内部递归深度控制，外部调用通常传 0。
  SubagentResult Run(const std::string& task, const std::string& model = "",
                     int depth = 0);

  // 当前活跃子任务数量
  int ActiveCount() const;

  // 累计 spawn 次数
  int TotalSpawned() const;

 private:
  SubagentConfig config_;
  SubagentRunner runner_;
  mutable std::mutex mu_;
  int active_count_ = 0;
  int total_spawned_ = 0;
};

}  // namespace quantclaw::core
