#pragma once
#include "EmbeddingProvider.h"


namespace quantclaw::providers {

class OpenAIEmbeddingProvider : public EmbeddingProvider {
public:
  OpenAIEmbeddingProvider(std::string api_key, std::string model, std::string base_url);

  EmbeddingResponse Embed(const EmbeddingRequest request) override;


  int Dimensions() const override;

  std::string Name() const override {return "openai-embedding";}

private:
  std::string api_key_;
  std::string model_;
  std::string base_url_;
};


}
