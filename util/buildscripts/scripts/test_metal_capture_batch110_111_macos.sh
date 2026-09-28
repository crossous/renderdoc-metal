#!/bin/bash
# Async dynamic source and async executable source with dynamic dependency.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
mkdir -p "${CAPTURE_DIR}"
for number in 110 111; do
  if (( number == 110 )); then variant=RENDERDOC_METAL_T110_ASYNC_DYNAMIC
  else variant=RENDERDOC_METAL_T111_ASYNC_EXECUTABLE; fi
  for trial in 1 2 3; do
    env MTL_DEBUG_LAYER=1 "${variant}=1" \
      "${REPO_ROOT}/bin/demos_x64" Metal_Dynamic_Library --frames 5
  done
  env MTL_DEBUG_LAYER=1 "${variant}=1" \
    RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t${number}" \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" Metal_Dynamic_Library --frames 3
  "${CMD}" convert -f "${CAPTURE_DIR}/t${number}_capture.rdc" \
    -o "${CAPTURE_DIR}/t${number}.zip.xml" -c zip.xml
  python3 - "${CAPTURE_DIR}/t${number}.zip.xml" "$number" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
number = int(sys.argv[2])
sources = [c for c in chunks if c.get('name') in (
    'MTLDevice::newLibraryWithSource',
    'MTLDevice::newLibraryWithSource(completionHandler)')]
dynamic = next(c for c in chunks if c.get('name') == 'MTLDevice::newDynamicLibrary')
field = lambda node, name: next(x for x in node if x.get('name') == name)
assert len(sources) == 2
assert sources[0 if number == 110 else 1].get('name').endswith('(completionHandler)')
assert field(sources[0],'libraryType').text == '1'
assert field(sources[1],'libraryType').text == '0'
assert field(dynamic,'library').text == field(sources[0],'Library').text
assert field(sources[1],'dependencies')[0].text == field(dynamic,'DynamicLibrary').text
print(f'T{number} async source compile options and dynamic dependency captured')
PY
done
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
for number in 110 111; do
  MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t${number}_capture.rdc"
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t${number}_capture.rdc" "${CAPTURE_DIR}/t${number}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_library_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t${number}_capture.rdc"
done
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t48 t51 t55 t104 t105 t106 t107 t108 t109 t110 t111
echo "T110/T111 targeted terminal validation passed."
