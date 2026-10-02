#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-attachmentless-uav.XXXXXX")"
echo "Attachmentless UAV logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 2 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_attachmentless_uav_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture" >"${LOG_DIR}/fixture-build.log" 2>&1
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_attachmentless_uav_replay.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
export RENDERDOC_METAL_ATTACHMENTLESS_SOURCED=1 RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS=1
for kind in tiny ue boundary parallel_ue unretained_ue parallel_unretained_ue; do
  unset RENDERDOC_METAL_ATTACHMENTLESS_PARALLEL RENDERDOC_METAL_ATTACHMENTLESS_UNRETAINED
  export RENDERDOC_METAL_ATTACHMENTLESS_WIDTH=320 RENDERDOC_METAL_ATTACHMENTLESS_HEIGHT=240
  if [[ "$kind" == tiny ]]; then export RENDERDOC_METAL_ATTACHMENTLESS_WIDTH=2 RENDERDOC_METAL_ATTACHMENTLESS_HEIGHT=2; fi
  if [[ "$kind" == boundary ]]; then export RENDERDOC_METAL_ATTACHMENTLESS_WIDTH=512 RENDERDOC_METAL_ATTACHMENTLESS_HEIGHT=512; fi
  if [[ "$kind" == parallel* ]]; then export RENDERDOC_METAL_ATTACHMENTLESS_PARALLEL=1; fi
  if [[ "$kind" == *unretained* ]]; then export RENDERDOC_METAL_ATTACHMENTLESS_UNRETAINED=1; fi
  env MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
  env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
  env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture.rdc" >"${LOG_DIR}/${kind}-replay.log" 2>&1
  python3 util/test/metal/metal_attachmentless_uav_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-gate" >"${LOG_DIR}/${kind}-gate.log" 2>&1
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-end-hash.log"
cmp "${LOG_DIR}/library-hash.log" "${LOG_DIR}/library-end-hash.log"
echo 'PASS attachmentless sourced UAV: 6 captures/42 reset-seeks/252 API+CLI rejection groups, actual vertex writes and sums, explicit 2x2/320x240/512x512, parallel and unretained, Native original indirect and PSO'
