#pragma once

#include <atomic>
#include <memory>
#include <string>

namespace quantclaw::tools {
class ToolRegistry;
}

namespace quantclaw::plugins {

// 管理 Node.js / TypeScript Sidecar 连接和工具注册
class SidecarManager {
 public:
  explicit SidecarManager(std::string url = DefaultUrl());
  ~SidecarManager();

  // 启动 sidecar 进程（如果配置了 QUANTCLAW_SIDECAR_COMMAND 或能找到本地 sidecar 目录）
  bool Start();

  // 停止 sidecar 进程
  void Stop();

  // 是否已启动
  bool IsRunning() const;

  // 从 Sidecar 获取工具列表并注册到 ToolRegistry
  void RegisterTools(tools::ToolRegistry& registry) const;

  // 检查是否已配置 Sidecar
  [[nodiscard]] bool Enabled() const;

 private:
  std::string url_;
  std::string command_;
  int pid_ = 0;
  std::atomic<bool> started_by_us_{false};

  static std::string DefaultUrl();
  static std::string DefaultCommand();
};

}  // namespace quantclaw::plugins
