#!/bin/bash
# ICB operations: native, capture, structured metadata and combined terminal regression. No GUI.
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
MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_ICB_Operations --frames 12
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t53" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_ICB_Operations --frames 12
"${CMD}" convert -f "${CAPTURE_DIR}/t53_capture.rdc" -o "${CAPTURE_DIR}/t53.zip.xml" -c zip.xml
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t53_capture.rdc"
python3 - "${CAPTURE_DIR}/t53.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
def child(node, name):
    return next(n for n in node if n.get('name') == name)
def pick(chunk_id):
    return [n for n in chunks if int(n.get('id')) == chunk_id]
def span(node, field='range'):
    return tuple(int(n.text) for n in child(node, field))
icbs = pick(1031)
assert len(icbs) == 2
src, dst = [child(n, 'IndirectCommandBuffer').text for n in icbs]
assert [int(child(n, 'maxCount').text) for n in icbs] == [5, 6]
assert all(child(n, 'commandTypes').text == '3' for n in icbs)
assert len(pick(1270)) == 2
assert [span(n) for n in pick(1224)] == [(0, 1), (3, 1)]
assert all(child(n, 'buffer').text == dst for n in pick(1224))
assert [span(n, 'sourceRange') for n in pick(1225)] == [(1, 4), (2, 1)]
assert [child(n, 'source').text for n in pick(1225)] == [src, dst]
assert [child(n, 'destination').text for n in pick(1225)] == [dst, dst]
assert [int(child(n, 'destinationIndex').text) for n in pick(1225)] == [2, 1]
assert [span(n) for n in pick(1226)] == [(0, 1), (3, 1), (1, 2)]
assert [span(n) for n in pick(1240)] == [(0, 1), (0, 6), (2, 3), (1, 4)]
assert len(pick(1184)) == 14
print('T53 two ICBs, command resets, seven GPU operations and four execute groups passed')
PY
RENDERDOC_METAL_LAST_TEST=53 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
# This application's first draw depends on an opaque pre-capture GPU copy. Native must still
# work; offline replay must reject it explicitly instead of drawing the old CPU-encoded packet.
RENDERDOC_METAL_T53_PREFRAME_GPU=1 MTL_DEBUG_LAYER=1 \
  "${REPO_ROOT}/bin/demos_x64" Metal_ICB_Operations --frames 3
RENDERDOC_METAL_T53_PREFRAME_GPU=1 RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t53_preframe_unknown" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_ICB_Operations --frames 3
python3 - "${REPO_ROOT}" "${CMD}" "${CAPTURE_DIR}/t53_preframe_unknown_capture.rdc" <<'PY'
import pathlib
import sys
sys.path.insert(0, str(pathlib.Path(sys.argv[1]) / 'util/test/metal'))
from metal_compute_inline_invalid import run
message = run(sys.argv[2], 'replay', '--loops', '1', sys.argv[3], success=False)
assert 'unavailableInitialContents' in message, message
print('T53 pre-capture GPU contents: native/capture survived; offline rejected explicitly')
PY
echo "BATCH53 terminal validation passed; GUI QA remains deferred."
