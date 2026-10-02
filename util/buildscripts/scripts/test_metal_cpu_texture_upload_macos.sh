#!/bin/bash
# Native, capture, GPU output/seeks and malformed footprint checks for tiny CPU uploads.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-cpu-upload.XXXXXX")"
echo "CPU texture upload logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. \
  util/test/metal/metal_cpu_texture_upload_capture.mm -framework Foundation -framework Metal \
  -framework QuartzCore -o "${LOG_DIR}/capture"
for kind in replay open_probe; do
  source="util/test/metal/metal_cpu_texture_upload_replay.cpp"
  if [[ "$kind" == open_probe ]]; then source="util/ue/ue_capture_open_probe.cpp"; fi
  clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=13.0 \
    -DRENDERDOC_PLATFORM_APPLE -I. "$source" -L"${BUILD_DIR}/lib" -lrenderdoc \
    -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/${kind}"
done
MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/native.log" 2>&1
MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/upload" "${LOG_DIR}/capture" >"${LOG_DIR}/capture.log" 2>&1
MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${LOG_DIR}/upload_capture.rdc" >"${LOG_DIR}/replay.log" 2>&1
python3 util/test/metal/metal_cpu_texture_upload_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/upload_capture.rdc" "${LOG_DIR}/gate" >"${LOG_DIR}/gate.log" 2>&1
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo "PASS native/capture/GPU=407/DEADBEEF/four seeks/12 API+CLI negative upload footprints"
