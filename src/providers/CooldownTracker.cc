//
// Created by xinfang on 2026/9/10.
//

#include "CooldownTracker.h"

namespace quantclaw::providers {

bool CooldownTracker::IsInCooldown(const std::string& key) const {
  std::lock_guard lock(mu_);

  auto it = states_.find(key);
  if (it == states_.end()) return false;
  return std::chrono::steady_clock::now() < it->second.cooldown_util;
}

void CooldownTracker::RecordFailure(const std::string& key, ProviderErrorKind kind, int retry_after_seconds) {
  std::lock_guard lock(mu_);
  auto& state = states_[key];
  auto now = std::chrono::steady_clock::now();

  if (state.consecutive_failures > 0 && (now - state.last_failure_at) > kFailureWindowDecay)
    state.consecutive_failures = 0;

  state.last_error = kind;
  state.last_failure_at = now;
  state.last_probe_at = now;
  ++state.consecutive_failures;

  auto duration = ComputeCooldown(kind, state.consecutive_failures);
  if (retry_after_seconds > 0) {
    duration = std::chrono::seconds(retry_after_seconds);
  }

  state.cooldown_util = now + duration;
}

void CooldownTracker::RecordSuccess(const std::string& key) {
  std::lock_guard lock(mu_);
  auto it = states_.find(key);
  if (it == states_.end()) return;
  it->second.consecutive_failures = 0;
  it->second.cooldown_util = std::chrono::steady_clock::time_point();
}

std::chrono::seconds CooldownTracker::CooldownRemaining(const std::string& key) const {
  std::lock_guard lock(mu_);
  auto it = states_.find(key);
  if (it == states_.end()) return std::chrono::seconds{0};

  auto now = std::chrono::steady_clock::now();
  if (now >= it->second.cooldown_util) return std::chrono::seconds{0};

  return std::chrono::duration_cast<std::chrono::seconds>(it->second.cooldown_util - now);
}

void CooldownTracker::Reset() {
  std::lock_guard lock(mu_);
  states_.clear();
}

int CooldownTracker::FailureCount(const std::string& key) const {
  std::lock_guard lock(mu_);
  auto it = states_.find(key);
  if (it == states_.end()) return 0;

  auto now = std::chrono::steady_clock::now();
  if (it->second.consecutive_failures > 0 && now - it->second.last_failure_at > kFailureWindowDecay)
    return 0;

  return it->second.consecutive_failures;
}

bool CooldownTracker::TryProbe(const std::string& key) {
  std::lock_guard lock(mu_);
  auto it = states_.find(key);
  if (it == states_.end()) return false;

  auto now = std::chrono::steady_clock::now();
  if (now >= it->second.cooldown_util) return false;
  if (now - it->second.last_probe_at < kProbeInterval) return false;

  it->second.last_probe_at = now;

  return true;
}

std::chrono::seconds CooldownTracker::ComputeCooldown(ProviderErrorKind kind, int failure_count) {
  switch (kind) {
  case ProviderErrorKind::kModelNotFound:
  case ProviderErrorKind::kContextOverflow:
  case ProviderErrorKind::kBadRequest:
    return std::chrono::seconds{0};

  case ProviderErrorKind::kAuthError:
  case ProviderErrorKind::kBillingError:
    return std::chrono::seconds{3600};

  case ProviderErrorKind::kRateLimit:
    std::chrono::seconds{std::min(3600, 60*(1 << std::min(failure_count - 1, 6)))};
  case ProviderErrorKind::kTransient:
  case ProviderErrorKind::kTimeout:
    return std::chrono::seconds{
      std::min(300, 30 * (1 << std::min(failure_count - 1, 4)))};
  default:
    return std::chrono::seconds{
      std::min(300, 10 * (1 << std::min(failure_count - 1, 5)))};
  }
}



}