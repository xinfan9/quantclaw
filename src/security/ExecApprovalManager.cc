#include "ExecApprovalManager.h"

#include <iostream>

namespace quantclaw::security {

AskMode ParseAskMode(const std::string& s) {
  std::string lower;
  for (char c : s) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (lower == "off" || lower == "false" || lower == "0") return AskMode::kOff;
  if (lower == "always" || lower == "true" || lower == "1") return AskMode::kAlways;
  return AskMode::kOnMiss;
}

void ExecAllowlist::AddPattern(const std::string& pattern) {
  patterns_.push_back(pattern);
}

bool ExecAllowlist::Matches(const std::string& command) const {
  for (const auto& pattern : patterns_) {
    if (GlobMatch(pattern, command)) return true;
  }
  return false;
}

bool ExecAllowlist::GlobMatch(const std::string& pattern,
                              const std::string& text) {
  // glob 匹配的动态规划表。
  std::vector<std::vector<bool>> dp(pattern.size() + 1,
                                    std::vector<bool>(text.size() + 1, false));
  dp[0][0] = true;

  for (std::size_t i = 1; i <= pattern.size(); ++i) {
    if (pattern[i - 1] == '*') dp[i][0] = dp[i - 1][0];
  }

  for (std::size_t i = 1; i <= pattern.size(); ++i) {
    for (std::size_t j = 1; j <= text.size(); ++j) {
      char p = pattern[i - 1];
      if (p == '*') {
        dp[i][j] = dp[i - 1][j] || dp[i][j - 1];
      } else if (p == '?') {
        dp[i][j] = dp[i - 1][j - 1];
      } else {
        dp[i][j] = dp[i - 1][j - 1] && p == text[j - 1];
      }
    }
  }

  return dp[pattern.size()][text.size()];
}

ExecApprovalManager::ExecApprovalManager(ExecApprovalConfig config)
    : config_(std::move(config)) {
  for (const auto& p : config_.allowlist) {
    allowlist_.AddPattern(p);
  }
}

bool ExecApprovalManager::RequestApproval(
    const std::string& command_summary) const {
  if (config_.mode == AskMode::kOff) return true;
  if (allowlist_.Matches(command_summary)) return true;
  if (config_.mode == AskMode::kAlways) return AskUser(command_summary);

  // kOnMiss 模式：仅对看起来有风险的命令询问用户。
  if (IsDangerous(command_summary)) return AskUser(command_summary);
  return true;
}

bool ExecApprovalManager::AskUser(const std::string& command_summary) const {
  std::cout << "Approve execution of: " << command_summary << "? (y/n): ";
  char answer = 0;
  std::cin >> answer;
  return answer == 'y' || answer == 'Y';
}

bool ExecApprovalManager::IsDangerous(
    const std::string& command_summary) const {
  std::string lower;
  for (char c : command_summary) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  for (const auto& pattern : DefaultDangerousPatterns()) {
    if (lower.find(pattern) != std::string::npos) return true;
  }
  return false;
}

const std::vector<std::string>& ExecApprovalManager::DefaultDangerousPatterns() {
  static std::vector<std::string> patterns = {
      "rm ",      "rm -",     "del ",    "delete ", "curl ",    "wget ",
      "bash ",    "sh ",      "zsh ",    "exec ",   "eval ",    "python ",
      "python3 ", "node ",    "npm ",    "npx ",    "docker ",  "kubectl ",
      "mv ",      "cp -",     "chmod ",  "chown ",  "sudo ",    "su ",
      "fork ",    "system(",  "popen",   "shell",   "spawn",    "subprocess"};
  return patterns;
}

}  // namespace quantclaw::security
