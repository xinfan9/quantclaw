#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace quantclaw::gateway {

// 队列模式：新消息如何与当前同 session 正在运行的任务交互
enum class QueueMode {
  kCollect,       // 收集后续消息，当前任务结束后再批量处理
  kFollowup,      // 排队一个后续任务，等当前任务结束后执行
  kSteer,         // 将新消息注入当前运行上下文（本实现中暂不支持运行时注入，按 kCollect 处理）
  kSteerBacklog,  // 运行时活跃则 steer，否则正常排队（暂按 kCollect 处理）
  kInterrupt,     // 中止当前任务，用新消息重新开始
};

QueueMode QueueModeFromString(const std::string& s);
std::string QueueModeToString(QueueMode mode);

// 队列溢出策略
enum class DropPolicy {
  kDropOldest,  // 丢弃最旧的消息
  kReject,      // 拒绝新消息
};

DropPolicy DropPolicyFromString(const std::string& s);
std::string DropPolicyToString(DropPolicy policy);

// 队列全局配置
struct QueueConfig {
  int max_concurrent = 4;             // 最大并发任务数
  int debounce_ms = 1000;             // 消息去抖等待毫秒
  int cap = 20;                       // 单 session 队列上限
  DropPolicy drop = DropPolicy::kDropOldest;
  QueueMode default_mode = QueueMode::kCollect;

  static QueueConfig FromJson(const nlohmann::json& json);
  nlohmann::json ToJson() const;
};

// 队列中的单个命令
struct QueuedCommand {
  std::string id;
  std::string session_key;
  std::string message;
  nlohmann::json params;
  std::string connection_id;
  std::string rpc_request_id;
  QueueMode mode = QueueMode::kCollect;
  std::chrono::steady_clock::time_point enqueued_at;

  enum class State {
    kPending,
    kActive,
    kComplete,
    kDropped,
  };
  State state = State::kPending;
};

// Agent 执行器：在 worker 线程中运行，阻塞直到本次 agent 调用结束
using AgentExecutor = std::function<nlohmann::json(
    const QueuedCommand& cmd,
    std::function<void(const std::string& event, const nlohmann::json& payload)>
        event_sink)>;

// 发送最终 RPC 响应到客户端
using ResponseSender = std::function<void(
    const std::string& connection_id, const std::string& rpc_request_id,
    bool ok, const nlohmann::json& payload_or_error)>;

// 发送 RPC 事件到指定客户端
using EventSender = std::function<void(
    const std::string& connection_id, const std::string& event_name,
    const nlohmann::json& payload)>;

// 单个 session 的队列（lane），保证同 session 串行执行
class SessionLane {
 public:
  explicit SessionLane(const std::string& session_key);

  const std::string& SessionKey() const { return session_key_; }

  // 配置覆盖
  void SetMode(QueueMode mode) { mode_ = mode; }
  QueueMode GetMode() const { return mode_; }
  void SetDebounceMs(int ms) { debounce_ms_ = ms; }
  int GetDebounceMs() const { return debounce_ms_; }
  void SetCap(int cap) { cap_ = cap; }
  int GetCap() const { return cap_; }
  void SetDropPolicy(DropPolicy policy) { drop_ = policy; }
  DropPolicy GetDropPolicy() const { return drop_; }

  // 入队
  void Enqueue(QueuedCommand cmd);
  bool HasPending() const;
  bool HasActive() const { return active_command_.has_value(); }
  const QueuedCommand& ActiveCommand() const { return *active_command_; }

  // 尝试激活下一个 pending 命令
  std::optional<QueuedCommand> TryActivate(
      std::chrono::steady_clock::time_point now);

  // 完成当前激活命令
  std::optional<QueuedCommand> CompleteActive();

  // 应用容量溢出策略，返回被丢弃的命令 ID
  std::vector<std::string> ApplyCapOverflow();

  // 取消指定 pending 命令
  bool CancelPending(const std::string& command_id);

  // 中断：清空 pending 并中止 active
  std::optional<std::string> InterruptActive();

  size_t PendingCount() const { return pending_.size(); }
  bool IsIdle() const { return !HasActive() && !HasPending(); }

  // 查看队列头部 pending 命令（用于调度器判断去抖是否到期）
  const QueuedCommand& PeekPendingFront() const { return pending_.front(); }

  nlohmann::json ToJson() const;

 private:
  std::string session_key_;
  QueueMode mode_ = QueueMode::kCollect;
  int debounce_ms_ = 1000;
  int cap_ = 20;
  DropPolicy drop_ = DropPolicy::kDropOldest;

  std::deque<QueuedCommand> pending_;
  std::optional<QueuedCommand> active_command_;
};

// 命令队列总控：调度线程从各 lane 取任务并分发给 worker
class CommandQueue {
 public:
  CommandQueue(const QueueConfig& config, AgentExecutor executor,
               ResponseSender response_sender, EventSender event_sender,
               std::shared_ptr<spdlog::logger> logger = nullptr);
  ~CommandQueue();

  // 启动调度线程
  void Start();

  // 停止处理，等待活跃任务完成
  void Stop();

  // 提交新命令，返回命令 ID
  std::string Submit(const std::string& session_key,
                     const std::string& message,
                     const nlohmann::json& params,
                     const std::string& connection_id,
                     const std::string& rpc_request_id, QueueMode mode);

  // 取消指定排队命令
  bool Cancel(const std::string& command_id);

  // 中止某 session 的当前任务
  bool AbortSession(const std::string& session_key);

  // 配置 session 覆盖
  void ConfigureSession(const std::string& session_key, QueueMode mode,
                        int debounce_ms = -1, int cap = -1,
                        const std::string& drop = "");

  // 查询队列状态
  nlohmann::json SessionQueueStatus(const std::string& session_key) const;
  nlohmann::json GlobalStatus() const;

  // 更新全局配置
  void SetConfig(const QueueConfig& config);

 private:
  std::string GenerateId() const;
  SessionLane& GetLane(const std::string& session_key);
  void DispatcherLoop();
  bool HasWork();
  void ExecuteCommand(QueuedCommand cmd);
  void EmitQueueEvent(const QueuedCommand& cmd, const std::string& event_type,
                      const nlohmann::json& data = {});

  QueueConfig config_;
  AgentExecutor executor_;
  ResponseSender response_sender_;
  EventSender event_sender_;
  std::shared_ptr<spdlog::logger> logger_;

  mutable std::mutex mu_;
  std::condition_variable cv_;

  std::unordered_map<std::string, std::unique_ptr<SessionLane>> lanes_;
  std::thread dispatcher_;
  std::atomic<bool> running_{false};
  std::atomic<int> active_count_{0};
  std::vector<std::thread> workers_;

  std::unordered_map<std::string, std::string> command_to_session_;
};

}  // namespace quantclaw::gateway
