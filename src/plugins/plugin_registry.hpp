#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "plugin_manifest.hpp"

namespace quantclaw::plugins {

// 已加载插件条目
struct LoadedPlugin {
  std::string id;
  std::string directory;
  PluginManifest manifest;
  bool enabled = true;
};

// 插件注册表：管理插件目录扫描与生命周期
class PluginRegistry {
 public:
  // 扫描目录加载插件
  void LoadDirectory(const std::string& dir);

  // 加载单个插件目录
  bool LoadPlugin(const std::string& dir);

  // 卸载插件
  bool UnloadPlugin(const std::string& id);

  // 列出已加载插件
  std::vector<LoadedPlugin> ListPlugins() const;

  // 获取插件清单
  std::optional<PluginManifest> GetManifest(const std::string& id) const;

 private:
  mutable std::mutex mu_;
  std::unordered_map<std::string, LoadedPlugin> plugins_;
};

}  // namespace quantclaw::plugins
