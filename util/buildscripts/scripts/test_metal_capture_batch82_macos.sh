#!/bin/bash
# Async mesh pipeline snapshot, capture, replay and malformed checks; terminal only.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${DEMO_BUILD_DIR}" -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
mkdir -p "${CAPTURE_DIR}"
for trial in 1 2 3 4 5; do
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T82_MESH_ASYNC=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Mesh --frames 3
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T82_MESH_ASYNC=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t82" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Mesh --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t82_capture.rdc" \
  -o "${CAPTURE_DIR}/t82.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t82.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
def child(node, name):
    return next(item for item in node if item.get('name') == name)
pipelines = [node for node in chunks if node.get('id') == '1300']
assert len(pipelines) == 1 and sum(node.get('id') == '1287' for node in chunks) == 0
p = pipelines[0]
assert child(p,'objectFunction').text == '0'
assert child(p,'meshFunction').text != '0'
assert child(p,'colorFormats')[0].text == '80'
assert child(p,'options').text == '1' and child(p,'supported').text == 'true'
assert sum(node.get('id') == '1288' for node in chunks) == 1
print('T82 async mesh pipeline call-time descriptor snapshot captured')
PY
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t82_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t82_capture.rdc" "${CAPTURE_DIR}/t82_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_mesh_async_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t82_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t78 t79 t80 t81
echo "T82 targeted terminal validation passed; full replay regression and GUI QA remain separate."
