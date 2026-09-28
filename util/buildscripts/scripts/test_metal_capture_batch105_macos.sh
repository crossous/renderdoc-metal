#!/bin/bash
# Two dynamically linked libraries and one executable; terminal-only QA.
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
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T105_MULTIPLE_DYNAMIC=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Dynamic_Library --frames 5
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T105_MULTIPLE_DYNAMIC=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t105" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Dynamic_Library --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t105_capture.rdc" \
  -o "${CAPTURE_DIR}/t105.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t105.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
sources = [c for c in chunks if c.get('name') == 'MTLDevice::newLibraryWithSource']
dynamics = [c for c in chunks if c.get('name') == 'MTLDevice::newDynamicLibrary']
field = lambda node, name: next(x for x in node if x.get('name') == name)
assert len(sources) == 3 and len(dynamics) == 2
assert [field(c,'libraryType').text for c in sources] == ['1','1','0']
for source, dynamic in zip(sources,dynamics):
    assert field(dynamic,'library').text == field(source,'Library').text
    assert field(source,'installName').text
assert [item.text for item in field(sources[2],'dependencies')] == [
    field(c,'DynamicLibrary').text for c in dynamics]
print('T105 two dynamic-library identities, install paths and ordered dependencies captured')
PY
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t105_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t105_capture.rdc" "${CAPTURE_DIR}/t105_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_multi_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t105_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t48 t55 t101 t104 t105
echo "T105 targeted terminal validation passed."
