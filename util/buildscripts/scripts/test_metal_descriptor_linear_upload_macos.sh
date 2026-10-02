#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-linear-upload.XXXXXX")"
echo "Linear upload logs: ${LOG_DIR}"
cd "$REPO_ROOT"
cmake --build "$BUILD_DIR" --target renderdoc renderdoccmd -j2 >"$LOG_DIR/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_linear_upload_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "$LOG_DIR/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_linear_upload_replay.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/opener"
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-hash.log"
for format in 10 81;do
 export RENDERDOC_METAL_LINEAR_UPLOAD_FORMAT="$format"
 env MTL_DEBUG_LAYER=1 "$LOG_DIR/capture" >"$LOG_DIR/native-$format.log" 2>&1
 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="$LOG_DIR/frame-$format" "$LOG_DIR/capture" >"$LOG_DIR/capture-$format.log" 2>&1
 bpp=1;if [[ "$format" == 81 ]];then bpp=4;fi
 for suffix in '' '_2';do
  frame=0;if [[ -n "$suffix" ]];then frame=1;fi
  env MTL_DEBUG_LAYER=1 "$LOG_DIR/replay" "$LOG_DIR/frame-${format}_capture${suffix}.rdc" "$bpp" "$frame" >"$LOG_DIR/replay-${format}${suffix}.log" 2>&1
 done
 python3 util/test/metal/metal_descriptor_linear_upload_gate.py "$BUILD_DIR/bin/renderdoccmd" "$LOG_DIR/opener" "$LOG_DIR/frame-${format}_capture.rdc" "$LOG_DIR/gate-$format" >"$LOG_DIR/gate-$format.log" 2>&1
done
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-end-hash.log"
cmp "$LOG_DIR/library-hash.log" "$LOG_DIR/library-end-hash.log"
echo 'PASS sourced linear upload: R8/BGRA8_sRGB, 4 captures/16 seeks/all pixels/first copy/partial patch/initial reset, original Native blits and frame staging snapshots'
