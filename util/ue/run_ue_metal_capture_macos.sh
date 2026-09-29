#!/bin/bash
# Run the installed UE editor with this checkout's Metal capture library.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PROJECT="${UE_METAL_PROJECT:-${HOME}/Documents/Unreal Projects/SocoTestProj/SocoTestProj.uproject}"
ENGINE="${UE_METAL_ENGINE:-/Users/Shared/Epic Games/UE_5.8/Engine}"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-private-initial-viewer}"
LIBRARY="${BUILD_DIR}/lib/librenderdoc.dylib"
MODE="${1:---check}"

if [[ "${MODE}" != "--check" && "${MODE}" != "--run" ]]; then
  echo "Usage: $0 [--check|--run]" >&2
  exit 2
fi
if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
  echo "Requires Apple Silicon macOS" >&2
  exit 2
fi
if [[ ! -f "${PROJECT}" || ! -d "${ENGINE}" || ! -f "${LIBRARY}" ]]; then
  echo "Missing project, UE Engine, or RenderDoc library:" >&2
  printf '  project: %s\n  engine: %s\n  library: %s\n' "${PROJECT}" "${ENGINE}" "${LIBRARY}" >&2
  exit 2
fi
# dyld reports the physical path via dladdr(). Canonicalize the injected library before
# passing it to the project plugin, so /tmp and /private/tmp name the same dylib.
LIBRARY="$(cd "$(dirname "${LIBRARY}")" && pwd -P)/$(basename "${LIBRARY}")"
EDITOR="${ENGINE}/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
PLUGIN="$(dirname "${PROJECT}")/Plugins/RenderDocMetalCapture/RenderDocMetalCapture.uplugin"
if [[ ! -x "${EDITOR}" || ! -f "${PLUGIN}" ]]; then
  echo "Editor or project-level RenderDocMetalCapture plugin is missing" >&2
  exit 2
fi

SESSION_ROOT="$(dirname "${PROJECT}")/Saved/RenderDocMetalSessions"
CAPTURE_DIR="$(dirname "${PROJECT}")/Saved/RenderDocMetalCaptures"
mkdir -p "${SESSION_ROOT}" "${CAPTURE_DIR}"
SESSION="${SESSION_ROOT}/$(date +%Y%m%d-%H%M%S)"
mkdir "${SESSION}"

{
  echo "repo_commit=$(git -C "${REPO_ROOT}" rev-parse HEAD)"
  echo "project=${PROJECT}"
  echo "engine=${ENGINE}"
  echo "editor=${EDITOR}"
  echo "renderdoc_library=${LIBRARY}"
  echo "capture_dir=${CAPTURE_DIR}"
  echo "ddc_mode=${UE_METAL_DDC_MODE:-InstalledNoZenLocalFallback}"
  echo "editor_args=${UE_METAL_EDITOR_ARGS:-}"
  echo "startup_timeout_seconds=${UE_METAL_TIMEOUT_SECONDS:-7200}"
  echo "auto_capture_delay_seconds=${UE_METAL_AUTO_CAPTURE_DELAY_SECONDS:-disabled}"
  echo "macos=$(sw_vers -productVersion)"
  echo "arch=$(uname -m)"
  echo "ue_version=$(tr -d '\n' < "${ENGINE}/Build/Build.version")"
  shasum -a 256 "${LIBRARY}" "${EDITOR}" "${PROJECT}"
  codesign -dv --verbose=2 "${EDITOR}" 2>&1 | rg 'Identifier=|flags=' || true
} > "${SESSION}/manifest.txt"

echo "Session: ${SESSION}"
echo "Capture output: ${CAPTURE_DIR}"
echo "Library SHA256: $(shasum -a 256 "${LIBRARY}" | cut -d ' ' -f 1)"
if [[ "${MODE}" == "--check" ]]; then
  echo "Preflight only. Use --run to launch UE."
  exit 0
fi

echo "Launching UE. Capture one frame with the viewport button, then close the editor."
echo "UE output: ${SESSION}/ue-stdout.log"
echo "First launch after changing Metal shader settings may spend a long time compiling shaders before a window appears."
echo "Startup timeout: ${UE_METAL_TIMEOUT_SECONDS:-7200}s (override with UE_METAL_TIMEOUT_SECONDS)."
python3 - "${EDITOR}" "${PROJECT}" "${LIBRARY}" "${CAPTURE_DIR}" "${SESSION}" \
  "${UE_METAL_TIMEOUT_SECONDS:-7200}" "${UE_METAL_EDITOR_ARGS:-}" <<'PY'
import os
from pathlib import Path
import shlex
import signal
import subprocess
import sys
import time

editor, project, library, capture_dir, session, timeout, extra_args = sys.argv[1:]
env = os.environ.copy()
env.update({
    "DYLD_INSERT_LIBRARIES": library,
    "DYLD_PRINT_LIBRARIES": "1",
    "RENDERDOC_METAL_LIBRARY": library,
    "RENDERDOC_CAPFILE": str(Path(capture_dir) / "UE58"),
    "RENDERDOC_DEBUG_LOG_FILE": str(Path(session) / "renderdoc.log"),
})
# /usr/bin/arch is protected by SIP and strips DYLD_* before exec'ing UE.
# Apple Silicon selects the editor's arm64 slice directly on this host.
ddc_mode = os.environ.get("UE_METAL_DDC_MODE", "InstalledNoZenLocalFallback")
command = [editor, project, "-Metal", "-NoSplash", f"-ddc={ddc_mode}",
           f"-ABSLOG={Path(session) / 'ue-editor.log'}"] + shlex.split(extra_args)
print("Editor command:", " ".join(shlex.quote(arg) for arg in command), flush=True)
with (Path(session) / "ue-stdout.log").open("wb") as log:
    process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                               start_new_session=True)
    started = time.monotonic()
    deadline = started + int(timeout)
    next_status = started + 60
    editor_log = Path(session) / "ue-editor.log"
    checked_log_bytes = 0
    previous_log_tail = b""
    startup_complete = False

    def stop_editor():
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            return
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()

    while True:
        remaining = deadline - time.monotonic()
        if not startup_complete and remaining <= 0:
            print(f"UE exceeded {timeout}s; stopping process group", file=sys.stderr)
            stop_editor()
            sys.exit(124)
        try:
            code = process.wait(timeout=15 if startup_complete else min(15, remaining))
            break
        except subprocess.TimeoutExpired:
            if editor_log.exists():
                with editor_log.open("rb") as editor_output:
                    editor_output.seek(checked_log_bytes)
                    new_output = editor_output.read()
                    checked_log_bytes += len(new_output)
                if b"Shader compiler errors compiling global shaders" in previous_log_tail + new_output:
                    print("UE global shader compilation failed; stopping editor. "
                          f"Inspect {editor_log}", file=sys.stderr)
                    stop_editor()
                    sys.exit(65)
                if b"Engine is initialized. Leaving FEngineLoop::Init()" in previous_log_tail + new_output:
                    startup_complete = True
                    print("UE editor initialized; startup timeout is disabled. "
                          "Close the editor when capture is done.", flush=True)
                previous_log_tail = new_output[-64:]
            now = time.monotonic()
            if not startup_complete and now >= next_status:
                elapsed = int(now - started)
                print(f"UE still running after {elapsed}s; session: {session}", flush=True)
                next_status = now + 60
sys.exit(code if code >= 0 else 1)
PY
