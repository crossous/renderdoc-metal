#!/bin/bash
# Tile pipeline, tile buffer overloads, and tile dispatch. Terminal-only.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
cmake --build "${BUILD_DIR}" --target renderdoccmd -j "$(sysctl -n hw.ncpu)"
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
mkdir -p "${CAPTURE_DIR}"
for trial in 1 2 3 4 5; do
  MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Tile --frames 3
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t74" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Tile --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t74_capture.rdc" \
  -o "${CAPTURE_DIR}/t74.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t74.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
def child(node, name):
    return next(item for item in node if item.get('name') == name)
def selected(name):
    return [node for node in chunks if node.get('name') == name]
pipeline = selected('MTLDevice::newRenderPipelineStateWithTileDescriptor')
single = selected('MTLRenderCommandEncoder::setTileBuffer')
batch = selected('MTLRenderCommandEncoder::setTileBuffers')
offset = selected('MTLRenderCommandEncoder::setTileBufferOffset')
bytes_ = selected('MTLRenderCommandEncoder::setTileBytes')
dispatch = selected('MTLRenderCommandEncoder::dispatchThreadsPerTile')
assert len(pipeline) == 1 and len(single) == 3 and len(batch) == 2
assert len(offset) == 1 and len(bytes_) == len(dispatch) == 3
assert child(pipeline[0], 'supported').text == 'true'
counter = child(single[0], 'buffer').text
assert counter != '0' and child(single[1], 'buffer').text == '0'
assert child(batch[0], 'buffers')[0].text == counter
assert child(batch[1], 'buffers')[0].text == '0'
assert [child(node, 'data')[0].text for node in bytes_] == ['1','2','3']
for node in dispatch:
    dims = child(node, 'threadsPerTile')
    assert [child(dims, axis).text for axis in ('width','height','depth')] == ['16','16','1']
print('T74 tile pipeline, six binding forms and three dispatches captured')
PY
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t74_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t74_capture.rdc" "${CAPTURE_DIR}/t74_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_tile_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t74_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t34 t43 t73
echo "T74 targeted terminal validation passed; full replay regression and GUI QA remain separate."
