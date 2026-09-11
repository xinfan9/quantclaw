#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace quantclaw::gateway {

// RPC 帧类型
enum class FrameType { kRequest, kResponse, kEvent };

inline std::string FrameTypeToString(FrameType type) {
  switch (type) {
    case FrameType::kRequest:
      return "req";
    case FrameType::kResponse:
      return "res";
    case FrameType::kEvent:
      return "event";
  }
  return "unknown";
}

inline FrameType FrameTypeFromString(const std::string& str) {
  if (str == "req") return FrameType::kRequest;
  if (str == "res") return FrameType::kResponse;
  if (str == "event") return FrameType::kEvent;
  throw std::runtime_error("Unknown frame type: " + str);
}

// RPC 请求
struct RpcRequest {
  std::string id;
  std::string method;
  nlohmann::json params;

  nlohmann::json ToJson() const {
    return {{"type", "req"}, {"id", id}, {"method", method}, {"params", params}};
  }

  static RpcRequest FromJson(const nlohmann::json& j) {
    RpcRequest req;
    req.id = j.at("id").get<std::string>();
    req.method = j.at("method").get<std::string>();
    auto it = j.find("params");
    req.params =
        (it != j.end() && !it->is_null()) ? *it : nlohmann::json::object();
    return req;
  }
};

// RPC 错误
struct RpcError {
  std::string code = "INTERNAL_ERROR";
  std::string message;
  bool retryable = false;

  nlohmann::json ToJson() const {
    return {{"code", code}, {"message", message}, {"retryable", retryable}};
  }
};

// RPC 响应
struct RpcResponse {
  std::string id;
  bool ok = true;
  nlohmann::json payload;
  RpcError error;

  nlohmann::json ToJson() const {
    nlohmann::json j = {{"type", "res"}, {"id", id}, {"ok", ok}};
    if (ok) {
      j["payload"] = payload;
    } else {
      j["error"] = error.ToJson();
    }
    return j;
  }

  static RpcResponse Success(const std::string& id,
                             const nlohmann::json& payload) {
    return {id, true, payload, {}};
  }

  static RpcResponse Failure(const std::string& id, const std::string& message,
                             const std::string& code = "INTERNAL_ERROR",
                             bool retryable = false) {
    return {id, false, {}, {code, message, retryable}};
  }
};

// RPC 事件
struct RpcEvent {
  std::string event;
  nlohmann::json payload;

  nlohmann::json ToJson() const {
    return {{"type", "event"}, {"event", event}, {"payload", payload}};
  }
};

// 握手参数
struct ConnectHelloParams {
  std::string client_name;
  std::string client_version;
  std::string role = "operator";
  std::vector<std::string> scopes = {"operator.read", "operator.write"};
  std::string auth_token;

  static ConnectHelloParams FromJson(const nlohmann::json& j) {
    ConnectHelloParams p;
    p.client_name = j.value("clientName", "");
    p.client_version = j.value("clientVersion", "");
    p.role = j.value("role", "operator");
    if (j.contains("scopes") && j["scopes"].is_array()) {
      p.scopes.clear();
      for (const auto& s : j["scopes"]) {
        if (s.is_string()) p.scopes.push_back(s.get<std::string>());
      }
    }
    p.auth_token = j.value("authToken", "");
    return p;
  }
};

// 握手成功响应
struct HelloOkPayload {
  int protocol = 1;
  bool authenticated = true;
  int tick_interval_ms = 15000;
  std::string server_version = "0.3.0";
  std::string conn_id;

  nlohmann::json ToJson() const {
    nlohmann::json methods = nlohmann::json::array(
        {"connect.hello", "gateway.health", "gateway.status", "agent.request",
         "agent.stop", "queue.status", "queue.cancel", "queue.abort",
         "cron.list", "cron.add", "cron.remove", "cron.run"});
    nlohmann::json events = nlohmann::json::array(
        {"agent.text_delta", "agent.tool_use", "agent.tool_result",
         "agent.message_end", "gateway.tick", "queue.started",
         "queue.completed", "queue.dropped"});
    nlohmann::json features;
    features["methods"] = methods;
    features["events"] = events;

    nlohmann::json j;
    j["protocol"] = protocol;
    j["authenticated"] = authenticated;
    j["tickIntervalMs"] = tick_interval_ms;
    j["server"] = {{"version", server_version}, {"connId", conn_id}};
    j["features"] = features;
    return j;
  }
};

// 客户端连接信息
struct ClientConnection {
  std::string connection_id;
  std::string role;
  std::vector<std::string> scopes;
  std::string client_name;
  std::string client_version;
  bool authenticated = false;
};

// RPC 方法名
namespace methods {
constexpr const char* kConnectHello = "connect.hello";
constexpr const char* kGatewayHealth = "gateway.health";
constexpr const char* kGatewayStatus = "gateway.status";
constexpr const char* kAgentRequest = "agent.request";
constexpr const char* kAgentStop = "agent.stop";
constexpr const char* kQueueStatus = "queue.status";
constexpr const char* kQueueCancel = "queue.cancel";
constexpr const char* kQueueAbort = "queue.abort";
constexpr const char* kCronList = "cron.list";
constexpr const char* kCronAdd = "cron.add";
constexpr const char* kCronRemove = "cron.remove";
constexpr const char* kCronRun = "cron.run";
}  // namespace methods

// 事件名
namespace events {
constexpr const char* kTextDelta = "agent.text_delta";
constexpr const char* kToolUse = "agent.tool_use";
constexpr const char* kToolResult = "agent.tool_result";
constexpr const char* kMessageEnd = "agent.message_end";
constexpr const char* kTick = "gateway.tick";
constexpr const char* kQueueStarted = "queue.started";
constexpr const char* kQueueCompleted = "queue.completed";
constexpr const char* kQueueDropped = "queue.dropped";
}  // namespace events

inline FrameType ParseFrameType(const nlohmann::json& j) {
  return FrameTypeFromString(j.at("type").get<std::string>());
}

}  // namespace quantclaw::gateway
