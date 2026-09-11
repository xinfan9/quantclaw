#include "plugin_manifest.hpp"

#include <sstream>

namespace quantclaw::plugins {

nlohmann::json PluginManifest::ToJson() const {
  return {
      {"id", id},
      {"name", name},
      {"version", version},
      {"description", description},
      {"entry", entry},
      {"tools", tools},
  };
}

PluginManifest PluginManifest::FromJson(const nlohmann::json& j) {
  PluginManifest m;
  m.id = j.value("id", "");
  m.name = j.value("name", "");
  m.version = j.value("version", "");
  m.description = j.value("description", "");
  m.entry = j.value("entry", "");
  if (j.contains("tools") && j["tools"].is_array()) {
    for (const auto& t : j["tools"]) {
      if (t.is_string()) m.tools.push_back(t.get<std::string>());
    }
  }
  return m;
}

PluginManifest PluginManifest::FromSkillMd(const std::string& content) {
  PluginManifest m;
  std::istringstream iss(content);
  std::string line;

  // 简单解析 SKILL.md 中的 YAML front matter 或标题
  while (std::getline(iss, line)) {
    if (line.rfind("# ", 0) == 0 && m.name.empty()) {
      m.name = line.substr(2);
    } else if (line.find("id:") != std::string::npos) {
      auto pos = line.find(':');
      if (pos != std::string::npos) {
        m.id = line.substr(pos + 1);
        // 去除首尾空格
        auto start = m.id.find_first_not_of(" \t");
        auto end = m.id.find_last_not_of(" \t");
        if (start != std::string::npos) {
          m.id = m.id.substr(start, end - start + 1);
        }
      }
    } else if (line.find("version:") != std::string::npos) {
      auto pos = line.find(':');
      if (pos != std::string::npos) {
        m.version = line.substr(pos + 1);
        auto start = m.version.find_first_not_of(" \t");
        auto end = m.version.find_last_not_of(" \t");
        if (start != std::string::npos) {
          m.version = m.version.substr(start, end - start + 1);
        }
      }
    }
  }

  if (m.id.empty()) m.id = m.name;
  if (m.name.empty()) m.name = "Unnamed Plugin";
  return m;
}

}  // namespace quantclaw::plugins
