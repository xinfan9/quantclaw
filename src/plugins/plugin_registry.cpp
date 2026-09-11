#include "plugin_registry.hpp"

#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

namespace quantclaw::plugins {

void PluginRegistry::LoadDirectory(const std::string& dir) {
  if (!std::filesystem::exists(dir)) return;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.is_directory()) {
      LoadPlugin(entry.path().string());
    }
  }
}

bool PluginRegistry::LoadPlugin(const std::string& dir) {
  std::filesystem::path p(dir);
  if (!std::filesystem::exists(p)) return false;

  PluginManifest manifest;
  bool found = false;

  // 优先读取 plugin.json
  auto json_path = p / "plugin.json";
  if (std::filesystem::exists(json_path)) {
    try {
      std::ifstream file(json_path);
      nlohmann::json j;
      file >> j;
      manifest = PluginManifest::FromJson(j);
      found = true;
    } catch (const std::exception& e) {
      spdlog::warn("Failed to parse plugin.json {}: {}", json_path.string(),
                   e.what());
    }
  }

  // 其次读取 SKILL.md
  if (!found) {
    auto md_path = p / "SKILL.md";
    if (std::filesystem::exists(md_path)) {
      std::ifstream file(md_path);
      std::string content((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
      manifest = PluginManifest::FromSkillMd(content);
      found = true;
    }
  }

  if (!found) return false;
  if (manifest.id.empty()) manifest.id = p.filename().string();
  if (manifest.entry.empty()) {
    manifest.entry = (p / "index.js").string();
  }

  std::lock_guard<std::mutex> lock(mu_);
  LoadedPlugin plugin;
  plugin.id = manifest.id;
  plugin.directory = dir;
  plugin.manifest = std::move(manifest);
  plugins_[plugin.id] = std::move(plugin);
  spdlog::info("Loaded plugin: {} from {}", plugin.id, dir);
  return true;
}

bool PluginRegistry::UnloadPlugin(const std::string& id) {
  std::lock_guard<std::mutex> lock(mu_);
  return plugins_.erase(id) > 0;
}

std::vector<LoadedPlugin> PluginRegistry::ListPlugins() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<LoadedPlugin> result;
  for (const auto& [id, plugin] : plugins_) result.push_back(plugin);
  return result;
}

std::optional<PluginManifest> PluginRegistry::GetManifest(
    const std::string& id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = plugins_.find(id);
  if (it == plugins_.end()) return std::nullopt;
  return it->second.manifest;
}

}  // namespace quantclaw::plugins
