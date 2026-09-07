#pragma once
#include "LLMProvider.h"

namespace quantclaw::providers {
class AnthropicProvider : public LLMProvider {
public:
  AnthropicProvider(std::string api_key, std::string model, std::string base_url) :
    _api_key(std::move(api_key)), _model(std::move(model)), _base_url(std::move(base_url)) {}

  std::string Chat(std::vector<Message>& messages) override;
private:
  std::string _api_key;
  std::string _model;
  std::string _base_url;
};




}
