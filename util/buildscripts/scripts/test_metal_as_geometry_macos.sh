#!/bin/bash
# Bounded bottom-level AS geometry and descriptor-allocation variants; terminal-only.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "$BUILD_DIR" --target renderdoccmd -j 8
mkdir -p "$CAPTURE_DIR"
for test in 135 136 137 138 139 140; do
  case "$test" in
    135) fixture=RENDERDOC_METAL_T135_AS_BUILD ;;
    136) fixture=RENDERDOC_METAL_T136_AS_INDEXED ;;
    137) fixture=RENDERDOC_METAL_T137_AS_BOX ;;
    138) fixture=RENDERDOC_METAL_T138_AS_DESCRIPTOR ;;
    139) fixture=RENDERDOC_METAL_T139_AS_COPY ;;
    140) fixture=RENDERDOC_METAL_T140_AS_COMPACT ;;
  esac
  for trial in 1 2 3; do
    env MTL_DEBUG_LAYER=1 "${fixture}=1" \
      "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
  done
  env MTL_DEBUG_LAYER=1 "${fixture}=1" \
    RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t${test}" \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
done
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" \
  t135 t136 t137 t138 t139 t140 t134 t12 t39 t64 t71 t116 t117 t131 t132
for test in 135 136 137 138 139 140; do
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${BUILD_DIR}/bin/renderdoccmd" "${CAPTURE_DIR}/t${test}_capture.rdc"
done
echo "T135-T140 AS build/copy/compact native, capture, targeted and malformed QA passed."
