//
// 由 xinfang 创建于 2026/9/10.
//

#include "FailoverResolver.h"

namespace quantclaw::providers {

// 构造 FailoverResolver，绑定 Provider 注册表和全局配置
FailoverResolver::FailoverResolver(ProviderRegistry* registry, const Config& cfg) : registry_(registry), cfg_(cfg) {}

// 设置模型降级链，主模型不可用时依次尝试这些模型
void FailoverResolver::SetFallbackChain(const std::vector<std::string>& models) {
  std::lock_guard lock(mu_);
  fallback_chain_ = models;
}

// 为指定 Provider 设置多个认证子账号
void FailoverResolver::SetProfiles(const std::string& provider_id, const std::vector<AuthProfile>& profiles) {
  std::lock_guard lock(mu_);
  profiles_[provider_id] = profiles;
}

// 解析可用 Provider：先尝试主模型，失败则遍历降级链
std::optional<ResolvedProvider> FailoverResolver::Resolve(const std::string& model, const std::string& session_key) {
  // 优先尝试用户指定的主模型
  auto result = try_resolve_model(model, session_key);
  if (result) return result;

  // 拷贝一份降级链快照，避免在遍历过程中被其他线程修改
  std::vector<std::string> chain_snapshot;
  {
    std::lock_guard lock(mu_);
    chain_snapshot = fallback_chain_;
  }

  // 依次尝试降级链中的模型
  for (const auto& fallback_model : chain_snapshot) {
    if (fallback_model == model) continue; // 避免重复尝试主模型

    result = try_resolve_model(fallback_model, session_key);
    if (result) {
      result->is_fallback = true; // 标记为降级选项
      return result;
    }
  }

  // 主模型和降级链都不可用，返回空值
  return std::nullopt;
}

// 记录某 Provider 子账号的一次成功调用：重置冷却、更新使用记录、绑定会话粘性
void FailoverResolver::RecordSuccess(const std::string& provider_id,
  const std::string& profile_id, const std::string& session_key) {
  auto key = cooldown_key(provider_id, profile_id);
  cooldown_.RecordSuccess(key);
  {
    std::lock_guard lock(mu_);
    // 记录当前时间戳，用于后续轮询选择最近最少使用的子账号
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    profile_last_used_[key] = now;

    // 若指定了 session_key，则建立会话粘性绑定
    if (!session_key.empty())
      session_pins_[session_key] = {provider_id, profile_id};
  }
}

// 记录某 Provider 子账号的一次失败调用，触发或延长冷却时间
void FailoverResolver::RecordFailure(const std::string& provider_id, const std::string& profile_id, ProviderErrorKind kind, int retry_after_seconds) {
  cooldown_.RecordFailure(cooldown_key(provider_id, profile_id), kind, retry_after_seconds);
}

// 清除指定会话的 Provider 粘性绑定
void FailoverResolver::ClearSessionPin(const std::string& session_key) {
  std::lock_guard lock(mu_);
  session_pins_.erase(session_key);
}

// 根据 provider_id 和 profile_id 生成冷却状态的唯一 key
std::string FailoverResolver::cooldown_key(const std::string& provider_id, const std::string& profile_id) const {
  if (profile_id.empty()) return provider_id;
  return provider_id + ":" + profile_id;
}


// 尝试解析单个模型对应的可用 Provider
std::optional<ResolvedProvider> FailoverResolver::try_resolve_model(const std::string& model, const std::string& session_key) {
  // 通过注册表解析模型对应的 Provider 引用
  auto ref = registry_->ResolveModel(model);
  const std::string& provider_id = ref.provider;

  std::lock_guard lock(mu_);

  // 若存在会话粘性绑定，优先尝试复用之前的 provider/profile
  if (!session_key.empty()) {
    auto pin_it = session_pins_.find(session_key);
    if (pin_it != session_pins_.end() && pin_it->second.provider_id == provider_id) {
      const auto& pin = pin_it->second;
      auto key = cooldown_key(provider_id, pin.profile_id);
      // 若该子账号未在冷却中，则创建对应 Provider 实例
      if (cooldown_.IsInCooldown(key)) {
        auto prof_it = profiles_.find(provider_id);
        if (prof_it != profiles_.end()) {
          for (const auto& profile : prof_it->second) {
            if (profile.id == pin.profile_id) {
              auto entry = registry_->ResolveEntry(ref, cfg_);
              entry.api_key = profile.api_key;
              auto provider = registry_->CreateProvider(ref, entry);
              if (provider)
                return ResolvedProvider {std::shared_ptr<LLMProvider>(std::move(provider)),
                provider_id, pin.profile_id, ref.model, false};
            }
          }
        }
      }

      // 若该子账号已不可用，清除会话粘性绑定
      session_pins_.erase(pin_it);
    }
  }


  // 查找该 Provider 是否配置了多个子账号
  auto prof_it = profiles_.find(provider_id);
  if (prof_it != profiles_.end() && !prof_it->second.empty()) {
    // 候选子账号结构：记录原始 profile 指针、冷却状态、优先级、上次使用时间、原始索引
    struct Candidate {
      const AuthProfile* profile;
      bool in_cooldown;
      int priority;
      int64_t last_used;
      int index;
    };

    std::vector<Candidate> available;   // 未冷却的候选子账号
    std::vector<Candidate> cooled_down; // 处于冷却中的候选子账号

    // 遍历该 Provider 的所有子账号，按冷却状态分组
    int idx = 0;
    for (const auto& profile : prof_it->second) {
      auto key = cooldown_key(provider_id, profile.id);
      auto lu_it = profile_last_used_.find(key);
      int64_t last_used = (lu_it != profile_last_used_.end()) ? lu_it->second : 0;

      if (cooldown_.IsInCooldown(key))
        cooled_down.push_back({&profile, true, profile.priority, last_used, idx++});
      else
        available.push_back({&profile, false, profile.priority, last_used, idx++});
    }

    // 对可用子账号排序：优先级小 -> 上次使用时间早 -> 原始索引小
    std::sort(available.begin(), available.end(), [](const Candidate& a, const Candidate& b) {
      if (a.priority != b.priority) return a.priority < b.priority;
      if (a.last_used != b.last_used) return a.last_used < b.last_used;
      return a.index < b.index;
    });

    // 依次尝试创建可用子账号对应的 Provider，成功则更新使用时间并返回
    for (const auto& c : available) {
      auto entry = registry_->ResolveEntry(ref, cfg_);
      entry.api_key = c.profile->api_key;
      auto provider = registry_->CreateProvider(ref, entry);
      if (provider) {
        auto key = cooldown_key(provider_id, c.profile->id);
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
        profile_last_used_[key] = now;
        return ResolvedProvider{std::shared_ptr<LLMProvider>(std::move(provider)),
                                provider_id, c.profile->id, ref.model, false};
      }
    }

    // 所有子账号都在冷却中：尝试对首个冷却子账号进行一次探测（受探测间隔限制）
    if (!cooled_down.empty()) {
      auto probe_key = cooldown_key(provider_id, cooled_down[0].profile->id);
      if (cooldown_.TryProbe(probe_key)) {
        auto entry = registry_->ResolveEntry(ref, cfg_);
        entry.api_key = cooled_down[0].profile->api_key;
        auto provider = registry_->CreateProvider(ref, entry);
        if (provider) {
          return ResolvedProvider{
            std::shared_ptr<LLMProvider>(std::move(provider)), provider_id,
            cooled_down[0].profile->id, ref.model, false};
        }
      }
    }

    // 所有子账号均不可用，返回空值
    return std::nullopt;
  }

  // 未配置子账号时：使用默认 Provider 入口（profile_id 为空）
  auto key = cooldown_key(provider_id, "");
  if (cooldown_.IsInCooldown(key)) {
    // 处于冷却中时，尝试探测一次；若探测间隔未到则返回空值
    if (cooldown_.TryProbe(key)) {
      auto provider = registry_->CreateProvider(ref, cfg_);
      if (provider) {
        return ResolvedProvider{std::shared_ptr<LLMProvider>(std::move(provider)),
                                provider_id, "", ref.model, false};
      }
    }
    return std::nullopt;
  }

  // 默认 Provider 未冷却，直接创建实例
  auto provider = registry_->CreateProvider(ref, cfg_);
  if (provider) {
    return ResolvedProvider{std::shared_ptr(std::move(provider)),
                            provider_id, "", ref.model, false};
  }
  
  // 创建失败，返回空值
  return std::nullopt;
}

}