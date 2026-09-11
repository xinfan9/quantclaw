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

static curl_slist* BuildHeaders(const std::vector<HttpHeader>& headers) {
  curl_slist* curl_headers = nullptr;
  for (const auto& h : headers) {
    std::string header_line = h.name + ": " + h.value;
    curl_headers = curl_slist_append(curl_headers, header_line.c_str());
  }
  return curl_headers;
}

std::string HttpGet(const std::string& url,
                    const std::vector<HttpHeader>& headers,
                    long timeout_seconds) {
  std::string response;

  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize CURL");
  }

  curl_slist* curl_headers = BuildHeaders(headers);

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
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

std::string HttpPost(const std::string& url,
                     const std::vector<HttpHeader>& headers,
                     const std::string& body, long timeout_seconds) {
  std::string response;

  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize CURL");
  }

  curl_slist* curl_headers = BuildHeaders(headers);

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

// SSE 流式写入回调：累积数据，遇到完整行就解析并调用 on_data。
struct StreamContext {
  std::string buffer;
  std::string error_body;  // 累积非 SSE 响应体（用于错误信息）
  SseCallback on_data;
  bool is_sse = false;     // 是否检测到 SSE 格式
};

static size_t StreamWriteCallback(void* contents, size_t size, size_t nmemb,
                                  void* userp) {
  auto* ctx = static_cast<StreamContext*>(userp);
  size_t total = size * nmemb;
  ctx->buffer.append(static_cast<char*>(contents), total);

  // 逐行处理完整行
  size_t pos;
  while ((pos = ctx->buffer.find('\n')) != std::string::npos) {
    std::string line = ctx->buffer.substr(0, pos);
    ctx->buffer.erase(0, pos + 1);

    // 去掉 \r
    if (!line.empty() && line.back() == '\r') line.pop_back();

    if (line.empty()) continue;

    // SSE 格式：以 "data: " 开头
    if (line.rfind("data: ", 0) == 0) {
      ctx->is_sse = true;
      std::string data = line.substr(6);
      if (data == "[DONE]") continue;  // OpenAI 流结束标志
      if (ctx->on_data) ctx->on_data(data);
    } else if (!ctx->is_sse) {
      // 非 SSE 响应，累积为错误体
      ctx->error_body += line + "\n";
    }
  }
  return total;
}

void HttpStreamPost(const std::string& url,
                    const std::vector<HttpHeader>& headers,
                    const std::string& body,
                    SseCallback on_data,
                    long timeout_seconds) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize CURL");
  }

  curl_slist* curl_headers = BuildHeaders(headers);

  StreamContext ctx;
  ctx.on_data = std::move(on_data);

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(curl_headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    throw std::runtime_error(std::string("CURL stream request failed: ") +
                             curl_easy_strerror(res));
  }

  if (http_code < 200 || http_code >= 300) {
    std::string err = ctx.error_body.empty() ? ctx.buffer : ctx.error_body;
    throw std::runtime_error("HTTP error " + std::to_string(http_code) +
                             ": " + err);
  }
}

}
