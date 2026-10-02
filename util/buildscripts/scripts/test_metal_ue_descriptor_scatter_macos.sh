#!/bin/bash
# Execute one captured UE update shader with a compact copy input, never the full frame.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
SOURCE_XML="${1:-${BUILD_DIR}/local-m2-descriptor-replay/ue-frame-births.zip.xml}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-ue-scatter.XXXXXX")"
echo "Actual UE shader scatter logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. \
  util/test/metal/metal_ue_descriptor_scatter_capture.mm -framework Foundation -framework Metal \
  -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -arch "$(uname -m)" -mmacosx-version-min=13.0 \
  -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_ue_descriptor_scatter_replay.mm \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Foundation -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
for minimum in 1 2; do
  assets="${LOG_DIR}/assets-${minimum}"
  python3 util/ue/extract_ue_metal_descriptor_scatter.py "${SOURCE_XML}" --minimum-updates "${minimum}" \
    --output "${assets}" >"${LOG_DIR}/extract-${minimum}.log" 2>&1
  MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" "${assets}" >"${LOG_DIR}/native-${minimum}.log" 2>&1
  MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/ue-${minimum}" \
    "${LOG_DIR}/capture" "${assets}" >"${LOG_DIR}/capture-${minimum}.log" 2>&1
  for suffix in '' _2; do
    capture="${LOG_DIR}/ue-${minimum}_capture${suffix}.rdc"
    MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "$capture" "${assets}" >"${LOG_DIR}/gpu-${minimum}${suffix}.log" 2>&1
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/bin/renderdoccmd" replay --loops 1 "$capture" >"${LOG_DIR}/cli-${minimum}${suffix}.log" 2>&1
  done
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo "PASS actual UE update shader: native/two captures/four seeks/CLI for one and multiple updates"
echo "Opaque copy only; descriptor consumers/full UE image/MRT/pass/UI remain unverified."
