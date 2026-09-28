#!/bin/bash
# Purgeable buffer/texture state and device-query bridge regression, no GUI launch.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
bash "${SCRIPT_DIR}/test_metal_device_queries_macos.sh"
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
mkdir -p "${CAPTURE_DIR}"
MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Purgeable_State --frames 4
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t62" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Purgeable_State --frames 4
RENDERDOC_METAL_LAST_TEST=62 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
