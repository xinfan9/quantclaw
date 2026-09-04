//
// Created by xinfang on 2026/9/4.
//

#include "AnthropicProvider.h"

#include <nlohmann/json.hpp>

#include "HttpClient.h"

namespace quantclaw::providers {

std::string AnthropicProvider::Chat(std::vector<Messages>& messages) {
  nlohmann::json body;
  body["model"] = _model;
  body["max_tokens"] = 4096;
  body["messages"] = nlohmann::json::array();

  for (const auto& m : messages) {
    if (m.role == "system") {
      body["system"] = m.content;
    } else {
      body["messages"].push_back({{"role", m.role}, {"content", m.content}});
    }
  }

  const std::string url = _base_url + "/messages";
  const std::string request_body = body.dump();

  const std::vector<HttpHeader> headers = {
    {"Content-Type", "application/json"},
    {"x-api-key", _api_key},
    {"anthropic-version", "2023-06-01"},
  };

  std::string response = HttpPost(url, headers, request_body);
  auto json = nlohmann::json::parse(response, nullptr, false);
  if (json.is_discarded()) throw std::runtime_error("Failed to parse JSON response: " + response);
  if (json.contains("error")) throw std::runtime_error("Anthropic API error: " + json["error"].dump());
  if (json.contains("content") && !json["content"].empty() && json["content"][0].contains("text"))
   return json["content"][0]["text"].get<std::string>();

  throw std::runtime_error("Unexpected response format: " + response);
}
}