#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-sourced-indirect.XXXXXX")"
echo "Sourced indirect logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_mrt_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_mrt_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
export RENDERDOC_METAL_MRT_COMPUTE_INDIRECT=1
export RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS=1
for kind in shared private depth_only_shared depth_only_private zero_shared zero_private; do
  unset RENDERDOC_METAL_MRT_ZERO_INDIRECT RENDERDOC_METAL_MRT_PRIVATE_ARGUMENTS RENDERDOC_METAL_MRT_DEPTH_FORMAT RENDERDOC_METAL_MRT_DEPTH_ONLY
  if [[ "$kind" == zero_* ]]; then export RENDERDOC_METAL_MRT_ZERO_INDIRECT=1; fi
  if [[ "$kind" == *private ]]; then export RENDERDOC_METAL_MRT_PRIVATE_ARGUMENTS=1; fi
  if [[ "$kind" == depth_only_* ]]; then export RENDERDOC_METAL_MRT_DEPTH_ONLY=1 RENDERDOC_METAL_MRT_DEPTH_FORMAT=d32s8; fi
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
  python3 util/test/metal/metal_descriptor_sourced_indirect_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-indirect-gate" >"${LOG_DIR}/${kind}-indirect-gate.log" 2>&1
done
echo "PASS sourced compute indirect: 12 captures, 48 reset cycles, Native groups 1 then GPU rewrite 2 and zero, Shared/Private arguments, descriptor producer, MRT/depth-only pixel checks and preflight negative groups"
