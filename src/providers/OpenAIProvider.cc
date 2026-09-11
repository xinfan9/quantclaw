#include "OpenAIProvider.h"
#include "HttpClient.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vector>

#include "tools/ToolRegistry.h"

namespace quantclaw::providers {
/***
std::move 是一个类型转换工具，把一个对象变成右值引用，允许接收方窃取它的资源。它本身不移动数据，真正移动的是移动构造或移动赋值函数。移动后原对象仍然有效，但内容不确定。
初始化列表方式直接构造成员变量，避免了先默认构造再赋值的额外步骤。遇到引用、常量或没有默认构造的成员时，初始化列表是唯一选择。
 */
OpenAIProvider::OpenAIProvider(std::string api_key, std::string model, std::string base_url):
 _api_key(std::move(api_key)),
 _model(std::move(model)),
 _base_url(std::move(base_url)) {
}

static nlohmann::json MessagesToJson(const std::vector<Message>& messages) {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& m : messages) {
    nlohmann::json msg;
    msg["role"] = m.role;

    if (m.role == "tool") {
      msg["content"] = m.content;
      msg["tool_call_id"] = m.tool_call_id;
    } else if (!m.tool_calls.empty()) {
      msg["content"] = m.content.empty() ? "" : m.content;
      msg["tool_calls"] = nlohmann::json::array();
      for (const auto& tc : m.tool_calls) {
        msg["tool_calls"].push_back({
            {"id", tc.id},
            {"type", "function"},
            {"function", {{"name", tc.name}, {"arguments", tc.arguments.dump()}}},
        });
      }
    } else {
      msg["content"] = m.content;
    }

    arr.push_back(msg);
  }
  return arr;
}

ChatResponse OpenAIProvider::Chat(const std::vector<Message>& messages, const tools::ToolRegistry& tools) {
  nlohmann::json body;
  body["model"] = _model;
  body["messages"] = MessagesToJson(messages);

  if (!tools.Empty()) {
    body["tools"] = tools.GetDefinitions();
  }

  std::string url = _base_url + "/chat/completions";
  std::string request_body = body.dump();

  std::vector<HttpHeader> headers = {
    {"Content-Type", "application/json"},
    {"Authorization", "Bearer " + _api_key},
};

  std::string response = HttpPost(url, headers, request_body);
  spdlog::debug("[openai] response received, length={}", response.size());

  auto json = nlohmann::json::parse(response, nullptr, false);
  if (json.is_discarded()) {
    throw std::runtime_error("Failed to parse JSON response: " + response);
  }

  if (json.contains("error")) {
    throw std::runtime_error("OpenAI API error: " + json["error"].dump());
  }

  if (!json.contains("choices") || json["choices"].empty()) {
    throw std::runtime_error("Unexpected response format: " + response);
  }

  const auto& choice = json["choices"][0];
  ChatResponse result;

  // 解析 content
  if (choice.contains("message") && choice["message"].contains("content") &&
      !choice["message"]["content"].is_null()) {
    result.content = choice["message"]["content"].get<std::string>();
      }
  spdlog::debug("[openai] content.length={}, has_tool_calls={}",
                result.content.size(),
                choice["message"].contains("tool_calls"));

  // 解析 tool_calls
  if (choice.contains("message") && choice["message"].contains("tool_calls")) {
    for (const auto& tc : choice["message"]["tool_calls"]) {
      if (tc.value("type", "") == "function" && tc.contains("function")) {
        std::string id = tc.value("id", "");
        std::string name = tc["function"].value("name", "");
        nlohmann::json args = nlohmann::json::object();
        if (tc["function"].contains("arguments")) {
          args = nlohmann::json::parse(tc["function"]["arguments"].get<std::string>(),
                                       nullptr, false);
          if (args.is_discarded()) {
            args = tc["function"]["arguments"];
          }
        }
        result.tool_calls.push_back({id, name, args});
      }
    }
  }

  return result;
}


std::string OpenAIProvider::Chat(std::vector<Message>& msg) {
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

ChatResponse OpenAIProvider::StreamChat(const std::vector<Message>& messages,
                                        const tools::ToolRegistry& tools,
                                        TokenCallback on_token) {
  nlohmann::json body;
  body["model"] = _model;
  body["messages"] = MessagesToJson(messages);
  body["stream"] = true;

  if (!tools.Empty()) {
    body["tools"] = tools.GetDefinitions();
  }

  std::string url = _base_url + "/chat/completions";
  std::string request_body = body.dump();

  std::vector<HttpHeader> headers = {
      {"Content-Type", "application/json"},
      {"Authorization", "Bearer " + _api_key},
  };

  ChatResponse result;
  bool has_tool_calls = false;

  HttpStreamPost(url, headers, request_body,
      [&result, &on_token, &has_tool_calls](const std::string& data) {
        auto json = nlohmann::json::parse(data, nullptr, false);
        if (json.is_discarded()) return;

        if (!json.contains("choices") || json["choices"].empty()) return;
        const auto& choice = json["choices"][0];

        if (!choice.contains("delta")) return;
        const auto& delta = choice["delta"];

        // 文本 token
        if (delta.contains("content") && !delta["content"].is_null()) {
          std::string token = delta["content"].get<std::string>();
          result.content += token;
          if (on_token) on_token(token);
        }

        // tool_calls delta（不支持流式 tool_calls，标记回退）
        if (delta.contains("tool_calls")) {
          has_tool_calls = true;
        }
      });

  spdlog::debug("[openai stream] received, content.length={}, has_tool_calls={}",
                result.content.size(), has_tool_calls);

  // 如果检测到 tool_calls，回退到非流式调用获取完整 tool_call 信息
  if (has_tool_calls) {
    spdlog::debug("[openai stream] tool_calls detected, falling back to non-stream");
    return Chat(messages, tools);
  }

  return result;
}
}
