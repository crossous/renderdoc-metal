#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-late-signal-prefix.XXXXXX")"
echo "Late signal prefix logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 2 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_indexed_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_graphics_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
export MTL_DEBUG_LAYER=1 RENDERDOC_METAL_PRIVATE_INDICES=1 RENDERDOC_METAL_FRAME_INDICES=1
export RENDERDOC_METAL_LATE_INDEX_UPLOAD=1 RENDERDOC_METAL_LATE_SIGNAL_PREFIX=1
export RENDERDOC_METAL_DRAW_WORK=1 RENDERDOC_METAL_DRAW_BATCH_COUNT=2 RENDERDOC_METAL_POISON_INDEX_TAIL=1
"${LOG_DIR}/capture" >"${LOG_DIR}/native.log" 2>&1
env DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/signal" "${LOG_DIR}/capture" >"${LOG_DIR}/capture.log" 2>&1
read -r va tex sampler < <(python3 - "${LOG_DIR}/capture.log" <<'PY'
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text();print(re.search(r'VA_A=(\d+)',s)[1],*re.search(r'TEX=(\d+) SAMP=(\d+)',s).groups())
PY
)
for suffix in '' _2; do
  before=122;after=186;if [[ -n "$suffix" ]];then before=186;after=122;fi
  env RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 "${LOG_DIR}/replay" "${LOG_DIR}/signal_capture${suffix}.rdc" "$va" 0 "$tex" "$sampler" "$before" "$after" >"${LOG_DIR}/replay${suffix}.log" 2>&1
done
python3 util/test/metal/metal_late_signal_prefix_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/signal_capture.rdc" "${LOG_DIR}/gate" >"${LOG_DIR}/gate.log" 2>&1
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS late signal submission prefix: original GPU signal, late index upload, first/last draw pixels, poisoned tail, EID0/reset seeks and invalid signals rejected before frame execution'
