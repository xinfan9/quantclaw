#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace quantclaw::plugins {

// 插件清单：描述插件元数据和提供的工具
struct PluginManifest {
  std::string id;
  std::string name;
  std::string version;
  std::string description;
  std::string entry;  // 入口命令或脚本路径
  std::vector<std::string> tools;

  nlohmann::json ToJson() const;
  static PluginManifest FromJson(const nlohmann::json& j);
  static PluginManifest FromSkillMd(const std::string& content);
};

}  // namespace quantclaw::plugins
