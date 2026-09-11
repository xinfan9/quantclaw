#pragma once

#include <string>

namespace quantclaw::auth {

// OpenAI Codex 认证（占位实现）
class OpenAICodexAuth {
 public:
  // 用 API key 获取 Codex session/token
  std::string Authenticate(const std::string& api_key);

  // 刷新 Codex token
  std::string Refresh(const std::string& refresh_token);
};

}  // namespace quantclaw::auth
