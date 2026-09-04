# QuantClaw

QuantClaw 是一个模块化的 C++ AI 助手框架，支持多 LLM 提供商、插件扩展、MCP 工具、网关服务、会话管理和安全沙箱。

## 功能特性

- **多 LLM 提供商支持**：OpenAI、OpenAI Codex、GitHub Copilot、Anthropic 等。
- **统一 Provider 接口**：通过抽象基类 `LLMProvider` 实现不同提供商的即插即用。
- **CLI 交互**：命令行入口，支持对话、配置管理、模型授权等命令。
- **会话管理**：多轮对话、会话维护、上下文压缩与记忆搜索。
- **工具系统**：工具注册、工具链编排、浏览器工具、MCP 工具管理。
- **网关服务**：RPC 处理、守护进程管理、命令队列。
- **插件系统**：插件清单、注册表、Sidecar、Hook 机制。
- **安全沙箱**：工具权限、执行审批、RBAC、速率限制。
- **Web 服务**：HTTP API 路由（基于 cpp-httplib）。
- **跨平台**：支持 Unix/Linux/macOS 和 Windows。

## 依赖

- C++17 编译器（GCC、Clang 或 MSVC）
- CMake 3.20+
- 必需依赖：
  - spdlog
  - libcurl
  - OpenSSL
  - nlohmann/json（FetchContent 自动获取）
  - IXWebSocket（优先 vcpkg，否则 FetchContent）
  - cpp-httplib（优先 vcpkg，否则 FetchContent）
- 测试依赖：GoogleTest（优先 vcpkg/系统，否则 FetchContent）

## 构建

```bash
cmake -B build
cmake --build build
```

### 构建选项

```bash
# 不构建测试
cmake .. -DBUILD_TESTS=OFF

# 启用 AddressSanitizer
cmake .. -DENABLE_ASAN=ON

# 启用 ThreadSanitizer（不能与 ASAN 同时启用）
cmake .. -DENABLE_TSAN=ON

# 启用 UndefinedBehaviorSanitizer
cmake .. -DENABLE_UBSAN=ON
```

## 运行

```bash
#export OPENAI_API_KEY=sk-...
./build/quantclaw 你好
```

## 测试

```bash
cd build
ctest --output-on-failure
```

或直接运行测试二进制：

```bash
./quantclaw_tests
```

## 项目结构

```text
include/quantclaw/          # 公共头文件
src/
  core/                     # 核心：配置、Agent 循环、会话压缩、内存管理等
  auth/                     # 认证：Provider 认证、OpenAI Codex、GitHub Copilot
  providers/                # LLM 提供商实现
  cli/                      # 命令行接口
  gateway/                  # 网关服务、RPC、守护进程
  session/                  # 会话管理
  tools/                    # 工具系统
  mcp/                      # MCP 客户端/服务器/工具管理
  channels/                 # 通道适配器与策略
  plugins/                  # 插件系统
  security/                 # 安全沙箱、权限、RBAC、速率限制
  web/                      # Web 服务与 API 路由
  platform/                 # 跨平台抽象（进程、服务、IPC）
tests/                      # 单元测试与集成测试
```

## 主要模块

| 模块 | 说明 |
|------|------|
| `core` | Agent 循环、配置加载、内存管理、Prompt 构建、上下文修剪 |
| `providers` | OpenAI、Anthropic、GitHub Copilot 等 LLM 接入与错误处理 |
| `cli` | 命令解析与交互命令实现 |
| `gateway` | 本地网关服务、RPC 处理器、守护进程 |
| `session` | 会话生命周期维护 |
| `tools` | 工具注册、链式调用、浏览器自动化 |
| `mcp` | Model Context Protocol 支持 |
| `security` | 沙箱、执行审批、RBAC、速率限制 |
| `web` | HTTP 服务与 REST API |

## 开发

### Sanitizers

项目支持三种 Sanitizer，用于调试内存、线程和未定义行为问题：

- `ENABLE_ASAN`：AddressSanitizer + LeakSanitizer
- `ENABLE_TSAN`：ThreadSanitizer
- `ENABLE_UBSAN`：UndefinedBehaviorSanitizer

ASAN 与 TSAN 不能同时启用。

### 代码规范

- 使用 C++17。
- 目标编译选项开启 `-Wall -Wextra`。
- 优先使用 RAII 管理资源。
- 虚析构函数必须声明为 `virtual`。
