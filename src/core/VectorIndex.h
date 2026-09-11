#pragma once
#include <vector>
#include <string>

namespace quantclaw::core {

struct VectorEntry {
  std::string id;
  std::vector<float> embedding;
  std::string content;
};

struct VectorSearchResult {
  std::string id;
  std::string content;
  float score;
};

class VectorIndex {
public:
  void Add(VectorEntry entry);

  std::vector<VectorSearchResult> Search(const std::vector<float>& query, int top_k = 10) const;

  size_t Size() const { return entries_.size();}

  void Clear() {
    entries_.clear();
  }

  static float CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);

private:
  std::vector<VectorEntry> entries_;
};


}
