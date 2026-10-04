#!/bin/bash
# Vulkan original-fragment/stencil-mask proof, followed immediately by current UE.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
if [[ $# -ne 0 && $# -ne 2 ]]; then echo "Usage: bash $0 [current-ue.rdc EID]" >&2;exit 2;fi
if pgrep -x qrenderdoc >/dev/null || pgrep -x UnrealEditor >/dev/null; then
  echo 'Close qrenderdoc/UnrealEditor before serial GPU tests.' >&2;exit 2
fi
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-depth-export.XXXXXX")"
cd "${REPO_ROOT}";echo "Depth export evidence: ${LOG_DIR}"
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-before.sha256"
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_test_overlay_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -I. util/test/metal/metal_clear_before_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/mrt-capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_test_overlay_replay.cpp \
  -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
RENDERDOC_METAL_OVERLAY_WRITE_SOURCE="${LOG_DIR}/source.metal" "${LOG_DIR}/capture" depth-export
xcrun -sdk macosx metal -c "${LOG_DIR}/source.metal" -o "${LOG_DIR}/source.air"
xcrun -sdk macosx metallib "${LOG_DIR}/source.air" -o "${LOG_DIR}/source.metallib"
for mode in serial parallel binary mrt less greater interleaved; do
  program="${LOG_DIR}/capture";capture_mode=depth-export
  [[ "$mode" != parallel ]] || capture_mode=depth-export-parallel
  [[ "$mode" != less ]] || capture_mode=depth-export-less
  [[ "$mode" != greater ]] || capture_mode=depth-export-greater
  [[ "$mode" != interleaved ]] || capture_mode=depth-export-interleaved
  [[ "$mode" != mrt ]] || program="${LOG_DIR}/mrt-capture"
  native_env=("MTL_DEBUG_LAYER=1");[[ "$mode" != binary ]] || native_env+=("RENDERDOC_METAL_OVERLAY_METALLIB=${LOG_DIR}/source.metallib")
  env "${native_env[@]}" "$program" "$capture_mode" >"${LOG_DIR}/native-${mode}.log" 2>&1
  env "${native_env[@]}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${mode}" "$program" "$capture_mode" >"${LOG_DIR}/capture-${mode}.log" 2>&1
  replay_mode=depth-export;[[ "$mode" != mrt ]] || replay_mode=depth-export-mrt
  [[ "$mode" != less ]] || replay_mode=depth-export-less
  [[ "$mode" != greater ]] || replay_mode=depth-export-greater
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_OVERLAY=1 "${LOG_DIR}/replay" "${LOG_DIR}/${mode}_capture.rdc" "$replay_mode" \
    >"${LOG_DIR}/replay-${mode}.log" 2>&1
done
if [[ $# -eq 2 ]]; then
  MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_OVERLAY=1 RENDERDOC_METAL_OVERLAY_DUMP_DIR="${LOG_DIR}" "${LOG_DIR}/replay" "$1" "$2" >"${LOG_DIR}/ue.log" 2>&1
  sha_inputs=("$1")
  for image in "${LOG_DIR}"/ue-*.bin; do [[ ! -f "$image" ]] || sha_inputs+=("$image");done
  shasum -a 256 "${sha_inputs[@]}" >"${LOG_DIR}/ue.sha256"
fi
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-after.sha256"
cmp "${LOG_DIR}/library-before.sha256" "${LOG_DIR}/library-after.sha256"
echo 'PASS original fragment depth, parallel, compiled AIR and MRT/discard masks; original resources restored'
