#include "command_queue.hpp"

#include <random>
#include <sstream>

namespace quantclaw::gateway {

QueueMode QueueModeFromString(const std::string& s) {
  if (s == "collect") return QueueMode::kCollect;
  if (s == "followup") return QueueMode::kFollowup;
  if (s == "steer") return QueueMode::kSteer;
  if (s == "steer_backlog") return QueueMode::kSteerBacklog;
  if (s == "interrupt") return QueueMode::kInterrupt;
  return QueueMode::kCollect;
}

std::string QueueModeToString(QueueMode mode) {
  switch (mode) {
    case QueueMode::kCollect:
      return "collect";
    case QueueMode::kFollowup:
      return "followup";
    case QueueMode::kSteer:
      return "steer";
    case QueueMode::kSteerBacklog:
      return "steer_backlog";
    case QueueMode::kInterrupt:
      return "interrupt";
  }
  return "collect";
}

DropPolicy DropPolicyFromString(const std::string& s) {
  if (s == "drop_oldest") return DropPolicy::kDropOldest;
  if (s == "reject") return DropPolicy::kReject;
  return DropPolicy::kDropOldest;
}

std::string DropPolicyToString(DropPolicy policy) {
  switch (policy) {
    case DropPolicy::kDropOldest:
      return "drop_oldest";
    case DropPolicy::kReject:
      return "reject";
  }
  return "drop_oldest";
}

QueueConfig QueueConfig::FromJson(const nlohmann::json& json) {
  QueueConfig cfg;
  if (json.contains("maxConcurrent"))
    cfg.max_concurrent = json["maxConcurrent"].get<int>();
  if (json.contains("debounceMs"))
    cfg.debounce_ms = json["debounceMs"].get<int>();
  if (json.contains("cap")) cfg.cap = json["cap"].get<int>();
  if (json.contains("drop"))
    cfg.drop = DropPolicyFromString(json["drop"].get<std::string>());
  if (json.contains("defaultMode"))
    cfg.default_mode = QueueModeFromString(json["defaultMode"].get<std::string>());
  return cfg;
}

nlohmann::json QueueConfig::ToJson() const {
  return {
      {"maxConcurrent", max_concurrent},
      {"debounceMs", debounce_ms},
      {"cap", cap},
      {"drop", DropPolicyToString(drop)},
      {"defaultMode", QueueModeToString(default_mode)},
  };
}

SessionLane::SessionLane(const std::string& session_key)
    : session_key_(session_key) {}

void SessionLane::Enqueue(QueuedCommand cmd) {
  pending_.push_back(std::move(cmd));
}

bool SessionLane::HasPending() const { return !pending_.empty(); }

std::optional<QueuedCommand> SessionLane::TryActivate(
    std::chrono::steady_clock::time_point now) {
  if (HasActive() || pending_.empty()) return std::nullopt;

  auto& front = pending_.front();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - front.enqueued_at)
                     .count();
  if (elapsed < debounce_ms_) return std::nullopt;

  QueuedCommand cmd = std::move(front);
  pending_.pop_front();
  cmd.state = QueuedCommand::State::kActive;
  active_command_ = std::move(cmd);
  return active_command_;
}

std::optional<QueuedCommand> SessionLane::CompleteActive() {
  if (!active_command_) return std::nullopt;
  QueuedCommand cmd = std::move(*active_command_);
  cmd.state = QueuedCommand::State::kComplete;
  active_command_.reset();
  return cmd;
}

std::vector<std::string> SessionLane::ApplyCapOverflow() {
  std::vector<std::string> dropped;
  if (static_cast<int>(pending_.size()) <= cap_) return dropped;

  if (drop_ == DropPolicy::kReject) {
    // 保留最早的，丢弃最新入队的
    while (static_cast<int>(pending_.size()) > cap_) {
      dropped.push_back(pending_.back().id);
      pending_.back().state = QueuedCommand::State::kDropped;
      pending_.pop_back();
    }
  } else {
    // 默认 drop_oldest：丢弃最早的
    while (static_cast<int>(pending_.size()) > cap_) {
      dropped.push_back(pending_.front().id);
      pending_.front().state = QueuedCommand::State::kDropped;
      pending_.pop_front();
    }
  }
  return dropped;
}

bool SessionLane::CancelPending(const std::string& command_id) {
  for (auto it = pending_.begin(); it != pending_.end(); ++it) {
    if (it->id == command_id) {
      it->state = QueuedCommand::State::kDropped;
      pending_.erase(it);
      return true;
    }
  }
  return false;
}

