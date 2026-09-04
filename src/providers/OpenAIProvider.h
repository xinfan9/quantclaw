#pragma once
#include "LLMProvider.h"

namespace quantclaw::providers {
class OpenAIProvider : public LLMProvider {
public:
  OpenAIProvider(std::string api_key, std::string model, std::string base_url);
  std::string Chat(std::vector<Messages>&) override;

private:
  std::string _api_key;
  std::string _model;
  std::string _base_url;
};

}
