#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-frame-color-load.XXXXXX")"
echo "Frame color Load logs: ${LOG_DIR}"
cd "$REPO_ROOT"
cmake --build "$BUILD_DIR" --target renderdoc renderdoccmd -j2 >"$LOG_DIR/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_frame_color_load_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "$LOG_DIR/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_frame_color_load_replay.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/opener"
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-hash.log"
env MTL_DEBUG_LAYER=1 "$LOG_DIR/capture" >"$LOG_DIR/native.log" 2>&1
env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="$LOG_DIR/frame" "$LOG_DIR/capture" >"$LOG_DIR/capture.log" 2>&1
for suffix in '' '_2';do
 env MTL_DEBUG_LAYER=1 "$LOG_DIR/replay" "$LOG_DIR/frame_capture${suffix}.rdc" 40 00 >"$LOG_DIR/replay${suffix}.log" 2>&1
 python3 util/test/metal/metal_frame_color_load_gate.py "$BUILD_DIR/bin/renderdoccmd" "$LOG_DIR/opener" "$LOG_DIR/frame_capture${suffix}.rdc" "$LOG_DIR/gate${suffix}" >"$LOG_DIR/gate${suffix}.log" 2>&1
done
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-end-hash.log"
cmp "$LOG_DIR/library-hash.log" "$LOG_DIR/library-end-hash.log"
echo 'PASS frame color Load: 2 captures/8 reset seeks, original Native UAV compute writes full32pixels, live frame birth/reset removal, six predecessor/work/UAV negative groups each capture'
