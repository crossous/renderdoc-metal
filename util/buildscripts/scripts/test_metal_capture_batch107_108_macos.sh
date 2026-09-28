#!/bin/bash
# Render pipeline fragment/vertex dynamic-library preload; terminal-only QA.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
mkdir -p "${CAPTURE_DIR}"
for number in 107 108; do
  if (( number == 107 )); then variant=RENDERDOC_METAL_T107_RENDER_PRELOADED
  else variant=RENDERDOC_METAL_T108_VERTEX_PRELOADED; fi
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
pipeline = next(c for c in chunks if c.get('name') ==
                'MTLDevice::newRenderPipelineStateWithDescriptor')
field = lambda node, name: next(x for x in node if x.get('name') == name)
descriptor = field(pipeline,'descriptor')
stage = 'fragment' if number == 107 else 'vertex'
other = 'vertex' if number == 107 else 'fragment'
assert field(descriptor,stage+'PreloadedLibraries')[0].text == \
       field(dynamic,'DynamicLibrary').text
assert len(field(descriptor,other+'PreloadedLibraries')) == 0
print(f'T{number} {stage} preloaded dynamic-library identity captured')
PY
done
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
for number in 107 108; do
  MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t${number}_capture.rdc"
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t${number}_capture.rdc" "${CAPTURE_DIR}/t${number}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_render_preload_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t${number}_capture.rdc"
done
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t47 t48 t55 t104 t105 t106 t107 t108
echo "T107/T108 targeted terminal validation passed."
