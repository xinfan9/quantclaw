#include "ContextPruner.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>

namespace quantclaw::core {

// 估算单条消息占用的 token 数（简化估算：按字符数/4，加上固定开销）
// 对英文和常见文本来说，OpenAI/Claude 的 tokenizer 大致是 1 token ≈ 4 个字符（或 0.75 个单词）。
// 这个比例虽然不准确，但用于“是否接近上下文上限”的粗估已经够用，而且实现简单。
int ContextPruner::EstimateTokens(const providers::Message& msg) {
  int tokens = 4;  // 消息本身的固定开销

  tokens += static_cast<int>(msg.content.size()) / 4;      // 内容按每 4 字符估算 1 token
  tokens += static_cast<int>(msg.tool_call_id.size()) / 4; // tool_call_id 长度

  for (const auto& tc : msg.tool_calls) {
    tokens += 10;                                          // 每个 tool call 的固定开销
    tokens += static_cast<int>(tc.name.size()) / 4;        // 工具名长度
    tokens += static_cast<int>(tc.arguments.dump().size()) / 4; // 工具参数 JSON 长度
  }

  return tokens;
}

// 估算多条消息的总 token 数
int ContextPruner::EstimateTokens(
    const std::vector<providers::Message>& msgs) {
  int total = 0;
  for (const auto& msg : msgs) total += EstimateTokens(msg);
  return total;
}

// 软修剪：保留内容首尾各 keep_lines 行，中间用省略提示替换
std::string ContextPruner::SoftPrune(const std::string& content,
                                     int keep_lines) {
  std::vector<std::string> lines;
  std::stringstream ss(content);
  std::string line;
  while (std::getline(ss, line)) {
    lines.push_back(line);
  }

  // 行数不足时无需修剪，直接返回原内容
  if (static_cast<int>(lines.size()) <= keep_lines * 2) return content;

  std::string result;
  // 保留开头 keep_lines 行
  for (int i = 0; i < keep_lines; ++i) {
    result += lines[i] + "\n";
  }
  // 中间省略提示
  result += "... (" + std::to_string(lines.size() - keep_lines * 2) +
            " lines pruned) ...\n";
  // 保留末尾 keep_lines 行
  for (int i = static_cast<int>(lines.size()) - keep_lines;
       i < static_cast<int>(lines.size()); ++i) {
    result += lines[i] + "\n";
  }
  return result;
}

// 根据 Options 对历史消息进行修剪，使其 token 占用不超过目标预算
std::vector<providers::Message> ContextPruner::Prune(
    const std::vector<providers::Message>& history, const Options& opts) {
  if (opts.context_window <= 0) return history; // 上下文窗口无效时不修剪

  // 计算目标 token 预算：上下文窗口 * 目标比例 - 输出预留
  int target = static_cast<int>(opts.context_window * opts.prune_target_ratio) -
               opts.max_tokens;
  if (target <= 0) target = opts.context_window / 2; // 目标非法时退化为窗口的一半

  auto pruned = history;

  // 统计每条消息之前有多少条 assistant 消息，用于判断消息“年龄”
  int assistant_count = 0;
  std::vector<int> assistant_index(pruned.size(), 0);
  for (std::size_t i = 0; i < pruned.size(); ++i) {
    assistant_index[i] = assistant_count;
    if (pruned[i].role == "assistant") {
      ++assistant_count;
    }
  }

  // 第一阶段：对 tool result 进行软/硬修剪
  for (std::size_t i = 0; i < pruned.size(); ++i) {
    if (pruned[i].role != "tool") continue; // 只处理 tool 角色消息

    // distance 表示该 tool result 距离最新 assistant 消息有多远
    int distance = assistant_count - assistant_index[i];
    if (distance <= opts.protect_recent) continue; // 最近 N 条 assistant 相关的 tool result 受保护

    if (distance > opts.hard_prune_after) {
      // 太旧的 tool result 直接硬修剪为占位符
      pruned[i].content = "[pruned]";
      continue;
    }

    // 内容过长时进行软修剪，保留首尾部分行
    if (static_cast<int>(pruned[i].content.size()) > opts.max_tool_result_chars) {
      pruned[i].content = SoftPrune(pruned[i].content, opts.soft_prune_lines);
    }
  }

  // 第二阶段：如果仍然超出预算，从最早的消息开始丢弃
  int tokens = EstimateTokens(pruned);
  spdlog::debug("[context] tokens={} target={}", tokens, target);

  // 至少保留：protect_recent 条 assistant 及其 tool result + 最新 user 消息 + system 消息
  const std::size_t min_keep = static_cast<std::size_t>(opts.protect_recent * 2 + 2);

  while (tokens > target && pruned.size() > min_keep) {
    // 找到最早的一条非 system 消息
    auto it = pruned.begin();
    if (it->role == "system") ++it; // 尽量保留 system 提示词
    if (it == pruned.end()) break;

    // 如果该 assistant 消息带有 tool_calls，则一并删除对应的 tool result
    if (it->role == "assistant" && !it->tool_calls.empty()) {
      std::vector<std::string> ids;
      for (const auto& tc : it->tool_calls) ids.push_back(tc.id);
      it = pruned.erase(it);
      while (it != pruned.end() && it->role == "tool" &&
             std::find(ids.begin(), ids.end(), it->tool_call_id) != ids.end()) {
        it = pruned.erase(it);
      }
    } else {
      pruned.erase(it);
    }

    tokens = EstimateTokens(pruned); // 重新估算 token 数
  }

  spdlog::info("[context] pruned to {} messages, estimated tokens={}",
               pruned.size(), tokens);
  return pruned;
}

}  // namespace quantclaw::core
