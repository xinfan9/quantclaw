#include "GatewayClient.h"

#include <chrono>
#include <spdlog/spdlog.h>

#include "protocol.hpp"

namespace quantclaw::gateway {

GatewayClient::GatewayClient(std::string url, std::string auth_token)
    : url_(std::move(url)), auth_token_(std::move(auth_token)) {
  websocket_.setUrl(url_);
  websocket_.setOnMessageCallback(
      [this](const ix::WebSocketMessagePtr& msg) { OnMessage(msg); });
}

GatewayClient::~GatewayClient() { Disconnect(); }

void GatewayClient::Connect(int timeout_seconds) {
  websocket_.start();

  auto start = std::chrono::steady_clock::now();
  while (websocket_.getReadyState() != ix::ReadyState::Open) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed > std::chrono::seconds(timeout_seconds)) {
      throw std::runtime_error("连接网关超时");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // 发送 connect.hello 握手
  nlohmann::json params = {{"clientName", "my_claw"},
                           {"clientVersion", "0.3.0"},
                           {"role", "operator"},
                           {"authToken", auth_token_}};
  auto hello = Request(methods::kConnectHello, params, timeout_seconds);
  if (!hello.value("ok", false)) {
    throw std::runtime_error("网关握手失败: " + hello.dump());
  }

  spdlog::info("已连接到网关 {}", url_);
}

void GatewayClient::Disconnect() { websocket_.stop(); }

nlohmann::json GatewayClient::Request(const std::string& method,
                                      const nlohmann::json& params,
                                      int timeout_seconds) {
  if (websocket_.getReadyState() != ix::ReadyState::Open) {
    throw std::runtime_error("WebSocket 未连接");
  }

  ++request_id_;
  RpcRequest req;
  req.id = std::to_string(request_id_);
  req.method = method;
  req.params = params;

  {
    std::lock_guard lock(mutex_);
    response_ready_ = false;
    last_response_.clear();
  }

  websocket_.send(req.ToJson().dump());

  std::unique_lock lock(mutex_);
  bool received = cv_.wait_for(lock, std::chrono::seconds(timeout_seconds),
                               [this] { return response_ready_; });

  if (!received) throw std::runtime_error("等待网关响应超时");
  return last_response_;
}

std::string GatewayClient::Chat(const std::string& message,
                                int timeout_seconds,
                                const std::string& session_key) {
  accumulated_reply_.clear();

  nlohmann::json params = {{"message", message},
                           {"sessionKey", session_key},
                           {"mode", "collect"}};
  auto res = Request(methods::kAgentRequest, params, timeout_seconds);

  if (!res.value("ok", false)) {
    throw std::runtime_error("请求失败: " + res.dump());
  }

  // 等待 message_end 事件（通过事件流聚合）
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start <
         std::chrono::seconds(timeout_seconds)) {
    {
      std::lock_guard lock(mutex_);
      if (!last_response_.empty() && last_response_.contains("event") &&
          last_response_["event"] == events::kMessageEnd) {
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  return accumulated_reply_.empty() ? res.dump() : accumulated_reply_;
}

void GatewayClient::OnMessage(const ix::WebSocketMessagePtr& msg) {
  if (msg->type != ix::WebSocketMessageType::Message) return;

  try {
    auto j = nlohmann::json::parse(msg->str);
    std::string type = j.value("type", "");

    std::lock_guard lock(mutex_);
    if (type == "res") {
      last_response_ = j;
      response_ready_ = true;
      cv_.notify_one();
    } else if (type == "event") {
      std::string event_name = j.value("event", "");
      auto payload = j.value("payload", nlohmann::json::object());
      if (event_name == events::kTextDelta) {
        accumulated_reply_ += payload.value("delta", "");
      } else if (event_name == events::kMessageEnd) {
        if (payload.contains("reply")) {
          accumulated_reply_ = payload.value("reply", accumulated_reply_);
        }
        last_response_ = j;
        response_ready_ = true;
        cv_.notify_one();
      }
    }
  } catch (const std::exception& e) {
    spdlog::error("解析网关消息失败: {}", e.what());
  }
}

}  // namespace quantclaw::gateway
