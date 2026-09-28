#!/bin/bash
# T143 two translated instances of one primitive AS, terminal-only.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "$BUILD_DIR" --target renderdoccmd -j 8
mkdir -p "$CAPTURE_DIR"
for trial in 1 2 3; do
  env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T143_MULTI=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Ray_Instance --frames 3
done
env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T143_MULTI=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t143" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Ray_Instance --frames 3
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" \
  t143 t142 t141 t140 t139 t138 t137 t136 t135
python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
  "${BUILD_DIR}/bin/renderdoccmd" "${CAPTURE_DIR}/t143_capture.rdc"
echo "T143 two-instance ray tracing native, capture, targeted and malformed QA passed."
