//
// 由 xinfang 创建于 2026/9/11.
//

#include "OpenAIEmbeddingProvider.h"

#include "HttpClient.h"
#include "nlohmann/json.hpp"


namespace quantclaw::providers {

OpenAIEmbeddingProvider::OpenAIEmbeddingProvider(std::string api_key, std::string model, std::string base_url)
  : api_key_(std::move(api_key)), model_(std::move(model)), base_url_(std::move(base_url)) {}

EmbeddingResponse OpenAIEmbeddingProvider::Embed(const EmbeddingRequest request) {
  EmbeddingResponse response;
  if (request.texts.empty()) return response;

  nlohmann::json body;
  body["input"] = request.texts;
  body["model"] = request.model.empty() ? model_ : request.model;
  body["encoding_format"] = "float";

  std::string url = base_url_ + "/embeddings";
  std::vector<HttpHeader> headers = {
    {"Content-Type", "application/json"},
    {"Authorization", "Bearer " + api_key_}
  };

  std::string raw = HttpPost(url, headers, body.dump());

  auto  json = nlohmann::json::parse(raw, nullptr, false);
  if (json.is_discarded()) {
    throw std::runtime_error("Failed to parse embedding response: " + raw);
  }

  if (json.contains("error")) {
    throw std::runtime_error("Embedding API error: " + json["error"].dump());
  }

  if (!json.contains("data") || !json["data"].is_array()) {
    throw std::runtime_error("Unexpected embedding response format: " + raw);
  }

  response.embeddings.resize(request.texts.size());
  for (const auto& item : json["data"]) {
    if (!item.contains("embedding") || !item["embedding"].is_array()) continue;

    std::size_t idx = 0;
    if (item.contains("index") && item["index"].is_number_unsigned()) {
      idx = item["index"].get<std::size_t>();
    }
    if (idx >= response.embeddings.size()) continue;

    for (const auto& v : item["embedding"]) {
      if (v.is_number()) {
        response.embeddings[idx].push_back(v.get<float>());
      }
    }
  }

  if (json.contains("usage") && json["usage"].contains("total_tokens") &&
      json["usage"]["total_tokens"].is_number_integer()) {
    response.total_tokens = json["usage"]["total_tokens"].get<int>();
      }

  return response;
}

int OpenAIEmbeddingProvider::Dimensions() const {
  if (model_.find("text-embedding-3-small") != std::string::npos) return 1536;
  if (model_.find("text-embedding-3-large") != std::string::npos) return 3072;
  if (model_.find("text-embedding-ada-002") != std::string::npos) return 1536;
  return 1536;  // OpenAI Embedding 模型的安全默认值。
}




}
