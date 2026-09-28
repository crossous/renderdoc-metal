#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
DEMO="${REPO_ROOT}/bin/demos_x64"
CMD="${BUILD_DIR}/bin/renderdoccmd"

cmake -S "${REPO_ROOT}/util/test/demos" -B "${DEMO_BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
"${SCRIPT_DIR}/build_metal_dev_macos.sh"
mkdir -p "${CAPTURE_DIR}"

for fixture in t37 t10_debug; do
  if [[ "$fixture" == t37 ]]; then name=Metal_Blit_Transfer; else name=Metal_Blit_Operations; fi
  "${DEMO}" "$name" --frames 5
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/${fixture}" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" "${DEMO}" "$name" --frames 8
  "${CMD}" convert -f "${CAPTURE_DIR}/${fixture}_capture.rdc" \
    -o "${CAPTURE_DIR}/${fixture}.xml" -c xml
done

python3 - "${CAPTURE_DIR}" <<'PY'
import pathlib
import sys
import xml.etree.ElementTree as ET

directory = pathlib.Path(sys.argv[1])
def named(node, field):
    return next(child for child in node if child.get('name') == field)
def chunks(root, name, field=None):
    return [node for node in root.findall('./chunks/chunk')
            if node.get('name') == 'MTLBlitCommandEncoder::' + name
            and (field is None or any(child.get('name') == field for child in node))]
for fixture, group, marker in [('t37', 'T37 pitched transfers', 'T37 readback'),
                               ('t10_debug', 'T10 blit debug group', 'T10 blit operations')]:
    root = ET.parse(directory / f'{fixture}.xml').getroot()
    assert named(chunks(root, 'pushDebugGroup')[0], 'string').text == group
    assert named(chunks(root, 'insertDebugSignpost')[0], 'string').text == marker
    assert len(chunks(root, 'popDebugGroup')) == 1
    if fixture != 't37':
        continue
    uploads = chunks(root, 'copyFromBuffer', 'destinationTexture')
    reads = chunks(root, 'copyFromTexture', 'destinationBuffer')
    ranged = chunks(root, 'copyFromTexture', 'sliceCount')
    assert len(uploads) == len(reads) == 2
    assert len(ranged) == len(chunks(root, 'copyFromTexture_toTexture')) == 1
    for node in uploads:
        assert [int(named(node, f).text) for f in
                ('sourceOffset', 'sourceBytesPerRow', 'sourceBytesPerImage', 'options')] == \
               [256, 256, 512, 0]
        assert [int(child.text) for child in named(node, 'sourceSize')] == [3, 2, 1]
    assert [int(named(node, 'destinationOffset').text) for node in reads] == [512, 1536]
    for node in reads:
        assert [int(named(node, f).text) for f in
                ('destinationBytesPerRow', 'destinationBytesPerImage', 'options')] == [256, 512, 0]
    assert [int(named(ranged[0], f).text) for f in
            ('sourceSlice', 'sourceLevel', 'destinationSlice', 'destinationLevel',
             'sliceCount', 'levelCount')] == [1, 1, 0, 1, 1, 1]
print('T37/T10 debug structured XML passed')
PY

for test in output lifecycle; do
  clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
    -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
    "${REPO_ROOT}/util/test/metal/metal_replay_${test}_smoke.mm" \
    -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
    -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_${test}_smoke"
done
for fixture in t37 t10_debug; do
  "${BUILD_DIR}/metal_replay_output_smoke" "${CAPTURE_DIR}/${fixture}_capture.rdc" \
    "${CAPTURE_DIR}/${fixture}_replay.ppm"
  "${CMD}" replay --loops 3 "${CAPTURE_DIR}/${fixture}_capture.rdc"
done
python3 "${REPO_ROOT}/util/test/metal/metal_blit_transfer_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t37_capture.rdc"
# Lifecycle requires a no-draw capture first, then draw captures. Generate a batch-local clear
# capture so this script can also run in an empty capture directory without earlier phases.
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t38_empty" \
DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${DEMO}" Metal_Empty_Frame --frames 8
"${BUILD_DIR}/metal_replay_lifecycle_smoke" "${CAPTURE_DIR}/t38_empty_capture.rdc" \
  "${CAPTURE_DIR}/t37_capture.rdc" \
  "${CAPTURE_DIR}/t10_debug_capture.rdc" 10
echo "Metal phase 38 capture batch passed (GUI QA deferred)."
