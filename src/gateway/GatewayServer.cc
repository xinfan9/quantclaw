#include "GatewayServer.h"

#include <random>
#include <sstream>

#include "core/MemoryEngine.h"
#include "core/cron_scheduler.hpp"
#include "platform.h"
#include "providers/ProviderRegistry.h"
#include "session/ChatHistory.h"
#include "spdlog/spdlog.h"
#include "tools/CalculatorTool.h"

namespace quantclaw::gateway {

GatewayServer::GatewayServer(Config cfg, int port, std::string host)
    : cfg_(std::move(cfg)), port_(port), host_(std::move(host)) {
  tools_ = std::make_unique<tools::ToolRegistry>();
  tools_->Register(std::make_unique<tools::CalculatorTool>());

  // 工具级权限
  permission_checker_ = std::make_unique<security::ToolPermissionChecker>(
      cfg_.tool_permissions.allow, cfg_.tool_permissions.deny);

  // 执行级审批
  security::ExecApprovalConfig exec_cfg;
  exec_cfg.mode = security::ParseAskMode(cfg_.exec_approval.mode);
  exec_cfg.timeout_seconds = cfg_.exec_approval.timeout_seconds;
  exec_cfg.allowlist = cfg_.exec_approval.allowlist;
  approval_manager_ =
      std::make_unique<security::ExecApprovalManager>(std::move(exec_cfg));

  providers::ProviderRegistry registry;
  for (const auto& [id, pc] : cfg_.providers)
    registry.AddProvider({id, pc.api_key, pc.base_url});
  for (const auto& [alias, target] : cfg_.aliases)
    registry.AddAlias(alias, target);
  provider_ = registry.CreateProvider(cfg_);

  auth_token_ = cfg_.gateway_auth_token;

  // 初始化命令队列
  QueueConfig qcfg;
  auto executor = [this](const QueuedCommand& cmd,
                         std::function<void(const std::string& event,
                                            const nlohmann::json& payload)>
                             event_sink) {
    return ExecuteAgent(cmd, std::move(event_sink));
  };
  auto response_sender =
      [this](const std::string& connection_id,
             const std::string& rpc_request_id, bool ok,
             const nlohmann::json& payload_or_error) {
        RpcResponse res;
        res.id = rpc_request_id;
        res.ok = ok;
        if (ok)
          res.payload = payload_or_error;
        else
          res.error = {"INTERNAL_ERROR", payload_or_error.value("error", "")};
        SendResponse(connection_id, res);
      };
  auto event_sender = [this](const std::string& connection_id,
                             const std::string& event_name,
                             const nlohmann::json& payload) {
    SendEvent(connection_id, {event_name, payload});
  };
  queue_ = std::make_unique<CommandQueue>(qcfg, executor, response_sender,
                                          event_sender);

  // 初始化定时任务调度器
  cron_scheduler_ = std::make_unique<core::CronScheduler>();
  std::filesystem::path cron_path =
      std::filesystem::path(quantclaw::platform::home_directory()) /
      ".quantclaw" / "cron_jobs.json";
  cron_scheduler_->Load(cron_path.string());
}

GatewayServer::~GatewayServer() {
  if (queue_) queue_->Stop();
  Stop();
}

std::string GatewayServer::GenerateConnectionId() {
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, 15);
  std::ostringstream oss;
  oss << "conn-";
  for (int i = 0; i < 16; ++i) oss << std::hex << dist(gen);
  return oss.str();
}

void GatewayServer::SendResponse(const std::string& connection_id,
                                 const RpcResponse& response) {
  std::lock_guard<std::mutex> lock(connections_mu_);
  auto it = connections_.find(connection_id);
  if (it == connections_.end()) return;
  auto ws = it->second.lock();
  if (!ws) return;
  ws->send(response.ToJson().dump());
}

void GatewayServer::SendEvent(const std::string& connection_id,
                              const RpcEvent& event) {
  std::lock_guard<std::mutex> lock(connections_mu_);
  auto it = connections_.find(connection_id);
  if (it == connections_.end()) return;
  auto ws = it->second.lock();
  if (!ws) return;
  ws->send(event.ToJson().dump());
}

void GatewayServer::BroadcastEvent(const RpcEvent& event) {
  std::lock_guard<std::mutex> lock(connections_mu_);
  for (auto it = connections_.begin(); it != connections_.end();) {
    auto ws = it->second.lock();
    if (!ws) {
      it = connections_.erase(it);
      continue;
    }
    ws->send(event.ToJson().dump());
    ++it;
  }
}

