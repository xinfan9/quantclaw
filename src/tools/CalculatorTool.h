#pragma once
#include "Tool.h"
#include <string>
#include <nlohmann/json.hpp>

namespace quantclaw::tools {
class CalculatorTool : public Tool {
public:
  CalculatorTool();

  std::string Execute(const nlohmann::json& args) const override;

};
} // quantclaw