std::optional<std::string> SessionLane::InterruptActive() {
  pending_.clear();
  if (!active_command_) return std::nullopt;
  std::string id = active_command_->id;
  active_command_->state = QueuedCommand::State::kDropped;
  active_command_.reset();
  return id;
}

nlohmann::json SessionLane::ToJson() const {
  nlohmann::json j;
  j["sessionKey"] = session_key_;
  j["mode"] = QueueModeToString(mode_);
  j["pendingCount"] = pending_.size();
  j["hasActive"] = HasActive();
  return j;
}

CommandQueue::CommandQueue(const QueueConfig& config, AgentExecutor executor,
                           ResponseSender response_sender,
                           EventSender event_sender,
                           std::shared_ptr<spdlog::logger> logger)
    : config_(config),
      executor_(std::move(executor)),
      response_sender_(std::move(response_sender)),
      event_sender_(std::move(event_sender)),
      logger_(logger ? logger : spdlog::default_logger()) {}

CommandQueue::~CommandQueue() { Stop(); }

std::string CommandQueue::GenerateId() const {
  static thread_local std::mt19937 gen(
      std::random_device{}() +
      static_cast<unsigned>(
          std::hash<std::thread::id>{}(std::this_thread::get_id())));
  std::uniform_int_distribution<int> dist(0, 15);
  std::ostringstream oss;
  for (int i = 0; i < 16; ++i) oss << std::hex << dist(gen);
  return oss.str();
}

SessionLane& CommandQueue::GetLane(const std::string& session_key) {
  auto it = lanes_.find(session_key);
  if (it == lanes_.end()) {
    auto lane = std::make_unique<SessionLane>(session_key);
    lane->SetMode(config_.default_mode);
    lane->SetDebounceMs(config_.debounce_ms);
    lane->SetCap(config_.cap);
    lane->SetDropPolicy(config_.drop);
    it = lanes_.emplace(session_key, std::move(lane)).first;
  }
  return *it->second;
}

void CommandQueue::Start() {
  std::lock_guard<std::mutex> lock(mu_);
  if (running_) return;
  running_ = true;
  dispatcher_ = std::thread(&CommandQueue::DispatcherLoop, this);
}

void CommandQueue::Stop() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!running_) return;
    running_ = false;
  }
  cv_.notify_all();

  if (dispatcher_.joinable()) dispatcher_.join();

  // 等待所有 worker 结束
  std::vector<std::thread> workers;
  {
    std::lock_guard<std::mutex> lock(mu_);
    workers = std::move(workers_);
  }
  for (auto& t : workers) {
    if (t.joinable()) t.join();
  }
}

std::string CommandQueue::Submit(const std::string& session_key,
                                 const std::string& message,
                                 const nlohmann::json& params,
                                 const std::string& connection_id,
                                 const std::string& rpc_request_id,
                                 QueueMode mode) {
  std::lock_guard<std::mutex> lock(mu_);

  auto& lane = GetLane(session_key);

  // 中断模式：清空当前 session 队列
  if (mode == QueueMode::kInterrupt) {
    auto interrupted = lane.InterruptActive();
    if (interrupted) {
      QueuedCommand stub;
      stub.id = *interrupted;
      stub.session_key = session_key;
      EmitQueueEvent(stub, "queue.interrupted");
    }
  }

  QueuedCommand cmd;
  cmd.id = GenerateId();
  cmd.session_key = session_key;
  cmd.message = message;
  cmd.params = params;
  cmd.connection_id = connection_id;
  cmd.rpc_request_id = rpc_request_id;
  cmd.mode = mode;
  cmd.enqueued_at = std::chrono::steady_clock::now();
  cmd.state = QueuedCommand::State::kPending;

  command_to_session_[cmd.id] = session_key;
  lane.Enqueue(std::move(cmd));

  auto dropped = lane.ApplyCapOverflow();
  for (const auto& id : dropped) {
    QueuedCommand stub;
    stub.id = id;
    stub.session_key = session_key;
    EmitQueueEvent(stub, "queue.dropped", {{"reason", "cap_overflow"}});
  }

  cv_.notify_one();
  return cmd.id;
}

bool CommandQueue::Cancel(const std::string& command_id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = command_to_session_.find(command_id);
  if (it == command_to_session_.end()) return false;
  auto& lane = GetLane(it->second);
  return lane.CancelPending(command_id);
}

