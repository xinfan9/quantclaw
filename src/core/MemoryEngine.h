#pragma once
#include "providers/LLMProvider.h"

#include <unordered_map>

namespace quantclaw::core {

class MemoryEngine {
 public:
  explicit MemoryEngine(std::vector<providers::Message> messages);
  std::vector<std::string> Search(const std::string& query, std::size_t top_k = 3) const;
  static std::string FormatContext(const std::vector<std::string>& relevant_messages);

 private:
  std::vector<providers::Message> _messages;

  // BM25 超参数
  static constexpr double _k1 = 1.5;
  static constexpr double _b = 0.75;

  static std::vector<std::string> Tokenize(const std::string& text);
  static double Bm25Score(const std::vector<std::string>& query_tokens,
                          const std::vector<std::string>& doc_tokens,
                          const std::unordered_map<std::string, std::size_t>& doc_freq,
                          std::size_t total_docs,
                          double avg_doc_len);
};

}
