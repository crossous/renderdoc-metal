#!/bin/bash
# Serial directed query resource birth/CPU initialization and native result checks.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-frame-visibility.XXXXXX")"
cd "${REPO_ROOT}"
echo "Frame visibility logs: ${LOG_DIR}"
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_descriptor_mrt_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_mrt_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
export MTL_DEBUG_LAYER=1 RENDERDOC_METAL_MRT_VISIBILITY=1 RENDERDOC_METAL_MRT_FRAME_VISIBILITY=1
run_bounded() {
  python3 - "$@" <<'PY'
import subprocess,sys
sys.exit(subprocess.run(sys.argv[1:],timeout=30).returncode)
PY
}
for kind in standalone heap parallel_heap earlier_snapshot; do
  unset RENDERDOC_METAL_MRT_FRAME_VISIBILITY_HEAP RENDERDOC_METAL_PARALLEL_MRT RENDERDOC_METAL_MRT_VISIBILITY_EARLIER_SNAPSHOT
  [[ "$kind" == standalone ]] || export RENDERDOC_METAL_MRT_FRAME_VISIBILITY_HEAP=1
  [[ "$kind" != parallel_heap ]] || export RENDERDOC_METAL_PARALLEL_MRT=1
  [[ "$kind" != earlier_snapshot ]] || export RENDERDOC_METAL_MRT_VISIBILITY_EARLIER_SNAPSHOT=1
  run_bounded "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
  run_bounded env DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
  read -r va table tex sampler < <(python3 - "${LOG_DIR}/${kind}-capture.log" <<'PY'
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text();print(*(re.search(k+r'=(\d+)',s)[1] for k in ('VA_A','VA_TABLE','TEX','SAMP')))
PY
  )
  for suffix in '' _2; do
    before=122; after=186
    [[ -z "$suffix" ]] || { before=186; after=122; }
    run_bounded "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" "$va" "$table" "$tex" "$sampler" "$before" "$after" >"${LOG_DIR}/${kind}-replay${suffix}.log" 2>&1
  done
  python3 util/test/metal/metal_descriptor_visibility_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-gate" >"${LOG_DIR}/${kind}-gate.log" 2>&1
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS frame-born visibility: standalone/placement/parallel/earlier submission snapshot, 8 captures, 32 seeks, Native count4, sentinels, 40 API+CLI rejection groups'
