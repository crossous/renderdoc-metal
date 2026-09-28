#!/bin/bash
# Async render/compute pipeline descriptor dynamic-library preloads.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
mkdir -p "${CAPTURE_DIR}"
for entry in 112:RENDERDOC_METAL_T112_ASYNC_RENDER 113:RENDERDOC_METAL_T113_ASYNC_RENDER_OPTIONS 114:RENDERDOC_METAL_T114_ASYNC_COMPUTE 115:RENDERDOC_METAL_T115_ASYNC_VERTEX; do
  number="${entry%%:*}"
  variant="${entry#*:}"
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
dynamic = next(c for c in chunks if c.get('name') == 'MTLDevice::newDynamicLibrary')
field = lambda node, name: next(x for x in node if x.get('name') == name)
if number == 114:
    pipeline = next(c for c in chunks if c.get('name') ==
                    'MTLDevice::newComputePipelineStateWithDescriptor(completionHandler)')
    array = field(field(pipeline,'descriptor'),'preloadedLibraries')
else:
    suffix = ('options, completionHandler' if number == 113 else 'completionHandler')
    pipeline = next(c for c in chunks if c.get('name') ==
                    f'MTLDevice::newRenderPipelineStateWithDescriptor({suffix})')
    stage = 'vertex' if number == 115 else 'fragment'
    array = field(field(pipeline,'descriptor'),stage+'PreloadedLibraries')
assert len(array) == 1 and array[0].text == field(dynamic,'DynamicLibrary').text
assert field(pipeline,'supported').text == 'true'
print(f'T{number} async pipeline preloaded dynamic-library identity captured')
PY
done
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
for number in 112 113 114 115; do
  MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t${number}_capture.rdc"
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t${number}_capture.rdc" "${CAPTURE_DIR}/t${number}_validation.ppm"
  if (( number == 114 )); then invalid=metal_dynamic_preload_invalid.py
  else invalid=metal_dynamic_render_preload_invalid.py; fi
  python3 "${REPO_ROOT}/util/test/metal/${invalid}" \
    "${CMD}" "${CAPTURE_DIR}/t${number}_capture.rdc"
done
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t47 t48 t51 t55 t104 t106 t109 t110 t111 t112 t113 t114 t115
echo "T112–T115 targeted terminal validation passed."