void GatewayServer::Run() {
  // 启动 cron 调度器，触发时向命令队列提交 agent.request
  cron_scheduler_->Start([this](const core::CronJob& job) {
    queue_->Submit(job.session_key, job.message,
                   {{"source", "cron"}, {"jobName", job.name}}, "",
                   "cron-" + job.id, QueueMode::kCollect);
    spdlog::info("Cron job triggered: {} -> {}", job.name, job.session_key);
  });

  server_ = std::make_unique<ix::WebSocketServer>(port_, host_);

  server_->setOnConnectionCallback(
      [this](std::weak_ptr<ix::WebSocket> webSocket,
             std::shared_ptr<ix::ConnectionState> /*connectionState*/) {
        auto ws = webSocket.lock();
        if (!ws) return;

        std::string conn_id = GenerateConnectionId();
        {
          std::lock_guard<std::mutex> lock(connections_mu_);
          connections_[conn_id] = ws;
        }

        ws->setOnMessageCallback(
            [this, conn_id, ws](const ix::WebSocketMessagePtr& msg) {
              if (msg->type == ix::WebSocketMessageType::Message) {
                OnMessage(ws, msg->str);
              }
            });
      });

  queue_->Start();

  auto res = server_->listen();
  if (!res.first) {
    throw std::runtime_error("Failed to start gateway server: " + res.second);
  }

  spdlog::info("Gateway server listening on {}:{}", host_, port_);
  running_ = true;
  server_->start();

  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  server_->stop();
  queue_->Stop();
}

void GatewayServer::Stop() {
  running_ = false;
  if (queue_) queue_->Stop();
}

void GatewayServer::OnMessage(const std::shared_ptr<ix::WebSocket>& ws,
                              const std::string& text) {
  RpcRequest req;
  try {
    req = RpcRequest::FromJson(nlohmann::json::parse(text));
  } catch (const std::exception& e) {
    RpcResponse res;
    res.id = "";
    res.ok = false;
    res.error = {"PARSE_ERROR", e.what()};
    ws->send(res.ToJson().dump());
    return;
  }

  std::string conn_id;
  {
    std::lock_guard<std::mutex> lock(connections_mu_);
    for (const auto& [id, weak_ws] : connections_) {
      if (weak_ws.lock() == ws) {
        conn_id = id;
        break;
      }
    }
  }
  if (conn_id.empty()) {
    ws->send(RpcResponse::Failure(req.id, "Connection not registered")
                 .ToJson()
                 .dump());
    return;
  }

  if (req.method == methods::kConnectHello)
    HandleHello(conn_id, req);
  else if (req.method == methods::kGatewayHealth)
    HandleHealth(conn_id, req);
  else if (req.method == methods::kGatewayStatus)
    HandleStatus(conn_id, req);
  else if (req.method == methods::kAgentRequest)
    HandleAgentRequest(conn_id, req);
  else if (req.method == methods::kAgentStop)
    HandleAgentStop(conn_id, req);
  else if (req.method == methods::kQueueStatus)
    HandleQueueStatus(conn_id, req);
  else if (req.method == methods::kQueueCancel)
    HandleQueueCancel(conn_id, req);
  else if (req.method == methods::kQueueAbort)
    HandleQueueAbort(conn_id, req);
  else if (req.method == methods::kCronList)
    HandleCronList(conn_id, req);
  else if (req.method == methods::kCronAdd)
    HandleCronAdd(conn_id, req);
  else if (req.method == methods::kCronRemove)
    HandleCronRemove(conn_id, req);
  else if (req.method == methods::kCronRun)
    HandleCronRun(conn_id, req);
  else
    ws->send(RpcResponse::Failure(req.id, "Method not found", "METHOD_NOT_FOUND")
                 .ToJson()
                 .dump());
}

void GatewayServer::HandleHello(const std::string& conn_id,
                                const RpcRequest& req) {
  auto params = ConnectHelloParams::FromJson(req.params);

  bool authenticated = true;
  if (!auth_token_.empty() && params.auth_token != auth_token_) {
    authenticated = false;
  }

  {
    std::lock_guard<std::mutex> lock(clients_mu_);
    ClientConnection conn;
    conn.connection_id = conn_id;
    conn.role = params.role;
    conn.scopes = params.scopes;
    conn.client_name = params.client_name;
    conn.client_version = params.client_version;
    conn.authenticated = authenticated;
    clients_[conn_id] = std::move(conn);
  }

  if (!authenticated) {
    SendResponse(conn_id, RpcResponse::Failure(req.id, "Authentication failed",
                                               "AUTH_FAILED"));
    return;
  }

  HelloOkPayload payload;
  payload.conn_id = conn_id;
  SendResponse(conn_id, RpcResponse::Success(req.id, payload.ToJson()));
}

