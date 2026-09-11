#include "cron_scheduler.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

namespace quantclaw::core {

nlohmann::json CronJob::ToJson() const {
  auto to_iso = [](std::chrono::system_clock::time_point tp) -> std::string {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
  };

  return {
      {"id", id},
      {"name", name},
      {"schedule", schedule},
      {"message", message},
      {"sessionKey", session_key},
      {"enabled", enabled},
      {"lastRun", to_iso(last_run)},
      {"nextRun", to_iso(next_run)},
  };
}

CronJob CronJob::FromJson(const nlohmann::json& j) {
  auto from_iso = [](const std::string& s) -> std::chrono::system_clock::time_point {
    std::tm tm{};
    std::istringstream iss(s);
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (iss.fail()) return std::chrono::system_clock::now();
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
  };

  CronJob job;
  job.id = j.value("id", "");
  job.name = j.value("name", "");
  job.schedule = j.value("schedule", "");
  job.message = j.value("message", "");
  job.session_key = j.value("sessionKey", "default");
  job.enabled = j.value("enabled", true);
  job.last_run = from_iso(j.value("lastRun", ""));
  job.next_run = from_iso(j.value("nextRun", ""));
  return job;
}

CronExpression::Field CronExpression::ParseField(const std::string& field,
                                                  int min, int max) {
  Field result;
  if (field == "*") return result;  // 空表示通配符

  std::stringstream ss(field);
  std::string part;
  while (std::getline(ss, part, ',')) {
    if (part.find('/') != std::string::npos) {
      auto slash = part.find('/');
      std::string base = part.substr(0, slash);
      int step = std::stoi(part.substr(slash + 1));
      int start = min;
      int end = max;
      if (base != "*") {
        auto dash = base.find('-');
        if (dash != std::string::npos) {
          start = std::stoi(base.substr(0, dash));
          end = std::stoi(base.substr(dash + 1));
        } else {
          start = std::stoi(base);
        }
      }
      for (int i = start; i <= end; i += step) result.values.push_back(i);
    } else if (part.find('-') != std::string::npos) {
      auto dash = part.find('-');
      int start = std::stoi(part.substr(0, dash));
      int end = std::stoi(part.substr(dash + 1));
      for (int i = start; i <= end; ++i) result.values.push_back(i);
    } else {
      result.values.push_back(std::stoi(part));
    }
  }

  std::sort(result.values.begin(), result.values.end());
  result.values.erase(std::unique(result.values.begin(), result.values.end()),
                      result.values.end());
  return result;
}

bool CronExpression::FieldMatches(const Field& f, int value) {
  if (f.values.empty()) return true;
  return std::binary_search(f.values.begin(), f.values.end(), value);
}

CronExpression::CronExpression(const std::string& expr) {
  std::stringstream ss(expr);
  std::vector<std::string> parts;
  std::string part;
  while (ss >> part) parts.push_back(part);
  if (parts.size() != 5) {
    throw std::runtime_error("Invalid cron expression: " + expr);
  }
  minute_ = ParseField(parts[0], 0, 59);
  hour_ = ParseField(parts[1], 0, 23);
  day_of_month_ = ParseField(parts[2], 1, 31);
  month_ = ParseField(parts[3], 1, 12);
  day_of_week_ = ParseField(parts[4], 0, 6);
}

bool CronExpression::Matches(const std::tm& tm) const {
  if (!FieldMatches(minute_, tm.tm_min)) return false;
  if (!FieldMatches(hour_, tm.tm_hour)) return false;
  if (!FieldMatches(day_of_month_, tm.tm_mday)) return false;
  if (!FieldMatches(month_, tm.tm_mon + 1)) return false;
  if (!FieldMatches(day_of_week_, tm.tm_wday)) return false;
  return true;
}

std::chrono::system_clock::time_point CronExpression::NextAfter(
    std::chrono::system_clock::time_point after) const {
  // 简单暴力搜索：从 after 开始按分钟递增，找到第一个匹配时间
  auto t = std::chrono::system_clock::to_time_t(after);
  std::tm tm{};
  localtime_r(&t, &tm);
  tm.tm_sec = 0;
  auto candidate = std::chrono::system_clock::from_time_t(std::mktime(&tm));
  candidate += std::chrono::minutes(1);

  for (int i = 0; i < 366 * 24 * 60; ++i) {
    auto ct = std::chrono::system_clock::to_time_t(candidate);
    std::tm ctm{};
    localtime_r(&ct, &ctm);
    if (Matches(ctm)) return candidate;
    candidate += std::chrono::minutes(1);
  }
  return after;
}

CronScheduler::CronScheduler(std::shared_ptr<spdlog::logger> logger)
    : logger_(logger ? logger : spdlog::default_logger()) {}

CronScheduler::~CronScheduler() { Stop(); }

void CronScheduler::Load(const std::string& filepath) {
  std::lock_guard<std::mutex> lock(mu_);
  storage_path_ = filepath;
  if (!std::filesystem::exists(filepath)) return;

  std::ifstream file(filepath);
  if (!file) return;
  try {
    nlohmann::json json;
    file >> json;
    if (!json.is_array()) return;
    jobs_.clear();
    for (const auto& item : json) {
      jobs_.push_back(CronJob::FromJson(item));
    }
  } catch (const std::exception& e) {
    logger_->error("Failed to load cron jobs: {}", e.what());
  }
}

void CronScheduler::Save(const std::string& filepath) const {
  std::lock_guard<std::mutex> lock(mu_);
  nlohmann::json json = nlohmann::json::array();
  for (const auto& job : jobs_) json.push_back(job.ToJson());

  std::filesystem::create_directories(
      std::filesystem::path(filepath).parent_path());
  std::ofstream file(filepath);
  if (file) file << json.dump(2);
}

std::string CronScheduler::GenerateId() const {
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, 15);
  std::ostringstream oss;
  for (int i = 0; i < 12; ++i) oss << std::hex << dist(gen);
  return oss.str();
}

