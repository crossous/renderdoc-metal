#!/bin/bash
# Shared argument packets: native/capture, aggregate replay, explicit unsupported CPU re-encoding.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
cmake --build "${BUILD_DIR}" --target renderdoccmd -j "$(sysctl -n hw.ncpu)"
mkdir -p "${CAPTURE_DIR}"
MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Argument_Data --frames 6
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t57" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Argument_Data --frames 6
RENDERDOC_METAL_LAST_TEST=57 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
# Shared initial-state restoration also affects existing capture-side paths. Keep formal old
# fixtures unchanged; use separate compatibility captures and the freshly built API helper.
bash "${SCRIPT_DIR}/test_metal_source_library_compat_macos.sh"
MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Command_Creation_Variants --frames 6
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t57_compat_t35" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Command_Creation_Variants --frames 6
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t57_compat_t35_capture.rdc" "${CAPTURE_DIR}/t57_compat_t35.ppm"
"${BUILD_DIR}/bin/renderdoccmd" replay --loops 3 "${CAPTURE_DIR}/t57_compat_t35_capture.rdc"
RENDERDOC_METAL_T57_REENCODE=1 MTL_DEBUG_LAYER=1 \
  "${REPO_ROOT}/bin/demos_x64" Metal_Argument_Data --frames 3
RENDERDOC_METAL_T57_REENCODE=1 RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t57_reencode" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Argument_Data --frames 3
python3 - "${REPO_ROOT}" "${BUILD_DIR}/bin/renderdoccmd" "${CAPTURE_DIR}/t57_reencode_capture.rdc" <<'PY'
import pathlib
import sys
sys.path.insert(0,str(pathlib.Path(sys.argv[1])/'util/test/metal'))
from metal_compute_inline_invalid import run
message = run(sys.argv[2],'replay','--loops','1',sys.argv[3],success=False)
assert 'unsupportedEncoding' in message, message
print('T57 frame CPU resource re-encoding: native/capture survived; replay explicitly rejected')
PY
