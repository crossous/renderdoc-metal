#!/bin/bash
# Automatic Private tracked heap and heap-backed buffer: native, capture and replay. No GUI.
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
  RENDERDOC_METAL_T71_HEAP_BUFFER=1 MTL_DEBUG_LAYER=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
done
RENDERDOC_METAL_T71_HEAP_BUFFER=1 MTL_DEBUG_LAYER=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t71" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t71_capture.rdc" \
  -o "${CAPTURE_DIR}/t71.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t71.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
def child(node, name):
    return next(item for item in node if item.get('name') == name)
heap = [node for node in chunks if node.get('name') == 'MTLDevice::newHeapWithDescriptor']
buffer = [node for node in chunks if node.get('name') == 'MTLHeap::newBuffer']
assert len(heap) == len(buffer) == 1
assert child(heap[0], 'Heap').text == child(buffer[0], 'Heap').text
assert child(heap[0], 'size').text == '65536'
assert child(heap[0], 'storageMode').text == '2'
assert child(heap[0], 'hazardMode').text == '2'
assert child(heap[0], 'type').text == '0'
assert child(buffer[0], 'length').text == '516'
assert child(buffer[0], 'options').text == '32'
print('T71 heap and child buffer identity/options captured')
PY
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t71_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t71_capture.rdc" "${CAPTURE_DIR}/t71_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_heap_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t71_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t39 t62
echo "T71 targeted terminal validation passed; full replay regression and GUI QA remain separate."
