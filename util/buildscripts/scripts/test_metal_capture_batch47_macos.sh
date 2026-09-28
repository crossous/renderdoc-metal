#!/bin/bash
# T47 native reflection, capture/XML and full terminal replay regression. No GUI automation.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
DEMO="${REPO_ROOT}/bin/demos_x64"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
"${SCRIPT_DIR}/build_metal_dev_macos.sh"
mkdir -p "${CAPTURE_DIR}"
MTL_DEBUG_LAYER=1 "${DEMO}" Metal_Pipeline_Variants --frames 5
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t47" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${DEMO}" Metal_Pipeline_Variants --frames 8
"${CMD}" convert -f "${CAPTURE_DIR}/t47_capture.rdc" -o "${CAPTURE_DIR}/t47.xml" -c xml
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t47_capture.rdc"
python3 - "${CAPTURE_DIR}/t47.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
def child(node, name):
    return next(item for item in node if item.get('name') == name)
pipelines = []
for chunk_id in (1022, 1024, 1025):
    nodes = [n for n in chunks if int(n.get('id')) == chunk_id]
    assert len(nodes) == 2
    assert [child(n, 'optionsValue').text for n in nodes] == ['3', '0']
    resource = 'RenderPipelineState' if chunk_id == 1022 else 'ComputePipelineState'
    pipelines += [child(n, resource).text for n in nodes]
    if chunk_id != 1024:
        assert all(child(n, 'supported').text == 'true' for n in nodes)
    if chunk_id == 1025:
        for node in nodes:
            descriptor = child(node, 'descriptor')
            assert child(descriptor, 'label').text == 'T47 bounded compute descriptor'
            assert child(descriptor, 'maxTotalThreadsPerThreadgroup').text == '64'
            assert child(descriptor, 'threadGroupSizeIsMultipleOfThreadExecution').text == 'true'
            assert [child(n, 'mutability').text for n in child(descriptor, 'buffers')] == ['1', '2']
assert len(set(pipelines)) == 6
assert sum(n.get('name') == 'MTLComputeCommandEncoder::dispatchThreadgroups' for n in chunks) == 3
assert sum(n.get('name') == 'MTLComputeCommandEncoder::dispatchThreads' for n in chunks) == 1
print('T47 six pipeline identities/options/reflection descriptor XML passed')
PY
RENDERDOC_METAL_LAST_TEST=47 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
echo "BATCH47 native/capture/replay passed; GUI QA remains deferred."