std::string CronScheduler::AddJob(const std::string& name,
                                  const std::string& schedule,
                                  const std::string& message,
                                  const std::string& session_key) {
  CronExpression expr(schedule);
  std::lock_guard<std::mutex> lock(mu_);
  CronJob job;
  job.id = GenerateId();
  job.name = name;
  job.schedule = schedule;
  job.message = message;
  job.session_key = session_key;
  job.next_run = expr.NextAfter(std::chrono::system_clock::now());
  jobs_.push_back(std::move(job));
  return job.id;
}

bool CronScheduler::RemoveJob(const std::string& id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = std::remove_if(jobs_.begin(), jobs_.end(),
                           [&id](const CronJob& j) { return j.id == id; });
  if (it == jobs_.end()) return false;
  jobs_.erase(it, jobs_.end());
  return true;
}

std::vector<CronJob> CronScheduler::ListJobs() const {
  std::lock_guard<std::mutex> lock(mu_);
  return jobs_;
}

void CronScheduler::Start(JobHandler handler) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (running_) return;
    handler_ = std::move(handler);
    running_ = true;
  }
  thread_ = std::thread(&CronScheduler::SchedulerLoop, this);
}

void CronScheduler::Stop() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!running_) return;
    running_ = false;
  }
  if (thread_.joinable()) thread_.join();
}

void CronScheduler::SchedulerLoop() {
  while (running_) {
    auto now = std::chrono::system_clock::now();

    std::vector<CronJob> to_run;
    {
      std::lock_guard<std::mutex> lock(mu_);
      for (auto& job : jobs_) {
        if (!job.enabled) continue;
        if (now >= job.next_run) {
          to_run.push_back(job);
          job.last_run = now;
          try {
            CronExpression expr(job.schedule);
            job.next_run = expr.NextAfter(now);
          } catch (...) {
            job.enabled = false;
          }
        }
      }
    }

    for (const auto& job : to_run) {
      if (handler_) {
        try {
          handler_(job);
        } catch (const std::exception& e) {
          logger_->error("Cron job {} handler failed: {}", job.name, e.what());
        }
      }
    }

    if (!storage_path_.empty()) Save(storage_path_);

    std::this_thread::sleep_for(std::chrono::seconds(30));
  }
}

}  // namespace quantclaw::core
