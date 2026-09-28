#!/bin/bash
# Direct patch tessellation with factor buffer, scale, malformed and combined replay checks.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
cmake --build "${BUILD_DIR}" --target renderdoccmd -j "$(sysctl -n hw.ncpu)"
mkdir -p "${CAPTURE_DIR}"
MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Tessellation --frames 3
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t66" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Tessellation --frames 3
RENDERDOC_METAL_LAST_TEST=66 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
