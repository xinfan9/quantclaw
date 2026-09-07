//
// Created by xinfang on 2026/9/7.
//

#include "MemoryEngine.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <unordered_map>

namespace quantclaw::core {

MemoryEngine::MemoryEngine(std::vector<providers::Message> messages) : _messages(std::move(messages)) {}

std::vector<std::string> MemoryEngine::Search(const std::string& query, std::size_t top_k) const {
  auto query_tokens = Tokenize(query);
  spdlog::debug("[memory] query='{}' tokens={}", query, query_tokens.size());
  if (query_tokens.empty() || _messages.empty()) {
    spdlog::info("[memory] no query tokens or no messages, returning empty");
    return {};
  }

  struct ScoredMessasge {
    double score;
    std::string text;
  };

  std::vector<ScoredMessasge> scored;
  for (const auto& m : _messages) {
    if (m.role == "system" || m.role == "tool") continue;

    std::string text = m.role + ":" + m.content;
    double s = Score(query_tokens, text);
    spdlog::debug("[memory] score role={} score={:.3f}", m.role, s);
    if (s > 0.0) scored.push_back({s, text});
  }
  std::sort(scored.begin(), scored.end(), [](const ScoredMessasge& a, const ScoredMessasge& b) {
    return a.score > b.score;
  });

  std::vector<std::string> results;
  for (std::size_t i = 0; i < std::min(top_k, scored.size()); ++i) {
    results.push_back(scored[i].text);
  }
  spdlog::info("[memory] returned {} of {} relevant messages", results.size(), scored.size());
  for (std::size_t i = 0; i < results.size(); ++i) {
    spdlog::info("[memory]  [{}] {}", i, results[i]);
  }
  return results;
}

std::string MemoryEngine::FormatContext(const std::vector<std::string>& relevant_messages) {
  if (relevant_messages.empty()) return "";
  std::string context = "Relevant context from memory:\n";
  for (const auto& msg : relevant_messages) context += "-" + msg + "\n";

  return context;
}

// 一个简单的空白切词
std::vector<std::string> MemoryEngine::Tokenize(const std::string& text) {
  std::vector<std::string> tokens;
  std::string current;
  for (char c : text) {
    if (std::isalnum(static_cast<unsigned char>(c)))
      current += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    else if (!current.empty()) {
      tokens.push_back(current);
      current.clear();
    }
  }

  if (!current.empty()) tokens.push_back(current);

  return tokens;
}

double MemoryEngine::Score(const std::vector<std::string>& query_tokens, const std::string& text) {
  auto text_tokens = Tokenize(text);
  if (text_tokens.empty()) return 0.0;

  std::unordered_map<std::string, std::size_t> text_freq;
  for (const auto& token : text_tokens) ++text_freq[token];

  double score = 0.0;
  for (const auto& qt : query_tokens) {
    auto it = text_freq.find(qt);
    if (it != text_freq.end()) score += static_cast<double>(it->second);
  }

  return score / std::sqrt(static_cast<double>(text_tokens.size()));
}





}