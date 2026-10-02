#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-buffer-texture.XXXXXX")"
echo "Buffer texture initial logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. \
  util/test/metal/metal_descriptor_resources_capture.mm -framework Foundation -framework Metal \
  -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. \
  util/test/metal/metal_descriptor_resources_replay.mm -framework Foundation -framework Metal \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. \
  util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/open_probe"
for format in rgba; do
  capture_environment=(env RENDERDOC_METAL_DESCRIPTOR_PRIVATE_TEXTURE=1 RENDERDOC_METAL_DESCRIPTOR_BUFFER_TEXTURE=1 MTL_DEBUG_LAYER=1)
  if [[ "${RENDERDOC_METAL_DESCRIPTOR_PRIVATE_TEXTURE_VIEW:-0}" == 1 ]]; then capture_environment+=(RENDERDOC_METAL_DESCRIPTOR_PRIVATE_TEXTURE_VIEW=1); fi
  if [[ "$format" == bgra ]]; then capture_environment+=(RENDERDOC_METAL_DESCRIPTOR_BGRA_TEXTURE=1); fi
  "${capture_environment[@]}" "${LOG_DIR}/capture" >"${LOG_DIR}/${format}-native.log" 2>&1
  "${capture_environment[@]}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${format}" "${LOG_DIR}/capture" >"${LOG_DIR}/${format}-capture.log" 2>&1
  read -r va table texture sampler < <(python3 - "${LOG_DIR}/${format}-capture.log" <<'PY'
import re, sys
from pathlib import Path
m = re.search(r'VA_A=(\d+) VA_B=(\d+) VA_TABLE=(\d+) TEX=(\d+) SAMP=(\d+)', Path(sys.argv[1]).read_text())
assert m, 'Missing native capture result'
print(m[1], m[3], m[4], m[5])
PY
  )
  for suffix in '' '_2'; do
    MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_INITIAL_PRIVATE=1 "${LOG_DIR}/replay" \
      "${LOG_DIR}/${format}_capture${suffix}.rdc" "$va" "$table" "$texture" "$sampler" private \
      >"${LOG_DIR}/${format}-replay${suffix}.log" 2>&1
    python3 util/test/metal/metal_buffer_texture_initial_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
      "${LOG_DIR}/open_probe" "${LOG_DIR}/${format}_capture${suffix}.rdc" "${LOG_DIR}/${format}-gate${suffix}" \
      >"${LOG_DIR}/${format}-gate${suffix}.log" 2>&1
  done
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo "PASS Private TextureBuffer: two captures, eight seeks, GPU descriptor read 122, offset 256, overwrite/reset pixels and PickPixel, negative gates"
