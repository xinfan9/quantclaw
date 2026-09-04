#include "OpenAIProvider.h"
#include "HttpClient.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>

namespace quantclaw::providers {

OpenAIProvider::OpenAIProvider(std::string api_key, std::string model, std::string base_url):
 _api_key(std::move(api_key)),
 _model(std::move(model)),
 _base_url(std::move(base_url)) {
}

std::string OpenAIProvider::Chat(std::vector<Messages>& msg) {
  nlohmann::json body;
  body["model"] = _model;
  body["messages"] = nlohmann::json::array();
  for (const auto& m : msg) {
    body["messages"].push_back({{"role", m.role}, {"content", m.content}});
  }

  const std::string url = _base_url + "/chat/completions";
  const std::string request_body = body.dump();

  const std::vector<HttpHeader> headers = {
      {"Content-Type", "application/json"},
      {"Authorization", "Bearer " + _api_key},
  };

  std::string response = HttpPost(url, headers, request_body);

  auto json = nlohmann::json::parse(response, nullptr, false);
  if (json.is_discarded()) {
    throw std::runtime_error("Failed to parse JSON response: " + response);
  }

  if (json.contains("error")) {
    throw std::runtime_error("OpenAI API error: " + json["error"].dump());
  }

  if (json.contains("choices") && !json["choices"].empty() &&
      json["choices"][0].contains("message") &&
      json["choices"][0]["message"].contains("content")) return json["choices"][0]["message"]["content"].get<std::string>();

  throw std::runtime_error("Unexpected response format: " + response);
}







}