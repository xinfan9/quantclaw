#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>

#include "config.h"
#include "providers/LLMProvider.h"
#include "security/ExecApprovalManager.h"
#include "security/ToolPermissionChecker.h"
#include "tools/ToolRegistry.h"

namespace quantclaw::gateway {

class GatewayServer {
public:
  explicit GatewayServer(Config  cfg, int port = 18800, std::string  host = "127.0.0.1");


  void Run();
  void Stop();

private:
  Config _cfg;
  int _port;
  std::string _host;

  // std::atomic 保证多线程下对 _running 的读写是原子的，
  // 并阻止有害的跨线程指令重排序。
  std::atomic<bool> _running{false};
  std::unique_ptr<tools::ToolRegistry> _tools;
  std::unique_ptr<security::ToolPermissionChecker> _permission_checker;
  std::unique_ptr<security::ExecApprovalManager> _approval_manager;
  std::unique_ptr<providers::LLMProvider> _provider;

  void HandleChat(const std::shared_ptr<ix::WebSocket>& websocket,
    int request_id,
    const nlohmann::json& params);

  nlohmann::json ExecuteToolLoop(const std::vector<providers::Message>& messages);

};

}
