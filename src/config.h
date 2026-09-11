#pragma once
#include <cstdlib>   // std::getenv、std::atoi
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace quantclaw {

// Provider 子账号配置：用于在同一 Provider 下区分不同 API Key
struct ProviderProfileConfig {
  std::string id;
  std::string api_key;
};

// Provider 配置：包含该 Provider 的 API Key、Base URL 以及可选的子账号列表
struct ProviderConfig {
  std::string api_key;
  std::string base_url;
  std::vector<ProviderProfileConfig> profiles;
};

// Embedding 模型配置：用于向量检索与记忆模块
struct EmbeddingConfig {
  std::string api_key;
  std::string model;
  std::string base_url;
};

// 工具权限配置：通过 allow/deny 列表控制工具白名单与黑名单
struct ToolPermissionConfig {
  std::vector<std::string> allow;
  std::vector<std::string> deny;
};

// 外部命令执行审批配置
// mode: off（不审批）| on_miss（不在 allowlist 中时审批）| always（总是审批）
struct ExecApprovalConfig {
  std::string mode = "on_miss"; // 审批模式：off | on_miss | always
  std::vector<std::string> allowlist;
  int timeout_seconds = 120;
  
};

// 全局配置：从 ~/.quantclaw/config.json 加载，并支持环境变量覆盖
struct Config {
  std::string api_key;    // 主 LLM API Key
  std::string model;      // 默认模型名称
  std::string base_url;   // 主 LLM Base URL

  std::unordered_map<std::string, ProviderConfig> providers; // 各 Provider 配置
  std::unordered_map<std::string, std::string> aliases;      // 模型别名映射

  std::vector<std::string> fallback_chain; // 模型失败后的降级链

  EmbeddingConfig embedding; // Embedding 配置

  ToolPermissionConfig tool_permissions; // 工具权限配置

  ExecApprovalConfig exec_approval; // 外部命令执行审批配置

  // Gateway 配置
  std::string gateway_auth_token; // WebSocket 网关认证 token（空表示不校验）
  int gateway_port = 0;           // 网关端口，0 表示使用默认值

  int context_window = 0; // 上下文窗口大小
  int max_tokens = 0;     // 单次请求最大生成 token 数

  static Config Load();

  // 从指定 JSON 文件加载配置，加载后仍会应用环境变量覆盖
  static Config LoadFromFile(const std::string& path);

  // 默认配置文件路径：~/.quantclaw/config.json
  static std::string DefaultPath();
  // 将路径中的 ~/ 展开为当前用户 HOME 目录
  static std::string ExpandHome(const std::string& path);

private:
  // 使用环境变量覆盖配置文件中的对应字段
  static void ApplyEnvOverrides(Config& cfg);
  // 将以逗号分隔的环境变量字符串拆分为字符串列表
  static std::vector<std::string> SplitEnvList(const char* value);
};

// 将 ~/path 展开为绝对路径；非 ~/ 开头的路径原样返回
inline std::string Config::ExpandHome(const std::string& path) {
  if (path.size() >= 2 && path.substr(0, 2) == "~/") {
    const char* home = std::getenv("HOME");

    if (home) {
      return (std::filesystem::path(home)/ path.substr(2)).string();
    }
  }

  return  path;
}

inline std::string Config::DefaultPath() {
  return ExpandHome("~/.quantclaw/config.json"); // 默认配置文件
}

// 按逗号拆分环境变量值，并去除首尾空白字符
inline std::vector<std::string> Config::SplitEnvList(const char* value) {
  std::vector<std::string> result;
  if (!value) return result;
  std::string s(value);
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    size_t start = item.find_first_not_of("\t");
    if (start == std::string::npos) continue;
    size_t end = item.find_last_not_of(" \t");
    result.push_back(item.substr(start, end - start + 1));
  }
  return result;
}


// 加载默认路径配置；若文件不存在则返回仅含环境变量的默认配置
inline Config Config::Load() {
  std::string path = DefaultPath();
  if (!std::filesystem::exists(path)) {
    Config cfg;
    ApplyEnvOverrides(cfg);
    return cfg;
  }

  return LoadFromFile(path);
}

