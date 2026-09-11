//
// 由 xinfang 创建于 2026/9/4.
//

#include "AnthropicProvider.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

#include "HttpClient.h"
#include "tools/ToolRegistry.h"

namespace quantclaw::providers {

namespace {

nlohmann::json MessagesToAnthropicJson(const std::vector<Message>& messages) {
  nlohmann::json result;
  result["messages"] = nlohmann::json::array();

  for (const auto& m : messages) {
    if (m.role == "system") {
      result["system"] = m.content;
    } else if (m.role == "tool") {
      nlohmann::json msg;
      msg["role"] = "user";
      msg["content"] = nlohmann::json::array();
      nlohmann::json tool_result;
      tool_result["type"] = "tool_result";
      tool_result["tool_use_id"] = m.tool_call_id;
      tool_result["content"] = m.content;
      msg["content"].push_back(tool_result);
      result["messages"].push_back(msg);
    } else if (!m.tool_calls.empty()) {
      nlohmann::json msg;
      msg["role"] = "assistant";
      msg["content"] = nlohmann::json::array();
      if (!m.content.empty()) {
        msg["content"].push_back({{"type", "text"}, {"text", m.content}});
      }
      for (const auto& tc : m.tool_calls) {
        nlohmann::json tool_use;
        tool_use["type"] = "tool_use";
        tool_use["id"] = tc.id;
        tool_use["name"] = tc.name;
        tool_use["input"] = tc.arguments;
        msg["content"].push_back(tool_use);
      }
      result["messages"].push_back(msg);
    } else {
      result["messages"].push_back({{"role", m.role}, {"content", m.content}});
    }
  }

  return result;
}

ChatResponse ParseAnthropicResponse(const nlohmann::json& json) {
  ChatResponse result;
  if (json.contains("content") && json["content"].is_array()) {
    for (const auto& block : json["content"]) {
      if (!block.contains("type")) continue;
      const std::string type = block["type"].get<std::string>();
      if (type == "text" && block.contains("text")) {
        result.content += block["text"].get<std::string>();
      } else if (type == "tool_use" && block.contains("id") && block.contains("name")) {
        ToolCall tc;
        tc.id = block["id"].get<std::string>();
        tc.name = block["name"].get<std::string>();
        tc.arguments = block.value("input", nlohmann::json::object());
        result.tool_calls.push_back(std::move(tc));
      }
    }
  }
  return result;
}

nlohmann::json ConvertToolsForAnthropic(const nlohmann::json& openai_tools) {
  nlohmann::json tools = nlohmann::json::array();
  for (const auto& tool : openai_tools) {
    if (!tool.contains("function")) continue;
    const auto& func = tool["function"];
    tools.push_back({
        {"name", func.value("name", "")},
        {"description", func.value("description", "")},
        {"input_schema", func.value("parameters", nlohmann::json::object())},
    });
  }
  return tools;
}

} // namespace

std::string AnthropicProvider::Chat(std::vector<Message>& messages) {
  nlohmann::json body;
  body["model"] = _model;
  body["max_tokens"] = 4096;

  const auto msg_json = MessagesToAnthropicJson(messages);
  if (msg_json.contains("system")) body["system"] = msg_json["system"];
  body["messages"] = msg_json["messages"];

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

  const ChatResponse result = ParseAnthropicResponse(json);
  if (!result.content.empty()) return result.content;
  if (!result.tool_calls.empty()) return "[tool call]";

  throw std::runtime_error("Unexpected response format: " + response);
}

ChatResponse AnthropicProvider::Chat(const std::vector<Message>& messages, const tools::ToolRegistry& tools) {
  nlohmann::json body;
  body["model"] = _model;
  body["max_tokens"] = 4096;

  const auto msg_json = MessagesToAnthropicJson(messages);
  if (msg_json.contains("system")) body["system"] = msg_json["system"];
  body["messages"] = msg_json["messages"];

  if (!tools.Empty()) {
    body["tools"] = ConvertToolsForAnthropic(tools.GetDefinitions());
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

  return ParseAnthropicResponse(json);
}

} // namespace quantclaw::providers
