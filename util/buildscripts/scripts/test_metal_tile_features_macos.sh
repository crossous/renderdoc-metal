#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-$REPO_ROOT/build-macos-debug}"
RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-$BUILD_DIR/metal-tile-features}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-$REPO_ROOT/captures/metal-features-b465}"
cd "$REPO_ROOT"
mkdir -p "$RESULT_DIR" "$CAPTURE_DIR"
cmake --build "$BUILD_DIR" --target renderdoccmd build-qrenderdoc -j2 > "$RESULT_DIR/build.log" 2>&1
# The GUI and CLI must use the same backend.
cp "$BUILD_DIR/lib/librenderdoc.dylib" "$BUILD_DIR/bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib"
clang++ -std=c++17 -I. util/test/metal/metal_tile_features_capture.mm -framework Metal -framework Foundation -framework QuartzCore -o "$RESULT_DIR/capture" > "$RESULT_DIR/capture-build.log" 2>&1
clang++ -std=c++17 -I. util/test/metal/metal_fx_capture.mm -framework Metal -framework MetalFX -framework Foundation -framework QuartzCore -o "$RESULT_DIR/fx-capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_tile_features_replay.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$RESULT_DIR/replay"
clang++ -std=c++17 -I. util/test/metal/metal_shader_features_test.cpp -o "$RESULT_DIR/metadata-test"
xcrun metal -c util/test/metal/metal_tile_features.metal -o "$RESULT_DIR/shader.air"
xcrun metallib "$RESULT_DIR/shader.air" -o "$RESULT_DIR/shader.metallib"
xcrun metal-objdump --metallib --disassemble "$RESULT_DIR/shader.metallib" > "$RESULT_DIR/shader.ll"
"$RESULT_DIR/metadata-test" "$RESULT_DIR/shader.ll" > "$RESULT_DIR/metadata-test.log"
for mode in tile private memoryless combined; do
  MTL_DEBUG_LAYER=1 "$RESULT_DIR/capture" "$mode" "$RESULT_DIR/shader.metallib" /tmp/metal-feature-native > "$RESULT_DIR/$mode-native.log" 2>&1
  MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
    "$RESULT_DIR/capture" "$mode" "$RESULT_DIR/shader.metallib" "$CAPTURE_DIR/$mode" > "$RESULT_DIR/$mode-capture.log" 2>&1
  MTL_DEBUG_LAYER=1 "$RESULT_DIR/replay" "$CAPTURE_DIR/${mode}_capture.rdc" "$mode" > "$RESULT_DIR/$mode-inspection.log" 2>&1
  "$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/${mode}_capture.rdc" > "$RESULT_DIR/$mode-replay.log" 2>&1
  echo "PASS $mode: native / capture / pipeline and pixels / 3-loop replay"
done
for mode in tile private; do
  MTL_DEBUG_LAYER=1 METAL_FEATURES_SOURCE=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
    "$RESULT_DIR/capture" "$mode" "$REPO_ROOT/util/test/metal/metal_tile_features.metal" "$CAPTURE_DIR/${mode}-source" > "$RESULT_DIR/${mode}-source-capture.log" 2>&1
  MTL_DEBUG_LAYER=1 "$RESULT_DIR/replay" "$CAPTURE_DIR/${mode}-source_capture.rdc" "$mode" > "$RESULT_DIR/${mode}-source-inspection.log" 2>&1
  echo "PASS $mode source metadata"
done
MTL_DEBUG_LAYER=1 METAL_FX_ORACLE_PATH="$RESULT_DIR/metalfx-native.bin" \
  "$RESULT_DIR/fx-capture" /tmp/metal-fx-native > "$RESULT_DIR/metalfx-native.log" 2>&1
MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
  "$RESULT_DIR/fx-capture" "$CAPTURE_DIR/metalfx-spatial" > "$RESULT_DIR/metalfx-capture.log" 2>&1
MTL_DEBUG_LAYER=1 "$RESULT_DIR/replay" "$CAPTURE_DIR/metalfx-spatial_capture.rdc" metalfx "$RESULT_DIR/metalfx-native.bin" > "$RESULT_DIR/metalfx-inspection.log" 2>&1
"$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/metalfx-spatial_capture.rdc" > "$RESULT_DIR/metalfx-replay.log" 2>&1
python3 util/test/metal/metal_fx_invalid.py "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/metalfx-spatial_capture.rdc" > "$RESULT_DIR/metalfx-invalid.log" 2>&1
shasum -a 256 "$BUILD_DIR/lib/librenderdoc.dylib" "$BUILD_DIR/bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib" > "$RESULT_DIR/library-hash.log"
echo "PASS MetalFX Spatial: native pixel oracle / capture / opaque replay / parameters / malformed inputs"
echo "Captures: $CAPTURE_DIR"
echo "Logs: $RESULT_DIR"