// 从 JSON 文件解析配置，解析异常时静默回退到默认配置，最后应用环境变量覆盖
inline Config Config::LoadFromFile(const std::string& path) {
  Config cfg;
  std::ifstream file(path);
  if (!file.is_open()) {
    ApplyEnvOverrides(cfg);
    return cfg;
  }

  try {
    nlohmann::json json;
    file >> json;

    // 解析 agent 节点：通用模型参数
    if (json.contains("agent") && json["agent"].is_object()) {
      const auto& agent = json["agent"];
      if (agent.contains("models") && agent["model"].is_string())
        cfg.model = agent["model"].get<std::string>();

      if (agent.contains("contextWindow") && agent["contextWindow"].is_number_integer()) {
        cfg.context_window = agent["contextWindow"].get<int>();
      }
      if (agent.contains("maxTokens") && agent["maxTokens"].is_number_integer()) {
        cfg.max_tokens = agent["maxTokens"].get<int>();
      }
    }


    // 解析 llm 节点：主 LLM 配置
    if (json.contains("llm") && json["llm"].is_object()) {
      const auto& llm = json["llm"];
      if (llm.contains("apiKey") && llm["apiKey"].is_string()) {
        cfg.api_key = llm["apiKey"].get<std::string>();
      }
      if (llm.contains("baseUrl") && llm["baseUrl"].is_string()) {
        cfg.base_url = llm["baseUrl"].get<std::string>();
      }
      if (llm.contains("model") && llm["model"].is_string()) {
        cfg.model = llm["model"].get<std::string>();
      }

      if (llm.contains("contextWindow") && llm["contextWindow"].is_number_integer()) {
        cfg.context_window = llm["contextWindow"].get<int>();
      }
      if (llm.contains("maxTokens") && llm["maxTokens"].is_number_integer()) {
        cfg.max_tokens = llm["maxTokens"].get<int>();
      }
    }

    // 解析 providers 节点：各 Provider 独立配置
    if (json.contains("providers") && json["providers"].is_object()) {
      for (const auto& [key, value] : json["providers"].items()) {
        ProviderConfig pc;
        if (value.contains("apiKey") && value["apiKey"].is_string()) {
          pc.api_key = value["apiKey"].get<std::string>();
        }
        if (value.contains("baseUrl") && value["baseUrl"].is_string()) {
          pc.base_url = value["baseUrl"].get<std::string>();
        }

        if (value.contains("profiles") && value["profiles"].is_array()) {
          for (const auto& p : value["profiles"].items()) {
            ProviderProfileConfig profile;
            if (p.value().contains("id") && p.value()["id"].is_string()) {
              profile.id = p.value()["id"].get<std::string>();
            }
            if (p.value().contains("apiKey") && p.value()["apiKey"].is_string()) {
              profile.api_key = p.value()["apiKey"].get<std::string>();
            }
            pc.profiles.push_back(std::move(profile));
          }
        }
        cfg.providers[key] = std::move(pc);
      }
    }

    // 解析 failover 节点：模型失败后的降级链
    if (json.contains("failover") && json["failover"].is_object()) {
      const auto& fo = json["failover"];
      if (fo.contains("fallbackChain") && fo["fallbackChain"].is_array()) {
        for (const auto& item : fo["fallbackChain"]) {
          if (item.is_string()) cfg.fallback_chain.push_back(item.get<std::string>());
        }
      }
    }

    // 解析 embedding 节点：Embedding 服务配置
    if (json.contains("embedding") && json["embedding"].is_object()) {
      const auto& emb = json["embedding"];
      if (emb.contains("apiKey") && emb["apiKey"].is_string()) {
        cfg.embedding.api_key = emb["apiKey"].get<std::string>();
      }
      if (emb.contains("model") && emb["model"].is_string()) {
        cfg.embedding.model = emb["model"].get<std::string>();
      }
      if (emb.contains("baseUrl") && emb["baseUrl"].is_string()) {
        cfg.embedding.base_url = emb["baseUrl"].get<std::string>();
      }
    }

    // 解析 toolPermissions 节点：工具白名单/黑名单
    if (json.contains("toolPermissions") && json["toolPermissions"].is_object()) {
      const auto& tp = json["toolPermissions"];
      if (tp.contains("allow") && tp["allow"].is_array()) {
        for (const auto& item : tp["allow"]) {
          if (item.is_string()) cfg.tool_permissions.allow.push_back(item.get<std::string>());
        }
      }
      if (tp.contains("deny") && tp["deny"].is_array()) {
        for (const auto& item : tp["deny"]) {
          if (item.is_string()) cfg.tool_permissions.deny.push_back(item.get<std::string>());
        }
      }
    }

    // 解析 execApproval 节点：外部命令执行审批策略
    if (json.contains("execApproval") && json["execApproval"].is_object()) {
      const auto& ea = json["execApproval"];
      if (ea.contains("mode") && ea["mode"].is_string()) {
        cfg.exec_approval.mode = ea["mode"].get<std::string>();
      }
      if (ea.contains("allowlist") && ea["allowlist"].is_array()) {
        for (const auto& item : ea["allowlist"]) {
          if (item.is_string()) cfg.exec_approval.allowlist.push_back(item.get<std::string>());
        }
      }
      if (ea.contains("timeoutSeconds") && ea["timeoutSeconds"].is_number_integer()) {
        cfg.exec_approval.timeout_seconds = ea["timeoutSeconds"].get<int>();
      }
    }

    // 解析 gateway 节点
    if (json.contains("gateway") && json["gateway"].is_object()) {
      const auto& gw = json["gateway"];
      if (gw.contains("authToken") && gw["authToken"].is_string()) {
        cfg.gateway_auth_token = gw["authToken"].get<std::string>();
      }
      if (gw.contains("port") && gw["port"].is_number_integer()) {
        cfg.gateway_port = gw["port"].get<int>();
      }
    }


  } catch (const std::exception& e) {}

  ApplyEnvOverrides(cfg);
  return cfg;
}

