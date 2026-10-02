#!/bin/bash
# Real captured UE descriptor updater followed by compact vertex/fragment consumers.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
SOURCE_XML="${1:-${BUILD_DIR}/local-m2-descriptor-replay/ue-frame-births.zip.xml}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-ue-batch-graphics.XXXXXX")"
echo "Actual UE update-to-graphics logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
python3 util/ue/extract_ue_metal_descriptor_scatter.py "${SOURCE_XML}" --output "${LOG_DIR}/assets" >"${LOG_DIR}/extract.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. \
  util/test/metal/metal_descriptor_ue_graphics_capture.mm -framework Foundation -framework Metal \
  -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -arch "$(uname -m)" -mmacosx-version-min=13.0 \
  -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_graphics_replay.mm \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Foundation -framework Metal -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. \
  util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
RENDERDOC_METAL_UE_BATCH_UPDATES="${RENDERDOC_METAL_UE_BATCH_UPDATES:-52}" MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" "${LOG_DIR}/assets" >"${LOG_DIR}/native.log" 2>&1
RENDERDOC_METAL_UE_BATCH_UPDATES="${RENDERDOC_METAL_UE_BATCH_UPDATES:-52}" MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/ue" "${LOG_DIR}/capture" "${LOG_DIR}/assets" >"${LOG_DIR}/capture.log" 2>&1
read -r va tex sampler < <(python3 - "${LOG_DIR}/capture.log" <<'PY'
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text()
print(re.search(r'VA_A=(\d+)',s)[1],*re.search(r'TEX=(\d+) SAMP=(\d+)',s).groups())
PY
)
for suffix in '' _2; do
  before=122; after=186
  if [[ -n "${suffix}" ]]; then before=186; after=122; fi
  MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${LOG_DIR}/ue_capture${suffix}.rdc" "$va" 0 "$tex" "$sampler" "$before" "$after" "batch${RENDERDOC_METAL_UE_BATCH_UPDATES:-52}" >"${LOG_DIR}/gpu${suffix}.log" 2>&1
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/bin/renderdoccmd" replay --loops 1 "${LOG_DIR}/ue_capture${suffix}.rdc" >"${LOG_DIR}/cli${suffix}.log" 2>&1
done
python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" \
  "${LOG_DIR}/ue_capture.rdc" "${LOG_DIR}/gate" >"${LOG_DIR}/gate.log" 2>&1
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo "PASS actual UE update shader to sourced vertex/fragment consumers: native, two captures, eight seek cycles, all produced slots, last-slot vertex/fragment, GPU bytes/pixels, CLI and 39 API+CLI negative groups"
echo "Compact consumer shaders; complete UE graphics, MRT/pass/UI acceptance still pending."
