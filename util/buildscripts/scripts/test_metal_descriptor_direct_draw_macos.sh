#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-direct-draw.XXXXXX")"
echo "Direct draw logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_indexed_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_graphics_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
for variant in triangle16 strip32;do
 wide=0;strip=0;if [[ "$variant" == strip32 ]];then wide=1;strip=1;fi
 export RENDERDOC_METAL_DRAW_WORK=1 RENDERDOC_METAL_DRAW_BATCH_COUNT="${RENDERDOC_METAL_DRAW_BATCH_COUNT:-256}"
 unset RENDERDOC_METAL_TRIANGLE_STRIP
 if [[ "$strip" == 1 ]];then export RENDERDOC_METAL_TRIANGLE_STRIP=1;fi
 env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_PRIVATE_INDICES=1 RENDERDOC_METAL_WIDE_INDICES="$wide" "${LOG_DIR}/capture" >"${LOG_DIR}/${wide}-native.log" 2>&1
 env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_PRIVATE_INDICES=1 RENDERDOC_METAL_WIDE_INDICES="$wide" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/index${wide}" "${LOG_DIR}/capture" >"${LOG_DIR}/${wide}-capture.log" 2>&1
 read -r va tex sampler < <(python3 - "${LOG_DIR}/${wide}-capture.log" <<'PY'
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text();print(re.search(r'VA_A=(\d+)',s)[1],*re.search(r'TEX=(\d+) SAMP=(\d+)',s).groups())
PY
)
 for suffix in '' _2;do
  before=122;after=186;if [[ -n "$suffix" ]];then before=186;after=122;fi
  env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 "${LOG_DIR}/replay" "${LOG_DIR}/index${wide}_capture${suffix}.rdc" "$va" 0 "$tex" "$sampler" "$before" "$after" >"${LOG_DIR}/${wide}-replay${suffix}.log" 2>&1
 done
 python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/index${wide}_capture.rdc" "${LOG_DIR}/${wide}-gate" >"${LOG_DIR}/${wide}-gate.log" 2>&1
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo "PASS bounded indexed draws: ${RENDERDOC_METAL_DRAW_BATCH_COUNT:-256} draws per capture, 4 captures, 16 seek cycles, Private UInt16/UInt32, Triangle/TriangleStrip, reordered indices, instances/baseVertex/baseInstance, action metadata, descriptor producer/consumers and all 2x2 pixels"
