#!/bin/bash
# Exercise read-only Metal device queries natively and through the injected bridge.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  "${REPO_ROOT}/util/test/metal/metal_device_queries_smoke.mm" \
  -framework Foundation -framework Metal -o "${BUILD_DIR}/metal_device_queries_smoke"
cmake --build "${BUILD_DIR}" --target renderdoccmd -j "$(sysctl -n hw.ncpu)"
native_result="$("${BUILD_DIR}/metal_device_queries_smoke")"
printf '%s\n' "${native_result}"
if [[ "${native_result}" == *"raytracing=1"* ]]; then
  RENDERDOC_METAL_FORCE_RAY_QUERY=1 RENDERDOC_METAL_EXPECT_BRIDGE=1 \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${BUILD_DIR}/metal_device_queries_smoke"
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_FORCE_RAY_QUERY=1 RENDERDOC_METAL_EXPECT_BRIDGE=1 \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${BUILD_DIR}/metal_device_queries_smoke"
else
  RENDERDOC_METAL_EXPECT_BRIDGE=1 \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${BUILD_DIR}/metal_device_queries_smoke"
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_EXPECT_BRIDGE=1 \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${BUILD_DIR}/metal_device_queries_smoke"
fi
