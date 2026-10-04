#!/bin/bash
# Serial GPU checks for marker ownership, future snapshots and submission-prefix seeks.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-event-navigation.XXXXXX")"
echo "Event navigation logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 2 >"${LOG_DIR}/build.log" 2>&1
for fixture in interleaved_marker future_descriptor_snapshot; do
  clang++ -std=c++17 -fobjc-arc -I. "util/test/metal/metal_${fixture}_capture.mm" \
    -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/${fixture}-capture"
  clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. "util/test/metal/metal_${fixture}_replay.cpp" \
    -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/${fixture}-replay"
done
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
env MTL_DEBUG_LAYER=1 "${LOG_DIR}/interleaved_marker-capture" >"${LOG_DIR}/marker-native.log" 2>&1
env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/marker" "${LOG_DIR}/interleaved_marker-capture" \
  >"${LOG_DIR}/marker-capture.log" 2>&1
env MTL_DEBUG_LAYER=1 "${LOG_DIR}/interleaved_marker-replay" "${LOG_DIR}/marker_capture.rdc" \
  >"${LOG_DIR}/marker-replay.log" 2>&1
for retention in retained unretained; do
  for scenario in alias nonoverlap producer tail split_binding; do
    run=(env MTL_DEBUG_LAYER=1)
    [[ "$retention" != unretained ]] || run+=(RENDERDOC_METAL_UNRETAINED_SUBMISSIONS=1)
    prefix="${LOG_DIR}/${retention}-${scenario}"
    args=("${prefix}_capture.rdc")
    case "$scenario" in
      nonoverlap) run+=(RENDERDOC_METAL_NONOVERLAPPING_FUTURE_TABLE=1) ;;
      producer) run+=(RENDERDOC_METAL_INTERLEAVED_SUBMISSIONS=1); args+=(--interleaved) ;;
      tail) run+=(RENDERDOC_METAL_INTERLEAVED_SUBMISSIONS=tail); args+=(--interleaved-tail) ;;
      split_binding) run+=(RENDERDOC_METAL_SPLIT_DESCRIPTOR_BINDING=1) ;;
    esac
    "${run[@]}" "${LOG_DIR}/future_descriptor_snapshot-capture" >"${prefix}-native.log" 2>&1
    "${run[@]}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
      RENDERDOC_METAL_CAPTURE_PATH="$prefix" "${LOG_DIR}/future_descriptor_snapshot-capture" \
      >"${prefix}-capture.log" 2>&1
    "${run[@]}" "${LOG_DIR}/future_descriptor_snapshot-replay" "${args[@]}" \
      >"${prefix}-replay.log" 2>&1
    if [[ "$scenario" == alias ]]; then
      python3 util/test/metal/metal_future_descriptor_snapshot_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
        "${LOG_DIR}/opener" "${prefix}_capture.rdc" "${prefix}-gate" >"${prefix}-gate.log" 2>&1
    elif [[ "$scenario" == split_binding ]]; then
      python3 util/test/metal/metal_descriptor_submission_scope_gate.py "${BUILD_DIR}/bin/renderdoccmd" \
        "${LOG_DIR}/opener" "${prefix}_capture.rdc" "${prefix}-gate" >"${prefix}-gate.log" 2>&1
    fi
  done
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS marker owners and unique continuation IDs; retained/unretained alias gates, fresh ranges, late producers, producer tails and delayed unrelated slot binding; EID0 reset Native readbacks'
