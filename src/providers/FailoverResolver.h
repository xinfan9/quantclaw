#pragma once
#include <string>

#include "CooldownTracker.h"
#include "LLMProvider.h"
#include "ProviderError.h"
#include "ProviderRegistry.h"

namespace quantclaw::providers {

// Provider 认证子账号：包含子账号 ID、API Key 以及优先级
struct AuthProfile {
  std::string id;
  std::string api_key;
  int priority = 0;
};

// 解析后的 Provider 信息：包含实际 Provider 实例、ID、模型以及是否为降级选项
struct ResolvedProvider {
  std::shared_ptr<LLMProvider> provider; // LLM Provider 实例
  std::string provider_id;               // Provider 标识
  std::string profile_id;                // 子账号标识（空表示使用默认账号）
  std::string model;                     // 实际使用的模型名称
  bool is_fallback = false;              // 是否来自降级链
};

// Provider 故障转移解析器：根据模型名称、会话 key 和 Provider 冷却状态，选择可用的 Provider
class FailoverResolver {
public:
  FailoverResolver(ProviderRegistry* registry, const Config& cfg);

  // 设置模型降级链：主模型不可用时依次尝试这些模型
  void SetFallbackChain(const std::vector<std::string>& models);

  // 为指定 Provider 设置多个认证子账号，用于轮询或失败切换
  void SetProfiles(const std::string& provider_id, const std::vector<AuthProfile>& profiles);

  // 根据模型名称解析可用 Provider；session_key 用于会话级 Provider 粘性
  // optional 表示若无可用 Provider，返回 std::nullopt
  std::optional<ResolvedProvider> Resolve(const std::string& model, const std::string& session_key = "");

  // 记录某 Provider 子账号的一次成功调用，可用于重置冷却或保持会话粘性
  void RecordSuccess(const std::string& provider_id, const std::string& profile_id, const std::string& session_key = "");

  // 记录某 Provider 子账号的一次失败调用，用于触发或延长冷却时间
  // kind: 错误类型（如 kRateLimit 速率限制、kTransient 临时错误、kAuthError 认证失败等）
  // retry_after_seconds: 服务端建议的重试间隔（秒），默认为 0
  void RecordFailure(const std::string& provider_id, const std::string& profile_id, ProviderErrorKind kind, int retry_after_seconds = 0);

  // 清除指定会话的 Provider 粘性绑定
  void ClearSessionPin(const std::string& session_key);

  // 获取内部冷却追踪器的只读引用
  const CooldownTracker& GetCooldownTracker() const { return cooldown_;}

private:
  // 根据 provider_id 和 profile_id 生成冷却状态的唯一 key
  std::string cooldown_key(const std::string& provider_id, const std::string& profile_id) const;

  // 尝试解析单个模型对应的可用 Provider；Resolve() 内部会循环调用它遍历降级链
  std::optional<ResolvedProvider> try_resolve_model(const std::string& model, const std::string& session_key);

  ProviderRegistry* registry_; // Provider 注册表
  Config cfg_;                 // 全局配置
  CooldownTracker cooldown_;   // Provider 子账号冷却状态

  mutable std::mutex mu_;      // 保护内部状态的互斥锁

  std::vector<std::string> fallback_chain_; // 模型降级链

  std::unordered_map<std::string, std::vector<AuthProfile>> profiles_; // 各 Provider 的子账号列表

  std::unordered_map<std::string, int64_t> profile_last_used_; // 各子账号的上次使用时间戳，用于轮询选择

  // 会话级 Provider 粘性：同一会话尽量使用相同的 provider/profile
  struct SessionPin {
    std::string provider_id;
    std::string profile_id;
  };

  std::unordered_map<std::string, SessionPin> session_pins_; // 会话到 Provider 子账号的绑定

};

}
