#!/bin/bash
# Capture texture aliases and run the aggregate terminal gate without launching the GUI.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
cmake --build "${BUILD_DIR}" --target renderdoccmd -j "$(sysctl -n hw.ncpu)"
mkdir -p "${CAPTURE_DIR}"
for fixture in Metal_Texture_Views Metal_Buffer_Texture; do
  case "$fixture" in
    Metal_Texture_Views) prefix=t58 ;;
    Metal_Buffer_Texture) prefix=t59 ;;
  esac
  MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" "$fixture" --frames 4
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/${prefix}" \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" "$fixture" --frames 4
done
RENDERDOC_METAL_LAST_TEST=59 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
