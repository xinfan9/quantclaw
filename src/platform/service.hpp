#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace quantclaw::platform {

constexpr int kDefaultGatewayPort = 18800;

// 跨平台守护进程/服务管理
// Linux: 生成 systemd 用户级服务（无需 root）
// macOS: 生成 launchd 用户级代理
// 兜底方案: PID 文件 + 启动/结束进程
class ServiceManager {
 public:
  explicit ServiceManager(std::shared_ptr<spdlog::logger> logger = nullptr);

  // 安装当前平台对应的服务定义文件
  int Install(int port = kDefaultGatewayPort);

  // 卸载服务
  int Uninstall();

  // 启动服务
  int Start();

  // 停止服务
  int Stop();

  // 重启服务
  int Restart();

  // 打印服务状态
  int Status();

  // 判断服务是否正在运行
  bool IsRunning() const;

  // 获取运行中服务的 PID
  int GetPid() const;

  // 写入 PID 文件
  void WritePid(int pid);

  // 删除 PID 文件
  void RemovePid();

 private:
  std::shared_ptr<spdlog::logger> logger_;
  std::string state_dir_;
  std::string pid_file_;
  std::string log_file_;
  std::string service_file_;

  void EnsureDirs();
  bool WriteServiceFile(int port);
  bool RemoveServiceFile();
  bool StartViaServiceManager();
  bool StopViaServiceManager();
  bool StartViaPidFile();
  std::string ServiceName() const;
};

}  // namespace quantclaw::platform
