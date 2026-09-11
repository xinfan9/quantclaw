#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include "ProviderError.h"

namespace quantclaw::providers {

// Provider 冷却追踪器：根据错误类型和连续失败次数计算冷却时间，避免频繁重试故障 Provider
class CooldownTracker {
public:
  // 判断指定 key 是否仍处于冷却期
  bool IsInCooldown(const std::string& key) const;

  // 记录一次失败，根据错误类型和连续失败次数延长冷却时间；retry_after_seconds 用于服务端指定的重试间隔
  void RecordFailure(const std::string& key, ProviderErrorKind kind, int retry_after_seconds = 0);

  // 记录一次成功，通常用于重置该 key 的失败计数或冷却状态
  void RecordSuccess(const std::string& key);

  // 返回指定 key 剩余的冷却时间；若未在冷却中则返回 0
  std::chrono::seconds CooldownRemaining(const std::string& key) const;

  // 重置所有 key 的冷却状态
  void Reset();

  // 返回指定 key 的连续失败次数
  int FailureCount(const std::string& key) const;

  // 尝试对处于冷却期的 key 发起一次探测；返回 true 表示允许本次探测
  bool TryProbe(const std::string& key);

  // 探测间隔：两次探测之间至少需要间隔的秒数
  static constexpr std::chrono::seconds kProbeInterval{30};

private:
  // 失败窗口衰减时间：超过该时间未再失败，失败计数可被衰减或重置
  static constexpr std::chrono::hours kFailureWindowDecay{24};

  // 单个 Provider 的冷却状态
  struct CooldownState {
    int consecutive_failures = 0;                          // 连续失败次数
    ProviderErrorKind last_error = ProviderErrorKind::kUnknown; // 最后一次错误类型
    std::chrono::steady_clock::time_point last_failure_at; // 最后一次失败时间
    std::chrono::steady_clock::time_point last_probe_at;   // 最后一次探测时间
    std::chrono::steady_clock::time_point cooldown_util;   // 冷却结束时间
  };

  // 根据错误类型和连续失败次数计算本次冷却时长
  static std::chrono::seconds ComputeCooldown(ProviderErrorKind kind, int failure_count);


  // mutable 让 mu_ 在“只读”的 const 成员函数中也能被加锁，保证线程安全的同时不破坏 const 语义。
  mutable std::mutex mu_;                                 // 保护 states_ 的互斥锁
  std::unordered_map<std::string, CooldownState> states_; // 各 Provider 的冷却状态
};


}
