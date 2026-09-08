#pragma once


#include<string>
#include <ixwebsocket/IXWebSocket.h>
#include <mutex>

namespace quantclaw::gateway {

class GatewayClient {
public:
  explicit GatewayClient(std::string  url = "ws://127.0.0.1:18800");
  ~GatewayClient();

  void Connect(int timeout_seconds = 5);

  void Disconnect();

  std::string Chat(const std::string& message, int timeout_seconds = 60);

private:
  std::string _url;
  ix::WebSocket _websocket;
  std::mutex _mutex;

  std::condition_variable _cv;
  bool _response_ready = {false};
  std::string _last_response;
  int _request_id = 0;

  void OnMessage(const ix::WebSocketMessagePtr& msg);
};

}