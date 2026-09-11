#include "openai_codex_auth.hpp"

#include <stdexcept>

namespace quantclaw::auth {

std::string OpenAICodexAuth::Authenticate(const std::string& /*api_key*/) {
  // 占位：完整实现需要调用 OpenAI Codex 认证端点
  throw std::runtime_error("OpenAI Codex authentication not yet implemented");
}

std::string OpenAICodexAuth::Refresh(const std::string& /*refresh_token*/) {
  throw std::runtime_error("OpenAI Codex token refresh not yet implemented");
}

}  // namespace quantclaw::auth
