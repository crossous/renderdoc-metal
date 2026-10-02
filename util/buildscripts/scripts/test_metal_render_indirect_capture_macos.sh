#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-render-indirect-evidence.XXXXXX")"
echo "Render indirect capture evidence logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 2 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_render_indirect_evidence_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture" >"${LOG_DIR}/fixture-build.log" 2>&1
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_render_indirect_evidence_replay.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
export RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS=1
for variant in serial parallel indexed indexed_parallel unretained_serial unretained_parallel unretained_indexed unretained_indexed_parallel; do
  unset RENDERDOC_METAL_RENDER_INDIRECT_INDEXED RENDERDOC_METAL_RENDER_INDIRECT_PARALLEL RENDERDOC_METAL_RENDER_INDIRECT_UNRETAINED
  if [[ "$variant" == *indexed* ]]; then export RENDERDOC_METAL_RENDER_INDIRECT_INDEXED=1; fi
  if [[ "$variant" == *parallel* ]]; then export RENDERDOC_METAL_RENDER_INDIRECT_PARALLEL=1; fi
  if [[ "$variant" == unretained_* ]]; then export RENDERDOC_METAL_RENDER_INDIRECT_UNRETAINED=1; fi
  env MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/${variant}-native.log" 2>&1
  env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${variant}" "${LOG_DIR}/capture" >"${LOG_DIR}/${variant}-capture.log" 2>&1
  env MTL_DEBUG_LAYER=1 "${LOG_DIR}/opener" "${LOG_DIR}/${variant}_capture.rdc" >"${LOG_DIR}/${variant}-open.log" 2>&1
  env MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${LOG_DIR}/${variant}_capture.rdc" >"${LOG_DIR}/${variant}-replay.log" 2>&1
  python3 util/test/metal/metal_render_indirect_evidence_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${variant}_capture.rdc" "${LOG_DIR}/${variant}-gate" >"${LOG_DIR}/${variant}-gate.log" 2>&1
done
for variant in undeclared write_alias; do
  unset RENDERDOC_METAL_RENDER_INDIRECT_INDEXED RENDERDOC_METAL_RENDER_INDIRECT_PARALLEL RENDERDOC_METAL_RENDER_INDIRECT_UNRETAINED
  if [[ "$variant" == undeclared ]]; then export RENDERDOC_METAL_RENDER_INDIRECT_UNDECLARED=1; else export RENDERDOC_METAL_RENDER_INDIRECT_WRITE_ALIAS=1; fi
  env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${variant}" "${LOG_DIR}/capture" >"${LOG_DIR}/${variant}-capture.log" 2>&1
  set +e
  env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_REPLAY_WAITS=1 "${LOG_DIR}/opener" "${LOG_DIR}/${variant}_capture.rdc" >"${LOG_DIR}/${variant}-open.log" 2>&1
  result=$?
  set -e
  [[ "$result" == 1 ]]
  ! rg -q 'Metal replay wait begin' "${LOG_DIR}/${variant}-open.log"
  unset RENDERDOC_METAL_RENDER_INDIRECT_UNDECLARED RENDERDOC_METAL_RENDER_INDIRECT_WRITE_ALIAS
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-end-hash.log"
cmp "${LOG_DIR}/library-hash.log" "${LOG_DIR}/library-end-hash.log"
echo 'PASS eight render indirect capture/open/replay variants, 56 reset/seeks, Native and replay pixels, per-use action counts 3/6, malformed proof groups, missing declaration/write alias rejected before frame GPU work'
