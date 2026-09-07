//
// Created by xinfang on 2026/9/7.
//

#include "PermissionManager.h"

#include <iostream>

namespace quantclaw::security {
PermissionManager::PermissionManager(Mode mode) : _mode(mode) {}


bool PermissionManager::RequestPermission(
  const std::string& tool_name,
  const std::string& tool_args_summary) const {

  switch (_mode) {
    case Mode::kAutoAllow:
      return true;
    case Mode::KAutoDeny:
      return false;
    case Mode::kAlwaysAsk:
  default:
    std::cout << "Do you want to allow the use of " << tool_name
     << ", args:" << tool_args_summary << "? (y/n): " << std::endl;
    char answer = 0;
    std::cin >> answer;

    return answer == 'y' || answer == 'Y';
  }
}


}
