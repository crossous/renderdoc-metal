#!/bin/bash
# Native, injected, API/CLI, seek and malformed checks for placement reuse.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-/tmp/rdm-t312-build}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
LOG_DIR="$(mktemp -d /tmp/rdm-placement-reuse.XXXXXX)"
mkdir -p "${CAPTURE_DIR}"
echo "Logs: ${LOG_DIR}"

run_step() {
  local name="$1"
  shift
  python3 - "${LOG_DIR}/${name}.log" "$@" <<'PY'
import os
import signal
import subprocess
import sys

log_path, *command = sys.argv[1:]
with open(log_path, 'wb') as log:
    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        code = process.wait(timeout=60)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()
        code = 124
if code:
    print(f'FAIL {log_path}: exit {code}', file=sys.stderr)
    print(open(log_path, errors='replace').read()[-4000:], file=sys.stderr)
    sys.exit(code if code > 0 else 1)
print(f'PASS {log_path}')
PY
}

run_step build-demos cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
run_step build-viewer cmake --build "${BUILD_DIR}" --target renderdoccmd build-qrenderdoc -j 8
run_step build-api clang++ -std=c++17 -arch arm64 -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore \
  -framework Metal -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/metal-api"

for fixture in t313 t314; do
  if [[ "$fixture" == t313 ]]; then
    feature=RENDERDOC_METAL_T133_ALIAS_REUSE_PROBE
  else
    feature=RENDERDOC_METAL_T133_RELEASE_REUSE_PROBE
  fi
  run_step "${fixture}-native" env MTL_DEBUG_LAYER=1 "${feature}=1" \
    "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
  run_step "${fixture}-capture" env MTL_DEBUG_LAYER=1 "${feature}=1" \
    RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/${fixture}" \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
  capture="${CAPTURE_DIR}/${fixture}_capture.rdc"
  run_step "${fixture}-api" env MTL_DEBUG_LAYER=1 "${LOG_DIR}/metal-api" \
    "$capture" "${LOG_DIR}/${fixture}.ppm"
  run_step "${fixture}-cli" env MTL_DEBUG_LAYER=1 \
    "${BUILD_DIR}/bin/renderdoccmd" replay --loops 1 "$capture"
  run_step "${fixture}-invalid" python3 \
    "${REPO_ROOT}/util/test/metal/metal_placement_reuse_invalid.py" \
    "${BUILD_DIR}/bin/renderdoccmd" "$capture"
done
run_step legacy-rejection python3 -c \
  'import subprocess,sys; r=subprocess.run([sys.argv[1],"replay","--loops","1",sys.argv[2]],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30); assert r.returncode==1 and "Failed to process Metal chunk MTLHeap::newBuffer(offset)" in r.stdout' \
  "${BUILD_DIR}/bin/renderdoccmd" "${CAPTURE_DIR}/t133_capture.rdc"
echo 'Placement reuse targeted terminal checks passed; no full regression or GUI QA.'
