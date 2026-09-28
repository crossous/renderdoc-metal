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
for render in 0 1; do
  fixture="t$((42 + render))"
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_RENDER_BARRIERS="$render" \
    "${DEMO}" Metal_Resource_Barriers --frames 5
  RENDERDOC_METAL_RENDER_BARRIERS="$render" \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/${fixture}" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${DEMO}" Metal_Resource_Barriers --frames 8
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
for number in (42, 43):
    chunks = ET.parse(directory / f't{number}.xml').findall('./chunks/chunk')
    def by_id(chunk_id):
        return [node for node in chunks if int(node.get('id')) == chunk_id]
    single, batches = (1255, (1256,)) if number == 42 else (1177, (1178, 1179))
    assert [int(child(node, 'usageValue').text) for node in by_id(single)] == [3, 5]
    for chunk_id in batches:
        assert [len(child(node, 'resources')) for node in by_id(chunk_id)] == [3, 0]
    scope, resources = (1257, 1258) if number == 42 else (1186, 1187)
    assert len(by_id(scope)) == len(by_id(resources)) == 1
    assert child(by_id(scope)[0], 'scopeValue').text == '1'
    assert len(child(by_id(resources)[0], 'resources')) == 1
    # The intentionally invalid pipeline must not acquire a capture resource/chunk.
    assert len(by_id(1021)) == (1 if number == 42 else 2)
    if number == 42:
        assert child(by_id(1259)[0], 'string').text == 'T42 dependent compute'
        assert child(by_id(1260)[0], 'string').text == 'T42 resource barrier'
        assert len(by_id(1261)) == 1
        assert sum(node.get('name') == 'MTLComputeCommandEncoder::dispatchThreads'
                   for node in chunks) == 3
    else:
        assert [child(node, 'stagesValue').text for node in by_id(1177)] == ['1', '2']
        assert [child(node, 'stagesValue').text for node in by_id(1179)] == ['3', '1']
        for chunk_id, before in ((1186, '1'), (1187, '2')):
            assert child(by_id(chunk_id)[0], 'afterValue').text == '1'
            assert child(by_id(chunk_id)[0], 'beforeValue').text == before
print('T42/T43 structured resource/barrier/marker/pipeline XML passed')
PY
RENDERDOC_METAL_LAST_TEST=43 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
echo "BATCH43-44 native/capture/replay passed; all GUI QA remains deferred."
