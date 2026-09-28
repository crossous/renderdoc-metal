#!/bin/bash
# One-layer rasterization rate map, render pass and parameter copy; terminal only.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${DEMO_BUILD_DIR}" -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
mkdir -p "${CAPTURE_DIR}"
for trial in 1 2 3 4 5; do
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T95_RATE_MAP_PROBE=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Mesh --frames 3
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T95_RATE_MAP_PROBE=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t95" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Mesh --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t95_capture.rdc" \
  -o "${CAPTURE_DIR}/t95.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t95.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
assert sum(node.get('id') == '1030' for node in chunks) == 1
assert sum(node.get('id') == '1315' for node in chunks) == 1
passes = [node for node in chunks if node.get('name') ==
          'MTLCommandBuffer::renderCommandEncoderWithDescriptor']
assert len(passes) == 1
descriptor = next(node for node in passes[0] if node.get('name') == 'descriptor')
map_id = next(node for node in descriptor if node.get('name') == 'rasterizationRateMap')
raw_id = next(node for node in descriptor if node.get('name') == 'rasterizationRateMapId')
assert map_id.text == raw_id.text and map_id.text != '0'
print('T95 rate map creation, parameter copy and render-pass identity captured')
PY
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t95_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t95_capture.rdc" "${CAPTURE_DIR}/t95_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_rate_map_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t95_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t78 t88 t94
echo "T95 targeted terminal validation passed, including v1 capture compatibility."
