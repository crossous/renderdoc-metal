#!/bin/bash
# Small Native draw coverage first; optional current UE inspection second. GPU serial.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
if [[ $# -ne 0 && $# -ne 2 ]]; then
  echo "Usage: bash $0 [current-ue-capture.rdc EID]" >&2
  exit 2
fi
if pgrep -x qrenderdoc >/dev/null || pgrep -x UnrealEditor >/dev/null; then
  echo 'Close qrenderdoc/UnrealEditor before this serial GPU probe.' >&2
  exit 2
fi
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-test-overlay.XXXXXX")"
cd "${REPO_ROOT}"
echo "Native draw overlay evidence: ${LOG_DIR}"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-before.sha256"
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_test_overlay_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_test_overlay_replay.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
for mode in serial parallel cull ephemeral viewport viewport-array viewport-reset; do
  MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" "$mode" >"${LOG_DIR}/native-${mode}.log" 2>&1
  MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${mode}" "${LOG_DIR}/capture" "$mode" \
    >"${LOG_DIR}/capture-${mode}.log" 2>&1
  if [[ "$mode" == cull || "$mode" == ephemeral || "$mode" == viewport* ]]; then
    replay_mode=cull
    [[ "$mode" != viewport* ]] || replay_mode="$mode"
    MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_OVERLAY=1 \
      "${LOG_DIR}/replay" "${LOG_DIR}/${mode}_capture.rdc" "$replay_mode" \
      >"${LOG_DIR}/replay-${mode}.log" 2>&1
  else
    MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_OVERLAY=1 \
      "${LOG_DIR}/replay" "${LOG_DIR}/${mode}_capture.rdc" \
      >"${LOG_DIR}/replay-${mode}.log" 2>&1
  fi
done
if [[ $# -eq 2 ]]; then
  # This probe's input-byte checks use ResourceIds from the current acceptance capture.
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_OVERLAY=1 RENDERDOC_METAL_OVERLAY_DUMP_DIR="${LOG_DIR}" \
    "${LOG_DIR}/replay" "$1" "$2" >"${LOG_DIR}/ue-replay.log" 2>&1
  shasum -a 256 "$1" "${LOG_DIR}"/ue-*.bin >"${LOG_DIR}/ue.sha256"
fi
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-after.sha256"
cmp "${LOG_DIR}/library-before.sha256" "${LOG_DIR}/library-after.sha256"
echo 'PASS Native depth/stencil/cull/wireframe/viewport-scissor, indirect and parallel draws, multi-state guards and restored originals'
