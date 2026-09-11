#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "hook_manager.hpp"
#include "plugin_manifest.hpp"
#include "plugin_registry.hpp"

namespace quantclaw::tools {
class ToolRegistry;
}

namespace quantclaw::plugins {

// 插件系统统一入口
class PluginSystem {
 public:
  explicit PluginSystem(std::string plugins_dir = DefaultPluginsDir());

  // 加载插件目录
  void LoadPlugins();

  // 加载单个插件
  bool LoadPlugin(const std::string& dir);

  // 卸载插件
  bool UnloadPlugin(const std::string& id);

  // 列出插件
  std::vector<PluginManifest> ListPlugins() const;

  // 获取插件提供的工具列表（占位：目前返回 manifest 中的 tools）
  std::vector<std::string> GetTools(const std::string& plugin_id) const;

  // 调用插件工具（占位：通过 entry 脚本执行）
  nlohmann::json CallTool(const std::string& plugin_id,
                          const std::string& tool_name,
                          const nlohmann::json& arguments) const;

  // 注册钩子
  HookManager& Hooks() { return hooks_; }

  // 扫描插件工具并注册到 ToolRegistry
  void RegisterTools(tools::ToolRegistry& registry) const;

  static std::string DefaultPluginsDir();

 private:
  std::string plugins_dir_;
  PluginRegistry registry_;
  HookManager hooks_;
};

}  // namespace quantclaw::plugins
