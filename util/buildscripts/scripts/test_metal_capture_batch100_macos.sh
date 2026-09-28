#!/bin/bash
# Async mesh pipeline with non-default grid and rate-map slice 1; terminal-only QA.
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
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T82_MESH_ASYNC=1 \
    RENDERDOC_METAL_T99_RATE_MAP_LAYER_ONE=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Mesh --frames 3
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T82_MESH_ASYNC=1 \
  RENDERDOC_METAL_T99_RATE_MAP_LAYER_ONE=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t100" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Mesh --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t100_capture.rdc" \
  -o "${CAPTURE_DIR}/t100.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t100.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
pipeline = next(c for c in chunks if c.get('name') ==
                'MTLDevice::newRenderPipelineStateWithMeshDescriptor(completionHandler)')
assert next(c for c in pipeline if c.get('name') == 'maxMeshGrid').text == '2'
assert next(c for c in pipeline if c.get('name') == 'supported').text == 'true'
copy = next(c for c in chunks if c.get('name') == 'MTLBlitCommandEncoder::copyFromTexture')
assert next(c for c in copy if c.get('name') == 'sourceSlice').text == '1'
print('T100 async mesh descriptor grid 2 and second rate-map layer captured')
PY
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t100_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t100_capture.rdc" "${CAPTURE_DIR}/t100_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_mesh_async_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t100_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_rate_map_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t100_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t78 t82 t94 t95 t98 t99
echo "T100 targeted terminal validation passed, including v1/v2/v3 compatibility."
