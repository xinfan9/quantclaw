#include "SubagentTool.h"

#include <nlohmann/json.hpp>

namespace quantclaw::tools {

SubagentTool::SubagentTool(
    std::shared_ptr<quantclaw::core::SubagentManager> manager)
    : manager_(std::move(manager)) {
  name = "spawn_subagent";
  description =
      "将复杂任务拆分为子任务，由子 agent 独立执行并返回结果。"
      "输入应包含清晰的任务描述，可选指定模型。";
  parameters = nlohmann::json::object({
      {"type", "object"},
      {"properties",
       nlohmann::json::object({
           {"task",
            nlohmann::json::object({
                {"type", "string"},
                {"description", "需要子 agent 执行的具体任务描述"},
            })},
           {"model",
            nlohmann::json::object({
                {"type", "string"},
                {"description", "可选的模型名称，为空则使用默认模型"},
            })},
       })},
      {"required", nlohmann::json::array({"task"})},
  });
}

std::string SubagentTool::Execute(const nlohmann::json& args) const {
  if (!manager_) {
    return "[subagent error] SubagentManager 未初始化";
  }

  std::string task = args.value("task", "");
  std::string model = args.value("model", "");

  auto result = manager_->Run(task, model);
  if (result.success) {
    return result.output;
  }
  return "[subagent error] " + result.error;
}

}  // namespace quantclaw::tools
