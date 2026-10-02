#!/bin/bash
# Validate two live buffer objects sharing a placement heap without makeAliasable.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-async-alias.XXXXXX")"
PRIVATE_ALIAS="${RENDERDOC_METAL_PRIVATE_BUFFER_ALIAS:-0}"
echo "Async alias test logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. \
  util/test/metal/metal_descriptor_alias_capture.mm -framework Foundation \
  -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -arch "$(uname -m)" -mmacosx-version-min=13.0 \
  -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_graphics_replay.mm \
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
    print('Implicit alias tiny test exceeded 60 seconds', file=sys.stderr)
    sys.exit(124)
PYRUN
}
for kind in retained unretained background background_unretained; do
  capture_environment=(env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_IMPLICIT_BUFFER_ALIAS=1 RENDERDOC_METAL_ASYNC_BUFFER_ALIAS=1)
  if [[ "${RENDERDOC_METAL_ALIAS_CPU_BEFORE_COMMIT:-0}" == 1 ]]; then capture_environment+=(RENDERDOC_METAL_ALIAS_CPU_BEFORE_COMMIT=1); fi
  if [[ "${RENDERDOC_METAL_ALIAS_BEFORE_COMMIT:-0}" == 1 ]]; then capture_environment+=(RENDERDOC_METAL_ALIAS_BEFORE_COMMIT=1); fi
  if [[ "${PRIVATE_ALIAS}" == 1 ]]; then capture_environment+=(RENDERDOC_METAL_PRIVATE_BUFFER_ALIAS=1); fi
  if [[ "${kind}" == *unretained ]]; then capture_environment+=(RENDERDOC_METAL_UNRETAINED_SUBMISSIONS=1 RENDERDOC_METAL_IMPLICIT_ALIAS_LENGTH=224); fi
  if [[ "${kind}" == background* ]]; then capture_environment+=(RENDERDOC_METAL_BACKGROUND_BUFFER_ALIAS=1); fi
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
    before=122; pixel=225
    if [[ -n "${suffix}" ]]; then before=186; pixel=161; fi
    if [[ "${RENDERDOC_METAL_ALIAS_CPU_BEFORE_COMMIT:-0}" == 1 ]]; then
      before=184;if [[ -n "${suffix}" ]]; then before=248; fi
    fi
    replay_args=("$va_a" "$va_table" "$tex_id" "$sampler_id" "$before" "$pixel")
    if [[ "${PRIVATE_ALIAS}" == 1 && "${kind}" != background* ]]; then replay_args+=(private-frame); fi
    if [[ "${RENDERDOC_METAL_ALIAS_CPU_BEFORE_COMMIT:-0}" == 1 ]]; then replay_args+=(future-alias-cpu); fi
    run_bounded env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_REPLAY_WAITS=1 RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 \
      "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
      "${replay_args[@]}" \
      >"${LOG_DIR}/${kind}-bytes${suffix}.log" 2>&1
    if [[ "${RENDERDOC_METAL_ALIAS_CPU_BEFORE_COMMIT:-0}" == 1 ]]; then
      python3 util/test/metal/metal_future_alias_snapshot_probe.py "${BUILD_DIR}/bin/renderdoccmd" \
        "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" \
        "${LOG_DIR}/${kind}-future-only${suffix}" "$va_a" "$va_table" "$tex_id" "$sampler_id" "$before" "$pixel" \
        >"${LOG_DIR}/${kind}-future-only${suffix}.log" 2>&1
    fi
  done
  python3 util/test/metal/metal_descriptor_async_alias_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
    "${LOG_DIR}/open_probe" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-command-gate" \
    >"${LOG_DIR}/${kind}-command-gate.log" 2>&1
  python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
    "${LOG_DIR}/open_probe" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-graphics-gate" \
    >"${LOG_DIR}/${kind}-graphics-gate.log" 2>&1
done
negative_groups=200
if [[ "${PRIVATE_ALIAS}" == 1 ]]; then negative_groups=$((negative_groups+16)); fi
if [[ "${RENDERDOC_METAL_ALIAS_BEFORE_COMMIT:-0}" == 1 ]]; then negative_groups=$((negative_groups+4)); fi
values="122-to-225 and 186-to-161"
if [[ "${RENDERDOC_METAL_ALIAS_CPU_BEFORE_COMMIT:-0}" == 1 ]]; then values="first reads 184/248, final pixels 225/161, plus 8 future-only snapshot captures and 32 additional seek cycles"; fi
echo "PASS async tracked placement aliases: frame/background retained/unretained, 8 captures, 32 seek cycles, write through B/read through live A descriptor, GPU values/pixels ${values}, ${negative_groups} API+CLI negative groups"
