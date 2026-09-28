#!/bin/bash
# Minimal mesh pipeline and direct mesh dispatch. Terminal-only.
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
  MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Mesh --frames 3
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t78" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Mesh --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t78_capture.rdc" \
  -o "${CAPTURE_DIR}/t78.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t78.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
def child(node, name):
    return next(item for item in node if item.get('name') == name)
pipeline = [node for node in chunks if node.get('id') == '1287']
draw = [node for node in chunks if node.get('id') == '1288']
assert len(pipeline) == len(draw) == 1
assert child(pipeline[0], 'objectFunction').text == '0'
assert child(pipeline[0], 'meshFunction').text != '0'
assert child(pipeline[0], 'fragmentFunction').text != '0'
assert child(pipeline[0], 'colorFormats')[0].text == '80'
assert child(pipeline[0], 'maxMeshThreads').text == '32'
assert child(pipeline[0], 'supported').text == 'true'
assert child(child(draw[0], 'threadgroupsPerGrid'), 'width').text == '1'
assert child(child(draw[0], 'threadsPerMeshThreadgroup'), 'width').text == '32'
print('T78 mesh pipeline and direct mesh dispatch captured')
PY
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t78_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t78_capture.rdc" "${CAPTURE_DIR}/t78_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_mesh_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t78_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t74 t75 t76 t77
echo "T78 targeted terminal validation passed; full replay regression and GUI QA remain separate."
