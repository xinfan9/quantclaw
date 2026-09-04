#pragma once

namespace quantclaw {
struct Config {
  std::string api_key;
  std::string model;
  std::string base_url;

  static Config Load();
};

inline Config Config::Load() {
  Config cfg;

  const char* api_key = std::getenv("CLAW_API_KEY");
  if (api_key && !std::string(api_key).empty()) cfg.api_key = api_key;

  const char* model = std::getenv("CLAW_MODEL");
  if (model && !std::string(model).empty()) cfg.model = model;

  const char* base_url = std::getenv("CLAW_BASE_URL");
  if (base_url && !std::string(base_url).empty()) cfg.base_url = base_url;

  return cfg;
}

} // quantclaw