// 常用环境变量设置示例：
// export CLAW_API_KEY="sk-..."
// export CLAW_MODEL="claude-3-5-sonnet-20240620"
// export CLAW_BASE_URL="https://api.anthropic.com/v1"
// export CLAW_CONTEXT_WINDOW="200000"
// export CLAW_MAX_TOKENS="4096"
// export CLAW_FALLBACK_MODELS="model-a,model-b,model-c"
// export CLAW_EMBEDDING_API_KEY="sk-..."
// export CLAW_EMBEDDING_MODEL="text-embedding-3-small"
// export CLAW_EMBEDDING_BASE_URL="https://api.openai.com/v1"
// export CLAW_TOOL_ALLOW="calculator,read_file"
// export CLAW_TOOL_DENY="execute_command"
// export CLAW_EXEC_APPROVAL_MODE="on_miss"

// 使用 CLAW_* 系列环境变量覆盖配置值
inline void Config::ApplyEnvOverrides(Config& cfg) {
  const char* api_key = std::getenv("CLAW_API_KEY");
  if (api_key && !std::string(api_key).empty()) cfg.api_key = api_key;
  const char* model = std::getenv("CLAW_MODEL");
  if (model && !std::string(model).empty()) cfg.model = model;
  const char* base_url = std::getenv("CLAW_BASE_URL");
  if (base_url && !std::string(base_url).empty()) cfg.base_url = base_url;

  const char* context_window = std::getenv("CLAW_CONTEXT_WINDOW");
  if (context_window && !std::string(context_window).empty()) {
    cfg.context_window = std::stoi(context_window);
  }

  const char* max_tokens = std::getenv("CLAW_MAX_TOKENS");
  if (max_tokens && !std::string(max_tokens).empty()) {
    cfg.max_tokens = std::stoi(max_tokens);
  }

  const char* fallback_models = std::getenv("CLAW_FALLBACK_MODELS");
  if (fallback_models && !std::string(fallback_models).empty()) {
    cfg.fallback_chain = SplitEnvList(fallback_models);
  }

  const char* embedding_api_key = std::getenv("CLAW_EMBEDDING_API_KEY");
  if (embedding_api_key && !std::string(embedding_api_key).empty()) {
    cfg.embedding.api_key = embedding_api_key;
  }

  const char* embedding_model = std::getenv("CLAW_EMBEDDING_MODEL");
  if (embedding_model && !std::string(embedding_model).empty()) {
    cfg.embedding.model = embedding_model;
  }

  const char* embedding_base_url = std::getenv("CLAW_EMBEDDING_BASE_URL");
  if (embedding_base_url && !std::string(embedding_base_url).empty()) {
    cfg.embedding.base_url = embedding_base_url;
  }

  const char* tool_allow = std::getenv("CLAW_TOOL_ALLOW");
  if (tool_allow && !std::string(tool_allow).empty()) {
    cfg.tool_permissions.allow = SplitEnvList(tool_allow);
  }

  const char* tool_deny = std::getenv("CLAW_TOOL_DENY");
  if (tool_deny && !std::string(tool_deny).empty()) {
    cfg.tool_permissions.deny = SplitEnvList(tool_deny);
  }

  const char* exec_mode = std::getenv("CLAW_EXEC_APPROVAL_MODE");
  if (exec_mode && !std::string(exec_mode).empty()) {
    cfg.exec_approval.mode = exec_mode;
  }

  const char* gateway_token = std::getenv("CLAW_GATEWAY_AUTH_TOKEN");
  if (gateway_token && !std::string(gateway_token).empty()) {
    cfg.gateway_auth_token = gateway_token;
  }

  const char* gateway_port = std::getenv("CLAW_GATEWAY_PORT");
  if (gateway_port && !std::string(gateway_port).empty()) {
    cfg.gateway_port = std::stoi(gateway_port);
  }
}

}