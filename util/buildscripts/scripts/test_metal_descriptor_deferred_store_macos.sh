#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-deferred-store.XXXXXX")"
echo "Deferred StoreAction logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_mrt_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_mrt_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
export RENDERDOC_METAL_MRT_DEFERRED_STORE=1
unset RENDERDOC_METAL_MRT_COUNTERS
for kind in serial_two parallel_two serial_d16 serial_d32 serial_depth frame_depth parallel_depth five_depth counter_depth; do
  unset RENDERDOC_METAL_MRT_COUNTERS RENDERDOC_METAL_MRT_FRAME_DEPTH RENDERDOC_METAL_FIVE_MRT RENDERDOC_METAL_MRT_DEPTH_FORMAT RENDERDOC_METAL_PARALLEL_MRT
  if [[ "$kind" == *depth ]]; then export RENDERDOC_METAL_MRT_DEPTH_FORMAT=d32s8; fi
  if [[ "$kind" == serial_d16 ]]; then export RENDERDOC_METAL_MRT_DEPTH_FORMAT=d16; fi
  if [[ "$kind" == serial_d32 ]]; then export RENDERDOC_METAL_MRT_DEPTH_FORMAT=d32; fi
  if [[ "$kind" == counter_* ]]; then export RENDERDOC_METAL_MRT_COUNTERS=1; fi
  if [[ "$kind" == frame_* ]]; then export RENDERDOC_METAL_MRT_FRAME_DEPTH=1; fi
  if [[ "$kind" == parallel_* ]]; then export RENDERDOC_METAL_PARALLEL_MRT=1; fi
  if [[ "$kind" == five_* ]]; then export RENDERDOC_METAL_FIVE_MRT=1; fi
  env MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
  env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
  read -r va table tex sampler < <(python3 - "${LOG_DIR}/${kind}-capture.log" <<'PY'
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text();print(*(re.search(k+r'=(\d+)',s)[1] for k in ('VA_A','VA_TABLE','TEX','SAMP')))
PY
  )
  for suffix in '' _2; do
    before=122; after=186
    if [[ -n "$suffix" ]]; then before=186; after=122; fi
    env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" "$va" "$table" "$tex" "$sampler" "$before" "$after" >"${LOG_DIR}/${kind}-replay${suffix}.log" 2>&1
  done
  python3 util/test/metal/metal_descriptor_deferred_store_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-store-gate" >"${LOG_DIR}/${kind}-store-gate.log" 2>&1
done
echo "PASS sourced deferred stores: 18 captures, 72 seeks, serial/parallel colors, D16/D32/D32S8, frame heap/five MRT/counter, partial and full pixels, unresolved/malformed final stores rejected before frame submission"
