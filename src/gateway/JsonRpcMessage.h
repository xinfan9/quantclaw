#pragma once

#include <string>
#include <nlohmann/json.hpp>
namespace quantclaw::gateway {

struct JsonRpcMessage {
  std::string jsonrpc = "2.0";
  int id = 0;
  std::string method;

  nlohmann::json params;
  nlohmann::json result;
  nlohmann::json error;

  bool IsRequest() const { return !method.empty(); }
  bool IsResponse() const { return method.empty(); }
  bool HasError() const { return !error.is_null(); }

  nlohmann::json ToJson() const;
  static JsonRpcMessage FromJson(const nlohmann::json& json);
  static JsonRpcMessage FromString(const std::string& str);
};

}