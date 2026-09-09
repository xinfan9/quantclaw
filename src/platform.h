#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

namespace quantclaw::platform {

// Return the user's home directory (from $HOME).
inline std::string home_directory() {
  const char* home = std::getenv("HOME");
  if (!home) {
    throw std::runtime_error("HOME env variable not set");
  }
  return home;
}

// Base directory for all QuantClaw data: ~/.quantclaw
inline std::filesystem::path base_dir() {
  return std::filesystem::path(home_directory()) / ".quantclaw";
}

// Workspace directory: ~/.quantclaw/agents/main/workspace
inline std::filesystem::path workspace_dir() {
  return base_dir() / "agents" / "main" / "workspace";
}

// Sessions directory: ~/.quantclaw/agents/main/sessions
inline std::filesystem::path sessions_dir() {
  return base_dir() / "agents" / "main" / "sessions";
}

// Default config file path: ~/.quantclaw/quantclaw.json
inline std::filesystem::path config_path() {
  return base_dir() / "quantclaw.json";
}

// Ensure a directory exists.
inline void ensure_dir(const std::filesystem::path& path) {
  std::filesystem::create_directories(path);
}

}  // namespace quantclaw::platform
