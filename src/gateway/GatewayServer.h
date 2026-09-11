#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>

#include "command_queue.hpp"
#include "config.h"
#include "core/cron_scheduler.hpp"
#include "protocol.hpp"
#include "providers/LLMProvider.h"
#include "security/ExecApprovalManager.h"
#include "security/ToolPermissionChecker.h"
#include "tools/ToolRegistry.h"

namespace quantclaw::gateway {

// WebSocket JSON-RPC 网关服务器
class GatewayServer {
 public:
  explicit GatewayServer(Config cfg, int port = 18800,
                         std::string host = "127.0.0.1");
  ~GatewayServer();

  void Run();
  void Stop();

 private:
  Config cfg_;
  int port_;
  std::string host_;

  std::atomic<bool> running_{false};
  std::unique_ptr<ix::WebSocketServer> server_;

  std::unique_ptr<tools::ToolRegistry> tools_;
  std::unique_ptr<security::ToolPermissionChecker> permission_checker_;
  std::unique_ptr<security::ExecApprovalManager> approval_manager_;
  std::unique_ptr<providers::LLMProvider> provider_;

  std::unique_ptr<CommandQueue> queue_;
  std::unique_ptr<core::CronScheduler> cron_scheduler_;

  // connection_id 到 WebSocket 的映射
  std::unordered_map<std::string, std::weak_ptr<ix::WebSocket>> connections_;
  std::mutex connections_mu_;

  // connection_id -> 客户端信息
  std::unordered_map<std::string, ClientConnection> clients_;
  std::mutex clients_mu_;

  std::string auth_token_;

  // 生成连接 ID
  std::string GenerateConnectionId();

  // 发送响应/事件
  void SendResponse(const std::string& connection_id,
                    const RpcResponse& response);
  void SendEvent(const std::string& connection_id, const RpcEvent& event);
  void BroadcastEvent(const RpcEvent& event);

  // RPC 方法处理
  void OnMessage(const std::shared_ptr<ix::WebSocket>& ws,
                 const std::string& text);
  void HandleHello(const std::string& conn_id, const RpcRequest& req);
  void HandleHealth(const std::string& conn_id, const RpcRequest& req);
  void HandleStatus(const std::string& conn_id, const RpcRequest& req);
  void HandleAgentRequest(const std::string& conn_id, const RpcRequest& req);
  void HandleAgentStop(const std::string& conn_id, const RpcRequest& req);
  void HandleQueueStatus(const std::string& conn_id, const RpcRequest& req);
  void HandleQueueCancel(const std::string& conn_id, const RpcRequest& req);
  void HandleQueueAbort(const std::string& conn_id, const RpcRequest& req);
  void HandleCronList(const std::string& conn_id, const RpcRequest& req);
  void HandleCronAdd(const std::string& conn_id, const RpcRequest& req);
  void HandleCronRemove(const std::string& conn_id, const RpcRequest& req);
  void HandleCronRun(const std::string& conn_id, const RpcRequest& req);

  // Agent 执行器，被 CommandQueue 调用
  nlohmann::json ExecuteAgent(
      const QueuedCommand& cmd,
      std::function<void(const std::string& event,
                         const nlohmann::json& payload)>
          event_sink);

  // 工具调用循环
  nlohmann::json ExecuteToolLoop(
      const std::string& user_message,
      const std::string& session_key,
      std::function<void(const std::string&, const nlohmann::json&)> event_sink);
};

}  // namespace quantclaw::gateway
