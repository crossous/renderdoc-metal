#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-retired-texture-alias.XXXXXX")"
echo "Retired texture alias logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 2 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_retired_texture_alias_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_future_descriptor_snapshot_replay.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
for kind in retained unretained view; do
  runtime=(env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_ASYNC_TEXTURE_ALIAS=1)
  if [[ "$kind" == unretained ]]; then runtime+=(RENDERDOC_METAL_UNRETAINED_SUBMISSIONS=1); fi
  if [[ "$kind" == view ]]; then runtime+=(RENDERDOC_METAL_TEXTURE_ALIAS_VIEW=1); fi
  "${runtime[@]}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
  "${runtime[@]}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
  "${runtime[@]}" RENDERDOC_METAL_TRACE_REPLAY_WAITS=1 "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture.rdc" >"${LOG_DIR}/${kind}-replay.log" 2>&1
  python3 util/test/metal/metal_retired_texture_alias_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-gate" >"${LOG_DIR}/${kind}-gate.log" 2>&1
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS retained/unretained/view retired Private texture -> buffer: Native 17/34/51, four EID0 seek cycles, committed consumer completion, API/CLI live and unsubmitted gates'
