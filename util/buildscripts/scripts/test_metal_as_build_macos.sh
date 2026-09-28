#!/bin/bash
# Single static triangle AS build and GPU compacted-size write; terminal-only.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "$BUILD_DIR" --target renderdoccmd -j 8
mkdir -p "$CAPTURE_DIR"
for trial in 1 2 3; do
  env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T135_AS_BUILD=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
done
env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T135_AS_BUILD=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t135" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" \
  t135 t134 t12 t39 t64 t71 t116 t117 t131 t132
python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
  "${BUILD_DIR}/bin/renderdoccmd" "${CAPTURE_DIR}/t135_capture.rdc"
echo "T135 static-triangle AS build, compacted size, seek, negatives and targeted replay passed."
