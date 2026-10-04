#!/bin/bash
# Serial targeted inspection checks. Close UE/qrenderdoc before running GPU probes.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-shader-inspection.XXXXXX")"
cd "${REPO_ROOT}"
echo "Shader inspection logs: ${LOG_DIR}"
clang++ -std=c++17 -I. util/test/metal/metal_air_access_test.cpp -o "${LOG_DIR}/air-access-test"
"${LOG_DIR}/air-access-test" >"${LOG_DIR}/parser.log"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. \
  util/test/metal/metal_replay_output_smoke.mm -L"${BUILD_DIR}/lib" -lrenderdoc \
  -framework Cocoa -framework QuartzCore -framework Metal -Wl,-rpath,"${BUILD_DIR}/lib" \
  -o "${LOG_DIR}/output-smoke"
for fixture in t01 t12 t40 t47 t48; do
  RENDERDOC_METAL_TEST_AIR=1 MTL_DEBUG_LAYER=1 "${LOG_DIR}/output-smoke" \
    "captures/metal-smoke/${fixture}_capture.rdc" "${LOG_DIR}/${fixture}.ppm" \
    >"${LOG_DIR}/${fixture}.log" 2>&1
done
# Optional: immediately return to a real capture and an explicit event.
if [[ $# -gt 0 ]]; then
  if [[ $# -ne 2 ]]; then echo "Usage: $0 [capture.rdc EID]" >&2; exit 2; fi
  clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_metal_replay_event_probe.cpp \
    -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/event-probe"
  "${LOG_DIR}/event-probe" "$1" "$LOG_DIR" "$2" --inspect-shaders >"${LOG_DIR}/real-ue.log" 2>&1
fi
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS targeted shader inspection; see individual logs for actual input resources and AIR'
