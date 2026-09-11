#include "provider_auth.hpp"

namespace quantclaw::auth {

std::string BearerAuthHeader(const std::string& api_key) {
  return "Authorization: Bearer " + api_key;
}

std::string InferProviderType(const std::string& api_key) {
  if (api_key.empty()) return "unknown";
  // GitHub Copilot token 通常以 gho_ 或 github_pat_ 开头
  if (api_key.rfind("gho_", 0) == 0 || api_key.rfind("github_pat_", 0) == 0) {
    return "github_copilot";
  }
  // OpenAI key 通常以 sk- 开头
  if (api_key.rfind("sk-", 0) == 0) return "openai";
  return "unknown";
}

}  // namespace quantclaw::auth
