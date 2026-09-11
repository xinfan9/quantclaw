//
// Created by xinfang on 2026/9/11.
//

#include "VectorIndex.h"

namespace quantclaw::core {
void VectorIndex::Add(VectorEntry entry) {
  entries_.push_back(std::move(entry));
}

std::vector<VectorSearchResult> VectorIndex::Search(const std::vector<float>& query, int top_k) const {
  std::vector<VectorSearchResult> results;
  if (query.empty() || entries_.empty() || top_k <= 0) return results;

  results.reserve(entries_.size());

  for (const auto& entry : entries_) {
    results.push_back({entry.id, entry.content, CosineSimilarity(query, entry.embedding)});
  }

    std::sort(results.begin(), results.end(), [](const VectorSearchResult& a, const VectorSearchResult& b) {
      return a.score > b.score;
    });

    if (static_cast<size_t>(top_k) < results.size())
    results.resize(top_k);

  return results;
}

float VectorIndex::CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
  if (a.size() != b.size() || a.empty()) return 0.0f;
  double dot = 0.0;
  double norm_a = 0.0;
  double norm_b = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    dot += static_cast<double>(a[i]) * static_cast<double>(b[i]);
    norm_a += static_cast<double>(a[i]) * static_cast<double>(a[i]);
    norm_b += static_cast<double>(b[i]) * static_cast<double>(b[i]);
  }

  if (norm_a == 0.0 || norm_b == 0.0) return 0.0f;
  return static_cast<float>(dot / (std::sqrt(norm_a) * std::sqrt(norm_b)));
}
}
