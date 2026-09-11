#pragma once

#include <string>
#include <vector>

namespace quantclaw::providers {

struct EmbeddingRequest {
  std::vector<std::string> texts;
  std::string model = "text-embedding-3-small";
};

struct EmbeddingResponse {
  std::vector<std::vector<float>> embeddings;
  int total_tokens = 0;
};

class EmbeddingProvider {
public:
  virtual ~EmbeddingProvider() = default;

  virtual EmbeddingResponse Embed(EmbeddingRequest) = 0;

  virtual int Dimensions() const = 0;

  virtual std::string Name() const = 0;

};

}