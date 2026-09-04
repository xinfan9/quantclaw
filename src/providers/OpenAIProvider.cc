//
// Created by xinfang on 2026/9/4.
//

#include "OpenAIProvider.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <stdexcept>

namespace quantclaw::providers {

OpenAIProvider::OpenAIProvider(std::string api_key, std::string model, std::string base_url):
 _api_key(std::move(api_key)),
 _model(std::move(model)),
 _base_url(std::move(base_url)) {
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
  userp->append(static_cast<char*>(contents), size * nmemb);
  return size * nmemb;
};

std::string OpenAIProvider::Chat(std::vector<Messages>& msg) {
  nlohmann::json body;
  body["model"] = _model;
  body["messages"] = nlohmann::json::array();
  for (const auto& m : msg) {
    body["messages"].push_back({{"role", m.role}, {"content", m.content}});
  }

  std::string url = _base_url + "/chat/completions";
  std::string request_body = body.dump();
  std::string response;

  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize CURL");
  }

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  std::string auth_header = "Authorization: Bearer " + _api_key;
  headers = curl_slist_append(headers, auth_header.c_str());

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  // CURLOPT_WRITEFUNCTION：告诉 curl，收到响应数据时调用哪个函数。
  // CURLOPT_WRITEDATA：告诉 curl，把哪个指针作为 userp 传给回调函数。
  // 这样 curl 就会把服务器返回的所有数据通过 WriteCallback 追加到 response 字符串里。
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));
  }

  if (http_code != 200) {
    throw std::runtime_error("HTTP error " + std::to_string(http_code) + ": " + response);
  }

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

  throw std::runtime_error("Unexpected response format:" + response);
}







}