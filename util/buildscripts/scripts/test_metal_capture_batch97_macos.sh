#!/bin/bash
# Two-layer rasterization-rate map creation and parameter data; terminal-only QA.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${REPO_ROOT}/build-metal-demos" -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
mkdir -p "${CAPTURE_DIR}"
for trial in 1 2 3 4 5; do
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T97_RATE_MAP_TWO_LAYERS=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Mesh --frames 3
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T97_RATE_MAP_TWO_LAYERS=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t97" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Mesh --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t97_capture.rdc" \
  -o "${CAPTURE_DIR}/t97.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t97.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
creation = [c for c in chunks if c.get('name') ==
            'MTLDevice::newRasterizationRateMapWithDescriptor']
assert len(creation) == 1
extra_h = next(c for c in creation[0] if c.get('name') == 'extraHorizontal')
extra_v = next(c for c in creation[0] if c.get('name') == 'extraVertical')
assert len(extra_h) == len(extra_v) == 1
assert [float(c.text) for c in extra_h[0]] == [0.5, 0.5]
assert [float(c.text) for c in extra_v[0]] == [1.0, 1.0]
assert sum(c.get('id') == '1315' for c in chunks) == 1
print('T97 two-layer map and parameter copy captured')
PY
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t97_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t97_capture.rdc" "${CAPTURE_DIR}/t97_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_rate_map_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t97_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t78 t88 t94 t95 t96
echo "T97 targeted terminal validation passed, including v1/v2 compatibility."
