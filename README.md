# QuantClaw

QuantClaw 是一个模块化的 C++ AI 助手程序，支持命令行对话、Web UI、WebSocket 网关、多 LLM 提供商、本地工具、MCP 外部工具和 Node.js Sidecar 扩展。

## 功能特性

- **多 LLM 提供商**：支持 OpenAI、Anthropic，通过 `ProviderRegistry` 按模型前缀自动分发。
- **CLI 对话**：命令行输入，自动保存多轮对话历史。
- **工具系统**：内置 `calculator` 工具；支持 MCP 工具和 Node.js Sidecar 工具动态注册。
- **MCP 支持**：通过 stdio transport 启动外部 MCP 服务器，将其工具接入对话。
- **网关模式**：`--gateway` 启动 WebSocket JSON-RPC 网关；客户端可通过 `QUANTCLAW_USE_GATEWAY=1` 走网关访问 LLM。
- **Web UI**：`--web` 启动内置 HTTP 服务，浏览器访问即可聊天。
- **对话管理**：基于文件的 `ChatHistory`，支持 `--clear` / `clear` 清空历史。
- **记忆检索**：`MemoryEngine` 根据用户问题搜索历史中的相关上下文。
- **权限控制**：`PermissionManager` 在调用工具前进行审批（CLI 默认询问，Web UI 自动允许）。

## 依赖

- C++17 编译器（GCC、Clang）
- CMake 4.0+
- libcurl
- 其他依赖通过 CMake `FetchContent` 自动下载：
  - nlohmann/json
  - spdlog
  - IXWebSocket
  - cpp-httplib

## 快速开始

```bash
cmake -B build
cmake --build build

export CLAW_API_KEY=sk-...
export CLAW_MODEL=gpt-4o
export CLAW_BASE_URL=https://api.openai.com/v1

./build/quantclaw "你好"
```

## 模型名称格式

`CLAW_MODEL` 支持两种写法：

| 写法 | 说明 | 示例 |
|---|---|---|
| `provider/model` | 显式指定提供商 | `openai/gpt-4o`、`anthropic/claude-3-5-sonnet-20240620` |
| `model` | 根据模型名自动推断提供商 | `gpt-4o` → openai，`claude-...` → anthropic |

## 环境变量

| 变量 | 说明 | 示例 |
|---|---|---|
| `CLAW_API_KEY` | LLM API 密钥 | `sk-...` |
| `CLAW_MODEL` | 模型名称 | `gpt-4o`、`claude-3-5-sonnet-20240620` |
| `CLAW_BASE_URL` | LLM API 基础地址 | `https://api.openai.com/v1` |
| `QUANTCLAW_USE_GATEWAY` | 设为 `1` 时通过网关客户端发送请求 | `1` |
| `QUANTCLAW_MCP_SERVER` | MCP 服务器启动命令 | `npx -y @modelcontextprotocol/server-filesystem /tmp` |
| `QUANTCLAW_SIDECAR_URL` | Node.js Sidecar 地址 | `http://127.0.0.1:18802` |
| `QUANTCLAW_SIDECAR_DISABLED` | 设为 `1` 时禁用 Sidecar 工具注册 | `1` |

## 运行模式

### CLI 直接对话

```bash
./build/quantclaw "你好"
./build/quantclaw "4 + 6 等于多少"
```

### 清空历史

```bash
./build/quantclaw --clear
# 或
./build/quantclaw clear
```

### 网关模式

终端 1：启动网关服务器

```bash
./build/quantclaw --gateway
```

终端 2：通过网关客户端发送请求

```bash
QUANTCLAW_USE_GATEWAY=1 ./build/quantclaw "你好"
```

- 网关默认监听 `ws://127.0.0.1:18800`。
- 客户端默认连接 `ws://127.0.0.1:18800`，与服务端保持一致。

### Web UI

```bash
./build/quantclaw --web
```

默认监听 `http://127.0.0.1:8080`，浏览器打开即可使用。

### 接入 MCP 工具

```bash
export QUANTCLAW_MCP_SERVER="npx -y @modelcontextprotocol/server-filesystem /tmp"
./build/quantclaw "帮我列出 /tmp 下的文件"
```

## 项目结构

```text
src/
  main.cpp              # 入口：CLI、网关、Web UI 路由
  config.h              # 配置加载（环境变量）
  cli/                  # CliManager 命令管理
  core/                 # MemoryEngine 记忆检索
  providers/            # LLMProvider 抽象、OpenAI/Anthropic 实现、HttpClient、ProviderFactory、ProviderRegistry
  session/              # ChatHistory 对话历史
  tools/                # Tool 抽象、ToolRegistry、CalculatorTool
  mcp/                  # MCPClient、MCPTool、StdioTransport
  gateway/              # GatewayServer（WebSocket）、GatewayClient、JsonRpcMessage
  plugins/              # SidecarManager、SidecarTool（Node.js 扩展）
  security/             # PermissionManager 权限审批
  web/                  # WebServer（HTTP + 内置前端页面）
```

## 常见问题

### 出现 `[sidecar] failed to connect` 警告

这表示 Node.js Sidecar 服务没有启动。如果不需要 Sidecar 工具，可以禁用：

```bash
QUANTCLAW_SIDECAR_DISABLED=1 ./build/quantclaw "你好"
```

### 网关客户端提示 `Timeout: Unable to connect`

请确认：
1. 已先用 `./build/quantclaw --gateway` 启动网关服务器。
2. 服务端与客户端使用相同的端口（默认 `18800`）。

## 开发

- 使用 C++17。
- 编译选项开启 `-Wall -Wextra -O2`。
- 优先使用 RAII 管理资源。
