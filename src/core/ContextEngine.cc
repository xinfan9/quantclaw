#include "ContextEngine.h"

#include "ContextPruner.h"

#include <spdlog/spdlog.h>

namespace quantclaw::core {

AssembleResult DefaultContextEngine::Assemble(
    const std::vector<providers::Message>& history,
    const std::string& user_message, int context_window, int max_tokens) {
  AssembleResult result;
  result.messages = history;
  result.messages.push_back({"user", user_message, "", {}});

  if (context_window <= 0) context_window = 8192;
  if (max_tokens <= 0) max_tokens = 4096;

  ContextPruner::Options opts;
  opts.context_window = context_window;
  opts.max_tokens = max_tokens;

  result.messages = ContextPruner::Prune(result.messages, opts);
  result.estimated_tokens = ContextPruner::EstimateTokens(result.messages);

  spdlog::info("[context] engine={} messages={} estimated_tokens={}", Name(),
               result.messages.size(), result.estimated_tokens);
  return result;
}

}  // namespace quantclaw::core
