#include "plugin_system.hpp"

#include <filesystem>
#include <spdlog/spdlog.h>

#include "../platform.h"
#include "../platform/process.hpp"
#include "../tools/ToolRegistry.h"

namespace quantclaw::plugins {

std::string PluginSystem::DefaultPluginsDir() {
  const char* dir = std::getenv("QUANTCLAW_PLUGINS_DIR");
  if (dir && !std::string(dir).empty()) return dir;
  return std::filesystem::path(quantclaw::platform::home_directory()) /
         ".quantclaw" / "plugins";
}

PluginSystem::PluginSystem(std::string plugins_dir)
    : plugins_dir_(std::move(plugins_dir)) {}

void PluginSystem::LoadPlugins() {
  std::filesystem::create_directories(plugins_dir_);
  registry_.LoadDirectory(plugins_dir_);
}

bool PluginSystem::LoadPlugin(const std::string& dir) {
  return registry_.LoadPlugin(dir);
}

bool PluginSystem::UnloadPlugin(const std::string& id) {
  return registry_.UnloadPlugin(id);
}

std::vector<PluginManifest> PluginSystem::ListPlugins() const {
  auto plugins = registry_.ListPlugins();
  std::vector<PluginManifest> result;
  for (const auto& p : plugins) result.push_back(p.manifest);
  return result;
}

std::vector<std::string> PluginSystem::GetTools(
    const std::string& plugin_id) const {
  auto manifest = registry_.GetManifest(plugin_id);
  if (!manifest) return {};
  return manifest->tools;
}

nlohmann::json PluginSystem::CallTool(const std::string& plugin_id,
                                      const std::string& tool_name,
                                      const nlohmann::json& arguments) const {
  auto manifest = registry_.GetManifest(plugin_id);
  if (!manifest) return {{"error", "Plugin not found"}};

  // 占位实现：通过插件 entry 命令执行工具调用
  nlohmann::json request = {
      {"tool", tool_name}, {"arguments", arguments}, {"plugin", plugin_id}};

  try {
    auto result = quantclaw::platform::ExecCapture(
        manifest->entry + " '" + request.dump() + "'", 30,
        std::filesystem::path(manifest->entry).parent_path().string());
    if (result.exit_code != 0) {
      return {{"error", result.output.empty() ? "Plugin execution failed"
                                              : result.output}};
    }
    return nlohmann::json::parse(result.output);
  } catch (const std::exception& e) {
    return {{"error", e.what()}};
  }
}

void PluginSystem::RegisterTools(tools::ToolRegistry& /*registry*/) const {
  auto plugins = registry_.ListPlugins();
  for (const auto& plugin : plugins) {
    for (const auto& tool_name : plugin.manifest.tools) {
      // 占位：使用 PluginTool 包装（这里简化为日志记录）
      spdlog::info("[plugin] registered tool: {}.{}", plugin.id, tool_name);
    }
  }
}

}  // namespace quantclaw::plugins
