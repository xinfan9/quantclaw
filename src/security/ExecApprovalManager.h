#pragma once

#include <string>
#include <vector>

namespace quantclaw::security {

// 执行命令审批模式
enum class AskMode {
  kOff,     // 不需要审批
  kOnMiss,  // 不在白名单中且看起来危险时才询问
  kAlways,  // 总是需要审批
};

// 将字符串解析为 AskMode（如 "off"/"on_miss"/"always"）
AskMode ParseAskMode(const std::string& s);

// 命令白名单：支持 glob 风格的通配符匹配
// '*' 匹配任意字符序列，'?' 匹配单个字符
class ExecAllowlist {
public:
  // 添加一条白名单模式
  void AddPattern(const std::string& pattern);

  // 判断给定命令是否匹配任意一条白名单模式
  bool Matches(const std::string& command) const;

private:
  std::vector<std::string> patterns_; // 白名单模式列表

  // glob 风格通配符匹配实现
  static bool GlobMatch(const std::string& pattern, const std::string& text);
};

// 外部命令执行审批配置
struct ExecApprovalConfig {
  AskMode mode = AskMode::kOnMiss; // 审批模式
  int timeout_seconds = 120;        // 等待用户审批的超时时间（秒）
  std::vector<std::string> allowlist; // 无需审批即可执行的命令白名单
};

// 外部命令执行审批管理器：根据配置决定是否要求用户确认
class ExecApprovalManager {
public:
  explicit ExecApprovalManager(ExecApprovalConfig config = {});

  // 判断命令是否需要审批；若需要则向用户请求确认
  // 返回 true 表示已批准或无需审批
  bool RequestApproval(const std::string& command_summary) const;

  // 默认危险命令模式（如 rm、curl、bash 等）
  static const std::vector<std::string>& DefaultDangerousPatterns();

private:
  // 判断命令是否属于危险操作
  bool IsDangerous(const std::string& command_summary) const;
  // 向用户发起确认询问
  bool AskUser(const std::string& command_summary) const;

  ExecApprovalConfig config_; // 审批配置
  ExecAllowlist allowlist_;   // 命令白名单
};

}  // namespace quantclaw::security
