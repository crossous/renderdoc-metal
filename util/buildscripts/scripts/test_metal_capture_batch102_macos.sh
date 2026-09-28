#!/bin/bash
# Reflection MTLBufferBinding argument encoder; terminal-only QA.
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
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T102_BINDING_ENCODER=1 \
    "${REPO_ROOT}/bin/demos_x64" Metal_Device_Argument_Encoder --frames 3
done
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T102_BINDING_ENCODER=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t102" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Device_Argument_Encoder --frames 3
"${CMD}" convert -f "${CAPTURE_DIR}/t102_capture.rdc" \
  -o "${CAPTURE_DIR}/t102.zip.xml" -c zip.xml
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_output_smoke"
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t102_capture.rdc"
MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
  "${CAPTURE_DIR}/t102_capture.rdc" "${CAPTURE_DIR}/t102_validation.ppm"
python3 "${REPO_ROOT}/util/test/metal/metal_binding_encoder_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t102_capture.rdc"
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" t01 t57 t60 t95 t100 t101 t102
echo "T102 targeted terminal validation passed."
