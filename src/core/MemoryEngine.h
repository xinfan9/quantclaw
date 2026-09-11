#pragma once
#include "providers/LLMProvider.h"

#include <unordered_map>

#include "VectorIndex.h"
#include "providers/EmbeddingProvider.h"

namespace quantclaw::core {

class MemoryEngine {
 public:
  explicit MemoryEngine(std::vector<providers::Message> messages,
    std::shared_ptr<providers::EmbeddingProvider> embedding_provider = nullptr);
  std::vector<std::string> Search(const std::string& query, std::size_t top_k = 3) const;
  static std::string FormatContext(const std::vector<std::string>& relevant_messages);
  static std::vector<std::string> Tokenize(const std::string& text);

 private:
  std::vector<providers::Message> _messages;

  std::shared_ptr<providers::EmbeddingProvider> _embedder;
  VectorIndex _vector_index;
  bool _has_vector = false;

  // BM25 超参数
  static constexpr double _k1 = 1.5;
  static constexpr double _b = 0.75;

  void BuildVectorIndex();

  static double Bm25Score(const std::vector<std::string>& query_tokens,
                          const std::vector<std::string>& doc_tokens,
                          const std::unordered_map<std::string, std::size_t>& doc_freq,
                          std::size_t total_docs,
                          double avg_doc_len);
};

}
