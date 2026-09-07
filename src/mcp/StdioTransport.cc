//
// Created by xinfang on 2026/9/7.
//

// StdioTransport 实现 MCP stdio transport：通过标准输入输出与外部子进程通信。
//
// 核心流程：
//   1. 创建两个匿名管道：
//      - stdin_pipe ：父进程写 → 子进程读
//      - stdout_pipe：子进程写 → 父进程读
//   2. fork() 创建子进程。
//   3. 子进程路径：
//      - 关闭父进程端的 pipe fd；
//      - 用 dup2() 把 stdin_pipe[0]  重定向到 STDIN_FILENO；
//      - 用 dup2() 把 stdout_pipe[1] 重定向到 STDOUT_FILENO；
//      - 关闭已被 dup2 复制过的原始 fd；
//      - 构造 argv 并 execvp() 执行外部命令；
//      - execvp 失败则 _exit(127)，避免子进程继续执行父进程代码。
//   4. 父进程路径：
//      - 关闭子进程端的 pipe fd；
//      - 保留 stdin_pipe[1] 用于向子进程写命令；
//      - 保留 stdout_pipe[0] 用于从子进程读响应。
//   5. 通信协议为 JSON Lines：每条消息以 '\n' 结尾。
//   6. 析构时：
//      - 关闭 pipe fd；
//      - 向子进程发送 SIGTERM 请求其退出；
//      - waitpid() 等待子进程结束，避免产生僵尸进程。

#include "StdioTransport.h"

#include <sys/wait.h>   // waitpid
#include <unistd.h>     // fork, pipe, dup2, close, read, write
#include <csignal>      // kill, SIGTERM

namespace quantclaw::mcp {

// PIMPL 实现：隐藏 POSIX 细节，避免污染头文件。
struct StdioTransport::Impl {
  pid_t pid = -1;       // 子进程 ID，>0 表示父进程持有的有效子进程
  int stdin_fd = -1;    // 父进程写入端（对应子进程 stdin）
  int stdout_fd = -1;   // 父进程读取端（对应子进程 stdout）
};
StdioTransport::StdioTransport(const std::string& command, const std::vector<std::string>& args)
  : _impl(std::make_unique<Impl>()) {
  int stdin_pipe[2];   // [0]=子进程读, [1]=父进程写
  int stdout_pipe[2];  // [0]=父进程读, [1]=子进程写

  // 1. 创建两个匿名管道，分别用于父子进程的 stdin 和 stdout。
  if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
    throw std::runtime_error("Failed to create pipe");
  }

  // 2. fork 子进程。
  _impl->pid = fork();
  if (_impl->pid < 0) {
    throw std::runtime_error("Failed to fork");
  }

  // 3. 子进程路径：重定向 stdin/stdout 并执行外部命令。
  if (_impl->pid == 0) {
    // 子进程不需要父进程端的 fd，先关闭。
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);

    // 把管道端重定向到标准输入输出，使外部命令通过管道通信。
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);

    // dup2 已经复制了 fd，原始 fd 可以关闭。
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    // 构造 argv：命令名 + 参数列表 + 结尾 nullptr。
    std::vector<const char*> argv;
    argv.push_back(command.c_str());
    for (const auto& arg : args) {
      argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    // 执行外部命令；成功则不会返回。
    execvp(command.c_str(), const_cast<char* const*>(argv.data()));

    // execvp 失败时立即退出，不要使用 exit() 以免刷新父进程的缓冲区等。
    _exit(127);
  }

  // 4. 父进程路径：关闭子进程端的 fd，保留父进程端。
  close(stdin_pipe[0]);
  close(stdout_pipe[1]);

  _impl->stdin_fd = stdin_pipe[1];
  _impl->stdout_fd = stdout_pipe[0];
}

StdioTransport::~StdioTransport() {
  if (_impl->pid > 0) {
    // 5. 析构时先关闭通信管道，子进程读取端关闭后通常会自然退出。
    close(_impl->stdin_fd);
    close(_impl->stdout_fd);

    // 发送 SIGTERM 请求子进程优雅退出。
    kill(_impl->pid, SIGTERM);

    // 等待子进程结束，避免产生僵尸进程。
    waitpid(_impl->pid, nullptr, 0);
  }
}

void StdioTransport::Send(const std::string& json_line) {
  // JSON Lines 协议：每条消息以换行符结尾。
  std::string msg = json_line + "\n";

  ssize_t written = write(_impl->stdin_fd, msg.c_str(), msg.size());
  if (written != static_cast<ssize_t>(msg.size())) {
    throw std::runtime_error("Failed to write to pipe");
  }
}

std::string StdioTransport::Receive() {
  // 按 JSON Lines 协议读取一行，直到遇到换行符。
  std::string result;
  char c;
  while (read(_impl->stdout_fd, &c, 1) > 0) {
    if (c == '\n') break;
    result += c;
  }

  if (result.empty()) {
    throw std::runtime_error("Failed to read from pipe");
  }
  return result;
}
}