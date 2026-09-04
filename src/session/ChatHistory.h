#pragma once
#include <vector>
#include "../providers/LLMProvider.h"

namespace quantclaw::session {
class ChatHistory {
public:
  explicit ChatHistory(std::string path = DefaultPath());

  [[nodiscard]] std::vector<providers::Messages> Load() const;
  void Save(const std::vector<providers::Messages>&) const;
  void Clear() const;

  static std::string DefaultPath();

private:
  std::string _path;
};
}
