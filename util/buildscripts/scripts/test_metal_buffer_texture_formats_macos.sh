#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-buffer-formats.XXXXXX")"
echo "Buffer texture format logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_buffer_texture_formats_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_buffer_texture_formats_replay.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/open_probe"
run_bounded() {
  python3 - "$@" <<'PY'
import subprocess,sys
try:sys.exit(subprocess.run(sys.argv[1:],timeout=60).returncode)
except subprocess.TimeoutExpired:sys.exit(124)
PY
}
run_bounded env MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/native.log" 2>&1
run_bounded env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/formats" "${LOG_DIR}/capture" >"${LOG_DIR}/capture.log" 2>&1
for suffix in '' '_2'; do
  run_bounded env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_INITIAL_PRIVATE=1 "${LOG_DIR}/replay" \
    "${LOG_DIR}/formats_capture${suffix}.rdc" >"${LOG_DIR}/replay${suffix}.log" 2>&1
  python3 util/test/metal/metal_buffer_texture_formats_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
    "${LOG_DIR}/open_probe" "${LOG_DIR}/formats_capture${suffix}.rdc" "${LOG_DIR}/gate${suffix}" >"${LOG_DIR}/gate${suffix}.log" 2>&1
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo "PASS TextureBuffer16: native float/uint/sint decoding, two captures, 128 raw/PickPixel checks, eight seek cycles, 28 API+CLI negative groups"
