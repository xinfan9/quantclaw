#!/bin/bash
# my_claw smoke test：验证核心命令与 MCP server 基本可用性。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BIN="${BUILD_DIR}/quantclaw"
TEST_BIN="${BUILD_DIR}/quantclaw_tests"

echo "[smoke] 检查可执行文件"
if [[ ! -x "${BIN}" ]]; then
  echo "[smoke] 错误：未找到 ${BIN}，请先运行 ./build.sh"
  exit 1
fi

echo "[smoke] 检查单元测试可执行文件"
if [[ ! -x "${TEST_BIN}" ]]; then
  echo "[smoke] 错误：未找到 ${TEST_BIN}"
  exit 1
fi

echo "[smoke] 运行单元测试"
"${TEST_BIN}"

echo "[smoke] 测试 config 命令"
"${BIN}" config >/dev/null

echo "[smoke] 测试 models 命令"
"${BIN}" models >/dev/null

echo "[smoke] 测试 --mcp-server 初始化与工具列表"
MCP_RESPONSE=""
MCP_RESPONSE=$(
  (echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}';
   echo '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}') | "${BIN}" --mcp-server
) || true

if ! echo "${MCP_RESPONSE}" | grep -q '"tools"'; then
  echo "[smoke] 错误：MCP server 未返回 tools 列表"
  exit 1
fi

if ! echo "${MCP_RESPONSE}" | grep -q 'quantclaw_chat'; then
  echo "[smoke] 错误：MCP tools 列表中未包含 quantclaw_chat"
  exit 1
fi

echo "[smoke] 全部通过"
