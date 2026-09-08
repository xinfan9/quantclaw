#include "SidecarManager.h"

#include <spdlog/spdlog.h>
#include <cstdlib>
#include <stdexcept>

#include "../providers/HttpClient.h"
#include "../tools/ToolRegistry.h"
#include "SidecarTool.h"

namespace quantclaw::plugins {

std::string SidecarManager::DefaultUrl() {
  const char* url = std::getenv("QUANTCLAW_SIDECAR_URL");
  if (url && !std::string(url).empty()) return url;
  return "http://127.0.0.1:18802";
}

SidecarManager::SidecarManager(std::string url) : _url(std::move(url)) {}

bool SidecarManager::Enabled() const {
  // 如果没有显式禁用，默认尝试连接
  const char* disabled = std::getenv("QUANTCLAW_SIDECAR_DISABLED");
  return !(disabled && std::string(disabled) == "1");
}

void SidecarManager::RegisterTools(tools::ToolRegistry& registry) const {
  if (!Enabled()) return;

  try {
    spdlog::info("[sidecar] connecting to {}", _url);
    std::string response = providers::HttpGet(
        _url + "/tools",
        {{"Accept", "application/json"}},
        5);

    auto json = nlohmann::json::parse(response);
    if (!json.contains("tools") || !json["tools"].is_array()) {
      spdlog::warn("[sidecar] invalid tools response");
      return;
    }

    for (const auto& tool_def : json["tools"]) {
      std::string name = tool_def.value("name", "");
      std::string description = tool_def.value("description", "");
      nlohmann::json parameters = tool_def.contains("parameters")
                                      ? tool_def["parameters"]
                                      : nlohmann::json::object();

      if (name.empty()) continue;

      registry.Register(std::make_unique<SidecarTool>(
          _url, name, description, parameters));
      spdlog::info("[sidecar] registered tool: {}", name);
    }
  } catch (const std::exception& e) {
    spdlog::warn("[sidecar] failed to connect or register tools: {}", e.what());
  }
}

}  // namespace quantclaw::plugins
