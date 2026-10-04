#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
if [[ $# -ne 0 && $# -ne 2 ]];then echo "Usage: bash $0 [same-ue.rdc EID]" >&2;exit 2;fi
if pgrep -x qrenderdoc >/dev/null || pgrep -x UnrealEditor >/dev/null;then
  echo 'Close qrenderdoc/UnrealEditor before serial GPU checks.' >&2;exit 2
fi
cd "$REPO_ROOT"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-discard.XXXXXX")"
echo "Discard evidence: ${LOG_DIR}"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/before.sha256"
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_discard_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_discard_replay.mm \
  -framework Foundation -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/native.log" 2>&1
MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/discard" "${LOG_DIR}/capture" >"${LOG_DIR}/capture.log" 2>&1
MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${LOG_DIR}/discard_capture.rdc" >"${LOG_DIR}/replay.log" 2>&1
MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${LOG_DIR}/discard_capture.rdc" --fastest >"${LOG_DIR}/fastest.log" 2>&1
MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${LOG_DIR}/discard_capture.rdc" --stress >"${LOG_DIR}/stress.log" 2>&1
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_msaa_discard_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/msaa-capture"
clang++ -std=c++17 -fobjc-arc -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_msaa_discard_replay.mm \
  -framework Foundation -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/msaa-replay"
MTL_DEBUG_LAYER=1 "${LOG_DIR}/msaa-capture" >"${LOG_DIR}/msaa-native.log" 2>&1
MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/msaa" "${LOG_DIR}/msaa-capture" >"${LOG_DIR}/msaa-capture.log" 2>&1
MTL_DEBUG_LAYER=1 "${LOG_DIR}/msaa-replay" "${LOG_DIR}/msaa_capture.rdc" >"${LOG_DIR}/msaa-replay.log" 2>&1
MTL_DEBUG_LAYER=1 "${LOG_DIR}/msaa-replay" "${LOG_DIR}/msaa_capture.rdc" --fastest >"${LOG_DIR}/msaa-fastest.log" 2>&1
MTL_DEBUG_LAYER=1 "${LOG_DIR}/msaa-replay" "${LOG_DIR}/msaa_capture.rdc" --stress >"${LOG_DIR}/msaa-stress.log" 2>&1
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_PROBE_STORE_PLANES=1 "${LOG_DIR}/msaa-capture" >"${LOG_DIR}/msaa-store-native.log" 2>&1
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_PROBE_STORE_PLANES=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/msaa-store" "${LOG_DIR}/msaa-capture" >"${LOG_DIR}/msaa-store-capture.log" 2>&1
MTL_DEBUG_LAYER=1 "${LOG_DIR}/msaa-replay" "${LOG_DIR}/msaa-store_capture.rdc" --store-planes >"${LOG_DIR}/msaa-store-replay.log" 2>&1
MTL_DEBUG_LAYER=1 "${LOG_DIR}/msaa-replay" "${LOG_DIR}/msaa-store_capture.rdc" --store-planes --fastest >"${LOG_DIR}/msaa-store-fastest.log" 2>&1
MTL_DEBUG_LAYER=1 "${LOG_DIR}/msaa-replay" "${LOG_DIR}/msaa-store_capture.rdc" --store-planes --stress >"${LOG_DIR}/msaa-store-stress.log" 2>&1
if [[ $# -eq 2 ]];then
  mkdir "${LOG_DIR}/ue"
  clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_metal_replay_event_probe.cpp \
    -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/event-probe"
  MTL_DEBUG_LAYER=1 "${LOG_DIR}/event-probe" "$1" "${LOG_DIR}/ue" "$2" \
    --inspect-shaders --usage=12323 --texture=12323 --texture=12331 --texture=12397 >"${LOG_DIR}/ue.log" 2>&1
fi
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/after.sha256"
cmp "${LOG_DIR}/before.sha256" "${LOG_DIR}/after.sha256"
echo 'PASS DontCare patterns: typed colour, independent depth/stencil, parallel store, unretained ownership, mip/array/cube/3D, rate maps, untracked standalone/heaps, all MSAA samples, resolve preservation and memoryless backing'
