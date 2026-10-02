#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-large-buffer-texture.XXXXXX")"
echo "Large TextureBuffer logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 2 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_large_buffer_texture_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_large_buffer_texture_replay.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
for kind in small read write; do
  unset RENDERDOC_METAL_LARGE_READ_WRITE
  export RENDERDOC_METAL_LARGE_TEXELS=4587520
  if [[ "$kind" == small ]]; then export RENDERDOC_METAL_LARGE_TEXELS=2621440; fi
  if [[ "$kind" == write ]]; then export RENDERDOC_METAL_LARGE_READ_WRITE=1; fi
  env MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
  env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
  env MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture.rdc" >"${LOG_DIR}/${kind}-replay.log" 2>&1
  python3 util/test/metal/metal_large_buffer_texture_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-gate" >"${LOG_DIR}/${kind}-gate.log" 2>&1
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-end-hash.log"
cmp "${LOG_DIR}/library-hash.log" "${LOG_DIR}/library-end-hash.log"
echo 'PASS large TextureBuffer: 3 captures/12 reset-seeks/48 API+CLI initial/range/format rejection groups, Native first/middle/last with only3 invocations, actual10/17.5MiB views and Read/ReadWrite usage'
