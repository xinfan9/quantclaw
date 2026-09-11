#include "ProviderRegistry.h"

#include <spdlog/spdlog.h>

#include "AnthropicProvider.h"
#include "GitHubCopilotProvider.h"
#include "OpenAICodexProvider.h"
#include "OpenAIProvider.h"

namespace quantclaw::providers {

ProviderRegistry::ProviderRegistry() { RegisterBuiltinFactories(); }

void ProviderRegistry::RegisterFactory(const std::string& provider_id,
                                       ProviderFactory factory) {
  factories_[provider_id] = std::move(factory);
}

void ProviderRegistry::AddProvider(const ProviderEntry& entry) {
  entries_[entry.id] = entry;
}

void ProviderRegistry::AddAlias(const std::string& alias,
                                const std::string& target) {
  aliases_[alias] = target;
}

ModelRef ProviderRegistry::ResolveModel(const std::string& raw) const {
  std::string resolved = raw;

  // 如果存在别名，则先展开。
  auto alias_it = aliases_.find(raw);
  if (alias_it != aliases_.end()) {
    resolved = alias_it->second;
    spdlog::debug("[provider] alias expanded: {} -> {}", raw, resolved);
  }

  // 解析 "provider/model" 或裸模型名。
  auto slash = resolved.find('/');
  if (slash == std::string::npos) {
    // 裸模型名：根据前缀推断 provider。
    if (resolved.rfind("anthropic/", 0) == 0 ||
        resolved.rfind("claude", 0) == 0) {
      return {"anthropic", resolved};
    }
    return {"openai", resolved};
  }

  return {resolved.substr(0, slash), resolved.substr(slash + 1)};
}

ProviderEntry ProviderRegistry::ResolveEntry(const ModelRef& ref,
                                             const Config& cfg) const {
  auto entry_it = entries_.find(ref.provider);
  if (entry_it != entries_.end()) {
    return entry_it->second;
  }
  return BuildDefaultEntry(ref.provider, cfg);
}

std::unique_ptr<LLMProvider> ProviderRegistry::CreateProvider(
    const ModelRef& ref, const ProviderEntry& entry) const {
  auto factory_it = factories_.find(ref.provider);
  if (factory_it == factories_.end()) {
    spdlog::error("No provider factory registered for '{}'", ref.provider);
    return nullptr;
  }

  return factory_it->second(entry, ref.model);
}

std::unique_ptr<LLMProvider> ProviderRegistry::CreateProvider(
    const ModelRef& ref, const Config& cfg) const {
  auto entry = ResolveEntry(ref, cfg);
  return CreateProvider(ref, entry);
}

std::unique_ptr<LLMProvider> ProviderRegistry::CreateProvider(
    const Config& cfg) const {
  auto ref = ResolveModel(cfg.model);
  return CreateProvider(ref, cfg);
}

std::shared_ptr<LLMProvider> ProviderRegistry::CreateProviderShared(
    const ModelRef& ref, const Config& cfg) const {
  auto provider = CreateProvider(ref, cfg);
  if (!provider) return nullptr;
  return std::shared_ptr<LLMProvider>(std::move(provider));
}

const ProviderEntry* ProviderRegistry::GetEntry(
    const std::string& provider_id) const {
  auto it = entries_.find(provider_id);
  if (it == entries_.end()) return nullptr;
  return &it->second;
}

std::vector<std::string> ProviderRegistry::ProviderIds() const {
  std::vector<std::string> ids;
  for (const auto& [id, _] : entries_) ids.push_back(id);
  return ids;
}

std::vector<std::pair<std::string, std::string>> ProviderRegistry::Aliases()
    const {
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto& [alias, target] : aliases_) result.emplace_back(alias, target);
  return result;
}

ProviderEntry ProviderRegistry::BuildDefaultEntry(const std::string& provider_id,
                                                  const Config& cfg) const {
  ProviderEntry entry;
  entry.id = provider_id;
  entry.api_key = cfg.api_key;
  entry.base_url = cfg.base_url;
  return entry;
}

void ProviderRegistry::RegisterBuiltinFactories() {
  RegisterFactory("openai",
                  [](const ProviderEntry& entry, const std::string& model) {
                    return std::make_unique<OpenAIProvider>(
                        entry.api_key, model, entry.base_url);
                  });
  RegisterFactory("anthropic",
                  [](const ProviderEntry& entry, const std::string& model) {
                    return std::make_unique<AnthropicProvider>(
                        entry.api_key, model, entry.base_url);
                  });
  RegisterFactory("github_copilot",
                  [](const ProviderEntry& entry, const std::string& model) {
                    return std::make_unique<GitHubCopilotProvider>(
                        entry.api_key, model, entry.base_url);
                  });
  RegisterFactory("openai_codex",
                  [](const ProviderEntry& entry, const std::string& model) {
                    return std::make_unique<OpenAICodexProvider>(
                        entry.api_key, model, entry.base_url);
                  });
}

}  // namespace quantclaw::providers
