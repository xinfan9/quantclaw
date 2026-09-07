//
// Created by xinfang on 2026/9/4.
//

#include "ChatHistory.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "platform.h"

namespace quantclaw {namespace session {

std::string ChatHistory::DefaultPath() {
  return (std::filesystem::path(platform::home_directory())) / "history.json";
}

ChatHistory::ChatHistory(std::string path) : _path(std::move(path)) {}

std::vector<providers::Message> ChatHistory::Load() const {
  std::vector<providers::Message> messages;
  if (!std::filesystem::exists(_path)) return messages;

  std::ifstream file(_path);
  if (!file.is_open()) throw std::runtime_error("Cannot open file for reading, path : " + _path);
  nlohmann::json json;

  try {
    file >> json;
  } catch (const nlohmann::json::exception& e) {
    Clear();
    return messages;
  }

  if (!json.is_array()) {
    Clear();
    return messages;
  }

  for (const auto& item : json) {
    if (item.contains("role") && item["role"].is_string()
     && item.contains("content") && item["content"].is_string())
      messages.push_back({item["role"].get<std::string>(), item["content"].get<std::string>(), "", {}});
  }


  return messages;
}

void ChatHistory::Save(const std::vector<providers::Message>& messages) const {
  std::filesystem::create_directories(std::filesystem::path(_path).parent_path());

  nlohmann::json json = nlohmann::json::array();
  std::transform(messages.begin(), messages.end(), std::back_inserter(json), [](const auto& m) {
    return nlohmann::json({{"role", m.role}, {"content", m.content}});
  });

  std::ofstream file(_path);
  if (!file.is_open()) throw std::runtime_error("Cannot open file for writing, path : " + _path);
  file << json.dump(2);
}


void ChatHistory::Clear() const {
  std::error_code ec;
  std::filesystem::remove(_path, ec);
}
} // session
}                                         // quantclaw