bool CommandQueue::AbortSession(const std::string& session_key) {
  std::lock_guard<std::mutex> lock(mu_);
  auto& lane = GetLane(session_key);
  auto id = lane.InterruptActive();
  if (id) {
    QueuedCommand stub;
    stub.id = *id;
    stub.session_key = session_key;
    EmitQueueEvent(stub, "queue.aborted");
    return true;
  }
  return false;
}

void CommandQueue::ConfigureSession(const std::string& session_key,
                                    QueueMode mode, int debounce_ms, int cap,
                                    const std::string& drop) {
  std::lock_guard<std::mutex> lock(mu_);
  auto& lane = GetLane(session_key);
  lane.SetMode(mode);
  if (debounce_ms >= 0) lane.SetDebounceMs(debounce_ms);
  if (cap >= 0) lane.SetCap(cap);
  if (!drop.empty()) lane.SetDropPolicy(DropPolicyFromString(drop));
}

nlohmann::json CommandQueue::SessionQueueStatus(
    const std::string& session_key) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = lanes_.find(session_key);
  if (it == lanes_.end()) return { {"sessionKey", session_key}, {"pendingCount", 0}, {"hasActive", false} };
  return it->second->ToJson();
}

nlohmann::json CommandQueue::GlobalStatus() const {
  std::lock_guard<std::mutex> lock(mu_);
  nlohmann::json j;
  j["config"] = config_.ToJson();
  j["activeCount"] = active_count_.load();
  j["sessionCount"] = lanes_.size();
  nlohmann::json sessions = nlohmann::json::array();
  for (const auto& [key, lane] : lanes_) {
    sessions.push_back(lane->ToJson());
  }
  j["sessions"] = sessions;
  return j;
}

void CommandQueue::SetConfig(const QueueConfig& config) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = config;
}

void CommandQueue::DispatcherLoop() {
  while (true) {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait_for(lock, std::chrono::milliseconds(100),
                 [this] { return !running_ || HasWork(); });

    if (!running_) break;

    if (active_count_ >= config_.max_concurrent) continue;

    auto now = std::chrono::steady_clock::now();
    std::optional<QueuedCommand> cmd_to_run;
    for (auto& [key, lane] : lanes_) {
      auto cmd = lane->TryActivate(now);
      if (cmd) {
        cmd_to_run = std::move(cmd);
        break;
      }
    }

    if (!cmd_to_run) continue;

    active_count_++;
    EmitQueueEvent(*cmd_to_run, "queue.started");

    // 释放锁后启动 worker
    auto cmd_copy = *cmd_to_run;
    lock.unlock();

    workers_.emplace_back([this, cmd = std::move(cmd_copy)]() mutable {
      ExecuteCommand(std::move(cmd));
      {
        std::lock_guard<std::mutex> l(mu_);
        active_count_--;
      }
      cv_.notify_one();
    });
  }
}

bool CommandQueue::HasWork() {
  if (active_count_ >= config_.max_concurrent) return false;
  auto now = std::chrono::steady_clock::now();
  for (auto& [key, lane] : lanes_) {
    if (lane->HasPending() && !lane->HasActive()) {
      auto& front = lane->PeekPendingFront();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - front.enqueued_at)
                         .count();
      if (elapsed >= lane->GetDebounceMs()) return true;
    }
  }
  return false;
}

void CommandQueue::ExecuteCommand(QueuedCommand cmd) {
  nlohmann::json result;
  bool ok = true;
  try {
    result = executor_(cmd, [this, &cmd](const std::string& event,
                                         const nlohmann::json& payload) {
      event_sender_(cmd.connection_id, event, payload);
    });
  } catch (const std::exception& e) {
    ok = false;
    result = { {"error", e.what()} };
    logger_->error("Command execution failed: {}", e.what());
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& lane = GetLane(cmd.session_key);
    lane.CompleteActive();
  }

  EmitQueueEvent(cmd, "queue.completed",
                 { {"ok", ok}, {"sessionKey", cmd.session_key} });
  response_sender_(cmd.connection_id, cmd.rpc_request_id, ok, result);
}

void CommandQueue::EmitQueueEvent(const QueuedCommand& cmd,
                                  const std::string& event_type,
                                  const nlohmann::json& data) {
  nlohmann::json payload = {
      {"commandId", cmd.id},
      {"sessionKey", cmd.session_key},
      {"event", event_type},
  };
  if (!data.is_null()) payload["data"] = data;
  event_sender_(cmd.connection_id, event_type, payload);
}

}  // namespace quantclaw::gateway
