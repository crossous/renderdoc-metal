#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-$REPO_ROOT/build-macos-debug}"
RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-$BUILD_DIR/metal-future-shared}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-$REPO_ROOT/captures/metal-future-shared-b468}"
cd "$REPO_ROOT"
mkdir -p "$RESULT_DIR" "$CAPTURE_DIR"
cmake --build "$BUILD_DIR" --target renderdoccmd build-qrenderdoc -j2 > "$RESULT_DIR/build.log" 2>&1
cp "$BUILD_DIR/lib/librenderdoc.dylib" "$BUILD_DIR/bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib"
clang++ -std=c++17 -I. util/test/metal/metal_future_shared_capture.mm -framework Metal -framework Foundation -framework QuartzCore -o "$RESULT_DIR/capture"
clang++ -std=c++17 -I. util/test/metal/metal_vrr_capture.mm -framework Metal -framework Foundation -framework QuartzCore -o "$RESULT_DIR/vrr-capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_future_shared_replay.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$RESULT_DIR/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_temporal_vrr_replay.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$RESULT_DIR/vrr-replay"
MTL_DEBUG_LAYER=1 "$RESULT_DIR/capture" /tmp/metal-future-shared-native > "$RESULT_DIR/native.log" 2>&1
MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
  "$RESULT_DIR/capture" "$CAPTURE_DIR/ordinary" > "$RESULT_DIR/capture.log" 2>&1
MTL_DEBUG_LAYER=1 "$RESULT_DIR/replay" "$CAPTURE_DIR/ordinary_capture.rdc" > "$RESULT_DIR/inspection.log" 2>&1
"$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/ordinary_capture.rdc" > "$RESULT_DIR/cli.log" 2>&1
echo 'PASS ordinary Shared: all API events forward/reverse/forward, birth visibility, CPU bytes, draw pixels and copy Usage'
for mode in late late-bytes; do
  MTL_DEBUG_LAYER=1 "$RESULT_DIR/vrr-capture" /tmp/metal-vrr-future-native "$mode" > "$RESULT_DIR/$mode-native.log" 2>&1
  MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
    "$RESULT_DIR/vrr-capture" "$CAPTURE_DIR/$mode" "$mode" > "$RESULT_DIR/$mode-capture.log" 2>&1
  MTL_DEBUG_LAYER=1 "$RESULT_DIR/vrr-replay" "$CAPTURE_DIR/${mode}_capture.rdc" vrr > "$RESULT_DIR/$mode-inspection.log" 2>&1
  "$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/${mode}_capture.rdc" > "$RESULT_DIR/$mode-cli.log" 2>&1
  echo "PASS VRR future Shared: $mode native copy and repeated draw seek"
done
python3 util/test/metal/metal_future_shared_invalid.py "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/ordinary_capture.rdc" > "$RESULT_DIR/invalid.log" 2>&1
echo 'PASS eight malformed future Shared cases rejected'
echo "Captures: $CAPTURE_DIR"
echo "Logs: $RESULT_DIR"
