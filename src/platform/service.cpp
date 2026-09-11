#include "service.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "process.hpp"

#ifndef _WIN32
#include <unistd.h>
#endif

namespace quantclaw::platform {

ServiceManager::ServiceManager(std::shared_ptr<spdlog::logger> logger)
    : logger_(logger ? logger : spdlog::default_logger()) {
  std::string home = HomeDirectory();
  state_dir_ = home + "/.quantclaw/service";
  pid_file_ = state_dir_ + "/gateway.pid";
  log_file_ = state_dir_ + "/gateway.log";

#ifdef __APPLE__
  service_file_ = home +
                  "/Library/LaunchAgents/io.quantclaw.my-claw.gateway.plist";
#elif defined(__linux__)
  service_file_ =
      home + "/.config/systemd/user/quantclaw-my-claw-gateway.service";
#else
  service_file_ = state_dir_ + "/gateway.service";
#endif
}

void ServiceManager::EnsureDirs() {
  std::filesystem::create_directories(state_dir_);
#ifdef __linux__
  std::filesystem::create_directories(HomeDirectory() + "/.config/systemd/user");
#endif
}

std::string ServiceManager::ServiceName() const {
#ifdef __APPLE__
  return "io.quantclaw.my-claw.gateway";
#elif defined(__linux__)
  return "quantclaw-my-claw-gateway";
#else
  return "quantclaw-my-claw-gateway";
#endif
}

bool ServiceManager::WriteServiceFile(int port) {
  std::string exe = ExecutablePath();
  if (exe.empty()) {
    logger_->error("无法确定可执行文件路径");
    return false;
  }

  std::ostringstream content;
#ifdef __APPLE__
  content << R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>)" << ServiceName() << R"(</string>
  <key>ProgramArguments</key>
  <array>
    <string>)" << exe << R"(</string>
    <string>--gateway</string>
  </array>
  <key>EnvironmentVariables</key>
  <dict>
    <key>CLAW_GATEWAY_PORT</key>
    <string>)" << port << R"(</string>
  </dict>
  <key>StandardOutPath</key>
  <string>)" << log_file_ << R"(</string>
  <key>StandardErrorPath</key>
  <string>)" << log_file_ << R"(</string>
  <key>KeepAlive</key>
  <false/>
  <key>RunAtLoad</key>
  <false/>
</dict>
</plist>
)";
#elif defined(__linux__)
  content << R"([Unit]
Description=QuantClaw my_claw Gateway
After=network.target

[Service]
Type=simple
ExecStart=)" << exe << R"( --gateway
Environment="CLAW_GATEWAY_PORT=)" << port << R"("
StandardOutput=append:)" << log_file_ << R"(
StandardError=append:)" << log_file_ << R"(
Restart=on-failure

[Install]
WantedBy=default.target
)";
#else
  content << R"(# QuantClaw my_claw Gateway service stub
# This platform uses PID file management.
EXEC=)" << exe << R"(
PORT=)" << port << R"(
PID_FILE=)" << pid_file_ << R"(
LOG_FILE=)" << log_file_ << R"(
)";
#endif

  std::ofstream out(service_file_);
  if (!out) return false;
  out << content.str();
  return out.good();
}

bool ServiceManager::RemoveServiceFile() {
  try {
    return std::filesystem::remove(service_file_);
  } catch (...) {
    return false;
  }
}

int ServiceManager::Install(int port) {
  EnsureDirs();
  if (!WriteServiceFile(port)) {
    std::cerr << "Failed to write service file: " << service_file_ << "\n";
    return 1;
  }
#ifdef __APPLE__
  auto r = ExecCapture("launchctl bootstrap gui/" +
                       std::to_string(getuid()) + " " + service_file_);
  if (r.exit_code != 0) {
    logger_->warn("launchctl bootstrap: {}", r.output);
  }
#elif defined(__linux__)
  ExecCapture("systemctl --user daemon-reload");
#endif
  std::cout << "Gateway 服务已安装: " << service_file_ << "\n";
  std::cout << "PID file: " << pid_file_ << "\n";
  std::cout << "Log file: " << log_file_ << "\n";
  return 0;
}

int ServiceManager::Uninstall() {
  Stop();
  RemoveServiceFile();
  RemovePid();
  std::cout << "Gateway 服务已卸载.\n";
  return 0;
}

bool ServiceManager::StartViaServiceManager() {
#ifdef __APPLE__
  auto r = ExecCapture("launchctl start " + ServiceName());
  return r.exit_code == 0;
#elif defined(__linux__)
  auto r = ExecCapture("systemctl --user start " + ServiceName());
  return r.exit_code == 0;
#else
  return false;
#endif
}

