#!/bin/bash
# Fresh capture-side checks after shader-proxy ownership changes. Run after the shared build/API batch.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
for demo in Metal_Simple_Triangle Metal_Argument_Buffer Metal_Pipeline_Variants; do
  prefix="${CAPTURE_DIR}/t48_compat_${demo}"
  RENDERDOC_METAL_CAPTURE_PATH="$prefix" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" "$demo" --frames 8
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" "${prefix}_capture.rdc" "${prefix}.ppm"
  "${BUILD_DIR}/bin/renderdoccmd" replay --loops 3 "${prefix}_capture.rdc"
done
echo "T48 source-library fresh capture compatibility passed: triangle, argument buffer, pipeline variants."
