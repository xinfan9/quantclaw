#pragma once
#include "providers/AnthropicProvider.h"

namespace quantclaw::core {

// 上下文修剪器：当对话历史过长时，按策略压缩或丢弃历史消息，使其适配模型上下文窗口
class ContextPruner {
public:
  // 修剪策略参数
  struct Options {
    int protect_recent = 2;      // 保留最近 N 条 assistant 消息不被修剪
    int soft_prune_lines = 3;    // 软修剪时，内容首尾各保留的行数
    int hard_prune_after = 5;    // 当 assistant 消息数量超过该阈值后，对更旧的 tool result 进行硬修剪
    int max_tool_result_chars = 1000;  // 单个 tool result 超过该字符数时触发软修剪
    int context_window = 8192;         // 模型上下文窗口大小（token 数）
    int max_tokens = 4096;             // 模型单次输出最大 token 数（预留输出预算）
    double prune_target_ratio = 0.75;  // 目标占用比例：历史消息最多占上下文窗口的该比例
  };

  // 根据 Options 对历史消息进行修剪，返回修剪后的消息列表
  static std::vector<providers::Message> Prune(const std::vector<providers::Message>& history, const Options& options);

  // 估算单条消息所占的 token 数
  static int EstimateTokens(const providers::Message& msg);
  // 估算多条消息总共占用的 token 数
  static int EstimateTokens(const std::vector<providers::Message>& msgs);
private:
  // 软修剪：保留内容首尾各 keep_lines 行，中间部分用省略号替代
  static std::string SoftPrune(const std::string& content, int keep_lines);
};

}
