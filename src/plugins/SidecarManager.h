#pragma once

#include <memory>
#include <string>
#include <vector>

namespace quantclaw::tools {
class ToolRegistry;
}

namespace quantclaw::plugins {

// 管理 Node.js Sidecar 连接和工具注册
class SidecarManager {
 public:
  explicit SidecarManager(std::string url = DefaultUrl());

  // 从 Sidecar 获取工具列表并注册到 ToolRegistry
  void RegisterTools(tools::ToolRegistry& registry) const;

  // 检查是否已配置 Sidecar
  [[nodiscard]] bool Enabled() const;

 private:
  std::string _url;

  static std::string DefaultUrl();
};

}  // namespace quantclaw::plugins
