#!/bin/bash
# my_claw 构建脚本：配置、编译并运行单元测试。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "[build] 配置 CMake: BUILD_TYPE=${BUILD_TYPE}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

echo "[build] 编译项目"
cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "[build] 运行单元测试"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo "[build] 构建完成：${BUILD_DIR}/quantclaw"