bool ServiceManager::StopViaServiceManager() {
#ifdef __APPLE__
  auto r = ExecCapture("launchctl stop " + ServiceName());
  return r.exit_code == 0;
#elif defined(__linux__)
  auto r = ExecCapture("systemctl --user stop " + ServiceName());
  return r.exit_code == 0;
#else
  return false;
#endif
}

bool ServiceManager::StartViaPidFile() {
  EnsureDirs();
  int existing = GetPid();
  if (existing > 0 && IsProcessAlive(existing)) {
    logger_->warn("Gateway 已在运行 (pid {})", existing);
    return true;
  }

  std::string exe = ExecutablePath();
  if (exe.empty()) {
    logger_->error("无法确定可执行文件路径");
    return false;
  }

  // 子进程启动 gateway，输出重定向到日志文件
  auto pid = SpawnProcess({exe, "--gateway", "--background"}, {},
                          std::filesystem::path(exe).parent_path().string());
  if (pid <= 0) {
    logger_->error("启动 gateway 进程失败");
    return false;
  }

  // 给新进程一点时间启动并写入 PID 文件
  for (int i = 0; i < 50; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (IsProcessAlive(pid)) {
      WritePid(pid);
      return true;
    }
  }
  return IsProcessAlive(pid);
}

int ServiceManager::Start() {
  if (IsRunning()) {
    std::cout << "Gateway 已在运行 (pid " << GetPid() << ").\n";
    return 0;
  }

  EnsureDirs();
  if (!std::filesystem::exists(service_file_)) {
    logger_->info("服务文件未安装，回退到 PID 文件方式");
    if (StartViaPidFile()) {
      std::cout << "Gateway 已启动 (pid " << GetPid() << ").\n";
      return 0;
    }
    std::cerr << "启动 gateway 失败.\n";
    return 1;
  }

  if (StartViaServiceManager()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (IsRunning()) {
      std::cout << "Gateway started via service manager (pid " << GetPid()
                << ").\n";
      return 0;
    }
  }

  logger_->warn("服务管理器启动失败，回退到 PID 文件方式");
  if (StartViaPidFile()) {
    std::cout << "Gateway 已启动 (pid " << GetPid() << ").\n";
    return 0;
  }

  std::cerr << "Failed to start gateway.\n";
  return 1;
}

int ServiceManager::Stop() {
  int pid = GetPid();
  if (pid > 0 && IsProcessAlive(pid)) {
    TerminateProcess(pid);
    WaitProcess(pid, 3000);
    if (IsProcessAlive(pid)) {
      KillProcess(pid);
      WaitProcess(pid, 2000);
    }
  }

  if (std::filesystem::exists(service_file_)) {
    StopViaServiceManager();
  }

  RemovePid();

  if (IsRunning()) {
    std::cerr << "停止 gateway 失败.\n";
    return 1;
  }
  std::cout << "Gateway 已停止.\n";
  return 0;
}

int ServiceManager::Restart() {
  if (Stop() != 0) return 1;
  return Start();
}

int ServiceManager::Status() {
  int pid = GetPid();
  bool running = IsRunning();
  std::cout << "Gateway 服务: " << (running ? "运行中" : "已停止") << "\n";
  if (running) {
    std::cout << "  PID: " << pid << "\n";
  }
  std::cout << "  Service file: " << service_file_ << "\n";
  std::cout << "  PID file: " << pid_file_ << "\n";
  std::cout << "  Log file: " << log_file_ << "\n";
  return running ? 0 : 1;
}

bool ServiceManager::IsRunning() const {
  int pid = GetPid();
  if (pid <= 0) {
    // 同时检查系统服务管理器是否知道该服务。
#ifdef __APPLE__
    auto r = ExecCapture("launchctl list | grep " + ServiceName());
    if (r.exit_code == 0 && r.output.find(ServiceName()) != std::string::npos) {
      return true;
    }
#elif defined(__linux__)
    auto r = ExecCapture("systemctl --user is-active " + ServiceName());
    if (r.exit_code == 0) return true;
#endif
    return false;
  }
  return IsProcessAlive(pid);
}

int ServiceManager::GetPid() const {
  std::ifstream in(pid_file_);
  if (!in) return 0;
  int pid = 0;
  in >> pid;
  return pid;
}

void ServiceManager::WritePid(int pid) {
  EnsureDirs();
  std::ofstream out(pid_file_);
  if (out) out << pid << "\n";
}

void ServiceManager::RemovePid() {
  try {
    std::filesystem::remove(pid_file_);
  } catch (...) {
  }
}

}  // namespace quantclaw::platform
