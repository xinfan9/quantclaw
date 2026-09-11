#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace quantclaw::core {

// Cron 任务定义
struct CronJob {
  std::string id;
  std::string name;
  std::string schedule;     // cron 表达式，如 "*/5 * * * *"
  std::string message;      // 触发时发送给 agent 的消息
  std::string session_key;  // 所属 session
  bool enabled = true;
  std::chrono::system_clock::time_point last_run;
  std::chrono::system_clock::time_point next_run;

  nlohmann::json ToJson() const;
  static CronJob FromJson(const nlohmann::json& j);
};

// 简单 cron 表达式求值器：minute hour day-of-month month day-of-week
// 支持 *, */N, N, N-M, N,M,O
class CronExpression {
 public:
  explicit CronExpression(const std::string& expr);

  // 判断给定时间是否匹配 cron 表达式
  bool Matches(const std::tm& tm) const;

  // 计算 after 之后的下一次运行时间
  std::chrono::system_clock::time_point NextAfter(
      std::chrono::system_clock::time_point after) const;

 private:
  struct Field {
    std::vector<int> values;  // 空表示通配符
  };

  Field minute_;
  Field hour_;
  Field day_of_month_;
  Field month_;
  Field day_of_week_;

  static Field ParseField(const std::string& field, int min, int max);
  static bool FieldMatches(const Field& f, int value);
};

// 基于 tick 的持久化 cron 调度器
class CronScheduler {
 public:
  using JobHandler = std::function<void(const CronJob&)>;

  explicit CronScheduler(std::shared_ptr<spdlog::logger> logger = nullptr);
  ~CronScheduler();

  // 从文件加载任务
  void Load(const std::string& filepath);

  // 保存任务到文件
  void Save(const std::string& filepath) const;

  // 添加新任务，返回任务 ID
  std::string AddJob(const std::string& name, const std::string& schedule,
                     const std::string& message,
                     const std::string& session_key = "default");

  // 删除任务
  bool RemoveJob(const std::string& id);

  // 列出所有任务
  std::vector<CronJob> ListJobs() const;

  // 启动调度循环
  void Start(JobHandler handler);

  // 停止调度
  void Stop();

  bool IsRunning() const { return running_; }

 private:
  void SchedulerLoop();
  std::string GenerateId() const;

  std::shared_ptr<spdlog::logger> logger_;
  mutable std::mutex mu_;
  std::vector<CronJob> jobs_;
  JobHandler handler_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::string storage_path_;
};

}  // namespace quantclaw::core
