#include "github_copilot_auth.hpp"

#include <stdexcept>

namespace quantclaw::auth {

GitHubCopilotAuth::DeviceCodeResponse GitHubCopilotAuth::StartDeviceFlow() {
  // 占位：完整实现需要调用 GitHub device flow API
  throw std::runtime_error("GitHub Copilot device flow not yet implemented");
}

std::string GitHubCopilotAuth::PollForToken(const std::string& /*device_code*/,
                                            int /*max_attempts*/) {
  // 占位
  throw std::runtime_error("GitHub Copilot token polling not yet implemented");
}

std::string GitHubCopilotAuth::RefreshToken(const std::string& /*refresh_token*/) {
  // 占位
  throw std::runtime_error("GitHub Copilot token refresh not yet implemented");
}

}  // namespace quantclaw::auth
