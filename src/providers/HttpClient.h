#pragma once
#include <functional>
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
                     const std::string& body, long timeout_seconds = 300);

// SSE 流式回调：每收到一条完整的 "data: ..." 行就调用一次，参数为 data: 后的内容。
using SseCallback = std::function<void(const std::string& data_line)>;

// 流式 HTTP POST：用于 SSE 接口（如 OpenAI stream:true），边接收边回调。
void HttpStreamPost(const std::string& url, const std::vector<HttpHeader>& headers,
                    const std::string& body, SseCallback on_data,
                    long timeout_seconds = 300);
}
