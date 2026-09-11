#pragma once

#include <string>

namespace quantclaw::providers {

// Provider 错误类型枚举：用于区分不同失败原因，以决定冷却策略和重试行为
enum class ProviderErrorKind {
  kUnknown,          // 未知错误
  kModelNotFound,    // 模型不存在
  kContextOverflow,  // 上下文长度超过限制
  kBadRequest,       // 请求参数错误（如 4xx）
  kAuthError,        // 认证失败（API Key 错误等）
  kBillingError,     // 计费/账户额度问题
  kRateLimit,        // 触发速率限制
  kTransient,        // 临时性服务端错误（如 5xx）
  kTimeout,          // 请求超时
};

// 将 ProviderErrorKind 转换为可读的字符串表示
inline std::string ProviderErrorKindToString(ProviderErrorKind kind) {
  switch (kind) {
  case ProviderErrorKind::kModelNotFound:
    return "model_not_found";
  case ProviderErrorKind::kContextOverflow:
    return "context_overflow";
  case ProviderErrorKind::kBadRequest:
    return "bad_request";
  case ProviderErrorKind::kAuthError:
    return "auth_error";
  case ProviderErrorKind::kBillingError:
    return "billing_error";
  case ProviderErrorKind::kRateLimit:
    return "rate_limit";
  case ProviderErrorKind::kTransient:
    return "transient";
  case ProviderErrorKind::kTimeout:
    return "timeout";
  default:
    return "unknown";
  }
}

}