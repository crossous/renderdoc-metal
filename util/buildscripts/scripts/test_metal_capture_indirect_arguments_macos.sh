#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-indirect-evidence.XXXXXX")"
echo "Indirect capture evidence logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_compute_indirect_reuse_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_compute_indirect_reuse_replay.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
export RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS=1
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
for variant in inline buffer concurrent_inline concurrent_buffer unretained_inline unretained_buffer unretained_concurrent_inline unretained_concurrent_buffer; do
  unset RENDERDOC_METAL_INDIRECT_BUFFER_WEIGHT RENDERDOC_METAL_INDIRECT_CONCURRENT RENDERDOC_METAL_INDIRECT_UNRETAINED
  value=0
  if [[ "$variant" == *concurrent_* ]]; then export RENDERDOC_METAL_INDIRECT_CONCURRENT=1; fi
  if [[ "$variant" == unretained_* ]]; then export RENDERDOC_METAL_INDIRECT_UNRETAINED=1; fi
  if [[ "$variant" == *buffer ]]; then export RENDERDOC_METAL_INDIRECT_BUFFER_WEIGHT=1; value=1; fi
  env MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/${variant}-native.log" 2>&1
  env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${variant}" "${LOG_DIR}/capture" >"${LOG_DIR}/${variant}-capture.log" 2>&1
  env MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${LOG_DIR}/${variant}_capture.rdc" "$value" >"${LOG_DIR}/${variant}-replay.log" 2>&1
  python3 util/test/metal/metal_capture_indirect_evidence_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${variant}_capture.rdc" "${LOG_DIR}/${variant}-evidence-gate" >"${LOG_DIR}/${variant}-evidence-gate.log" 2>&1
done
echo "PASS per-use indirect arguments, eight captures, 56 reset/seek checks and per-use capture proof, serial/concurrent encoders, inline and batch/offset buffer restoration, GPU rewrite and final-zero arguments, retained/unretained CB, malformed proof rejected before frame GPU work"
