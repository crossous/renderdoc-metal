#!/bin/bash
# Complete source -> dynamic-library -> executable -> GPU chain; terminal-only QA.
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
  MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Dynamic_Library --frames 5
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t104" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Dynamic_Library --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t104_capture.rdc" \
  -o "${CAPTURE_DIR}/t104.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t104.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
sources = [c for c in chunks if c.get('name') == 'MTLDevice::newLibraryWithSource']
dynamic = [c for c in chunks if c.get('name') == 'MTLDevice::newDynamicLibrary']
assert len(sources) == 2 and len(dynamic) == 1
field = lambda node, name: next(x for x in node if x.get('name') == name)
assert field(sources[0],'libraryType').text == '1'
assert field(sources[0],'installName').text
assert field(dynamic[0],'library').text == field(sources[0],'Library').text
assert field(sources[1],'libraryType').text == '0'
assert field(sources[1],'dependencies')[0].text == field(dynamic[0],'DynamicLibrary').text
assert all(field(c,'supported').text == 'true' for c in (*sources,*dynamic))
print('T104 dynamic-library source, identity and executable dependency captured')
PY
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t104_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t104_capture.rdc" "${CAPTURE_DIR}/t104_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_library_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t104_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t48 t55 t101 t103 t104
echo "T104 targeted terminal validation passed."
