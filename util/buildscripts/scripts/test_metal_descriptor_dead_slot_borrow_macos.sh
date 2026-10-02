#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-dead-slot-table-borrow.XXXXXX")"
echo "Sourced render indirect logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 2 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_mrt_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture" >"${LOG_DIR}/fixture-build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_mrt_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
export RENDERDOC_METAL_MRT_DEAD_SLOT_TABLE_BORROW=1
export RENDERDOC_METAL_MRT_SUBMITTED_GPU_REUSE=1
export RENDERDOC_METAL_MRT_LOGICAL_GPU_RETIREMENT=1
export RENDERDOC_METAL_MRT_RENDER_INDIRECT=1 RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS=1
export RENDERDOC_METAL_MRT_FRESH_CPU_SLOT=1 RENDERDOC_METAL_MRT_UNUSED_PRODUCER_SLOT=1 RENDERDOC_METAL_MRT_MIXED_FRESH_CPU_SLOT=1
unset RENDERDOC_METAL_MRT_INITIAL_CPU_SLOT
for kind in shared undeclared_private parallel_private; do
  unset RENDERDOC_METAL_MRT_PRIVATE_ARGUMENTS RENDERDOC_METAL_MRT_RENDER_INDEXED RENDERDOC_METAL_PARALLEL_MRT RENDERDOC_METAL_MRT_RENDER_ARGUMENT_HEAP RENDERDOC_METAL_FRAME_MRT_TEXTURE RENDERDOC_METAL_MRT_DEPTH_FORMAT RENDERDOC_METAL_MRT_RENDER_ZERO
  unset RENDERDOC_METAL_MRT_FRESH_UNDECLARED
  if [[ "$kind" == undeclared* ]]; then export RENDERDOC_METAL_MRT_FRESH_UNDECLARED=1; fi
  if [[ "$kind" == *private ]]; then export RENDERDOC_METAL_MRT_PRIVATE_ARGUMENTS=1; fi
  if [[ "$kind" == indexed_* ]]; then export RENDERDOC_METAL_MRT_RENDER_INDEXED=1; fi
  if [[ "$kind" == *parallel* ]]; then export RENDERDOC_METAL_PARALLEL_MRT=1; fi
  if [[ "$kind" == heap_* ]]; then export RENDERDOC_METAL_MRT_RENDER_ARGUMENT_HEAP=1; fi
  if [[ "$kind" == frame_* ]]; then export RENDERDOC_METAL_FRAME_MRT_TEXTURE=1; fi
  if [[ "$kind" == depth_* ]]; then export RENDERDOC_METAL_MRT_DEPTH_FORMAT=d32s8; fi
  if [[ "$kind" == *zero_* ]]; then export RENDERDOC_METAL_MRT_RENDER_ZERO=1; fi
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
    replay_args=("${LOG_DIR}/${kind}_capture${suffix}.rdc" "$va" "$table" "$tex" "$sampler" "$before" "$after")
    if [[ "$kind" == frame_* ]]; then
      value=$(python3 - "${LOG_DIR}/${kind}-capture.log" "$suffix" <<'PY'
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text();key='FRAME_TEX_1' if sys.argv[2] else 'FRAME_TEX_0';print(re.search(key+r'=(\d+)',s)[1])
PY
      )
      replay_args+=("frame-texture:$value")
    fi
    env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 "${LOG_DIR}/replay" "${replay_args[@]}" >"${LOG_DIR}/${kind}-replay${suffix}.log" 2>&1
  done
  python3 util/test/metal/metal_descriptor_sourced_render_indirect_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-gate" >"${LOG_DIR}/${kind}-gate.log" 2>&1
  python3 util/test/metal/metal_descriptor_mixed_fresh_cpu_slot_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-unused-gate" >"${LOG_DIR}/${kind}-unused-gate.log" 2>&1
  python3 util/test/metal/metal_descriptor_fresh_cpu_slot_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-cpu-gate" >"${LOG_DIR}/${kind}-cpu-gate.log" 2>&1
  python3 util/test/metal/metal_descriptor_logical_retirement_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-retirement-gate" >"${LOG_DIR}/${kind}-retirement-gate.log" 2>&1
  python3 util/test/metal/metal_descriptor_dead_slot_borrow_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-reuse-gate" >"${LOG_DIR}/${kind}-reuse-gate.log" 2>&1
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-end-hash.log"
cmp "${LOG_DIR}/library-hash.log" "${LOG_DIR}/library-end-hash.log"
echo 'PASS per-generation descriptor borrowers: 6 captures/24 reset cycles/108 indirect proof groups/24 fresh CPU slot rejection groups/24 mixed CPU-GPU rejection groups/24 logical-retirement rejection groups/30 submitted-reuse/slot-borrower rejection groups, Native fresh suballocation feeds later GPU producer, then Native GPU producer, ordinary metadata/MRT/parallel/Private source'
