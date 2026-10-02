#!/bin/bash
# Validate ordinary/shared signal-only events on sourced descriptor submissions.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-signals.XXXXXX")"
echo "Signal test logs: ${LOG_DIR}"
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
    print('Signal tiny test exceeded 60 seconds', file=sys.stderr)
    sys.exit(124)
PYRUN
}
for kind in event event_unretained shared shared_unretained; do
  capture_environment=(env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_SIGNALS_ONLY=1)
  if [[ "${kind}" == *unretained ]]; then capture_environment+=(RENDERDOC_METAL_UNRETAINED_SUBMISSIONS=1); fi
  if [[ "${kind}" == shared* ]]; then capture_environment+=(RENDERDOC_METAL_SHARED_SIGNALS=1); fi
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
  python3 util/test/metal/metal_descriptor_signals_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
    "${LOG_DIR}/open_probe" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-command-gate" \
    >"${LOG_DIR}/${kind}-command-gate.log" 2>&1
  python3 util/test/metal/metal_descriptor_graphics_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
    "${LOG_DIR}/open_probe" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-graphics-gate" \
    >"${LOG_DIR}/${kind}-graphics-gate.log" 2>&1
done
echo "PASS ordinary/shared signal-only retained/unretained: 8 captures, GPU accumulation 308, 32 seek cycles, pixels 186/122, shared initial host value 100 and increasing GPU values 101/102, 168 API+CLI negative groups"
