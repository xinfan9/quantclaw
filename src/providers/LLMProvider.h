#pragma once
#include <functional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace quantclaw::tools {
class ToolRegistry;
}

namespace quantclaw::providers {

struct Message {
  std::string role;
  std::string content;

  std::string tool_call_id;
  std::vector<struct ToolCall> tool_calls;
};

struct ToolCall {
  std::string id;
  std::string name;
  nlohmann::json arguments;
};

struct ChatResponse {
  std::string content;
  std::vector<ToolCall> tool_calls;

  [[nodiscard]] bool isToolCall() const {
    return !tool_calls.empty();
  }
};

// 流式 token 回调：每收到一个增量文本片段就调用一次。
using TokenCallback = std::function<void(const std::string& token)>;


class LLMProvider {
public:
  // LLMProvider 抽象基类：定义多轮对话接口，具体 Provider 需实现 Chat 方法。
  // 虚析构函数保证派生类对象通过基类指针释放时能正确调用自身析构函数。
  virtual ~LLMProvider() = default;
  virtual std::string Chat(std::vector<Message>&) = 0;
  virtual ChatResponse Chat(const std::vector<Message>& messages, const tools::ToolRegistry& tools) = 0;

  // 流式对话：边接收边通过 on_token 回调返回增量文本。
  // 不支持流式 tool_calls；若模型返回 tool_call 则回退到非流式。
  // 返回完整的 ChatResponse（content 为拼接后的全部文本）。
  virtual ChatResponse StreamChat(const std::vector<Message>& messages,
                                  const tools::ToolRegistry& tools,
                                  TokenCallback on_token) {
    // 默认实现：非流式回退
    (void)on_token;
    return Chat(messages, tools);
  }
};
} // providers
