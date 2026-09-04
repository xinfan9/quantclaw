//
// Created by xinfang on 2026/9/4.
//
#include "HttpClient.h"

#include <curl/curl.h>
#include <stdexcept>
#include <string>

namespace quantclaw::providers {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb,
                            std::string* userp) {
  userp->append(static_cast<char*>(contents), size * nmemb);
  return size * nmemb;
}


std::string HttpPost(const std::string& url,
                     const std::vector<HttpHeader>& headers,
                     const std::string& body, long timeout_seconds) {
  std::string response;

  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize CURL");
  }

  curl_slist* curl_headers = nullptr;
  for (const auto& h : headers) {
    std::string header_line = h.name + ": " + h.value;
    curl_headers = curl_slist_append(curl_headers, header_line.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(curl_headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    throw std::runtime_error(std::string("CURL request failed: ") +
                             curl_easy_strerror(res));
  }

  if (http_code < 200 || http_code >= 300) {
    throw std::runtime_error("HTTP error " + std::to_string(http_code) +
                             ": " + response);
  }

  return response;
}


}