#!/bin/bash
# Async creation + events: deterministic terminal tests, no qrenderdoc/Computer Use.
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
for fixture in 51 52; do
  if [[ "$fixture" == 51 ]]; then demo=Metal_Async_Creation; else demo=Metal_Event_Sync; fi
  MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" "$demo" --frames 12
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t${fixture}" \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" "$demo" --frames 12
  "${CMD}" convert -f "${CAPTURE_DIR}/t${fixture}_capture.rdc" -o "${CAPTURE_DIR}/t${fixture}.zip.xml" -c zip.xml
  MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t${fixture}_capture.rdc"
done
python3 - "${CAPTURE_DIR}" <<'PY'
import pathlib
import sys
import xml.etree.ElementTree as ET
root = pathlib.Path(sys.argv[1])
def child(node, name):
    return next(item for item in node if item.get('name') == name)
chunks = ET.parse(root / 't51.zip.xml').findall('./chunks/chunk')
creates = [n for n in chunks if 1264 <= int(n.get('id')) <= 1269]
assert sorted(int(n.get('id')) for n in creates) == list(range(1264, 1270))
for n in creates:
    chunk_id = int(n.get('id'))
    if chunk_id in (1264, 1265, 1266, 1269): assert child(n, 'supported').text == 'true'
    if chunk_id > 1264:
        assert int(child(n, 'optionsValue').text) == (0 if chunk_id in (1265, 1267) else 3)
    if chunk_id in (1265, 1266):
        descriptor = child(n, 'descriptor')
        attachment = child(descriptor, 'colorAttachments')[0]
        assert child(attachment, 'pixelFormat').text == '80', 'Caller mutation must not change snapshot'
    if chunk_id == 1269:
        assert child(child(n, 'descriptor'), 'maxTotalThreadsPerThreadgroup').text == '64'
    assert not any(c.get('name') in ('completionHandler', 'block', 'reflection', 'error') for c in n)
assert len([n for n in chunks if int(n.get('id')) == 1039]) == 3
assert len([n for n in chunks if 'newLibrary' in n.get('name', '')]) == 1, 'Failed result creates no resource'
print('T51 six async results, descriptor snapshots and no block/error payloads passed')
chunks = ET.parse(root / 't52.zip.xml').findall('./chunks/chunk')
events = [child(n, 'Event').text for n in chunks if int(n.get('id')) == 1032]
assert len(events) == 2 and len(set(events)) == 2
queues = [child(n, 'CommandQueue').text for n in chunks if int(n.get('id')) == 1001]
assert len(queues) == 2 and len(set(queues)) == 2
commands = [n for n in chunks if int(n.get('id')) == 1044]
assert len(commands) == 3
cb = [child(n, 'CommandBuffer').text for n in commands]
assert [child(n, 'CommandQueue').text for n in commands] == [queues[0], queues[1], queues[0]]
ops = [n for n in chunks if int(n.get('id')) in (1062, 1063)]
assert [int(n.get('id')) for n in ops] == [1063, 1062, 1063, 1062, 1063, 1062]
assert [child(n, 'event').text for n in ops] == [events[0], events[0], events[1], events[1], events[0], events[0]]
assert [child(n, 'CommandBuffer').text for n in ops] == [cb[0], cb[1], cb[1], cb[2], cb[2], cb[2]]
start = int(child(ops[0], 'value').text)
assert start > 0
assert [int(child(n, 'value').text) for n in ops] == [start, start, start+2, start+2, start+4, start+4]
print('T52 two queues/events, three submissions, six ordered signal/wait identities and values passed')
PY
RENDERDOC_METAL_LAST_TEST=52 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
# Pipeline proxy ownership now matches the library/function proxy lifetime contract.
bash "${SCRIPT_DIR}/test_metal_source_library_compat_macos.sh"
echo "BATCH51-52 terminal validation passed; GUI QA remains deferred."
