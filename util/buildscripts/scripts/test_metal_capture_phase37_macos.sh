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
rm -f "${CAPTURE_DIR}/t36_capture.rdc" "${CAPTURE_DIR}/t36.xml" \
      "${CAPTURE_DIR}/t36_replay.ppm"

"${DEMO_BIN}" Metal_Render_Dynamic_State --frames 5
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t36" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Render_Dynamic_State --frames 8
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t36_capture.rdc" \
  -o "${CAPTURE_DIR}/t36.xml" -c xml

python3 - "${CAPTURE_DIR}/t36.xml" <<'PY'
import math
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()

def one(name):
    nodes = [node for node in root.findall('./chunks/chunk') if node.get('name') == name]
    assert len(nodes) == 1
    return nodes[0]

def named(node, name):
    return next(child for child in node if child.get('name') == name)

def text(name, value):
    assert named(one(name), 'string').text == value

text('MTLCommandBuffer::pushDebugGroup', 'T36 command debug group')
assert one('MTLCommandBuffer::popDebugGroup') is not None
text('MTLRenderCommandEncoder::pushDebugGroup', 'T36 render debug group')
text('MTLRenderCommandEncoder::insertDebugSignpost', 'T36 dynamic state')
assert one('MTLRenderCommandEncoder::popDebugGroup') is not None

viewport = named(one('MTLRenderCommandEncoder::setViewports'), 'viewports')
assert len(viewport) == 1
assert [float(child.text) for child in viewport[0]] == [0.0, 0.0, 400.0, 300.0, 0.0, 1.0]
scissor = named(one('MTLRenderCommandEncoder::setScissorRects'), 'scissors')
assert len(scissor) == 1
assert [int(child.text) for child in scissor[0]] == [64, 48, 272, 204]
assert int(named(one('MTLRenderCommandEncoder::setDepthClipMode'), 'depthClipMode').text) == 1
bias = one('MTLRenderCommandEncoder::setDepthBias')
assert [float(named(bias, field).text) for field in ('depthBias', 'slopeScale', 'clamp')] == \
       [1.25, 2.5, 3.75]
assert int(named(one('MTLRenderCommandEncoder::setTriangleFillMode'), 'fillMode').text) == 0
blend = one('MTLRenderCommandEncoder::setBlendColor')
values = [float(named(blend, field).text) for field in ('red', 'green', 'blue', 'alpha')]
assert all(math.isclose(actual, expected, abs_tol=1e-6)
           for actual, expected in zip(values, (0.2, 0.4, 0.6, 0.8)))
visibility = one('MTLRenderCommandEncoder::setVisibilityResultMode')
assert int(named(visibility, 'mode').text) == 2
assert int(named(visibility, 'offset').text) == 0
assert int(named(one('MTLRenderCommandEncoder::setColorStoreAction'), 'storeAction').text) == 1
assert int(named(one('MTLRenderCommandEncoder::setColorStoreAction'),
                 'colorAttachmentIndex').text) == 0
assert int(named(one('MTLRenderCommandEncoder::setDepthStoreAction'), 'storeAction').text) == 1
assert int(named(one('MTLRenderCommandEncoder::setStencilStoreAction'), 'storeAction').text) == 1
for chunk in ('MTLRenderCommandEncoder::setColorStoreActionOptions',
              'MTLRenderCommandEncoder::setDepthStoreActionOptions',
              'MTLRenderCommandEncoder::setStencilStoreActionOptions'):
    assert int(named(one(chunk), 'storeActionOptions').text) == 0
assert one('MTLRenderCommandEncoder::textureBarrier') is not None
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

"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t36_capture.rdc" "${CAPTURE_DIR}/t36_replay.ppm"
"${RENDERDOCCMD}" replay --loops 3 "${CAPTURE_DIR}/t36_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_render_dynamic_state_invalid.py" \
  "${RENDERDOCCMD}" "${CAPTURE_DIR}/t36_capture.rdc"
"${LIFECYCLE_SMOKE}" "${CAPTURE_DIR}/t35_capture.rdc" \
  "${CAPTURE_DIR}/t34_capture.rdc" "${CAPTURE_DIR}/t36_capture.rdc" 10

echo "Metal phase 37 capture batch passed."
echo "T36: ${CAPTURE_DIR}/t36_capture.rdc"
