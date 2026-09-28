#!/bin/bash
# Non-unit object threadgroups for both direct mesh draw APIs; terminal only.
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
for fixture in t91 t92; do
  if [[ "${fixture}" == t91 ]]; then mode=RENDERDOC_METAL_T91_OBJECT_THREADS4
  else mode=RENDERDOC_METAL_T92_OBJECT_THREAD_GRID4; fi
  for trial in 1 2 3 4 5; do
    env MTL_DEBUG_LAYER=1 "${mode}=1" "${REPO_ROOT}/bin/demos_x64" Metal_Object --frames 3
  done
  env MTL_DEBUG_LAYER=1 "${mode}=1" \
    RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/${fixture}" \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" Metal_Object --frames 3
  "${CMD}" convert -f "${CAPTURE_DIR}/${fixture}_capture.rdc" \
    -o "${CAPTURE_DIR}/${fixture}.zip.xml" -c zip.xml
  MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/${fixture}_capture.rdc"
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
done
python3 - "${CAPTURE_DIR}/t91.zip.xml" "${CAPTURE_DIR}/t92.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
for path, chunk_id in zip(sys.argv[1:], ('1288','1299')):
    chunks = ET.parse(path).findall('./chunks/chunk')
    draws = [node for node in chunks if node.get('id') == chunk_id]
    assert len(draws) == 1
    object_group = next(node for node in draws[0] if node.get('name') == 'threadsPerObjectThreadgroup')
    width = next(node for node in object_group if node.get('name') == 'width')
    assert width.text == '4'
print('T91–T92 direct object threadgroups of four captured')
PY
python3 "${REPO_ROOT}/util/test/metal/metal_object_threadgroup_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t91_capture.rdc" "${CAPTURE_DIR}/t92_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t78 t81 t83 t88 t90
echo "T91–T92 targeted terminal validation passed; full replay regression and GUI QA remain separate."
