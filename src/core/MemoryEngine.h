#pragma once
#include "providers/LLMProvider.h"

namespace quantclaw::core {

class MemoryEngine {
public:
  explicit MemoryEngine(std::vector<providers::Message> messages);
  std::vector<std::string> Search(const std::string& query, std::size_t top_k = 3) const;
  static std::string FormatContext(const std::vector<std::string>& relevant_messages);

private:
  std::vector<providers::Message> _messages;
  static std::vector<std::string> Tokenize(const std::string& text);
  static double Score(const std::vector<std::string>& query_tokens, const std::string& text);
};

}
