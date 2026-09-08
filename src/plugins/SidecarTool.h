#pragma once

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "../tools/Tool.h"

namespace quantclaw::plugins {

// 通过 HTTP 调用 Node.js Sidecar 的工具
class SidecarTool : public tools::Tool {
 public:
  SidecarTool(std::string sidecar_url, std::string tool_name,
              std::string tool_description, nlohmann::json tool_parameters);

  std::string Execute(const nlohmann::json& args) const override;

 private:
  std::string _sidecar_url;
};

}  // namespace quantclaw::plugins
