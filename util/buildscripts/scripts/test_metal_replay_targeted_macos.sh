#!/bin/bash
# Fast replay-only checks on explicitly selected captures; no Qt/app build or recapture.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"

usage() {
  echo "Usage: bash $0 [--sentinel] [tNN/tNNN ... | t10_debug]"
  echo "Explicit captures are required unless --sentinel is used. Duplicates run once."
  echo "Sentinels: t01 t02 t09 t11 t12 t35 t49 t52 t53 (append affected/new fixtures)."
  echo "Runs incremental CLI/library build, Metal-validation API checks and one-loop CLI replay."
  echo "Does not run native/capture, negative mutations, lifecycle, or GUI acceptance."
}

fixtures=()
for argument in "$@"; do
  case "$argument" in
    --help|-h) usage; exit 0 ;;
    --sentinel) fixtures+=(t01 t02 t09 t11 t12 t35 t49 t52 t53) ;;
    *)
      if [[ "$argument" =~ ^t[0-9][0-9]([0-9])?$ || "$argument" == t10_debug ]]; then
        fixtures+=("$argument")
      else
        echo "Invalid fixture or option: $argument" >&2
        usage >&2
        exit 2
      fi ;;
  esac
done
if (( ${#fixtures[@]} == 0 )); then usage >&2; exit 2; fi

selected=()
for fixture in "${fixtures[@]}"; do
  duplicate=false
  if (( ${#selected[@]} > 0 )); then
    for existing in "${selected[@]}"; do
      if [[ "$existing" == "$fixture" ]]; then duplicate=true; break; fi
    done
  fi
  if "$duplicate"; then continue; fi
  if [[ ! -f "${CAPTURE_DIR}/${fixture}_capture.rdc" ]]; then
    echo "Missing capture: ${CAPTURE_DIR}/${fixture}_capture.rdc" >&2
    exit 2
  fi
  selected+=("$fixture")
done
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
  echo "Configure the development build first with build_metal_dev_macos.sh." >&2
  exit 2
fi

LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-targeted.XXXXXX")"
echo "Selected replay checks: ${selected[*]}"
echo "Logs: ${LOG_DIR}"
run_step() {
  local step="$1" timeout_seconds="$2" result=0
  shift 2
  python3 - "$timeout_seconds" "$@" >"${LOG_DIR}/${step}.log" 2>&1 <<'PY' || result=$?
import os
import signal
import subprocess
import sys

process = subprocess.Popen(sys.argv[2:], start_new_session=True)
try:
    code = process.wait(timeout=int(sys.argv[1]))
except subprocess.TimeoutExpired:
    os.killpg(process.pid, signal.SIGKILL)
    process.wait()
    print('FAIL: timed out; terminated this test process group', file=sys.stderr)
    sys.exit(124)
if code < 0:
    print(f'FAIL: terminated by signal {-code}', file=sys.stderr)
sys.exit(code if code >= 0 else 1)
PY
  if (( result != 0 )); then
    echo "FAIL ${step} (exit ${result}); log: ${LOG_DIR}/${step}.log" >&2
    tail -n 40 "${LOG_DIR}/${step}.log" >&2
    exit "$result"
  fi
  echo "PASS ${step}"
}

run_step build 600 cmake --build "${BUILD_DIR}" --target renderdoccmd -j "$(sysctl -n hw.ncpu)"
# Compile against current headers every run; do not accidentally reuse a stale API helper.
run_step api-helper 180 clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/metal_replay_output_smoke"
run_step library-hash 30 shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib"
for fixture in "${selected[@]}"; do
  capture="${CAPTURE_DIR}/${fixture}_capture.rdc"
  run_step "${fixture}-api" 120 env MTL_DEBUG_LAYER=1 \
    "${LOG_DIR}/metal_replay_output_smoke" "$capture" "${LOG_DIR}/${fixture}.ppm"
  run_step "${fixture}-cli" 120 "${BUILD_DIR}/bin/renderdoccmd" replay --loops 1 "$capture"
done
echo "Targeted replay passed: ${#selected[@]} captures. NOT a full regression or GUI acceptance."
