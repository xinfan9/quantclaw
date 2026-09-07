//
// Created by xinfang on 2026/9/4.
//

#include "ProviderFactory.h"

#include "AnthropicProvider.h"
#include "OpenAIProvider.h"

namespace quantclaw::providers {

std::unique_ptr<LLMProvider> CreateProvider(const quantclaw::Config& cfg) {
    if (cfg.model.rfind("anthropic", 0) == 0)
      return std::make_unique<AnthropicProvider>(cfg.api_key, cfg.model, cfg.base_url);
    return std::make_unique<OpenAIProvider>(cfg.api_key, cfg.model, cfg.base_url);
}

}
