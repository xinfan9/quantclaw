#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace quantclaw::security {

// 工具权限检查器：基于 allow/deny 列表判断某个工具是否允许执行
//
// 判定规则：
// - 若 deny 列表包含 "*" 或该工具名，则拒绝。
// - 若 allow 列表为空，或包含 "*" 或该工具名，则允许。
// - 其余情况默认拒绝。
class ToolPermissionChecker {
public:
  ToolPermissionChecker() = default;
  explicit ToolPermissionChecker(std::vector<std::string> allow,
                                 std::vector<std::string> deny);

  // 根据工具名判断该工具是否被允许执行
  bool IsAllowed(const std::string& tool_name) const;

private:
  std::unordered_set<std::string> allowed_; // 允许列表（用于快速查找）
  std::unordered_set<std::string> denied_;  // 拒绝列表（用于快速查找）
};

}  // namespace quantclaw::security
