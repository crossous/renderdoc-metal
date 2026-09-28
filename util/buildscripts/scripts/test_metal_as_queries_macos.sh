#!/bin/bash
# Static triangle acceleration-structure size queries, native vs injected.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
native=$(env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T134_AS_SIZE_QUERY=1 \
  "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3 2>&1)
injected=$(env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T134_AS_SIZE_QUERY=1 \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3 2>&1)
native_queries=$(printf '%s\n' "$native" | sed -n 's/^.* Log: \(T134 .*query: .*\)$/\1/p')
injected_queries=$(printf '%s\n' "$injected" | sed -n 's/^.* Log: \(T134 .*query: .*\)$/\1/p')
if [[ "$(printf '%s\n' "$native_queries" | wc -l | tr -d ' ')" != 3 ||
      "$native_queries" != "$injected_queries" ]]; then
  printf 'Native:\n%s\nInjected:\n%s\n' "$native" "$injected" >&2
  exit 1
fi
mkdir -p "$CAPTURE_DIR"
env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_T134_AS_SIZE_QUERY=1 \
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t134" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Private_Buffer --frames 3
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" \
  t134 t71 t116 t117 t131 t132
printf 'T134 static primitive AS query parity passed:\n%s\n' "$native_queries"
