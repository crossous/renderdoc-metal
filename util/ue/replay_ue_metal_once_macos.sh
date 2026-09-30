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
API_ONLY="${UE_METAL_REPLAY_API_ONLY:-0}"

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
if [[ "${API_ONLY}" != 0 && "${API_ONLY}" != 1 ]]; then
  echo "UE_METAL_REPLAY_API_ONLY must be 0 or 1" >&2
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
  echo "max_physical_footprint_mib=${MAX_RSS_MIB}"
  echo "api_only=${API_ONLY}"
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

python3 - "${LOG_DIR}" "${TIMEOUT}" "${MAX_RSS_MIB}" "${API_ONLY}" \
  "${PROBE}" "${CLI}" "${CAPTURE}" <<'PY'
import ctypes
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import time

log_dir, timeout_s, max_rss_mib, api_only, probe, cli, capture = sys.argv[1:]
timeout_s = int(timeout_s)
max_rss_kib = int(max_rss_mib) * 1024
max_footprint_bytes = int(max_rss_mib) * 1024 * 1024
libproc = ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True)
libproc.proc_pid_rusage.argtypes = (ctypes.c_int, ctypes.c_int, ctypes.c_void_p)
libproc.proc_pid_rusage.restype = ctypes.c_int

def physical_footprint(pid):
    # rusage_info_v0.ri_phys_footprint is at byte 72 in macOS sys/resource.h.
    info = ctypes.create_string_buffer(128)
    if libproc.proc_pid_rusage(pid, 0, info) != 0:
        return None
    return struct.unpack_from("=Q", info.raw, 72)[0]

def run_once(name, command):
    path = Path(log_dir) / f"{name}.log"
    renderdoc_path = Path(log_dir) / f"{name}.renderdoc.log"
    progress_path = Path(log_dir) / f"{name}.progress.txt"
    env = os.environ.copy()
    env["MTL_DEBUG_LAYER"] = "1"
    env["RENDERDOC_DEBUG_LOG_FILE"] = str(renderdoc_path)
    with path.open("wb") as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                   env=env, start_new_session=True)
        deadline = time.monotonic() + timeout_s
        started = time.monotonic()
        peak_kib = 0
        peak_footprint_bytes = 0
        footprint_failures = 0
        reason = None
        progress_path.write_text(
            f"started pid={process.pid} elapsed_s=0 peak_rss_mib=0 "
            f"peak_footprint_mib=0\n")
        last_progress_second = -1
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
            footprint = physical_footprint(process.pid)
            if footprint is None:
                footprint_failures += 1
                if footprint_failures >= 5 and process.poll() is None:
                    reason = "footprint_unavailable"
                    break
            else:
                footprint_failures = 0
                peak_footprint_bytes = max(peak_footprint_bytes, footprint)
                if peak_footprint_bytes > max_footprint_bytes:
                    reason = "footprint_limit"
                    break
            elapsed_second = int(time.monotonic() - started)
            if elapsed_second != last_progress_second:
                progress_path.write_text(
                    f"running pid={process.pid} elapsed_s={elapsed_second} "
                    f"peak_rss_mib={peak_kib / 1024:.1f} "
                    f"peak_footprint_mib={peak_footprint_bytes / 1048576:.1f}\n")
                last_progress_second = elapsed_second
            time.sleep(0.2)
        if reason:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            except PermissionError:
                reason += "+term_denied"
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                except PermissionError:
                    reason += "+kill_denied"
                try:
                    process.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    reason += "+still_running"
        code = process.poll()
    progress_path.write_text(
        f"finished pid={process.pid} exit={code} reason={reason or 'completed'} "
        f"peak_rss_mib={peak_kib / 1024:.1f} "
        f"peak_footprint_mib={peak_footprint_bytes / 1048576:.1f}\n")
    print(f"{name}: exit={code} reason={reason or 'completed'} "
          f"peak_rss_mib={peak_kib / 1024:.1f} "
          f"peak_footprint_mib={peak_footprint_bytes / 1048576:.1f} "
          f"log={path} renderdoc_log={renderdoc_path}", flush=True)
    with (Path(log_dir) / "result.txt").open("a") as result:
        result.write(f"{name}: exit={code} reason={reason or 'completed'} "
                     f"peak_rss_mib={peak_kib / 1024:.1f} "
                     f"peak_footprint_mib={peak_footprint_bytes / 1048576:.1f}\n")
    return code, reason

api_code, api_reason = run_once("api", [probe, capture])
if api_reason or api_code != 0:
    print("Stopping after API failure; CLI was not run to avoid repeating the GPU workload.", flush=True)
    sys.exit(1)
if api_only == "1":
    print("API-only check passed; CLI was not run.", flush=True)
    sys.exit(0)
cli_code, cli_reason = run_once("cli", [cli, "replay", "--loops", "1", capture])
sys.exit(0 if api_code == 0 and cli_code == 0 and not cli_reason else 1)
PY
