#!/bin/bash
# Builds the one-shot native probe. Pass --run only on a host approved for GPU testing.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [[ "$(uname -s)" != Darwin ]]; then
  echo "This probe requires macOS" >&2
  exit 2
fi

OUTPUT_DIR="${METAL_PURGEABLE_PROBE_OUTPUT_DIR:-${TMPDIR:-/tmp}/metal-purgeable-completion}"
mkdir -p "${OUTPUT_DIR}"
BINARY="${OUTPUT_DIR}/metal_purgeable_completion_probe"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=12.0 -I"${SCRIPT_DIR}/../../.." \
  -framework Foundation -framework Metal -framework QuartzCore \
  "${SCRIPT_DIR}/metal_purgeable_completion_probe.mm" -o "${BINARY}"
shasum -a 256 "${SCRIPT_DIR}/metal_purgeable_completion_probe.mm" "${BINARY}"

if [[ "${1:-}" != --run ]]; then
  echo "Compiled only. Run this script with --run on the selected GPU test host."
  exit 0
fi

python3 - "${BINARY}" "${OUTPUT_DIR}/run.log" <<'PY'
import os
import signal
import subprocess
import sys

binary, log_path = sys.argv[1:]
env = os.environ.copy()
env["MTL_DEBUG_LAYER"] = "1"
with open(log_path, "wb") as log:
    process = subprocess.Popen([binary], stdout=log, stderr=subprocess.STDOUT,
                               env=env, start_new_session=True)
    try:
        code = process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()
        code = 124
print(f"native Metal Validation probe exit={code} log={log_path}")
sys.exit(code)
PY
