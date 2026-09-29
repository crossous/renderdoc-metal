#!/bin/bash
# One bounded API open, then one CLI replay only if API open succeeds.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-private-initial-viewer}"
LIBRARY="${BUILD_DIR}/lib/librenderdoc.dylib"
CLI="${BUILD_DIR}/bin/renderdoccmd"
CAPTURE="${1:-}"
TIMEOUT="${UE_METAL_REPLAY_TIMEOUT_SECONDS:-75}"
MAX_RSS_MIB="${UE_METAL_REPLAY_MAX_RSS_MIB:-4096}"

if [[ $# != 1 || ! -f "${CAPTURE}" || "${CAPTURE}" != *.rdc ]]; then
  echo "Usage: RENDERDOC_METAL_BUILD_DIR=<build> bash $0 <capture.rdc>" >&2
  exit 2
fi
if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ||
      ! -f "${LIBRARY}" || ! -x "${CLI}" ]]; then
  echo "Requires Apple Silicon macOS and a built Metal replay library/renderdoccmd" >&2
  exit 2
fi
if [[ ! "${TIMEOUT}" =~ ^[1-9][0-9]*$ || ! "${MAX_RSS_MIB}" =~ ^[1-9][0-9]*$ ]]; then
  echo "Timeout and RSS limit must be positive integers" >&2
  exit 2
fi
CAPTURE="$(cd "$(dirname "${CAPTURE}")" && pwd)/$(basename "${CAPTURE}")"
BUILD_DIR="$(cd "${BUILD_DIR}" && pwd)"
LIBRARY="${BUILD_DIR}/lib/librenderdoc.dylib"
CLI="${BUILD_DIR}/bin/renderdoccmd"

LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/rdm-ue-replay.XXXXXX")"
PROBE="${LOG_DIR}/ue_capture_open_probe"
{
  echo "capture=${CAPTURE}"
  echo "build_dir=${BUILD_DIR}"
  echo "repo_head=$(git -C "${REPO_ROOT}" rev-parse HEAD)"
  echo "macos=$(sw_vers -productVersion)"
  echo "arch=$(uname -m)"
  echo "timeout_seconds=${TIMEOUT}"
  echo "max_rss_mib=${MAX_RSS_MIB}"
  echo "api_command=MTL_DEBUG_LAYER=1 <compiled-probe> <capture>"
  echo "cli_command=MTL_DEBUG_LAYER=1 ${CLI} replay --loops 1 <capture>"
  shasum -a 256 "${CAPTURE}" "${LIBRARY}" "${CLI}"
} > "${LOG_DIR}/manifest.txt"
echo "Replay logs: ${LOG_DIR}"

clang++ -std=c++17 -arch arm64 -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${SCRIPT_DIR}/ue_capture_open_probe.cpp" -L"${BUILD_DIR}/lib" -lrenderdoc \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${PROBE}" \
  >"${LOG_DIR}/build-probe.log" 2>&1 || {
    echo "API probe build failed: ${LOG_DIR}/build-probe.log" >&2
    exit 3
  }
shasum -a 256 "${PROBE}" >> "${LOG_DIR}/manifest.txt"

python3 - "${LOG_DIR}" "${TIMEOUT}" "${MAX_RSS_MIB}" \
  "${PROBE}" "${CLI}" "${CAPTURE}" <<'PY'
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

log_dir, timeout_s, max_rss_mib, probe, cli, capture = sys.argv[1:]
timeout_s = int(timeout_s)
max_rss_kib = int(max_rss_mib) * 1024

def run_once(name, command):
    path = Path(log_dir) / f"{name}.log"
    renderdoc_path = Path(log_dir) / f"{name}.renderdoc.log"
    env = os.environ.copy()
    env["MTL_DEBUG_LAYER"] = "1"
    env["RENDERDOC_DEBUG_LOG_FILE"] = str(renderdoc_path)
    with path.open("wb") as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                   env=env, start_new_session=True)
        deadline = time.monotonic() + timeout_s
        peak_kib = 0
        reason = None
        while process.poll() is None:
            if time.monotonic() >= deadline:
                reason = "timeout"
                break
            sample = subprocess.run(["ps", "-o", "rss=", "-p", str(process.pid)],
                                    capture_output=True, text=True)
            if sample.returncode == 0 and sample.stdout.strip().isdigit():
                peak_kib = max(peak_kib, int(sample.stdout.strip()))
                if peak_kib > max_rss_kib:
                    reason = "rss_limit"
                    break
            time.sleep(0.2)
        if reason:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
        code = process.returncode
    print(f"{name}: exit={code} reason={reason or 'completed'} peak_rss_mib={peak_kib / 1024:.1f} log={path} renderdoc_log={renderdoc_path}", flush=True)
    with (Path(log_dir) / "result.txt").open("a") as result:
        result.write(f"{name}: exit={code} reason={reason or 'completed'} "
                     f"peak_rss_mib={peak_kib / 1024:.1f}\n")
    return code, reason

api_code, api_reason = run_once("api", [probe, capture])
if api_reason or api_code != 0:
    print("Stopping after API failure; CLI was not run to avoid repeating the GPU workload.", flush=True)
    sys.exit(1)
cli_code, cli_reason = run_once("cli", [cli, "replay", "--loops", "1", capture])
sys.exit(0 if api_code == 0 and cli_code == 0 and not cli_reason else 1)
PY
