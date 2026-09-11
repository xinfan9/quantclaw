#pragma once

#include <string>

namespace quantclaw::auth {

// 统一的 Provider 认证辅助函数

// 构造 Bearer Token Authorization 头
std::string BearerAuthHeader(const std::string& api_key);

// 从 api_key 推断 Provider 类型（openai / anthropic / github_copilot / openai_codex）
std::string InferProviderType(const std::string& api_key);

}  // namespace quantclaw::auth
