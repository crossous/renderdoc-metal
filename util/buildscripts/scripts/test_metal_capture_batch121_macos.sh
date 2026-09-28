#!/bin/bash
# Visible-function-table function-array and fragment-table range binding.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
mkdir -p "${CAPTURE_DIR}"
for trial in 1 2 3; do
  env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T121_VISIBLE_TABLE_RANGE=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 5
done
env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T121_VISIBLE_TABLE_RANGE=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t121" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t121_capture.rdc" \
  -o "${CAPTURE_DIR}/t121.zip.xml" -c zip.xml
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t121_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t39 t120 t121
python3 "${REPO_ROOT}/util/test/metal/metal_visible_function_table_range_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t121_capture.rdc"
echo "T121 visible-function-table ranges targeted terminal validation passed."
