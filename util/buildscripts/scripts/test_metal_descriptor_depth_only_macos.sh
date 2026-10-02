#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-depth-only.XXXXXX")"
echo "Depth-only logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_mrt_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_mrt_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
export RENDERDOC_METAL_MRT_DEPTH_ONLY=1
for kind in background_d16 background_d32 background_d32s8 frame_d32s8 parallel_d32s8 deferred_d32s8 parallel_deferred_d32s8; do
  export RENDERDOC_METAL_MRT_DEPTH_FORMAT="${kind##*_}"
  unset RENDERDOC_METAL_MRT_FRAME_DEPTH RENDERDOC_METAL_FIVE_MRT RENDERDOC_METAL_PARALLEL_MRT RENDERDOC_METAL_MRT_DEFERRED_STORE
  if [[ "$kind" == *parallel* ]]; then export RENDERDOC_METAL_PARALLEL_MRT=1; fi
  if [[ "$kind" == *deferred* ]]; then export RENDERDOC_METAL_MRT_DEFERRED_STORE=1; fi
  if [[ "$kind" == frame_* ]]; then export RENDERDOC_METAL_MRT_FRAME_DEPTH=1; fi
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
  python3 util/test/metal/metal_descriptor_depth_only_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-depth-gate" >"${LOG_DIR}/${kind}-depth-gate.log" 2>&1
done
echo "PASS sourced depth-only: 14 captures, 56 seek cycles plus depth-only seeks, absent fragment reflection, D16/D32/D32S8, initial/frame heap, serial/parallel/deferred store, all depth/stencil and downstream MRT pixels"
