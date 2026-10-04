#!/bin/bash
# Vulkan four live-lane buckets, followed immediately by the same UE capture.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
if [[ $# -ne 0 && $# -ne 2 ]]; then echo "Usage: bash $0 [current-ue.rdc EID]" >&2;exit 2;fi
if pgrep -x qrenderdoc >/dev/null || pgrep -x UnrealEditor >/dev/null; then
  echo 'Close qrenderdoc/UnrealEditor before serial GPU tests.' >&2;exit 2
fi
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-quad-overdraw.XXXXXX")"
cd "${REPO_ROOT}"
echo "Quad Overdraw evidence: ${LOG_DIR}"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-before.sha256"
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_quad_overdraw_native.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_quad_overdraw_replay.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/native.log" 2>&1
for mode in 0 1 2 3 4 5 6 7 8 9 10; do
  MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/case${mode}" "${LOG_DIR}/capture" "$mode" \
    >"${LOG_DIR}/capture-${mode}.log" 2>&1
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_OVERLAY=1 "${LOG_DIR}/replay" \
    "${LOG_DIR}/case${mode}_capture.rdc" "$mode" >"${LOG_DIR}/replay-${mode}.log" 2>&1
done
if [[ $# -eq 2 ]]; then
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_OVERLAY=1 "${LOG_DIR}/replay" "$1" "ue:$2" >"${LOG_DIR}/ue.log" 2>&1
fi
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-after.sha256"
cmp "${LOG_DIR}/library-before.sha256" "${LOG_DIR}/library-after.sha256"
echo 'PASS Vulkan quad live-lane bucket semantics; Draw/Pass raw counts and resource restoration'
