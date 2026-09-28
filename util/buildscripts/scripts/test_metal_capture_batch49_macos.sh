#!/bin/bash
# T49 command buffer callbacks: native/capture identity and lifetime, offline replay. No GUI.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
bash "${SCRIPT_DIR}/build_metal_dev_macos.sh"
mkdir -p "${CAPTURE_DIR}"
MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Command_Handlers --frames 12
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t49" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Command_Handlers --frames 12
"${CMD}" convert -f "${CAPTURE_DIR}/t49_capture.rdc" -o "${CAPTURE_DIR}/t49.zip.xml" -c zip.xml
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t49_capture.rdc"
python3 - "${CAPTURE_DIR}/t49.zip.xml" <<'PY'
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
def child(node, name):
    return next(item for item in node if item.get('name') == name)
buffers = [child(n, 'CommandBuffer').text for n in chunks if int(n.get('id')) == 1044]
assert len(buffers) == 2 and len(set(buffers)) == 2
for chunk_id in (1049, 1054):
    nodes = [n for n in chunks if int(n.get('id')) == chunk_id]
    assert len(nodes) == 4
    assert [child(n, 'CommandBuffer').text for n in nodes] == [buffers[0]] * 2 + [buffers[1]] * 2
    assert all(len(n) == 1 for n in nodes), 'No application pointers or blocks may be captured'
parameters = child(next(n for n in chunks if int(n.get('id')) == 1004 and
                       child(n, 'length').text == '12'), 'Buffer').text
update = [n for n in chunks if int(n.get('id')) == 1198 and child(n, 'Buffer').text == parameters]
assert len(update) == 1
assert child(update[0], 'start').text == '0' and child(update[0], 'size').text == '9'
with zipfile.ZipFile(sys.argv[1][:-4]) as archive:
    index = int(child(update[0], 'data').text)
    assert archive.read(f'{index:06d}') == struct.pack('<III', 41, 67, 101)[:9]
print('T49 eight registration identities and callback CPU update payload passed')
PY
RENDERDOC_METAL_LAST_TEST=49 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
bash "${SCRIPT_DIR}/test_metal_source_library_compat_macos.sh"
echo "BATCH49 terminal validation passed; GUI QA remains deferred."
