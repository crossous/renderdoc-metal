#!/bin/bash
# Four render heap residency overloads: native, capture and replay. No GUI.
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
  RENDERDOC_METAL_T73_HEAP_USE=1 MTL_DEBUG_LAYER=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Shared_Texture --frames 3
done
RENDERDOC_METAL_T73_HEAP_USE=1 MTL_DEBUG_LAYER=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t73" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Shared_Texture --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t73_capture.rdc" \
  -o "${CAPTURE_DIR}/t73.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t73.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
def child(node, name):
    return next(item for item in node if item.get('name') == name)
heap = next(node for node in chunks if node.get('name') == 'MTLDevice::newHeapWithDescriptor')
heap_id = child(heap, 'Heap').text
for chunk_id, stage in ((1180, '1'), (1181, '2'), (1182, '1'), (1183, '2')):
    matches = [node for node in chunks if node.get('id') == str(chunk_id)]
    assert len(matches) == 1, (chunk_id, len(matches))
    assert len(child(matches[0], 'heaps')) == 1
    assert child(matches[0], 'heaps')[0].text == heap_id
    assert child(matches[0], 'stagesValue').text == stage
print('T73 four heap residency overloads captured with matching heap IDs and stages')
PY
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t73_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t73_capture.rdc" "${CAPTURE_DIR}/t73_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_heap_use_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t73_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t64 t72
echo "T73 targeted terminal validation passed; full replay regression and GUI QA remain separate."
