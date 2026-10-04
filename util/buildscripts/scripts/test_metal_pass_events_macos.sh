#!/bin/bash
# Pass metadata only: small interleaved fixture, existing parallel/blit fixtures, then same UE.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
if [[ $# -ne 4 ]]; then
  echo "Usage: bash $0 parallel.rdc blit.rdc current-ue.rdc EID" >&2
  exit 2
fi
if pgrep -x qrenderdoc >/dev/null || pgrep -x UnrealEditor >/dev/null; then
  echo 'Close qrenderdoc/UnrealEditor before this serial GPU probe.' >&2
  exit 2
fi
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-pass-events.XXXXXX")"
cd "${REPO_ROOT}"
echo "Pass event evidence: ${LOG_DIR}"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-before.sha256"
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_pass_events_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_pass_events_replay.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/native.log" 2>&1
MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/interleaved" "${LOG_DIR}/capture" >"${LOG_DIR}/capture.log" 2>&1
fixtures=("${LOG_DIR}/interleaved_capture.rdc" "$1" "$2")
names=(interleaved parallel blit)
for i in 0 1 2; do
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_PASS_EVENTS=1 "${LOG_DIR}/replay" "${fixtures[$i]}" \
    >"${LOG_DIR}/${names[$i]}.log" 2>&1
  python3 util/test/metal/metal_pass_events_gate.py "${LOG_DIR}/${names[$i]}.log"
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_PASS_EVENTS=1 "${LOG_DIR}/replay" "$3" "$4" >"${LOG_DIR}/ue.log" 2>&1
python3 util/test/metal/metal_pass_events_gate.py "${LOG_DIR}/ue.log"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-after.sha256"
cmp "${LOG_DIR}/library-before.sha256" "${LOG_DIR}/library-after.sha256"
echo 'PASS actual render-pass events: interleaved shared target, new encoder, parallel children, blit, same UE resets'
