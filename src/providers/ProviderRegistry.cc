//
// Created by xinfang on 2026/9/9.
//

#include "ProviderRegistry.h"

#include "AnthropicProvider.h"
#include "OpenAIProvider.h"
#include "spdlog/spdlog.h"


namespace quantclaw::providers {

ProviderRegistry::ProviderRegistry() {
  RegisterBuiltinFactories();
}

void ProviderRegistry::RegisterFactory(const std::string& provider_id, ProviderFactory factory) {
  factories_[provider_id] = std::move(factory);
}

void ProviderRegistry::AddProvider(const ProviderEntry& entry) {
  entries_[entry.id] = entry;
}

void ProviderRegistry::AddAlias(const std::string& alias, const std::string& target) {
  aliases_[alias] = target;
}

ModelRef ProviderRegistry::ResolveModel(const std::string& raw) const {
  std::string resolved = raw;

  auto alias_it = aliases_.find(raw);
  if (alias_it != aliases_.end()) {
    resolved = alias_it->second;
    spdlog::debug("[provider] alias expanded: {} -> {}", raw, resolved);
  }

  auto slash = resolved.find('/');
  if (slash != std::string::npos) {
    if (resolved.rfind("anthropic") != std::string::npos || resolved.rfind("claude") != std::string::npos) return {"anthropic", resolved};
    return {"openai", resolved};
  }

  return {"openai", resolved};
}

std::unique_ptr<LLMProvider> ProviderRegistry::CreateProvider(const Config& cfg) const {
  auto ref = ResolveModel(cfg.model);
  return CreateProvider(ref, cfg);
}

std::vector<std::string> ProviderRegistry::ProviderIds() const {
  std::vector<std::string> ids;
  ids.reserve(entries_.size());
  for (const auto& [id, _] : entries_) ids.push_back(id);
  return ids;
}

std::vector<std::pair<std::string, std::string>> ProviderRegistry::Aliases() const {
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(aliases_.size());
  // emplace_back 直接在容器尾部原地构造对象，push_back 则是先构造对象再拷贝或移动
  for (const auto& [alias, target] : aliases_) result.emplace_back(alias, target);
  return result;
}

std::unique_ptr<LLMProvider> ProviderRegistry::CreateProvider(const ModelRef& ref, const Config& cfg) const {
  auto entry_it = entries_.find(ref.provider);
  ProviderEntry entry = entry_it != entries_.end() ? entry_it->second : BuildDefaultEntry(ref.provider, cfg);

  auto factory_it = factories_.find(ref.provider);
  if (factory_it == factories_.end()) {
    spdlog::error("[provider] no factory registered for provider: {}", ref.provider);
    return nullptr;
  }

  return factory_it->second(entry, ref.model);
}

ProviderEntry ProviderRegistry::BuildDefaultEntry(const std::string& provider, const Config& cfg) const {
  return {provider, cfg.api_key, cfg.base_url};
}






void ProviderRegistry::RegisterBuiltinFactories() {
  RegisterFactory("openai", [](const ProviderEntry& entry, const std::string& model) -> std::unique_ptr<LLMProvider> {
    return std::make_unique<OpenAIProvider>(entry.api_key, model, entry.base_url);
  });
  RegisterFactory("anthropic",
                  [](const ProviderEntry& entry, const std::string& model) -> std::unique_ptr<LLMProvider> {
                    return std::make_unique<AnthropicProvider>(
                        entry.api_key, model, entry.base_url);
                  });

}



}
