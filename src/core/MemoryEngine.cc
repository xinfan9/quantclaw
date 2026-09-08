#include "MemoryEngine.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
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

  struct Document {
    std::string text;
    std::vector<std::string> tokens;
  };

  std::vector<Document> docs;
  docs.reserve(_messages.size());

  std::size_t total_doc_len = 0;
  for (const auto& m : _messages) {
    if (m.role == "system" || m.role == "tool") continue;

    std::string text = m.role + ":" + m.content;
    auto tokens = Tokenize(text);
    if (!tokens.empty()) {
      docs.push_back({std::move(text), std::move(tokens)});
      total_doc_len += docs.back().tokens.size();
    }
  }

  if (docs.empty()) {
    spdlog::info("[memory] no valid documents, returning empty");
    return {};
  }

  const double avg_doc_len = static_cast<double>(total_doc_len) / docs.size();

  // 计算每个词出现在多少篇文档中（用于 IDF）
  std::unordered_map<std::string, std::size_t> doc_freq;
  for (const auto& doc : docs) {
    std::unordered_map<std::string, bool> seen;
    for (const auto& t : doc.tokens) {
      if (!seen[t]) {
        ++doc_freq[t];
        seen[t] = true;
      }
    }
  }

  struct ScoredMessage {
    double score;
    std::string text;
  };

  std::vector<ScoredMessage> scored;
  scored.reserve(docs.size());
  for (const auto& doc : docs) {
    double s = Bm25Score(query_tokens, doc.tokens, doc_freq, docs.size(), avg_doc_len);
    spdlog::debug("[memory] score text_len={} score={:.3f}", doc.tokens.size(), s);
    if (s > 0.0) scored.push_back({s, doc.text});
  }

  std::sort(scored.begin(), scored.end(), [](const ScoredMessage& a, const ScoredMessage& b) {
    return a.score > b.score;
  });

  std::vector<std::string> results;
  for (std::size_t i = 0; i < std::min(top_k, scored.size()); ++i) {
    results.push_back(std::move(scored[i].text));
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

std::vector<std::string> MemoryEngine::Tokenize(const std::string& text) {
  std::vector<std::string> tokens;
  std::string current;
  for (char c : text) {
    if (std::isalnum(static_cast<unsigned char>(c)))
      current += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    else if (!current.empty()) {
      tokens.push_back(std::move(current));
      current.clear();
    }
  }

  if (!current.empty()) tokens.push_back(std::move(current));

  return tokens;
}

double MemoryEngine::Bm25Score(const std::vector<std::string>& query_tokens,
                               const std::vector<std::string>& doc_tokens,
                               const std::unordered_map<std::string, std::size_t>& doc_freq,
                               std::size_t total_docs,
                               double avg_doc_len) {
  // 文档内词频
  std::unordered_map<std::string, std::size_t> term_freq;
  term_freq.reserve(doc_tokens.size());
  for (const auto& t : doc_tokens) ++term_freq[t];

  const double doc_len = static_cast<double>(doc_tokens.size());
  const double norm = 1.0 - _b + _b * (doc_len / avg_doc_len);

  double score = 0.0;
  for (const auto& qt : query_tokens) {
    auto tf_it = term_freq.find(qt);
    if (tf_it == term_freq.end()) continue;

    auto df_it = doc_freq.find(qt);
    const std::size_t df = (df_it != doc_freq.end()) ? df_it->second : 1;

    // IDF: log((N - n(q) + 0.5) / (n(q) + 0.5) + 1)
    const double idf = std::log(
        (static_cast<double>(total_docs) - static_cast<double>(df) + 0.5) /
            (static_cast<double>(df) + 0.5) +
        1.0);

    const double tf = static_cast<double>(tf_it->second);
    const double numerator = tf * (_k1 + 1.0);
    const double denominator = tf + _k1 * norm;

    score += idf * numerator / denominator;
  }

  return score;
}

}
