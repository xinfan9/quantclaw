//
// Created by xinfang on 2026/9/4.
//

#include "ProviderFactory.h"

#include "AnthropicProvider.h"
#include "OpenAIProvider.h"

namespace quantclaw::providers {

static bool StartWith(const std::string& str, const std::string& prefix) {
  return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

std::unique_ptr<LLMProvider> CreateProvider(const quantclaw::Config& cfg) {
    // if (StartWith(cfg.model, "anthropic"))
    //   return std::make_unique<AnthropicProvider>(cfg.api_key, cfg.model, cfg.base_url);
    return std::make_unique<OpenAIProvider>(cfg.api_key, cfg.model, cfg.base_url);
}

}
