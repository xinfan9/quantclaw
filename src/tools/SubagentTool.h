#pragma once

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "Tool.h"
#include "core/SubagentManager.h"

namespace quantclaw::tools {

// 子 agent 工具：允许 LLM 通过调用此工具将复杂任务拆分为子任务并行/串行执行。
class SubagentTool : public Tool {
 public:
  explicit SubagentTool(std::shared_ptr<quantclaw::core::SubagentManager> manager);

  std::string Execute(const nlohmann::json& args) const override;

 private:
  std::shared_ptr<quantclaw::core::SubagentManager> manager_;
};

}  // namespace quantclaw::tools
