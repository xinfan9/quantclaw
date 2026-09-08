#pragma once

#include <string>

#include "../config.h"

namespace quantclaw::web {

class WebServer {
 public:
  explicit WebServer(const Config& cfg, int port = 18801,
                     const std::string& host = "127.0.0.1");

  void Run();
  void Stop();

 private:
  Config _cfg;
  int _port;
  std::string _host;
  bool _running = false;
};

}  // namespace quantclaw::web
