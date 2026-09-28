#!/bin/bash
# One wave: fresh new fixtures, then one aggregate regression. No GUI or old-fixture recapture.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
cmake --build "${BUILD_DIR}" --target renderdoccmd -j "$(sysctl -n hw.ncpu)"
mkdir -p "${CAPTURE_DIR}"
for spec in 't54 Metal_Indexed_Instancing' 't55 Metal_Function_Variants' \
            't56 Metal_Argument_Buffer'; do
  read -r fixture demo <<< "$spec"
  settings=()
  case "$fixture" in
    t54) settings+=(RENDERDOC_METAL_INDEXED_SHORT=1) ;;
    t56) settings+=(RENDERDOC_METAL_ARGUMENT_BATCH=1) ;;
  esac
  env "${settings[@]}" MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" "$demo" --frames 6
  env "${settings[@]}" RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/${fixture}" \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" "$demo" --frames 6
done
RENDERDOC_METAL_LAST_TEST=56 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
