#pragma once
#include "Tool.h"
#include <memory>
#include <unordered_map>
namespace quantclaw::tools {
class ToolRegistry {
public:
  void Register(std::unique_ptr<Tool> tool);
  [[nodiscard]] nlohmann::json GetDefinitions() const;
  [[nodiscard]] bool Has(const std::string& name) const;
  [[nodiscard]] std::string Execute(const std::string& name, const nlohmann::json& args) const;

  // 返回所有已注册工具名
  [[nodiscard]] std::vector<std::string> Names() const;
  // 返回指定工具参数 schema
  [[nodiscard]] nlohmann::json GetSchema(const std::string& name) const;
  // 返回指定工具描述
  [[nodiscard]] std::string GetDescription(const std::string& name) const;

  [[nodiscard]] bool Empty() const { return _tools.empty(); }

private:
  std::unordered_map<std::string, std::unique_ptr<Tool>> _tools;

};

}