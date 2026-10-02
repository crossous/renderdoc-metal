#!/bin/bash
# Native/capture/API GPU-byte/CLI/negative validation for tiny typed descriptor fixtures.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-descriptors.XXXXXX")"
echo "Descriptor test logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=13.0 \
  -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/open_probe"
for kind in table gpu_update frame frame_private frame_view inline shadow sourced_gpu sourced_compute resources sourced_resources sourced_frame graphics mrt submissions submissions_unretained heap_payload async_payload alias indexed indexed_wide; do
  source_kind="${kind%_private}"
  unretained_inputs=0
  if [[ "${kind}" == submissions_unretained ]]; then source_kind=submissions; unretained_inputs=1; fi
  wide_indices=0
  if [[ "${kind}" == indexed_wide ]]; then source_kind=indexed; wide_indices=1; fi
  private_inputs=0
  if [[ "${kind}" == frame_private ]]; then private_inputs=1; fi
  clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. \
    "util/test/metal/metal_descriptor_${source_kind}_capture.mm" -framework Foundation -framework Metal \
    -framework QuartzCore -o "${LOG_DIR}/${kind}_capture"
  replay_kind="${source_kind}"
  if [[ "${kind}" == sourced_compute ]]; then replay_kind=sourced_gpu; fi
  if [[ "${kind}" == sourced_frame ]]; then replay_kind=sourced_resources; fi
  if [[ "${source_kind}" == submissions || "${kind}" == heap_payload || "${kind}" == alias || "${source_kind}" == indexed ]]; then replay_kind=graphics; fi
  clang++ -std=c++17 -fobjc-arc -arch "$(uname -m)" -mmacosx-version-min=13.0 \
    -DRENDERDOC_PLATFORM_APPLE -I. "util/test/metal/metal_descriptor_${replay_kind}_replay.mm" \
    -L"${BUILD_DIR}/lib" -lrenderdoc -framework Foundation -framework Metal \
    -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/${kind}_replay"
  capture_environment=(env RENDERDOC_METAL_WIDE_INDICES="${wide_indices}")
  if (( unretained_inputs )); then capture_environment+=(RENDERDOC_METAL_UNRETAINED_SUBMISSIONS=1); fi
  "${capture_environment[@]}" MTL_DEBUG_LAYER=1 RENDERDOC_METAL_DESCRIPTOR_PRIVATE="${private_inputs}" "${LOG_DIR}/${kind}_capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
  "${capture_environment[@]}" MTL_DEBUG_LAYER=1 RENDERDOC_METAL_DESCRIPTOR_PRIVATE="${private_inputs}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" \
    "${LOG_DIR}/${kind}_capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
  read -r va_a va_b < <(python3 - "${LOG_DIR}/${kind}-capture.log" <<'PY'
import re, sys
from pathlib import Path
match = re.search(r'VA_A=(\d+) VA_B=(\d+)(?: VA_TABLE=\d+)?(?: TEX=\d+ SAMP=\d+)?(?: TEX_B=\d+)? captures=2', Path(sys.argv[1]).read_text())
if not match: raise SystemExit('Missing native capture result')
print(*match.groups())
PY
  )
  tex_a=0; tex_b=0
  if [[ "${kind}" == shadow || "${kind}" == sourced_gpu || "${kind}" == sourced_compute || "${kind}" == resources || "${kind}" == sourced_resources || "${kind}" == sourced_frame || "${kind}" == graphics || "${kind}" == mrt || "${kind}" == submissions || "${kind}" == submissions_unretained || "${kind}" == heap_payload || "${kind}" == async_payload || "${kind}" == alias || "${source_kind}" == indexed ]]; then
    va_table="$(python3 - "${LOG_DIR}/${kind}-capture.log" <<'PY3'
