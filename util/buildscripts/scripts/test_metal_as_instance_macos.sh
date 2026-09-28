#!/bin/bash
# T142 translated single-instance top-level AS, terminal-only.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "$BUILD_DIR" --target renderdoccmd -j 8
mkdir -p "$CAPTURE_DIR"
for trial in 1 2 3; do
  env MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Ray_Instance --frames 3
done
env MTL_DEBUG_LAYER=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t142" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Ray_Instance --frames 3
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" \
  t142 t141 t140 t139 t138 t137 t136 t135
python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
  "${BUILD_DIR}/bin/renderdoccmd" "${CAPTURE_DIR}/t142_capture.rdc"
echo "T142 translated instance AS native, capture, targeted replay and malformed QA passed."
