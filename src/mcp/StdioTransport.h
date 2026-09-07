#pragma once
#include <string>
#include <vector>
namespace quantclaw::mcp {

class StdioTransport {
public:
  explicit StdioTransport(const std::string& command, const std::vector<std::string>& args = {});
  ~StdioTransport();

  // 禁止拷贝
  StdioTransport(const StdioTransport&) = delete;
  StdioTransport& operator=(const StdioTransport&) = delete;

  void Send(const std::string& json_line);
  std::string Receive();

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;


};

}