void GatewayServer::HandleHealth(const std::string& conn_id,
                                 const RpcRequest& req) {
  size_t conn_count = 0;
  {
    std::lock_guard<std::mutex> lock(connections_mu_);
    conn_count = connections_.size();
  }
  nlohmann::json payload = {{"status", "ok"}, {"connections", conn_count}};
  SendResponse(conn_id, RpcResponse::Success(req.id, payload));
}

void GatewayServer::HandleStatus(const std::string& conn_id,
                                 const RpcRequest& req) {
  nlohmann::json payload;
  payload["running"] = running_.load();
  payload["host"] = host_;
  payload["port"] = port_;
  payload["queue"] = queue_->GlobalStatus();
  SendResponse(conn_id, RpcResponse::Success(req.id, payload));
}

void GatewayServer::HandleAgentRequest(const std::string& conn_id,
                                       const RpcRequest& req) {
  {
    std::lock_guard<std::mutex> lock(clients_mu_);
    auto it = clients_.find(conn_id);
    if (it == clients_.end() || !it->second.authenticated) {
      SendResponse(conn_id, RpcResponse::Failure(req.id, "Not authenticated",
                                                 "AUTH_REQUIRED"));
      return;
    }
  }

  std::string message = req.params.value("message", "");
  std::string session_key = req.params.value("sessionKey", "default");
  std::string mode_str = req.params.value("mode", "collect");
  QueueMode mode = QueueModeFromString(mode_str);

  auto id = queue_->Submit(session_key, message, req.params, conn_id, req.id,
                           mode);
  SendResponse(conn_id,
               RpcResponse::Success(req.id, {{"commandId", id}}));
}

void GatewayServer::HandleAgentStop(const std::string& conn_id,
                                    const RpcRequest& req) {
  std::string session_key = req.params.value("sessionKey", "default");
  bool aborted = queue_->AbortSession(session_key);
  SendResponse(conn_id, RpcResponse::Success(
                            req.id, {{"aborted", aborted}}));
}

void GatewayServer::HandleQueueStatus(const std::string& conn_id,
                                      const RpcRequest& req) {
  std::string session_key = req.params.value("sessionKey", "");
  nlohmann::json payload;
  payload["global"] = queue_->GlobalStatus();
  if (!session_key.empty()) {
    payload["session"] = queue_->SessionQueueStatus(session_key);
  }
  SendResponse(conn_id, RpcResponse::Success(req.id, payload));
}

void GatewayServer::HandleQueueCancel(const std::string& conn_id,
                                      const RpcRequest& req) {
  std::string command_id = req.params.value("commandId", "");
  bool cancelled = queue_->Cancel(command_id);
  SendResponse(conn_id, RpcResponse::Success(
                            req.id, {{"cancelled", cancelled}}));
}

void GatewayServer::HandleQueueAbort(const std::string& conn_id,
                                     const RpcRequest& req) {
  std::string session_key = req.params.value("sessionKey", "default");
  bool aborted = queue_->AbortSession(session_key);
  SendResponse(conn_id, RpcResponse::Success(
                            req.id, {{"aborted", aborted}}));
}

void GatewayServer::HandleCronList(const std::string& conn_id,
                                   const RpcRequest& req) {
  auto jobs = cron_scheduler_->ListJobs();
  nlohmann::json payload = nlohmann::json::array();
  for (const auto& job : jobs) payload.push_back(job.ToJson());
  SendResponse(conn_id, RpcResponse::Success(req.id, payload));
}

void GatewayServer::HandleCronAdd(const std::string& conn_id,
                                  const RpcRequest& req) {
  std::string name = req.params.value("name", "");
  std::string schedule = req.params.value("schedule", "");
  std::string message = req.params.value("message", "");
  std::string session_key = req.params.value("sessionKey", "default");

  if (name.empty() || schedule.empty() || message.empty()) {
    SendResponse(conn_id, RpcResponse::Failure(req.id, "Missing parameters"));
    return;
  }

  try {
    auto id = cron_scheduler_->AddJob(name, schedule, message, session_key);
    SendResponse(conn_id,
                 RpcResponse::Success(req.id, {{"jobId", id}}));
  } catch (const std::exception& e) {
    SendResponse(conn_id,
                 RpcResponse::Failure(req.id, e.what(), "INVALID_CRON"));
  }
}

void GatewayServer::HandleCronRemove(const std::string& conn_id,
                                     const RpcRequest& req) {
  std::string id = req.params.value("jobId", "");
  bool removed = cron_scheduler_->RemoveJob(id);
  SendResponse(conn_id,
               RpcResponse::Success(req.id, {{"removed", removed}}));
}

