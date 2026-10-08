#!/bin/bash
# Run the installed UE editor with this checkout's Metal capture library.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PROJECT="${UE_METAL_PROJECT:-${HOME}/Documents/Unreal Projects/SocoTestProj/SocoTestProj.uproject}"
ENGINE="${UE_METAL_ENGINE:-/Users/Shared/Epic Games/UE_5.8/Engine}"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-private-initial-viewer}"
LIBRARY="${BUILD_DIR}/lib/librenderdoc.dylib"
RHI_OVERRIDE="${UE_METAL_RHI_OVERRIDE:-}"
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
if [[ -n "${RHI_OVERRIDE}" ]]; then
  python3 - "${RHI_OVERRIDE}" <<'PY'
import hashlib, json, sys
from pathlib import Path
library = Path(sys.argv[1]).resolve()
manifest = library.parent / 'build-manifest.json'
if library.name != 'libUnrealEditor-MetalRHI.dylib' or not manifest.is_file():
    raise SystemExit('RHI override requires an isolated build and build-manifest.json')
record = json.loads(manifest.read_text())
if record.get('missing_exports') or record['output_sha256'] != hashlib.sha256(library.read_bytes()).hexdigest():
    raise SystemExit('RHI override does not match its verified build manifest')
PY
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
  echo "metal_rhi_override=${RHI_OVERRIDE:-installed}"
  echo "capture_dir=${CAPTURE_DIR}"
  echo "ddc_mode=${UE_METAL_DDC_MODE:-InstalledNoZenLocalFallback}"
  echo "editor_args=${UE_METAL_EDITOR_ARGS:-}"
  echo "startup_timeout_seconds=${UE_METAL_TIMEOUT_SECONDS:-7200}"
  echo "auto_capture_delay_seconds=${UE_METAL_AUTO_CAPTURE_DELAY_SECONDS:-disabled}"
  echo "exit_after_capture=${UE_METAL_EXIT_AFTER_CAPTURE:-0}"
  echo "capture_viewport_size=${UE_METAL_CAPTURE_VIEWPORT_WIDTH:-default}x${UE_METAL_CAPTURE_VIEWPORT_HEIGHT:-default}"
  echo "capture_window_size=${UE_METAL_CAPTURE_WINDOW_WIDTH:-default}x${UE_METAL_CAPTURE_WINDOW_HEIGHT:-default}"
  echo "macos=$(sw_vers -productVersion)"
  echo "arch=$(uname -m)"
  echo "ue_version=$(tr -d '\n' < "${ENGINE}/Build/Build.version")"
  shasum -a 256 "${LIBRARY}" "${EDITOR}" "${PROJECT}"
  if [[ -n "${RHI_OVERRIDE}" ]]; then shasum -a 256 "${RHI_OVERRIDE}"; fi
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
exec python3 "${SCRIPT_DIR}/ue_metal_capture_supervisor.py" \
  "${EDITOR}" "${PROJECT}" "${LIBRARY}" "${CAPTURE_DIR}" "${SESSION}" \
  "${UE_METAL_TIMEOUT_SECONDS:-7200}" "${UE_METAL_EDITOR_ARGS:-}"
