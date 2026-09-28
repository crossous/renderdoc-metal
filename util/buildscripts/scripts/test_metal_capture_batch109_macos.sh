#!/bin/bash
# Options/reflection render pipeline with a preloaded fragment dynamic library.
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
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T109_RENDER_OPTIONS=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Dynamic_Library --frames 5
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T109_RENDER_OPTIONS=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t109" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Dynamic_Library --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t109_capture.rdc" \
  -o "${CAPTURE_DIR}/t109.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t109.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
dynamic = next(c for c in chunks if c.get('name') == 'MTLDevice::newDynamicLibrary')
field = lambda node, name: next(x for x in node if x.get('name') == name)
pipeline = next(c for c in chunks if c.get('name') ==
                'MTLDevice::newRenderPipelineStateWithDescriptor' and
                any(x.get('name') == 'supported' for x in c))
assert field(pipeline,'supported').text == 'true'
assert field(pipeline,'optionsValue').text == '1'
descriptor = field(pipeline,'descriptor')
assert field(descriptor,'fragmentPreloadedLibraries')[0].text == \
       field(dynamic,'DynamicLibrary').text
print('T109 render options/reflection pipeline preloaded dependency captured')
PY
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t109_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t109_capture.rdc" "${CAPTURE_DIR}/t109_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_render_preload_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t109_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t47 t48 t55 t104 t105 t106 t107 t108 t109
echo "T109 targeted terminal validation passed."
