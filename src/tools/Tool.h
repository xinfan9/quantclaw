#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace quantclaw::tools {

class Tool {
public:
  std::string name;
  std::string description;
  nlohmann::json parameters;

  [[nodiscard]] virtual std::string Execute(const nlohmann::json& args) const = 0;
  virtual ~Tool() = default;
};



}
