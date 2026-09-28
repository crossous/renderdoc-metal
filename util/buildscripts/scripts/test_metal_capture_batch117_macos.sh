#!/bin/bash
# Non-overlapping explicit-offset placement heap texture, without aliasing.
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
  env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T117_PLACEMENT_TEXTURE=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Shared_Texture --frames 5
done
env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T117_PLACEMENT_TEXTURE=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t117" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Shared_Texture --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t117_capture.rdc" \
  -o "${CAPTURE_DIR}/t117.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t117.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
field = lambda node, name: next(x for x in node if x.get('name') == name)
heap = next(c for c in chunks if c.get('name') == 'MTLDevice::newHeapWithDescriptor')
texture = next(c for c in chunks if c.get('name') == 'MTLHeap::newTexture(offset)')
assert field(heap, 'type').text == '1'
assert field(texture, 'Heap').text == field(heap, 'Heap').text
assert int(field(texture, 'offset').text) > 0
assert field(field(texture, 'descriptor'), 'width').text == '1'
print('T117 placement heap texture identity, explicit offset and descriptor captured')
PY
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t117_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t117_capture.rdc" "${CAPTURE_DIR}/t117_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_heap_placement_texture_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t117_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_heap_texture_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t72_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t39 t64 t71 t72 t73 t116 t117
echo "T117 placement heap texture targeted terminal validation passed."
