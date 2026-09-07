#pragma once

#include <string>

namespace quantclaw::security {

class PermissionManager {
public:
  enum class Mode {
    kAlwaysAsk,
    kAutoAllow,
    KAutoDeny,
  };

  explicit PermissionManager(Mode mode = Mode::kAlwaysAsk);

  bool RequestPermission(const std::string& tool_name, const std::string& tool_args_summary) const;

private:
  Mode _mode;


};



}