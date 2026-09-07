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

  [[nodiscard]] bool Empty() const { return _tools.empty(); }

private:
  std::unordered_map<std::string, std::unique_ptr<Tool>> _tools;

};

}