import re, sys
from pathlib import Path
print(re.search(r'VA_TABLE=(\d+)', Path(sys.argv[1]).read_text()).group(1))
PY3
    )"
  fi
  if [[ "${kind}" == resources || "${kind}" == sourced_resources || "${kind}" == sourced_frame || "${kind}" == graphics || "${kind}" == mrt || "${kind}" == submissions || "${kind}" == submissions_unretained || "${kind}" == heap_payload || "${kind}" == async_payload || "${kind}" == alias || "${source_kind}" == indexed ]]; then
    read -r tex_a tex_b < <(python3 - "${LOG_DIR}/${kind}-capture.log" <<'PY4'
import re, sys
from pathlib import Path
print(*re.search(r'TEX=(\d+) SAMP=(\d+)', Path(sys.argv[1]).read_text()).groups())
PY4
    )
  fi
  if [[ "${kind}" == frame_view ]]; then
    read -r tex_a tex_b < <(python3 - "${LOG_DIR}/${kind}-capture.log" <<'PY2'
import re, sys
from pathlib import Path
match = re.search(r'TEX_A=(\d+) TEX_B=(\d+)', Path(sys.argv[1]).read_text())
if not match: raise SystemExit('Missing native texture identities')
print(*match.groups())
PY2
    )
  fi
  for suffix in '' '_2'; do
    if [[ "${kind}" == table ]]; then
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    elif [[ "${kind}" == alias ]]; then
      before=122; after=225
      if [[ -n "${suffix}" ]]; then before=186; after=161; fi
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" "$va_table" "$tex_a" "$tex_b" "$before" "$after" >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    elif [[ "${kind}" == async_payload ]]; then
      before=122; pixel=186
      if [[ -n "${suffix}" ]]; then before=186; pixel=122; fi
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" "$va_table" "$tex_a" "$tex_b" "$before" 308 "$pixel" >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    elif [[ "${kind}" == sourced_frame ]]; then
      before=122; after=186
      if [[ -n "${suffix}" ]]; then before=186; after=122; fi
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" "$va_table" "$tex_a" "$tex_b" "$before" "$after" frame >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    elif [[ "${kind}" == sourced_resources || "${kind}" == graphics || "${kind}" == mrt || "${kind}" == submissions || "${kind}" == submissions_unretained || "${kind}" == heap_payload || "${kind}" == async_payload || "${kind}" == alias || "${source_kind}" == indexed ]]; then
      before=122; after=186
      if [[ -n "${suffix}" ]]; then before=186; after=122; fi
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" "$va_table" "$tex_a" "$tex_b" "$before" "$after" >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    elif [[ "${kind}" == resources ]]; then
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" "$va_table" "$tex_a" "$tex_b" >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    elif [[ "${kind}" == sourced_compute ]]; then
      before=41; after=80
      if [[ -n "${suffix}" ]]; then before=80; after=41; fi
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" "$va_b" "$va_table" "$before" "$after" compute >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    elif [[ "${kind}" == sourced_gpu ]]; then
      before=41; after=80
      if [[ -n "${suffix}" ]]; then before=80; after=41; fi
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" "$va_b" "$va_table" "$before" "$after" >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    elif [[ "${kind}" == shadow ]]; then
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" "$va_b" "$va_table" >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    elif [[ "${kind}" == frame_view ]]; then
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" "$va_b" "$tex_a" "$tex_b" >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    else
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/${kind}_replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "$va_a" "$va_b" >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    fi
  done
  echo "PASS ${kind}: native, two captures, GPU bytes and event seeks"
