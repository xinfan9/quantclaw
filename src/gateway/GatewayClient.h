#pragma once

#include <condition_variable>
#include <functional>
#include <string>

#include <ixwebsocket/IXWebSocket.h>
#include <mutex>

#include <nlohmann/json.hpp>

namespace quantclaw::gateway {

// WebSocket JSON-RPC 网关客户端
class GatewayClient {
 public:
  explicit GatewayClient(std::string url = "ws://127.0.0.1:18800",
                         std::string auth_token = "");
  ~GatewayClient();

  // 连接并握手
  void Connect(int timeout_seconds = 5);

  // 断开连接
  void Disconnect();

  // 发送聊天请求并等待最终回复
  std::string Chat(const std::string& message, int timeout_seconds = 60,
                   const std::string& session_key = "default");

  // 发送原始 RPC 请求并等待响应
  nlohmann::json Request(const std::string& method,
                         const nlohmann::json& params,
                         int timeout_seconds = 10);

 private:
  std::string url_;
  std::string auth_token_;
  ix::WebSocket websocket_;
  std::mutex mutex_;
  std::condition_variable cv_;

  bool response_ready_ = false;
  nlohmann::json last_response_;
  std::string accumulated_reply_;
  int request_id_ = 0;

  void OnMessage(const ix::WebSocketMessagePtr& msg);
};

}  // namespace quantclaw::gateway
