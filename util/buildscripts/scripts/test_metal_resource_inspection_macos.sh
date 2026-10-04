#!/bin/bash
# Serial resource inspection checks. Close UE/qrenderdoc before running GPU probes.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-resource-inspection.XXXXXX")"
cd "${REPO_ROOT}"
echo "Resource inspection logs: ${LOG_DIR}"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_resource_usage_replay.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/usage-replay"
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_frame_heap_texture_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/label-capture"
for fixture in t01 t12; do
  MTL_DEBUG_LAYER=1 "${LOG_DIR}/usage-replay" "captures/metal-smoke/${fixture}_capture.rdc" \
    >"${LOG_DIR}/${fixture}.log" 2>&1
done
RENDERDOC_METAL_TEST_RESOURCE_LABELS=1 MTL_DEBUG_LAYER=1 "${LOG_DIR}/label-capture" \
  >"${LOG_DIR}/label-native.log" 2>&1
for variant in direct view; do
  view_env=()
  [[ "$variant" != view ]] || view_env+=(RENDERDOC_METAL_FRAME_TEXTURE_VIEW=1)
  env ${view_env[@]+"${view_env[@]}"} RENDERDOC_METAL_TEST_RESOURCE_LABELS=1 MTL_DEBUG_LAYER=1 \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${variant}" "${LOG_DIR}/label-capture" \
    >"${LOG_DIR}/${variant}-capture.log" 2>&1
  for capture in "${LOG_DIR}/${variant}"*.rdc; do
    MTL_DEBUG_LAYER=1 "${LOG_DIR}/usage-replay" "$capture" --labels \
      >"${capture%.rdc}-usage.log" 2>&1
  done
done
if [[ $# -gt 0 ]]; then
  if [[ $# -ne 3 ]]; then echo "Usage: $0 [capture.rdc EID TextureResourceId]" >&2; exit 2; fi
  clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_metal_replay_event_probe.cpp \
    -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/event-probe"
  "${LOG_DIR}/event-probe" "$1" "${LOG_DIR}" "$2" --inspect-shaders "--usage=$3" \
    >"${LOG_DIR}/real-ue.log" 2>&1
fi
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS directed resource names, view/parent usage, attachment writes and event navigation'
