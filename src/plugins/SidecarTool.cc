#include "SidecarTool.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

#include "../providers/HttpClient.h"

namespace quantclaw::plugins {

SidecarTool::SidecarTool(std::string sidecar_url, std::string tool_name,
                         std::string tool_description,
                         nlohmann::json tool_parameters)
    : _sidecar_url(std::move(sidecar_url)) {
  name = std::move(tool_name);
  description = std::move(tool_description);
  parameters = std::move(tool_parameters);
}

std::string SidecarTool::Execute(const nlohmann::json& args) const {
  nlohmann::json request;
  request["name"] = name;
  request["arguments"] = args;

  std::string response = providers::HttpPost(
      _sidecar_url + "/call",
      {{"Content-Type", "application/json"}},
      request.dump());

  auto json = nlohmann::json::parse(response);
  if (!json.contains("result")) {
    throw std::runtime_error("Sidecar response missing 'result'");
  }

  std::string result = json["result"].get<std::string>();
  spdlog::info("[sidecar tool] {} returned: {}", name, result);
  return result;
}

}  // namespace quantclaw::plugins
