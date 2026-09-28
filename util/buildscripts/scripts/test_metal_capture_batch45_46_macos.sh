#!/bin/bash
# Native/API validation, capture metadata and shared replay regression. No GUI automation.
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
number=44
for mode in normal time duration; do
  fixture="t${number}"
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TIMED_PRESENT="$mode" \
    "${DEMO}" Metal_Fence_Present --frames 5
  RENDERDOC_METAL_TIMED_PRESENT="$mode" RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/${fixture}" \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${DEMO}" Metal_Fence_Present --frames 8
  "${CMD}" convert -f "${CAPTURE_DIR}/${fixture}_capture.rdc" -o "${CAPTURE_DIR}/${fixture}.xml" -c xml
  MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/${fixture}_capture.rdc"
  number=$((number + 1))
done
python3 - "${CAPTURE_DIR}" <<'PY'
import pathlib
import sys
import xml.etree.ElementTree as ET
directory = pathlib.Path(sys.argv[1])
def child(node, name):
    return next(item for item in node if item.get('name') == name)
for number in (44, 45, 46):
    chunks = ET.parse(directory / f't{number}.xml').findall('./chunks/chunk')
    def by_id(chunk_id):
        return [node for node in chunks if int(node.get('id')) == chunk_id]
    fences = [child(node, 'Fence').text for node in by_id(1026)]
    assert len(set(fences)) == 4
    for chunk_id, count in ((1216, 2), (1217, 1), (1262, 2), (1263, 2), (1151, 1), (1152, 2)):
        assert len(by_id(chunk_id)) == count
        assert all(child(node, 'fence').text in fences for node in by_id(chunk_id))
    assert child(by_id(1262)[-1], 'fence').text == child(by_id(1216)[0], 'fence').text
    assert child(by_id(1151)[0], 'stagesValue').text == '1'
    assert [child(n, 'stagesValue').text for n in by_id(1152)] == ['1', '2']
    present = by_id(1050 + number - 44)
    assert len(present) == 1
    if number == 44:
        assert not by_id(1194) and not by_id(1195)
        continue
    assert float(child(present[0], 'time').text) == (1.25 if number == 45 else 0.001)
    begin = next(i for i, n in enumerate(chunks) if int(n.get('id')) == 4)
    background = [n for n in chunks[:begin] if int(n.get('id')) in (1194, 1195)]
    frame = [n for n in chunks[begin:] if int(n.get('id')) in (1194, 1195)]
    # Background resets prune superseded annotations but preserve the last annotation state.
    assert [int(n.get('id')) for n in background] == [1195, 1194]
    assert child(background[-1], 'marker').text == 'final range'
    assert [int(n.get('id')) for n in frame] == [1195, 1194, 1195, 1194]
    assert [child(n, 'marker').text for n in frame if int(n.get('id')) == 1194] == \
           ['payload range', 'final range']
    assert [(child(child(n, 'range'), 'location').text, child(child(n, 'range'), 'length').text)
            for n in frame if int(n.get('id')) == 1194] == [('4', '16'), ('32', '12')]
print('T44-T46 structured fences/present/annotation and background-pruning XML passed')
PY
RENDERDOC_METAL_LAST_TEST=46 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
echo "BATCH45-46 native/capture/replay passed; GUI QA remains deferred."
