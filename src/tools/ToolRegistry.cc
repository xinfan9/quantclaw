#include "ToolRegistry.h"

#include <spdlog/spdlog.h>

namespace quantclaw::tools {

void ToolRegistry::Register(std::unique_ptr<Tool> tool) {
  if (!tool) return;
  _tools[tool->name] = std::move(tool);
}

nlohmann::json ToolRegistry::GetDefinitions() const {
  nlohmann::json tools = nlohmann::json::array();
  for (const auto& [name, tool] : _tools)
    tools.push_back({{"type", "function"},
      {"function", {
        {"name", tool->name},
        {"description", tool->description},
        {"parameters", tool->parameters},
      }}});
  return tools;
}

bool ToolRegistry::Has(const std::string& name) const {
  return _tools.find(name) != _tools.end();
}

std::string ToolRegistry::Execute(const std::string& name, const nlohmann::json& args) const {
  if (!Has(name)) throw std::runtime_error("Tool not found:" + name);
  /***
    _tools.at(name) 的好处就是：只读访问，键不存在时报错，不会污染注册表。
    当key不存在时：
      map[key]插入默认值，返回引用
      map.at(key) 抛 std::out_of_range，不插入
      map.find(key) 返回 end()，不插入
      map.insert({key, val})插入（如果已存在则不插入）
   */
  spdlog::info("[tool execute] name={} args={}", name, args.dump());
  std::string result = _tools.at(name)->Execute(args);
  spdlog::info("[tool result] name={} result={}", name, result);
  return result;
}





}