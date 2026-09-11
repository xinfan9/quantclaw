#include "MemoryEngine.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace quantclaw::core {

namespace {

struct Document {
  std::string text;
  std::vector<std::string> tokens;
};

std::vector<Document> BuildDocuments(
    const std::vector<providers::Message>& messages,
    std::size_t& total_doc_len) {
  std::vector<Document> docs;
  total_doc_len = 0;
  for (const auto& m : messages) {
    if (m.role == "system" || m.role == "tool") continue;

    std::string text = m.role + ":" + m.content;
    auto tokens = MemoryEngine::Tokenize(text);
    if (!tokens.empty()) {
      docs.push_back({std::move(text), std::move(tokens)});
      total_doc_len += docs.back().tokens.size();
    }
  }
  return docs;
}

}  // namespace

MemoryEngine::MemoryEngine(
    std::vector<providers::Message> messages,
    std::shared_ptr<providers::EmbeddingProvider> embedder)
    : _messages(std::move(messages)), _embedder(std::move(embedder)) {
  BuildVectorIndex();
}

void MemoryEngine::BuildVectorIndex() {
  if (!_embedder) {
    _has_vector = false;
    return;
  }

  std::size_t total_doc_len = 0;
  auto docs = BuildDocuments(_messages, total_doc_len);
  if (docs.empty()) {
    _has_vector = false;
    return;
  }

  std::vector<std::string> texts;
  texts.reserve(docs.size());
  for (const auto& doc : docs) texts.push_back(doc.text);

  try {
    providers::EmbeddingRequest request;
    request.texts = texts;
    auto response = _embedder->Embed(request);
    if (response.embeddings.size() != docs.size()) {
      spdlog::warn("[memory] embedding count mismatch: got {} expected {}",
                   response.embeddings.size(), docs.size());
      _has_vector = false;
      return;
    }

    for (std::size_t i = 0; i < docs.size(); ++i) {
      _vector_index.Add({std::to_string(i), std::move(response.embeddings[i]),
                         docs[i].text});
    }
    _has_vector = true;
    spdlog::info("[memory] vector index built with {} entries", _vector_index.Size());
  } catch (const std::exception& e) {
    spdlog::warn("[memory] failed to build vector index: {}", e.what());
    _has_vector = false;
  }
}

std::vector<std::string> MemoryEngine::Search(const std::string& query,
                                              std::size_t top_k) const {
  auto query_tokens = Tokenize(query);
  spdlog::debug("[memory] query='{}' tokens={}", query, query_tokens.size());

  std::size_t total_doc_len = 0;
  auto docs = BuildDocuments(_messages, total_doc_len);
  if (docs.empty()) {
    spdlog::info("[memory] no valid documents, returning empty");
    return {};
  }

  const double avg_doc_len = static_cast<double>(total_doc_len) / docs.size();

  // Document frequency for IDF.
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

  // BM25 candidates.
  std::vector<ScoredMessage> bm25_scored;
  bm25_scored.reserve(docs.size());
  for (const auto& doc : docs) {
    double s = Bm25Score(query_tokens, doc.tokens, doc_freq, docs.size(),
                         avg_doc_len);
    if (s > 0.0) bm25_scored.push_back({s, doc.text});
  }
  std::sort(bm25_scored.begin(), bm25_scored.end(),
            [](const ScoredMessage& a, const ScoredMessage& b) {
              return a.score > b.score;
            });

  std::size_t candidate_count = std::min(top_k * 2, bm25_scored.size());
  if (candidate_count == 0 && !_has_vector) {
    spdlog::info("[memory] no BM25 matches and no vector index");
    return {};
  }

  // Vector candidates.
  std::vector<ScoredMessage> vector_scored;
  if (_has_vector) {
    try {
      providers::EmbeddingRequest request;
      request.texts = {query};
      auto response = _embedder->Embed(request);
      if (!response.embeddings.empty() && !response.embeddings[0].empty()) {
        auto vec_results =
            _vector_index.Search(response.embeddings[0], static_cast<int>(top_k * 2));
        for (const auto& r : vec_results) {
          vector_scored.push_back({static_cast<double>(r.score), r.content});
        }
      }
    } catch (const std::exception& e) {
      spdlog::warn("[memory] vector search failed: {}", e.what());
    }
  }

  // Normalize and merge scores.
  std::unordered_map<std::string, std::pair<double, bool>> bm25_map;
  double max_bm25 = 0.0;
  for (std::size_t i = 0; i < candidate_count; ++i) {
    max_bm25 = std::max(max_bm25, bm25_scored[i].score);
    bm25_map[bm25_scored[i].text] = {bm25_scored[i].score, true};
  }

  std::unordered_map<std::string, std::pair<double, bool>> vector_map;
  double max_vector = 0.0;
  for (const auto& v : vector_scored) {
    max_vector = std::max(max_vector, v.score);
    vector_map[v.text] = {v.score, true};
  }

  const double kBm25Weight = 0.5;
  const double kVectorWeight = 0.5;

  std::unordered_map<std::string, double> combined;
  for (const auto& [text, pair] : bm25_map) {
    double bm25_norm = (max_bm25 > 0.0) ? pair.first / max_bm25 : 0.0;
    double vec_norm = 0.0;
    auto it = vector_map.find(text);
    if (it != vector_map.end()) {
      vec_norm = (max_vector > 0.0) ? (it->second.first + 1.0) / 2.0 : 0.0;
    }
    if (_has_vector && !vector_scored.empty()) {
      combined[text] = kBm25Weight * bm25_norm + kVectorWeight * vec_norm;
    } else {
      combined[text] = bm25_norm;
    }
  }

  // Include vector-only candidates.
  if (_has_vector && !vector_scored.empty()) {
    for (const auto& [text, pair] : vector_map) {
      if (combined.find(text) != combined.end()) continue;
      double vec_norm = (max_vector > 0.0) ? (pair.first + 1.0) / 2.0 : 0.0;
      combined[text] = vec_norm;
    }
  }

  std::vector<ScoredMessage> merged;
  merged.reserve(combined.size());
  for (auto& [text, score] : combined) {
    merged.push_back({score, text});
  }
  std::sort(merged.begin(), merged.end(),
            [](const ScoredMessage& a, const ScoredMessage& b) {
              return a.score > b.score;
            });

  std::vector<std::string> results;
  std::size_t limit = std::min(top_k, merged.size());
  for (std::size_t i = 0; i < limit; ++i) {
    results.push_back(std::move(merged[i].text));
  }

  spdlog::info("[memory] returned {} of {} merged candidates", results.size(),
               merged.size());
  for (std::size_t i = 0; i < results.size(); ++i) {
    spdlog::info("[memory]  [{}] {}", i, results[i]);
  }
  return results;
}

std::string MemoryEngine::FormatContext(
    const std::vector<std::string>& relevant_messages) {
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

}  // namespace quantclaw::core
