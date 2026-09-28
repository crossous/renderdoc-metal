#!/bin/bash
# Non-overlapping explicit-offset placement heap buffer, without aliasing.
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
  env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T116_PLACEMENT_BUFFER=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 5
done
env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T116_PLACEMENT_BUFFER=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t116" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t116_capture.rdc" \
  -o "${CAPTURE_DIR}/t116.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t116.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
field = lambda node, name: next(x for x in node if x.get('name') == name)
heap = next(c for c in chunks if c.get('name') == 'MTLDevice::newHeapWithDescriptor')
buffer = next(c for c in chunks if c.get('name') == 'MTLHeap::newBuffer(offset)')
assert field(heap, 'type').text == '1'
assert field(buffer, 'Heap').text == field(heap, 'Heap').text
assert int(field(buffer, 'offset').text) > 0
assert field(buffer, 'length').text == '516'
print('T116 placement heap identity, explicit offset and size captured')
PY
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t116_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t116_capture.rdc" "${CAPTURE_DIR}/t116_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_heap_placement_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t116_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_heap_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t71_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t39 t71 t72 t104 t115 t116
echo "T116 placement heap buffer targeted terminal validation passed."
