#!/bin/bash
# Native public Metal 3 Temporal MetalFX and VRR, including inspection and operation Usage.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-$REPO_ROOT/build-macos-debug}"
RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-$BUILD_DIR/metal-temporal-vrr}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-$REPO_ROOT/captures/metal-features-b467}"
cd "$REPO_ROOT"
mkdir -p "$RESULT_DIR" "$CAPTURE_DIR"
cmake --build "$BUILD_DIR" --target renderdoccmd build-qrenderdoc -j2 > "$RESULT_DIR/build.log" 2>&1
cp "$BUILD_DIR/lib/librenderdoc.dylib" "$BUILD_DIR/bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib"
clang++ -std=c++17 -I. util/test/metal/metal_temporal_fx_capture.mm -framework Metal -framework MetalFX -framework Foundation -framework QuartzCore -o "$RESULT_DIR/temporal-capture"
clang++ -std=c++17 -I. util/test/metal/metal_vrr_capture.mm -framework Metal -framework Foundation -framework QuartzCore -o "$RESULT_DIR/vrr-capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_temporal_vrr_replay.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$RESULT_DIR/replay"
for mode in normal auto minimal missing recovery; do
  case "$mode" in
    normal) name=temporal;;
    missing) name=temporal-missing-history;;
    *) name=temporal-$mode;;
  esac
  MTL_DEBUG_LAYER=1 METAL_TEMPORAL_ORACLE="$RESULT_DIR/$name-native.bin" \
    "$RESULT_DIR/temporal-capture" /tmp/metal-temporal-native "$mode" > "$RESULT_DIR/$name-native.log" 2>&1
  MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
    "$RESULT_DIR/temporal-capture" "$CAPTURE_DIR/$name" "$mode" > "$RESULT_DIR/$name-capture.log" 2>&1
  # A mid-history capture cannot export MetalFX's private previous-frame state.
  # Check its explicit warning and captured reset recovery, not false pixel equality.
  if [[ "$mode" == missing || "$mode" == recovery ]]; then
    MTL_DEBUG_LAYER=1 "$RESULT_DIR/replay" "$CAPTURE_DIR/${name}_capture.rdc" "$mode" > "$RESULT_DIR/$name-inspection.log" 2>&1
  else
    MTL_DEBUG_LAYER=1 "$RESULT_DIR/replay" "$CAPTURE_DIR/${name}_capture.rdc" "$mode" "$RESULT_DIR/$name-native.bin" > "$RESULT_DIR/$name-inspection.log" 2>&1
  fi
  "$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/${name}_capture.rdc" > "$RESULT_DIR/$name-cli.log" 2>&1
  echo "PASS $name: native, capture, resources/Usage, event seek and CLI loops"
done
MTL_DEBUG_LAYER=1 "$RESULT_DIR/vrr-capture" /tmp/metal-vrr-native > "$RESULT_DIR/vrr-native.log" 2>&1
MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
  "$RESULT_DIR/vrr-capture" "$CAPTURE_DIR/vrr" > "$RESULT_DIR/vrr-capture.log" 2>&1
MTL_DEBUG_LAYER=1 "$RESULT_DIR/replay" "$CAPTURE_DIR/vrr_capture.rdc" vrr > "$RESULT_DIR/vrr-inspection.log" 2>&1
"$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/vrr_capture.rdc" > "$RESULT_DIR/vrr-cli.log" 2>&1
python3 util/test/metal/metal_temporal_fx_invalid.py "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/temporal_capture.rdc" > "$RESULT_DIR/temporal-invalid.log" 2>&1
python3 util/test/metal/metal_rate_map_invalid.py "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/vrr_capture.rdc" > "$RESULT_DIR/vrr-invalid.log" 2>&1
echo "PASS VRR and Temporal malformed captures"
echo "Captures: $CAPTURE_DIR"
echo "Logs: $RESULT_DIR"
