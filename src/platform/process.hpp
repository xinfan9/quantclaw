#pragma once

#include <functional>
#include <string>
#include <vector>

namespace quantclaw::platform {

using ProcessId = int;
constexpr ProcessId kInvalidPid = 0;

struct ExecResult {
  std::string output;
  int exit_code = -1;
};

// 启动子进程，成功返回 PID（>0），失败返回 0
ProcessId SpawnProcess(const std::vector<std::string>& args,
                       const std::vector<std::string>& env = {},
                       const std::string& working_dir = "");

// 判断进程是否仍在运行
bool IsProcessAlive(ProcessId pid);

// 发送优雅停止信号（Unix 下为 SIGTERM）
void TerminateProcess(ProcessId pid);

// 强制结束进程（Unix 下为 SIGKILL）
void KillProcess(ProcessId pid);

// 等待进程退出，返回退出码；出错返回 -1
int WaitProcess(ProcessId pid, int timeout_ms = -1);

// 执行命令并捕获标准输出，阻塞直到完成
ExecResult ExecCapture(const std::string& command, int timeout_seconds = 30,
                       const std::string& working_dir = "");

// 获取当前可执行文件的绝对路径
std::string ExecutablePath();

// 获取用户主目录
std::string HomeDirectory();

}  // namespace quantclaw::platform
