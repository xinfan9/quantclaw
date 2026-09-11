#include "SidecarManager.h"

#include <cstdlib>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>

#include "../platform/process.hpp"
#include "../providers/HttpClient.h"
#include "../tools/ToolRegistry.h"
#include "SidecarTool.h"

namespace quantclaw::plugins {

std::string SidecarManager::DefaultUrl() {
  const char* url = std::getenv("QUANTCLAW_SIDECAR_URL");
  if (url && !std::string(url).empty()) return url;
  return "http://127.0.0.1:18802";
}

std::string SidecarManager::DefaultCommand() {
  const char* cmd = std::getenv("QUANTCLAW_SIDECAR_COMMAND");
  if (cmd && !std::string(cmd).empty()) return cmd;

  // 尝试找到与可执行文件同目录下的 sidecar 目录
  std::string exe = quantclaw::platform::ExecutablePath();
  if (!exe.empty()) {
    std::filesystem::path sidecar_dir =
        std::filesystem::path(exe).parent_path() / "sidecar";
    if (std::filesystem::exists(sidecar_dir / "package.json")) {
      return "npm run --prefix " + sidecar_dir.string() + " start";
    }
  }
  return "";
}

SidecarManager::SidecarManager(std::string url)
    : url_(std::move(url)), command_(DefaultCommand()) {}

SidecarManager::~SidecarManager() { Stop(); }

bool SidecarManager::Enabled() const {
  const char* disabled = std::getenv("QUANTCLAW_SIDECAR_DISABLED");
  return !(disabled && std::string(disabled) == "1");
}

bool SidecarManager::Start() {
  if (!Enabled()) return false;
  if (command_.empty()) {
    spdlog::debug("[sidecar] no sidecar command configured, skipping start");
    return false;
  }
  if (pid_ > 0 && quantclaw::platform::IsProcessAlive(pid_)) {
    spdlog::info("[sidecar] already running (pid {})", pid_);
    return true;
  }

  // 解析命令：简单按空格拆分（不支持引号，仅用于示例）
  std::vector<std::string> parts;
  std::string current;
  for (char c : command_) {
    if (c == ' ') {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) parts.push_back(current);
  if (parts.empty()) return false;

  pid_ = quantclaw::platform::SpawnProcess(parts);
  if (pid_ <= 0) {
    spdlog::warn("[sidecar] failed to spawn sidecar process");
    return false;
  }
  started_by_us_ = true;
  spdlog::info("[sidecar] started with pid {}", pid_);

  // 等待 sidecar 就绪
  for (int i = 0; i < 50; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    try {
      auto r = providers::HttpGet(url_ + "/health", {{"Accept", "application/json"}}, 1);
      if (!r.empty()) return true;
    } catch (...) {
    }
  }
  return quantclaw::platform::IsProcessAlive(pid_);
}

void SidecarManager::Stop() {
  if (started_by_us_ && pid_ > 0) {
    quantclaw::platform::TerminateProcess(pid_);
    quantclaw::platform::WaitProcess(pid_, 2000);
    if (quantclaw::platform::IsProcessAlive(pid_)) {
      quantclaw::platform::KillProcess(pid_);
    }
    pid_ = 0;
    started_by_us_ = false;
  }
}

bool SidecarManager::IsRunning() const {
  if (pid_ <= 0) return false;
  return quantclaw::platform::IsProcessAlive(pid_);
}

void SidecarManager::RegisterTools(tools::ToolRegistry& registry) const {
  if (!Enabled()) return;

  try {
    spdlog::info("[sidecar] connecting to {}", url_);
    std::string response = providers::HttpGet(
        url_ + "/tools",
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
      nlohmann::json parameters = tool_def.contains("inputSchema")
                                      ? tool_def["inputSchema"]
                                      : nlohmann::json::object();

      if (name.empty()) continue;

      registry.Register(std::make_unique<SidecarTool>(
          url_, name, description, parameters));
      spdlog::info("[sidecar] registered tool: {}", name);
    }
  } catch (const std::exception& e) {
    spdlog::warn("[sidecar] failed to connect or register tools: {}", e.what());
  }
}

}  // namespace quantclaw::plugins
