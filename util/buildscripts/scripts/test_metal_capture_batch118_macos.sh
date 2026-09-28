#!/bin/bash
# One read-only nested argument buffer with texture/sampler child members.
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
  env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T118_NESTED_ARGUMENT=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Argument_Buffer --frames 5
done
env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T118_NESTED_ARGUMENT=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t118" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Argument_Buffer --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t118_capture.rdc" \
  -o "${CAPTURE_DIR}/t118.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t118.zip.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
chunks = ET.parse(sys.argv[1]).findall('./chunks/chunk')
field = lambda node, name: next(x for x in node if x.get('name') == name)
named = lambda name: [c for c in chunks if c.get('name') == name]
parent = named('MTLFunction::newArgumentEncoderWithBufferIndex')[0]
child = named('MTLArgumentEncoder::newArgumentEncoderForBufferAtIndex')[0]
outer = named('MTLArgumentEncoder::setBuffer')[0]
selection = named('MTLArgumentEncoder::setArgumentBuffer')
texture = named('MTLArgumentEncoder::setTexture')[0]
sampler = named('MTLArgumentEncoder::setSamplerState')[0]
assert field(child, 'ParentEncoder').text == field(parent, 'ArgumentEncoder').text
assert field(child, 'index').text == '0'
assert field(child, 'encodedLength').text == '16'
assert field(child, 'alignment').text == '8'
assert field(child, 'supported').text == 'true'
assert field(outer, 'ArgumentEncoder').text == field(parent, 'ArgumentEncoder').text
assert field(outer, 'buffer').text == field(selection[1], 'argumentBuffer').text
assert field(texture, 'ArgumentEncoder').text == field(child, 'Encoder').text
assert field(sampler, 'ArgumentEncoder').text == field(child, 'Encoder').text
print('T118 parent/child encoder, nested buffer and resource identities captured')
PY
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t118_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t118_capture.rdc" "${CAPTURE_DIR}/t118_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_nested_argument_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t118_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t12 t56 t57 t60 t102 t116 t117 t118
echo "T118 nested argument encoder targeted terminal validation passed."
