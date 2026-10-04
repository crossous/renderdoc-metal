#!/bin/bash
# SPDX-License-Identifier: MIT
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-${BUILD_DIR}/metal-shader-processors}"
if pgrep -x qrenderdoc >/dev/null || pgrep -x UnrealEditor >/dev/null; then
  echo 'Close qrenderdoc/UE before serial GPU replacement verification.' >&2; exit 2
fi
cd "${REPO_ROOT}"
mkdir -p "${RESULT_DIR}"
if [ ! -f "${BUILD_DIR}/metal-shader-edit/source_capture.rdc" ]; then
  RENDERDOC_METAL_RESULT_DIR="${BUILD_DIR}/metal-shader-edit" bash util/buildscripts/scripts/test_metal_shader_edit_macos.sh
fi
/usr/bin/python3 util/test/metal/metal_shader_processors_test.py "${BUILD_DIR}/bin/qrenderdoc.app" "${RESULT_DIR}" \
  --specialized-library "${BUILD_DIR}/metal-shader-edit/shader.metallib" > "${RESULT_DIR}/processor-directed-final.log" 2>&1
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_shader_processor_replay.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${RESULT_DIR}/replay"
"${RESULT_DIR}/replay" "${BUILD_DIR}/metal-shader-edit/source_capture.rdc" "${RESULT_DIR}/compile-msl.metallib" literal_fs \
  > "${RESULT_DIR}/literal-replay.log" 2>&1
echo "PASS bundled shader processors and native MSL round trip: ${RESULT_DIR}"
