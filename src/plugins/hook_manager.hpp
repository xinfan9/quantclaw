#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace quantclaw::plugins {

// 插件钩子类型
enum class HookType {
  kBeforeChat,
  kAfterTool,
  kRegisterTool,
};

// 钩子回调签名
using HookCallback = std::function<nlohmann::json(const nlohmann::json&)>;

// 管理插件钩子注册与调用
class HookManager {
 public:
  void Register(HookType type, std::string plugin_id, HookCallback callback);

  // 调用某类钩子，按注册顺序执行，前一个输出作为后一个输入
  nlohmann::json Invoke(HookType type, const nlohmann::json& input) const;

  // 列出某类钩子
  std::vector<std::string> ListHooks(HookType type) const;

 private:
  struct Hook {
    HookType type;
    std::string plugin_id;
    HookCallback callback;
  };
  std::vector<Hook> hooks_;
};

}  // namespace quantclaw::plugins
