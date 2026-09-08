#pragma once
#include <string>
#include <vector>

namespace quantclaw::providers {
struct HttpHeader {
  std::string name;
  std::string value;
};

std::string HttpGet(const std::string& url, const std::vector<HttpHeader>& headers,
                    long timeout_seconds = 60);
std::string HttpPost(const std::string& url, const std::vector<HttpHeader>& headers,
                     const std::string& body, long timeout_seconds = 60);
}
