#!/bin/bash
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
for fixture in t38 t39; do
  if [[ "$fixture" == t38 ]]; then name=Metal_Sampler_LOD; else name=Metal_Private_Buffer; fi
  "${DEMO}" "$name" --frames 5
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/${fixture}" \
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
root = ET.parse(directory / 't38.xml').getroot()
chunks = root.findall('./chunks/chunk')
def child(node, field):
    return next(item for item in node if item.get('name') == field)
for stage, minimum in [('Vertex', 2), ('Fragment', 1), ('Compute', 2)]:
    prefix = 'MTLComputeCommandEncoder::setSampler' if stage == 'Compute' else \
             f'MTLRenderCommandEncoder::set{stage}Sampler'
    singles = [node for node in chunks if node.get('name', '').startswith(prefix + 'State')
               and any(item.get('name') == 'lodMinClamp' for item in node)]
    batches = [node for node in chunks if node.get('name', '').startswith(prefix + 'States')
               and any(item.get('name') == 'lodMinClamps' for item in node)]
    assert len(singles) == 3 and len(batches) == 1
    assert [child(node, 'bound').text for node in singles] == ['true', 'true', 'false']
    node = batches[0]
    assert [int(item.text) for item in child(node, 'range')] == [2, 2]
    assert [int(item.text) for item in child(node, 'bound')] == [1, 0]
    assert [float(item.text) for item in child(node, 'lodMinClamps')] == [minimum, 0]
    assert [float(item.text) for item in child(node, 'lodMaxClamps')] == [minimum, 2]
root = ET.parse(directory / 't39.xml').getroot()
buffers = [node for node in root.findall('./chunks/chunk')
           if node.get('name', '').startswith('MTLDevice::newBuffer')
           and any(item.get('name') == 'length' and item.text == '516' for item in node)]
assert len(buffers) == 2
assert sorted(int(child(node, 'options').text) & 0xf0 for node in buffers) == [0, 0x20]
print('T38/T39 structured LOD/private-storage XML passed')
PY
# Reuse the earlier single regression entry point, extending it to T39 and old compute negatives.
RENDERDOC_METAL_LAST_TEST=39 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
echo "BATCH39-40 native/capture/replay passed; all GUI QA remains deferred."
