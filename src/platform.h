#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

namespace quantclaw::platform {

// 返回用户主目录（从 $HOME 环境变量读取）。
inline std::string home_directory() {
  const char* home = std::getenv("HOME");
  if (!home) {
    throw std::runtime_error("HOME env variable not set");
  }
  return home;
}

// QuantClaw 全部数据的根目录：~/.quantclaw
inline std::filesystem::path base_dir() {
  return std::filesystem::path(home_directory()) / ".quantclaw";
}

// Workspace 目录：~/.quantclaw/agents/main/workspace
inline std::filesystem::path workspace_dir() {
  return base_dir() / "agents" / "main" / "workspace";
}

// Session 目录：~/.quantclaw/agents/main/sessions
inline std::filesystem::path sessions_dir() {
  return base_dir() / "agents" / "main" / "sessions";
}

// 默认配置文件路径：~/.quantclaw/quantclaw.json
inline std::filesystem::path config_path() {
  return base_dir() / "quantclaw.json";
}

// 确保目录存在，不存在则创建。
inline void ensure_dir(const std::filesystem::path& path) {
  std::filesystem::create_directories(path);
}

}  // namespace quantclaw::platform
