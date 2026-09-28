#!/bin/bash
# T146/T147 vertex and tile AS GPU ray queries; terminal only.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "$BUILD_DIR" --target renderdoccmd -j 8
mkdir -p "$CAPTURE_DIR"
for fixture in t146 t147; do
  flag=RENDERDOC_METAL_T146_VERTEX_AS
  if [[ "$fixture" == t147 ]]; then flag=RENDERDOC_METAL_T147_TILE_AS; fi
  for trial in 1 2 3; do
    env MTL_DEBUG_LAYER=1 "$flag=1" "${REPO_ROOT}/bin/demos_x64" Metal_Ray_Instance --frames 3
  done
  env MTL_DEBUG_LAYER=1 "$flag=1" \
    RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/${fixture}" \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" Metal_Ray_Instance --frames 3
done
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" \
  t147 t146 t145 t144 t143 t142 t141 t140 t139 t138 t137 t136 t135
for fixture in t146 t147; do
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${BUILD_DIR}/bin/renderdoccmd" "${CAPTURE_DIR}/${fixture}_capture.rdc"
done
echo "T146/T147 render-stage AS native, capture, API/CLI and malformed QA passed."
