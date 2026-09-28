#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
RENDERDOC_BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
DEMO_BIN="${REPO_ROOT}/bin/demos_x64"
RENDERDOC_LIB="${RENDERDOC_BUILD_DIR}/lib/librenderdoc.dylib"
RENDERDOCCMD="${RENDERDOC_BUILD_DIR}/bin/renderdoccmd"
OUTPUT_SMOKE="${RENDERDOC_BUILD_DIR}/metal_replay_output_smoke"
LIFECYCLE_SMOKE="${RENDERDOC_BUILD_DIR}/metal_replay_lifecycle_smoke"
TARGET_ARCH="$(uname -m)"

cmake -S "${REPO_ROOT}/util/test/demos" -B "${DEMO_BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
"${SCRIPT_DIR}/build_metal_dev_macos.sh"

mkdir -p "${CAPTURE_DIR}"
rm -f "${CAPTURE_DIR}/t34_capture.rdc" "${CAPTURE_DIR}/t35_capture.rdc" \
      "${CAPTURE_DIR}/t34.xml" "${CAPTURE_DIR}/t35.xml" \
      "${CAPTURE_DIR}/t34_replay.ppm" "${CAPTURE_DIR}/t35_replay.ppm"

"${DEMO_BIN}" Metal_Render_Inline_Batch_Binding --frames 5
"${DEMO_BIN}" Metal_Command_Creation_Variants --frames 5

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t34" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Render_Inline_Batch_Binding --frames 8
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t35" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Command_Creation_Variants --frames 8

"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t34_capture.rdc" \
  -o "${CAPTURE_DIR}/t34.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t35_capture.rdc" \
  -o "${CAPTURE_DIR}/t35.xml" -c xml

python3 - "${CAPTURE_DIR}/t34.xml" "${CAPTURE_DIR}/t35.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET

t34, t35 = [ET.parse(path).getroot() for path in sys.argv[1:]]

def chunks(root, name):
    return [node for node in root.findall('./chunks/chunk') if node.get('name') == name]

def named(node, name):
    return next(child for child in node if child.get('name') == name)

vertex = chunks(t34, 'MTLRenderCommandEncoder::setVertexBuffers')
assert len(vertex) == 1
assert [int(node.text) for node in named(vertex[0], 'range')] == [0, 2]
assert [int(node.text) for node in named(vertex[0], 'bound')] == [1, 1]
assert [int(node.text) for node in named(vertex[0], 'offsets')] == [16, 0]
offset = chunks(t34, 'MTLRenderCommandEncoder::setVertexBufferOffset')
assert len(offset) == 1
assert int(named(offset[0], 'offset').text) == 256
assert int(named(offset[0], 'index').text) == 1
vertex_bytes = chunks(t34, 'MTLRenderCommandEncoder::setVertexBytes')
assert len(vertex_bytes) == 1 and len(named(vertex_bytes[0], 'data')) == 8
assert int(named(vertex_bytes[0], 'index').text) == 2
fragment = chunks(t34, 'MTLRenderCommandEncoder::setFragmentBuffers')
assert len(fragment) == 1
assert [int(node.text) for node in named(fragment[0], 'range')] == [3, 2]
assert [int(node.text) for node in named(fragment[0], 'offsets')] == [0, 16]
fragment_bytes = chunks(t34, 'MTLRenderCommandEncoder::setFragmentBytes')
assert len(fragment_bytes) == 1 and len(named(fragment_bytes[0], 'data')) == 16
assert int(named(fragment_bytes[0], 'index').text) == 2

limited = chunks(t35, 'MTLDevice::newCommandQueueWithMaxCommandBufferCount')
assert len(limited) == 1 and int(named(limited[0], 'maxCommandBufferCount').text) == 4
assert len(chunks(t35, 'MTLCommandQueue::commandBufferWithUnretainedReferences')) == 1
direct = chunks(t35, 'MTLCommandBuffer::computeCommandEncoderWithDispatchType')
assert len(direct) == 1 and int(named(direct[0], 'dispatchType').text) == 1
assert len(chunks(t35, 'MTLCommandBuffer::waitUntilScheduled')) == 1
descriptor = chunks(t35, 'MTLCommandQueue::commandBufferWithDescriptor')
assert len(descriptor) == 1
assert named(descriptor[0], 'retainedReferences').text == 'false'
assert int(named(descriptor[0], 'errorOptions').text) == 0
compute = chunks(t35, 'MTLCommandBuffer::computeCommandEncoderWithDescriptor')
assert len(compute) == 1 and int(named(compute[0], 'dispatchType').text) == 0
PY

clang++ -std=c++17 -arch "${TARGET_ARCH}" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${RENDERDOC_BUILD_DIR}/lib" -lrenderdoc \
  -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${RENDERDOC_BUILD_DIR}/lib" -o "${OUTPUT_SMOKE}"

clang++ -std=c++17 -arch "${TARGET_ARCH}" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_lifecycle_smoke.mm" \
  -L"${RENDERDOC_BUILD_DIR}/lib" -lrenderdoc \
  -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${RENDERDOC_BUILD_DIR}/lib" -o "${LIFECYCLE_SMOKE}"

"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t34_capture.rdc" "${CAPTURE_DIR}/t34_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t35_capture.rdc" "${CAPTURE_DIR}/t35_replay.ppm"
"${RENDERDOCCMD}" replay --loops 3 "${CAPTURE_DIR}/t34_capture.rdc"
"${RENDERDOCCMD}" replay --loops 3 "${CAPTURE_DIR}/t35_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_render_command_creation_invalid.py" \
  "${RENDERDOCCMD}" "${CAPTURE_DIR}/t34_capture.rdc" "${CAPTURE_DIR}/t35_capture.rdc"
"${LIFECYCLE_SMOKE}" "${CAPTURE_DIR}/t35_capture.rdc" \
  "${CAPTURE_DIR}/t34_capture.rdc" 10

echo "Metal phase 35-36 capture batch passed."
echo "T34: ${CAPTURE_DIR}/t34_capture.rdc"
echo "T35: ${CAPTURE_DIR}/t35_capture.rdc"
