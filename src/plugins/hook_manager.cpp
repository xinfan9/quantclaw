#include "hook_manager.hpp"

namespace quantclaw::plugins {

void HookManager::Register(HookType type, std::string plugin_id,
                           HookCallback callback) {
  hooks_.push_back({type, std::move(plugin_id), std::move(callback)});
}

nlohmann::json HookManager::Invoke(HookType type,
                                   const nlohmann::json& input) const {
  nlohmann::json result = input;
  for (const auto& hook : hooks_) {
    if (hook.type != type) continue;
    if (hook.callback) {
      result = hook.callback(result);
    }
  }
  return result;
}

std::vector<std::string> HookManager::ListHooks(HookType type) const {
  std::vector<std::string> ids;
  for (const auto& hook : hooks_) {
    if (hook.type == type) ids.push_back(hook.plugin_id);
  }
  return ids;
}

}  // namespace quantclaw::plugins
