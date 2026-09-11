#include "ToolPermissionChecker.h"

namespace quantclaw::security {

ToolPermissionChecker::ToolPermissionChecker(std::vector<std::string> allow,
                                             std::vector<std::string> deny)
    : allowed_(allow.begin(), allow.end()), denied_(deny.begin(), deny.end()) {}

bool ToolPermissionChecker::IsAllowed(const std::string& tool_name) const {
  if (denied_.count("*") || denied_.count(tool_name)) return false;
  if (allowed_.empty() || allowed_.count("*") || allowed_.count(tool_name))
    return true;
  return false;
}

}  // namespace quantclaw::security
