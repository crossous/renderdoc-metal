#!/bin/bash
# GPU-written ICB execution range: native/capture works; offline replay fails closed. No GUI.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
cmake --build "${BUILD_DIR}" --target renderdoccmd -j "$(sysctl -n hw.ncpu)"
mkdir -p "${CAPTURE_DIR}"
RENDERDOC_METAL_T70_GPU_INDIRECT_RANGE=1 MTL_DEBUG_LAYER=1 \
  "${REPO_ROOT}/bin/demos_x64" Metal_ICB_Operations --frames 3
RENDERDOC_METAL_T70_GPU_INDIRECT_RANGE=1 MTL_DEBUG_LAYER=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t70_gpu_icb_range" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_ICB_Operations --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t70_gpu_icb_range_capture.rdc" \
  -o "${CAPTURE_DIR}/t70_gpu_icb_range.zip.xml" -c zip.xml
python3 - "${REPO_ROOT}" "${CMD}" "${CAPTURE_DIR}/t70_gpu_icb_range_capture.rdc" \
  "${CAPTURE_DIR}/t70_gpu_icb_range.zip.xml" <<'PY'
import pathlib
import sys
import xml.etree.ElementTree as ET
sys.path.insert(0, str(pathlib.Path(sys.argv[1]) / 'util/test/metal'))
from metal_compute_inline_invalid import run
chunks = ET.parse(sys.argv[4]).findall('./chunks/chunk')
indirect = [chunk for chunk in chunks if chunk.get('id') == '1185']
assert len(indirect) == 1, len(indirect)
assert indirect[0].get('name') == 'MTLRenderCommandEncoder::executeCommandsInBuffer (indirect range)'
fields = {element.get('name'): element for element in indirect[0]}
assert fields['rangeBuffer'].text not in ('0', None)
assert fields['offset'].text == '0'
message = run(sys.argv[2], 'replay', '--loops', '1', sys.argv[3], success=False)
assert 'executeCommandsInBuffer (indirect range)' in message, message
print('T70 GPU-written ICB range: native/capture passed, one recorded chunk, replay rejected explicitly')
PY
