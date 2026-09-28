#!/bin/bash
# T50 CPU texture read metadata, native padded transfers and Managed synchronization. No GUI.
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
MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Texture_Readback --frames 12
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t50" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Texture_Readback --frames 12
"${CMD}" convert -f "${CAPTURE_DIR}/t50_capture.rdc" -o "${CAPTURE_DIR}/t50.zip.xml" -c zip.xml
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t50_capture.rdc"
python3 - "${CAPTURE_DIR}/t50.zip.xml" <<'PY'
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
def child(node, name):
    return next(item for item in node if item.get('name') == name)
textures = {int(child(child(n, 'descriptor'), 'width').text): child(n, 'Texture').text
            for n in chunks if int(n.get('id')) == 1008}
assert set(textures) == {11, 18, 7, 19, 8}
reads = [n for n in chunks if int(n.get('id')) in (1072, 1073)]
assert [int(n.get('id')) for n in reads] == [1072, 1073, 1072, 1073]
for node, width, pitch, mip, origin, size in zip(
        reads, (11, 18, 7, 8), (20, 28, 16, 32), (1, 1, 0, 0),
        ((1, 1, 0), (2, 1, 0), (3, 2, 0), (1, 1, 0)),
        ((3, 2, 1), (4, 3, 1), (2, 2, 1), (3, 2, 2))):
    assert child(node, 'Texture').text == textures[width]
    assert int(child(node, 'bytesPerRow').text) == pitch
    assert int(child(node, 'level').text) == mip
    region = child(node, 'region')
    assert tuple(int(n.text) for n in child(region, 'origin')) == origin
    assert tuple(int(n.text) for n in child(region, 'size')) == size
    fields = {'Texture', 'bytesPerRow', 'region', 'level'}
    if int(node.get('id')) == 1073:
        fields |= {'bytesPerImage', 'slice'}
    assert {n.get('name') for n in node} == fields, 'No CPU pointers or output/padding payloads'
assert [int(child(n, 'bytesPerImage').text) for n in (reads[1], reads[3])] == [84, 128]
assert [int(child(n, 'slice').text) for n in (reads[1], reads[3])] == [1, 0]
syncs = [n for n in chunks if int(n.get('id')) == 1205]
assert len(syncs) == 3
assert [child(n, 'texture').text for n in syncs] == [textures[w] for w in (11, 18, 19)]
assert [int(child(n, 'slice').text) for n in syncs] == [0, 1, 0]
assert [int(child(n, 'level').text) for n in syncs] == [1, 1, 0]
parameters = child(next(n for n in chunks if int(n.get('id')) == 1004 and
                        child(n, 'length').text == '68'), 'Buffer').text
updates = [n for n in chunks if int(n.get('id')) == 1198 and child(n, 'Buffer').text == parameters]
assert len(updates) == 1
assert child(updates[0], 'start').text == '0' and child(updates[0], 'size').text == '17'
with zipfile.ZipFile(sys.argv[1][:-4]) as archive:
    index = int(child(updates[0], 'data').text)
    assert archive.read(f'{index:06d}') == struct.pack('<IIIII', 43, 79, 113, 151, 152)[:17]
print('T50 four CPU reads, three Managed syncs, retained resources and GPU input payload passed')
PY
RENDERDOC_METAL_LAST_TEST=50 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
echo "BATCH50 terminal validation passed; GUI QA remains deferred."
