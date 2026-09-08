//
// Created by xinfang on 2026/9/8.
//

#include "GatewayClient.h"

#include <utility>
#include <spdlog/spdlog.h>

#include "JsonRpcMessage.h"

namespace quantclaw::gateway {

GatewayClient::GatewayClient(std::string  url) : _url(std::move(url)) {
  _websocket.setUrl(_url);
  _websocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
    OnMessage(msg);
  });
}

GatewayClient::~GatewayClient() {
  Disconnect();
}

void GatewayClient::Connect(int timeout_seconds) {
  _websocket.start();

  auto start = std::chrono::steady_clock::now();
  while (_websocket.getReadyState() != ix::ReadyState::Open) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed > std::chrono::seconds(timeout_seconds)) {
      throw std::runtime_error("Timeout: Unable to connect to the gateway within the specified time.");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  spdlog::info("Connected to the gateway {}.", _url);
}

void GatewayClient::Disconnect() {
  _websocket.stop();
}

std::string GatewayClient::Chat(const std::string& message, int timeout_seconds) {
  if (_websocket.getReadyState() != ix::ReadyState::Open) {
    throw std::runtime_error("WebSocket is not open.");
  }

  ++_request_id;

  JsonRpcMessage request;
  request.method = "chat";
  request.params = {{"message", message}};
  request.id = _request_id;

  // 加锁保护共享状态。
  // 将 _response_ready 置为 false，表示响应尚未到达。
  // 清空 _last_response，为接收新响应做准备
  {
    std::lock_guard lock(_mutex);
    _response_ready = false;
    _last_response.clear();
  }

  _websocket.send(request.ToJson().dump());

  // 再次加锁，准备用条件变量等待响应。代码在这里开始进入阻塞等待，直到服务端返回消息后被唤醒。
  // wait 内部在等待时会自动释放锁，被唤醒时重新加锁。
  // 这种“先解锁、再阻塞、再恢复加锁”的能力，std::lock_guard 不具备。
  std::unique_lock lock(_mutex);
  bool received = _cv.wait_for(lock, std::chrono::seconds(timeout_seconds), [this] {return _response_ready;});

  if (!received) throw std::runtime_error("Timeout: Unable to receive response within the specified time.");

  return _last_response;
}

void GatewayClient::OnMessage(const ix::WebSocketMessagePtr& msg) {
  if (msg->type != ix::WebSocketMessageType::Message) return;

  try {
    auto response = JsonRpcMessage::FromString(msg->str);

    std::lock_guard lock(_mutex);
    if (response.HasError()) {
      _last_response = "Error: " + response.error.value("message", "");
    } else if (response.result.contains("reply")) {
      _last_response = response.result["reply"].get<std::string>();
    } else {
      _last_response = response.result.dump();
    }

    _response_ready = true;
    _cv.notify_one();
  } catch (const std::exception& e) {
    spdlog::error("Error parsing JSON response: {}", e.what());
  }
}


}
