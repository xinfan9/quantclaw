#pragma once
#include <functional>
#include <string>

#include "config.h"
#include "LLMProvider.h"


namespace quantclaw::providers {

struct ProviderEntry {
  std::string id;
  std::string api_key;
  std::string base_url;
};

struct ModelRef {
  std::string provider;
  std::string model;

  std::string to_string() const { return provider + '/' + model; }
};

using ProviderFactory = std::function<std::unique_ptr<LLMProvider>(const ProviderEntry& entry, const std::string& model)>;

class ProviderRegistry {
public:
  ProviderRegistry();

  void RegisterFactory(const std::string& provider_id, ProviderFactory factory);

  void AddProvider(const ProviderEntry& entry);

  void AddAlias(const std::string& alias, const std::string& target);

  ModelRef ResolveModel(const std::string& raw) const;

  ProviderEntry ResolveEntry(const ModelRef& ref, const Config& cfg) const;

  std::unique_ptr<LLMProvider> CreateProvider(const ModelRef& ref, const ProviderEntry& entry) const;

  std::unique_ptr<LLMProvider> CreateProvider(const ModelRef& ref, const Config& cfg) const;

  std::unique_ptr<LLMProvider> CreateProvider(const Config& cfg) const;

  std::shared_ptr<LLMProvider> CreateProviderShared(const ModelRef& ref, const Config& cfg) const;

  const ProviderEntry* GetEntry(const std::string& provider_id) const;

  std::vector<std::string> ProviderIds() const;
  std::vector<std::pair<std::string, std::string>> Aliases() const;

private:
  std::unordered_map<std::string, ProviderFactory> factories_;
  std::unordered_map<std::string, ProviderEntry> entries_;
  std::unordered_map<std::string, std::string> aliases_;

  ProviderEntry BuildDefaultEntry(const std::string& provider_id, const Config& cfg) const;

  void RegisterBuiltinFactories();
};



}
