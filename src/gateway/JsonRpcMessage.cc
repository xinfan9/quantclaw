#include "JsonRpcMessage.h"

namespace quantclaw::gateway {

nlohmann::json JsonRpcMessage::ToJson() const {
  nlohmann::json json;
  json["jsonrpc"] = jsonrpc;

  if (IsRequest()) {
    json["id"] = id;
    json["method"] = method;
    json["params"] = params;
  } else {
    json["id"] = id;
    if (HasError()) json["error"] = error;
    else  json["result"] = result;
  }

  return json;
}

JsonRpcMessage JsonRpcMessage::FromJson(const nlohmann::json& json) {
  JsonRpcMessage msg;

  msg.jsonrpc = json.value("jsonrpc", "2.0");
  msg.id = json.value("id", 0);

  if (json.contains("method") && json["method"].is_string()) {
    msg.method = json["method"].get<std::string>();
  }

  if (json.contains("params")) {
    msg.params = json["params"];
  }

  if (json.contains("result")) {
    msg.result = json["result"];
  }

  if (json.contains("error")) {
    msg.error = json["error"];
  }

  return msg;
}

JsonRpcMessage JsonRpcMessage::FromString(const std::string& str) {
  auto json = nlohmann::json::parse(str, nullptr, false);
  if (json.is_discarded()) {
    throw std::runtime_error("Invalid JSON-RPC message: " + str);
  }
  return FromJson(json);
}

}