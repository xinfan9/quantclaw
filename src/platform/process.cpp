#include "process.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <thread>

#ifndef _WIN32
#include <libgen.h>
#include <spawn.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

namespace quantclaw::platform {

std::string HomeDirectory() {
  const char* home = std::getenv("HOME");
  if (home) return home;
#ifdef _WIN32
  const char* userprofile = std::getenv("USERPROFILE");
  if (userprofile) return userprofile;
#endif
  return ".";
}

std::string ExecutablePath() {
#ifdef _WIN32
  char buf[MAX_PATH];
  GetModuleFileNameA(nullptr, buf, MAX_PATH);
  return buf;
#else
  std::array<char, 4096> buf{};
  ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
  if (len > 0) {
    buf[len] = '\0';
    return buf.data();
  }
  return "";
#endif
}

bool IsProcessAlive(ProcessId pid) {
  if (pid <= 0) return false;
#ifdef _WIN32
  HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (!h) return false;
  DWORD code = 0;
  BOOL ok = GetExitCodeProcess(h, &code);
  CloseHandle(h);
  return ok && code == STILL_ACTIVE;
#else
  if (kill(pid, 0) == 0) return true;
  return errno != ESRCH;
#endif
}

void TerminateProcess(ProcessId pid) {
  if (pid <= 0) return;
#ifdef _WIN32
  HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
  if (h) {
    TerminateProcess(h, 1);
    CloseHandle(h);
  }
#else
  kill(pid, SIGTERM);
#endif
}

void KillProcess(ProcessId pid) {
  if (pid <= 0) return;
#ifdef _WIN32
  HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
  if (h) {
    TerminateProcess(h, 1);
    CloseHandle(h);
  }
#else
  kill(pid, SIGKILL);
#endif
}

int WaitProcess(ProcessId pid, int timeout_ms) {
  if (pid <= 0) return -1;
#ifdef _WIN32
  HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (!h) return -1;
  DWORD ms = timeout_ms < 0 ? INFINITE : timeout_ms;
  DWORD r = WaitForSingleObject(h, ms);
  DWORD code = -1;
  if (r == WAIT_OBJECT_0) GetExitCodeProcess(h, &code);
  CloseHandle(h);
  return static_cast<int>(code);
#else
  if (timeout_ms < 0) {
    int status = 0;
    if (waitpid(pid, &status, 0) == pid) {
      return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
  }
  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    int status = 0;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == pid) {
      return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    if (r < 0 && errno == ECHILD) return -1;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return -1;
#endif
}

ProcessId SpawnProcess(const std::vector<std::string>& args,
                       const std::vector<std::string>& env,
                       const std::string& working_dir) {
  if (args.empty()) return kInvalidPid;

#ifndef _WIN32
  pid_t pid = fork();
  if (pid < 0) return kInvalidPid;
  if (pid == 0) {
    // 子进程。
    for (const auto& e : env) {
      auto pos = e.find('=');
      if (pos == std::string::npos) continue;
      std::string key = e.substr(0, pos);
      std::string value = e.substr(pos + 1);
      if (setenv(key.c_str(), value.c_str(), 1) != 0) {
        _exit(127);
      }
    }
    if (!working_dir.empty()) {
      if (chdir(working_dir.c_str()) != 0) _exit(127);
    }
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  return pid;
#else
  // Windows 最小回退方案：使用 CreateProcessA + cmd /c。
  std::ostringstream cmd;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i) cmd << " ";
    cmd << "\"" << args[i] << "\"";
  }
  STARTUPINFOA si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  if (CreateProcessA(nullptr, const_cast<char*>(cmd.str().c_str()), nullptr,
                     nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                     working_dir.empty() ? nullptr : working_dir.c_str(), &si,
                     &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<ProcessId>(pi.dwProcessId);
  }
  return kInvalidPid;
#endif
}

ExecResult ExecCapture(const std::string& command, int timeout_seconds,
                       const std::string& working_dir) {
  ExecResult result;
#ifndef _WIN32
  std::array<int, 2> pipefd{};
  if (pipe(pipefd.data()) != 0) return result;

  pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return result;
  }

  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    if (!working_dir.empty()) chdir(working_dir.c_str());
    execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
    _exit(127);
  }

  close(pipefd[1]);
  std::array<char, 1024> buf{};
  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::seconds(timeout_seconds > 0 ? timeout_seconds
                                                           : 3600);
  int flags = fcntl(pipefd[0], F_GETFL, 0);
  fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

  while (true) {
    ssize_t n = read(pipefd[0], buf.data(), buf.size() - 1);
    if (n > 0) {
      result.output.append(buf.data(), n);
    } else if (n == 0) {
      break;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      if (timeout_seconds > 0 &&
          std::chrono::steady_clock::now() > deadline) {
        kill(pid, SIGKILL);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    } else {
      break;
    }
  }
  close(pipefd[0]);

  int status = 0;
  if (timeout_seconds > 0) {
    int remaining = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                              std::chrono::steady_clock::now())
            .count());
    if (remaining < 0) remaining = 0;
    WaitProcess(pid, remaining);
  } else {
    waitpid(pid, &status, 0);
  }
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
  // Windows 回退方案：简单的同步执行。
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  HANDLE rd, wr;
  CreatePipe(&rd, &wr, &sa, 0);
  SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA si{};
  si.cb = sizeof(si);
  si.hStdOutput = wr;
  si.hStdError = wr;
  si.dwFlags |= STARTF_USESTDHANDLES;
  PROCESS_INFORMATION pi{};
  if (CreateProcessA(nullptr, const_cast<char*>(command.c_str()), nullptr,
                     nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                     working_dir.empty() ? nullptr : working_dir.c_str(), &si,
                     &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(wr);
    std::array<char, 1024> buf{};
    DWORD read = 0;
    while (ReadFile(rd, buf.data(), buf.size() - 1, &read, nullptr) &&
           read > 0) {
      result.output.append(buf.data(), read);
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    result.exit_code = static_cast<int>(code);
    CloseHandle(pi.hProcess);
  } else {
    CloseHandle(wr);
  }
  CloseHandle(rd);
#endif
  return result;
}

}  // namespace quantclaw::platform