void GatewayServer::HandleCronRun(const std::string& conn_id,
                                  const RpcRequest& req) {
  std::string id = req.params.value("jobId", "");
  auto jobs = cron_scheduler_->ListJobs();
  auto it = std::find_if(jobs.begin(), jobs.end(),
                         [&id](const core::CronJob& j) { return j.id == id; });
  if (it == jobs.end()) {
    SendResponse(conn_id, RpcResponse::Failure(req.id, "Job not found"));
    return;
  }

  queue_->Submit(it->session_key, it->message,
                 {{"source", "cron"}, {"jobName", it->name}}, conn_id,
                 req.id, QueueMode::kCollect);
  SendResponse(conn_id, RpcResponse::Success(req.id, {{"triggered", true}}));
}

nlohmann::json GatewayServer::ExecuteAgent(
    const QueuedCommand& cmd,
    std::function<void(const std::string& event, const nlohmann::json& payload)>
        event_sink) {
  return ExecuteToolLoop(cmd.message, cmd.session_key, std::move(event_sink));
}

nlohmann::json GatewayServer::ExecuteToolLoop(
    const std::string& user_message,
    const std::string& session_key,
    std::function<void(const std::string&, const nlohmann::json&)> event_sink) {
  if (!provider_) {
    return {{"reply", "No provider configured"}};
  }

  // 加载历史，按 session 隔离存储
  std::filesystem::path history_dir =
      std::filesystem::path(quantclaw::platform::home_directory()) /
      ".quantclaw" / "sessions" / session_key;
  std::filesystem::create_directories(history_dir);
  session::ChatHistory history((history_dir / "history.json").string());
  auto messages = history.Load();
  constexpr size_t kMaxHistory = 20;
  if (messages.size() > kMaxHistory)
    messages = std::vector(messages.begin(), messages.begin() + kMaxHistory);

  // 记忆检索
  core::MemoryEngine memory(messages);
  auto relevant = memory.Search(user_message, 3);
  std::string memory_context = core::MemoryEngine::FormatContext(relevant);

  if (messages.empty()) {
    std::string system_prompt =
        "You are a helpful assistant. Use tools when they can help answer "
        "the user's question.";
    if (!memory_context.empty()) system_prompt += "\n\n" + memory_context;
    messages.push_back({"system", system_prompt, "", {}});
  }

  messages.push_back({"user", user_message, "", {}});

  spdlog::info("[gateway] User: {}", user_message);

  std::string final_reply;
  for (int iteration = 0; iteration < 5; ++iteration) {
    auto response = provider_->Chat(messages, *tools_);

    if (event_sink) {
      nlohmann::json delta = {{"delta", response.content}};
      event_sink(events::kTextDelta, delta);
    }

    if (!response.isToolCall()) {
      final_reply = response.content;
      messages.push_back({"assistant", response.content, "", {}});
      break;
    }

    providers::Message assistant_msg;
    assistant_msg.role = "assistant";
    assistant_msg.content = response.content;
    assistant_msg.tool_calls = response.tool_calls;
    messages.push_back(assistant_msg);

    for (const auto& tc : response.tool_calls) {
      if (event_sink) {
        event_sink(events::kToolUse, {{"name", tc.name}, {"arguments", tc.arguments}});
      }

      if (!permission_checker_->IsAllowed(tc.name)) {
        spdlog::info("[gateway] Tool {} denied", tc.name);
        messages.push_back({"tool", "tool permission denied", tc.id, {}});
        if (event_sink)
          event_sink(events::kToolResult,
                     {{"tool_call_id", tc.id}, {"result", "denied"}});
        continue;
      }

      std::string command_summary = tc.name + " " + tc.arguments.dump();
      if (!approval_manager_->RequestApproval(command_summary)) {
        spdlog::info("[gateway] Tool {} approval denied", tc.name);
        messages.push_back({"tool", "execution approval denied", tc.id, {}});
        if (event_sink)
          event_sink(events::kToolResult,
                     {{"tool_call_id", tc.id}, {"result", "approval_denied"}});
        continue;
      }

      std::string result = tools_->Execute(tc.name, tc.arguments);
      spdlog::info("[gateway] Tool {} result: {}", tc.name, result);
      messages.push_back({"tool", result, tc.id, {}});
      if (event_sink)
        event_sink(events::kToolResult,
                   {{"tool_call_id", tc.id}, {"result", result}});
    }
  }

  if (event_sink)
    event_sink(events::kMessageEnd,
               {{"reply", final_reply}, {"sessionKey", session_key}});

  history.Save(messages);
  spdlog::info("[gateway] Final reply: {}", final_reply);
  return {{"reply", final_reply}};
}

}  // namespace quantclaw::gateway
