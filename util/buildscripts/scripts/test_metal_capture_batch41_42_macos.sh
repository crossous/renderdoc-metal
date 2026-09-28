#!/bin/bash
# Terminal-only native validation, captures, structured checks and consolidated regression.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
DEMO="${REPO_ROOT}/bin/demos_x64"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake -S "${REPO_ROOT}/util/test/demos" -B "${DEMO_BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
"${SCRIPT_DIR}/build_metal_dev_macos.sh"
mkdir -p "${CAPTURE_DIR}"
for fixture in t40 t41; do
  if [[ "$fixture" == t40 ]]; then
    name=Metal_Compute_Inline; hints=0
  else
    name=Metal_Blit_Transfer; hints=1
  fi
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_BLIT_HINTS="$hints" "${DEMO}" "$name" --frames 5
  RENDERDOC_METAL_BLIT_HINTS="$hints" RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/${fixture}" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" "${DEMO}" "$name" --frames 8
  "${CMD}" convert -f "${CAPTURE_DIR}/${fixture}_capture.rdc" \
    -o "${CAPTURE_DIR}/${fixture}.xml" -c xml
  "${CMD}" replay --loops 3 "${CAPTURE_DIR}/${fixture}_capture.rdc"
done
python3 - "${CAPTURE_DIR}" <<'PY'
import pathlib
import sys
import xml.etree.ElementTree as ET
directory = pathlib.Path(sys.argv[1])
def child(node, field):
    return next(item for item in node if item.get('name') == field)
chunks = ET.parse(directory / 't40.xml').findall('./chunks/chunk')
def compute(method):
    return [node for node in chunks if node.get('name') == 'MTLComputeCommandEncoder::' + method]
inline = compute('setBytes')
assert len(inline) == 3
assert [int(child(node, 'data')[0].text) for node in inline] == [1, 2, 5]
assert all(len(child(node, 'data')) == 16 and child(node, 'index').text == '2' for node in inline)
assert [(int(child(node, 'offset').text), int(child(node, 'index').text))
        for node in compute('setBufferOffset')] == [(16, 0), (32, 0), (16, 2), (48, 0)]
assert [(int(child(node, 'length').text), int(child(node, 'index').text))
        for node in compute('setThreadgroupMemoryLength')] == \
       [(32, 0), (32, 1), (16, 5), (0, 5), (64, 0), (48, 1), (32, 0), (32, 1)]
assert len(compute('dispatchThreadgroups')) == 3 and len(compute('dispatchThreads')) == 2
chunks = ET.parse(directory / 't41.xml').findall('./chunks/chunk')
descriptor = [node for node in chunks if node.get('name') == 'MTLCommandBuffer::blitCommandEncoderWithDescriptor']
assert len(descriptor) == 1 and child(descriptor[0], 'hasSampleBuffers').text == 'false'
hints = [node for node in chunks if '::optimizeContentsFor' in node.get('name', '')]
assert [int(node.get('id')) for node in hints] == [1220, 1221, 1222, 1223]
assert [int(child(hints[1], field).text) for field in ('slice', 'level')] == [1, 1]
assert [int(child(hints[3], field).text) for field in ('slice', 'level')] == [0, 1]
print('T40/T41 structured inline/offset/threadgroup/descriptor/optimization XML passed')
PY
RENDERDOC_METAL_LAST_TEST=41 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
echo "BATCH41-42 native/capture/replay passed; all GUI QA remains deferred."
