#pragma once

#include <string>

namespace quantclaw::auth {

// GitHub Copilot OAuth Device Flow 认证（占位实现）
class GitHubCopilotAuth {
 public:
  // 请求 device code
  struct DeviceCodeResponse {
    std::string device_code;
    std::string user_code;
    std::string verification_uri;
    int interval = 5;
    int expires_in = 900;
  };

  // 启动 device flow，返回 device code 信息
  DeviceCodeResponse StartDeviceFlow();

  // 用 device code 轮询获取 access token
  // 成功返回 token，失败返回空字符串
  std::string PollForToken(const std::string& device_code, int max_attempts);

  // 用已有 token 刷新
  std::string RefreshToken(const std::string& refresh_token);
};

}  // namespace quantclaw::auth
