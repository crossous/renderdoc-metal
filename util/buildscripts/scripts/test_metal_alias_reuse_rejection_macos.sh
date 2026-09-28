#!/bin/bash
# Native placement alias reuse works; replay must reject until timeline-aware recreation exists.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
mkdir -p "${CAPTURE_DIR}"
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T133_ALIAS_REUSE_PROBE=1 \
  "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T133_ALIAS_REUSE_PROBE=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t133" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
python3 - "${BUILD_DIR}/bin/renderdoccmd" "${CAPTURE_DIR}/t133_capture.rdc" <<'PY'
import subprocess
import sys
result = subprocess.run([sys.argv[1], 'replay', '--loops', '1', sys.argv[2]],
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                        text=True, timeout=30)
assert result.returncode == 1, (result.returncode, result.stdout)
assert 'Failed to process Metal chunk MTLHeap::newBuffer(offset)' in result.stdout, result.stdout
print('T133 native/capture succeeded; overlapping placement reuse rejected safely on replay')
PY