done
python3 util/test/metal/metal_descriptor_table_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/table_replay" "${LOG_DIR}/table-capture.log" \
  "${LOG_DIR}/table_capture.rdc" "${LOG_DIR}/table-gate" >"${LOG_DIR}/table-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_gpu_update_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/gpu_update_capture.rdc" "${LOG_DIR}/gpu-gate" \
  >"${LOG_DIR}/gpu-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_frame_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/frame_capture.rdc" "${LOG_DIR}/frame-gate" \
  >"${LOG_DIR}/frame-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_frame_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/frame_private_capture.rdc" "${LOG_DIR}/frame-private-gate" \
  >"${LOG_DIR}/frame-private-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_frame_view_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/frame_view_capture.rdc" "${LOG_DIR}/frame-view-gate" \
  >"${LOG_DIR}/frame-view-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_inline_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/inline_capture.rdc" "${LOG_DIR}/inline-gate" \
  >"${LOG_DIR}/inline-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_shadow_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/shadow_capture.rdc" "${LOG_DIR}/shadow-gate" \
  >"${LOG_DIR}/shadow-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_sourced_gpu_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/sourced_gpu_capture.rdc" "${LOG_DIR}/sourced-gpu-gate" \
  >"${LOG_DIR}/sourced-gpu-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_sourced_gpu_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/sourced_compute_capture.rdc" "${LOG_DIR}/sourced-compute-gate" compute \
  >"${LOG_DIR}/sourced-compute-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_resources_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/resources_capture.rdc" "${LOG_DIR}/resources-gate" \
  >"${LOG_DIR}/resources-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_sourced_gpu_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/sourced_resources_capture.rdc" "${LOG_DIR}/sourced-resources-gate" compute \
  >"${LOG_DIR}/sourced-resources-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_sourced_gpu_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/sourced_frame_capture.rdc" "${LOG_DIR}/sourced-frame-gpu-gate" compute \
  >"${LOG_DIR}/sourced-frame-gpu-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_sourced_frame_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/sourced_frame_capture.rdc" "${LOG_DIR}/sourced-frame-gate" \
  >"${LOG_DIR}/sourced-frame-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/graphics_capture.rdc" "${LOG_DIR}/graphics-gate" \
  >"${LOG_DIR}/graphics-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/mrt_capture.rdc" "${LOG_DIR}/mrt-graphics-gate" \
  >"${LOG_DIR}/mrt-graphics-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_mrt_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/mrt_capture.rdc" "${LOG_DIR}/mrt-gate" \
  >"${LOG_DIR}/mrt-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/submissions_capture.rdc" "${LOG_DIR}/submissions-graphics-gate" \
  >"${LOG_DIR}/submissions-graphics-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_submissions_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/submissions_capture.rdc" "${LOG_DIR}/submissions-gate" \
  >"${LOG_DIR}/submissions-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/submissions_unretained_capture.rdc" "${LOG_DIR}/unretained-graphics-gate" \
  >"${LOG_DIR}/unretained-graphics-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_submissions_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/submissions_unretained_capture.rdc" "${LOG_DIR}/unretained-gate" \
  >"${LOG_DIR}/unretained-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/heap_payload_capture.rdc" "${LOG_DIR}/heap-graphics-gate" \
  >"${LOG_DIR}/heap-graphics-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_submissions_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/heap_payload_capture.rdc" "${LOG_DIR}/heap-submissions-gate" \
  >"${LOG_DIR}/heap-submissions-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_heap_payload_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/heap_payload_capture.rdc" "${LOG_DIR}/heap-gate" \
  >"${LOG_DIR}/heap-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/async_payload_capture.rdc" "${LOG_DIR}/async-graphics-gate" \
  >"${LOG_DIR}/async-graphics-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_async_payload_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/async_payload_capture.rdc" "${LOG_DIR}/async-gate" \
  >"${LOG_DIR}/async-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_alias_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/alias_capture.rdc" "${LOG_DIR}/alias-gate" \
  >"${LOG_DIR}/alias-gate.log" 2>&1
python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/alias_capture.rdc" "${LOG_DIR}/alias-graphics-gate" \
  >"${LOG_DIR}/alias-graphics-gate.log" 2>&1
for kind in indexed indexed_wide; do
  python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
    "${LOG_DIR}/open_probe" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-gate" \
    >"${LOG_DIR}/${kind}-gate.log" 2>&1
done
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. \
  util/test/metal/metal_descriptor_slot_capture.mm -framework Foundation -framework Metal \
  -framework QuartzCore -o "${LOG_DIR}/slot_capture"
MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/slots" \
  "${LOG_DIR}/slot_capture" >"${LOG_DIR}/slot-capture.log" 2>&1
python3 util/test/metal/metal_descriptor_slot_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
  "${LOG_DIR}/open_probe" "${LOG_DIR}/slots_capture.rdc" "${LOG_DIR}/slot-gate" \
  >"${LOG_DIR}/slot-gate.log" 2>&1
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo "PASS typed descriptors: twenty-one variants, UE Buffer/Texture/Sampler enums, sourced slot generations and inline VAs, frame buffer/table births, partial CPU diffs, GPU buffer/texture descriptor copies, vertex/fragment descriptor consumption, MRT and cross-pass pixels/scopes, waited CPU updates between two submissions, frame heap payloads, async same-queue CPU/GPU snapshots, explicitly retired equal-address heap aliases, UE IR draw scalar constants and UInt16/UInt32 indexed draws, Shared/Private placement/views, 674 API+CLI negative groups; frozen slot snapshots and diagnostic refusal"
echo "Tiny fixtures only; this is not full regression or UE/UI acceptance."
