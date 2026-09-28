#!/bin/bash
# Object-stage sampled texture/sampler capture, replay and negative checks; terminal only.
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
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T85_OBJECT_SAMPLE=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Object --frames 3
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T85_OBJECT_SAMPLE=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t85" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Object --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t85_capture.rdc" \
  -o "${CAPTURE_DIR}/t85.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t85.zip.xml" <<'PY'
import collections
import sys
import xml.etree.ElementTree as ET
count = collections.Counter(x.get('id') for x in ET.parse(sys.argv[1]).findall('./chunks/chunk'))
assert {key: count[key] for key in map(str, range(1306,1312))} == {
    '1306':2, '1307':1, '1308':1, '1309':1, '1310':1, '1311':1}
print('T85 six object texture/sampler overloads captured')
PY
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t85_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t85_capture.rdc" "${CAPTURE_DIR}/t85_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_object_resources_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t85_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t78 t79 t80 t81 t82 t83 t84
echo "T85 targeted terminal validation passed; full replay regression and GUI QA remain separate."
