#pragma once
#include <fstream>
#include <nlohmann/json.hpp>

namespace quantclaw {

struct ProviderConfig {
  std::string api_key;
  std::string base_url;
};

struct Config {
  std::string api_key;
  std::string model;
  std::string base_url;

  std::unordered_map<std::string, ProviderConfig> providers;
  std::unordered_map<std::string, std::string> aliases;

  static Config Load();

  static Config LoadFromFile(const std::string& path);

  static std::string DefaultPath();
  static std::string ExpandHome(const std::string& path);

private:
  static void ApplyEnvOverrides(Config& cfg);
};

inline std::string Config::ExpandHome(const std::string& path) {
  if (path.size() >= 2 && path.substr(0, 2) == "~/") {
    const char* home = std::getenv("HOME");

    if (home) {
      return (std::filesystem::path(home)/ path.substr(2)).string();
    }
  }

  return  path;
}

inline std::string Config::DefaultPath() {
  return ExpandHome("~/.quantclaw/config.json");
}

inline Config Config::Load() {
  std::string path = DefaultPath();
  if (!std::filesystem::exists(path)) {
    Config cfg;
    ApplyEnvOverrides(cfg);
    return cfg;
  }

  return LoadFromFile(path);
}

inline Config Config::LoadFromFile(const std::string& path) {
  Config cfg;
  std::ifstream file(path);
  if (!file.is_open()) {
    ApplyEnvOverrides(cfg);
    return cfg;
  }

  try {
    nlohmann::json json;
    file >> json;

    if (json.contains("agent") && json["agent"].is_object()) {
      const auto& agent = json["agent"];
      if (agent.contains("models") && agent["model"].is_string())
        cfg.model = agent["model"].get<std::string>();
    }

    if (json.contains("llm") && json["llm"].is_object()) {
      const auto& llm = json["llm"];
      if (llm.contains("apiKey") && llm["apiKey"].is_string()) {
        cfg.api_key = llm["apiKey"].get<std::string>();
      }
      if (llm.contains("baseUrl") && llm["baseUrl"].is_string()) {
        cfg.base_url = llm["baseUrl"].get<std::string>();
      }
      if (llm.contains("model") && llm["model"].is_string()) {
        cfg.model = llm["model"].get<std::string>();
      }
    }

    if (json.contains("providers") && json["providers"].is_object()) {
      for (const auto& [key, value] : json["providers"].items()) {
        ProviderConfig pc;
        if (value.contains("apiKey") && value["apiKey"].is_string()) {
          pc.api_key = value["apiKey"].get<std::string>();
        }
        if (value.contains("baseUrl") && value["baseUrl"].is_string()) {
          pc.base_url = value["baseUrl"].get<std::string>();
        }
        cfg.providers[key] = std::move(pc);
      }
    }

  } catch (const std::exception& e) {}

  ApplyEnvOverrides(cfg);
  return cfg;
}

inline void Config::ApplyEnvOverrides(Config& cfg) {
  const char* api_key = std::getenv("CLAW_API_KEY");
  if (api_key && !std::string(api_key).empty()) cfg.api_key = api_key;
  const char* model = std::getenv("CLAW_MODEL");
  if (model && !std::string(model).empty()) cfg.model = model;
  const char* base_url = std::getenv("CLAW_BASE_URL");
  if (base_url && !std::string(base_url).empty()) cfg.base_url = base_url;
}






}