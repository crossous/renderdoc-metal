#!/bin/bash
# Validate expired sources retired in a leading free prefix without GPU consumption.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-retirement.XXXXXX")"
echo "Retirement test logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. \
  util/test/metal/metal_descriptor_async_payload_capture.mm -framework Foundation \
  -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -arch "$(uname -m)" -mmacosx-version-min=13.0 \
  -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_async_payload_replay.mm \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Foundation -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=13.0 \
  -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/open_probe"
run_bounded() {
  python3 - "$@" <<'PYRUN'
import subprocess, sys
try:
    sys.exit(subprocess.run(sys.argv[1:], timeout=60).returncode)
except subprocess.TimeoutExpired:
    print('Retirement tiny test exceeded 60 seconds', file=sys.stderr)
    sys.exit(124)
PYRUN
}
for kind in cpu gpu_writes; do
  capture_environment=(env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_PRELUDE_RETIREMENT=1)
  if [[ "${RENDERDOC_METAL_MIXED_PRELUDE_RETIREMENT:-0}" == 1 ]]; then capture_environment+=(RENDERDOC_METAL_MIXED_PRELUDE_RETIREMENT=1); fi
  if [[ "${RENDERDOC_METAL_BLIT_PRELUDE_RETIREMENT:-0}" == 1 ]]; then capture_environment+=(RENDERDOC_METAL_BLIT_PRELUDE_RETIREMENT=1); fi
  if [[ "${RENDERDOC_METAL_TEXTURE_PRELUDE_RETIREMENT:-0}" == 1 ]]; then capture_environment+=(RENDERDOC_METAL_TEXTURE_PRELUDE_RETIREMENT=1); fi
  if [[ "${RENDERDOC_METAL_MANY_PRELUDE_RETIREMENTS:-0}" == 1 ]]; then capture_environment+=(RENDERDOC_METAL_MANY_PRELUDE_RETIREMENTS=1); fi
  if [[ "${kind}" == gpu_writes ]]; then capture_environment+=(RENDERDOC_METAL_PRELUDE_GPU_WRITES=1); fi
  run_bounded "${capture_environment[@]}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
  run_bounded "${capture_environment[@]}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" \
    "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
  read -r va_a va_table tex_id sampler_id < <(python3 - "${LOG_DIR}/${kind}-capture.log" <<'PY'
import re, sys
from pathlib import Path
text=Path(sys.argv[1]).read_text()
assert 'captures=2' in text, text
print(*(re.search(name+r'=(\d+)',text).group(1) for name in ['VA_A','VA_TABLE','TEX','SAMP']))
PY
  )
  for suffix in '' '_2'; do
    before=122; pixel=186
    if [[ -n "${suffix}" ]]; then before=186; pixel=122; fi
    run_bounded env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_REPLAY_WAITS=1 RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 \
      "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
      "$va_a" "$va_table" "$tex_id" "$sampler_id" "$before" 308 "$pixel" \
      >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
  done
  python3 util/test/metal/metal_descriptor_retirement_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
    "${LOG_DIR}/open_probe" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-command-gate" \
    >"${LOG_DIR}/${kind}-command-gate.log" 2>&1
  python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
    "${LOG_DIR}/open_probe" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-graphics-gate" \
    >"${LOG_DIR}/${kind}-graphics-gate.log" 2>&1
done
negative_groups=90
if [[ "${RENDERDOC_METAL_MIXED_PRELUDE_RETIREMENT:-0}" == 1 ]]; then negative_groups=92; fi
if [[ "${RENDERDOC_METAL_BLIT_PRELUDE_RETIREMENT:-0}" == 1 ]]; then negative_groups=112; fi
if [[ "${RENDERDOC_METAL_TEXTURE_PRELUDE_RETIREMENT:-0}" == 1 ]]; then negative_groups=128; fi
echo "PASS CPU/GPU-written table leading retirement: 4 captures, GPU accumulation 308, 16 seek cycles, pixels 186/122, stale VA/texture IDs cleared at EID0, ${negative_groups} API+CLI negative groups"
