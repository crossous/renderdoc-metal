#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
if [[ $# -ne 0 && $# -ne 2 ]]; then
  echo "Usage: bash $0 [current-ue.rdc EID]" >&2; exit 2
fi
if pgrep -x qrenderdoc >/dev/null || pgrep -x UnrealEditor >/dev/null; then
  echo 'Close qrenderdoc/UnrealEditor before serial GPU checks.' >&2; exit 2
fi
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-clear-before.XXXXXX")"
cd "${REPO_ROOT}"; echo "Clear Before evidence: ${LOG_DIR}"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-before.sha256"
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_clear_before_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_clear_before_replay.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
for mode in serial parallel greater equal always depth-export discard-store stencil; do
  mkdir "${LOG_DIR}/${mode}"
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_CLEAR_REFERENCE_DIR="${LOG_DIR}/${mode}" \
    "${LOG_DIR}/capture" "$mode" >"${LOG_DIR}/native-${mode}.log" 2>&1
  MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${mode}" "${LOG_DIR}/capture" "$mode" \
    >"${LOG_DIR}/capture-${mode}.log" 2>&1
  MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${LOG_DIR}/${mode}_capture.rdc" "${LOG_DIR}/${mode}" \
    >"${LOG_DIR}/replay-${mode}.log" 2>&1
done
if [[ $# -eq 2 ]]; then
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_CLEAR_DUMP_DIR="${LOG_DIR}" "${LOG_DIR}/replay" "$1" "$2" \
    >"${LOG_DIR}/ue.log" 2>&1
fi
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-after.sha256"
cmp "${LOG_DIR}/library-before.sha256" "${LOG_DIR}/library-after.sha256"
echo 'PASS Native Clear Before Draw/Pass MRTs, original FS/discard/blend/depth export, parallel and reversed/equal/always depth; common None restoration'
