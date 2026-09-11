#pragma once

#include <memory>
#include <string>
#include <vector>

#include "providers/LLMProvider.h"

namespace quantclaw::core {

// 上下文组装结果
struct AssembleResult {
  std::vector<providers::Message> messages; // 组装后的消息列表
  int estimated_tokens = 0;                 // 估算的总 token 数
};

// 上下文引擎抽象接口：负责将历史消息与用户输入组装成最终提交给 LLM 的消息上下文
class ContextEngine {
public:
  virtual ~ContextEngine() = default;

  // 返回引擎名称（用于日志记录）
  virtual std::string Name() const = 0;

  // 将历史消息和用户输入组装为最终上下文，需遵守 token 预算限制
  // history: 历史对话消息
  // user_message: 当前用户输入
  // context_window: 模型上下文窗口大小
  // max_tokens: 模型输出预留 token 数
  // 返回: 组装后的消息列表及估算 token 数
  virtual AssembleResult Assemble(const std::vector<providers::Message>& history,
                                  const std::string& user_message,
                                  int context_window, int max_tokens) = 0;
};

// 默认上下文引擎实现：将用户消息追加到历史末尾，并在超出 token 预算时调用修剪逻辑
class DefaultContextEngine : public ContextEngine {
public:
  std::string Name() const override { return "default"; }

  AssembleResult Assemble(const std::vector<providers::Message>& history,
                          const std::string& user_message, int context_window,
                          int max_tokens) override;
};

}  // namespace quantclaw::core
