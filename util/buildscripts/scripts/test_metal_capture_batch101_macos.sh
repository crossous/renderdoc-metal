#!/bin/bash
# Stage-boundary timestamp resource/pass/resolve chain; terminal-only QA.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
mkdir -p "${CAPTURE_DIR}"
for trial in 1 2 3; do
  MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Counter_Stage --frames 5
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t101" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Counter_Stage --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t101_capture.rdc" \
  -o "${CAPTURE_DIR}/t101.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t101.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
names = {c.get('name') for c in chunks}
assert {'MTLDevice::newCounterSampleBufferWithDescriptor',
        'MTLCommandBuffer::renderCommandEncoderWithDescriptor',
        'MTLBlitCommandEncoder::resolveCounters'} <= names
pass_chunk = next(c for c in chunks if c.get('name') ==
                  'MTLCommandBuffer::renderCommandEncoderWithDescriptor')
attachment = pass_chunk.find("./struct[@name='descriptor']/array[@name='sampleBufferAttachments']/struct")
assert attachment is not None
ids = [c.text for c in attachment if c.get('name') in ('sampleBuffer','sampleBufferId')]
assert len(ids) == 2 and ids[0] == ids[1] and ids[0] != '0'
print('T101 stage-boundary counter descriptor and identity captured')
PY
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t101_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t101_capture.rdc" "${CAPTURE_DIR}/t101_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_counter_stage_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t101_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t78 t95 t97 t98 t99 t100 t101
echo "T101 targeted terminal validation passed."
