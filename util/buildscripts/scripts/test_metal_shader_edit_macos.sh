#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-$REPO_ROOT/build-macos-debug}"
RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-$BUILD_DIR/metal-shader-edit}"
if pgrep -x qrenderdoc >/dev/null || pgrep -x UnrealEditor >/dev/null; then
  echo 'Close qrenderdoc/UE before serial shader replacement GPU tests.' >&2; exit 2
fi
cd "$REPO_ROOT"
mkdir -p "$RESULT_DIR"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_shader_edit_replay.cpp \
  -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$RESULT_DIR/replay"
clang++ -std=c++17 -I. util/test/metal/metal_shader_edit_capture.mm \
  -framework Metal -framework Foundation -framework QuartzCore -o "$RESULT_DIR/capture"
RENDERDOC_METAL_EDIT_SOURCE="$RESULT_DIR/shader.metal" "$RESULT_DIR/capture"
xcrun metal -c "$RESULT_DIR/shader.metal" -o "$RESULT_DIR/shader.air"
xcrun metallib "$RESULT_DIR/shader.air" -o "$RESULT_DIR/shader.metallib"
xcrun metal -c -frecord-sources -gline-tables-only "$RESULT_DIR/shader.metal" -o "$RESULT_DIR/debug.air"
xcrun metallib "$RESULT_DIR/debug.air" -o "$RESULT_DIR/debug.metallib"
for mode in source air; do
  if [[ "$mode" == air ]]; then export RENDERDOC_METAL_EDIT_BINARY="$RESULT_DIR/shader.metallib"; else unset RENDERDOC_METAL_EDIT_BINARY; fi
  DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="$RESULT_DIR/$mode" \
    "$RESULT_DIR/capture" > "$RESULT_DIR/$mode-capture.log" 2>&1
  unset RENDERDOC_METAL_EDIT_BINARY
  "$RESULT_DIR/replay" "$RESULT_DIR/${mode}_capture.rdc" "$mode" > "$RESULT_DIR/$mode-replay.log" 2>&1
done
for variant in debug alias-source alias-air frame-born; do
  unset RENDERDOC_METAL_EDIT_BINARY RENDERDOC_METAL_EDIT_ALIAS RENDERDOC_METAL_EDIT_FRAME_BORN
  mode=source
  case "$variant" in
    debug) export RENDERDOC_METAL_EDIT_BINARY="$RESULT_DIR/debug.metallib" ;;
    alias-source) export RENDERDOC_METAL_EDIT_ALIAS=1 ;;
    alias-air) export RENDERDOC_METAL_EDIT_ALIAS=1 RENDERDOC_METAL_EDIT_BINARY="$RESULT_DIR/shader.metallib"; mode=air ;;
    frame-born) export RENDERDOC_METAL_EDIT_FRAME_BORN=1 ;;
  esac
  DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="$RESULT_DIR/$variant" \
    "$RESULT_DIR/capture" > "$RESULT_DIR/$variant-capture.log" 2>&1
  "$RESULT_DIR/replay" "$RESULT_DIR/${variant}_capture.rdc" "$mode" > "$RESULT_DIR/$variant-replay.log" 2>&1
done
unset RENDERDOC_METAL_EDIT_BINARY RENDERDOC_METAL_EDIT_ALIAS RENDERDOC_METAL_EDIT_FRAME_BORN
python3 "$REPO_ROOT/util/shader_tools/metal_air_processor.py" "$RESULT_DIR/shader.metallib" "$RESULT_DIR/external-fs.ll" fs
xcrun metal -c "$RESULT_DIR/external-fs.ll" -o "$RESULT_DIR/external-fs.air"
xcrun metallib "$RESULT_DIR/external-fs.air" -o "$RESULT_DIR/external-fs.metallib"
if [[ $# == 1 ]]; then
  "$RESULT_DIR/replay" "$1" air 3928 > "$RESULT_DIR/ue-3928.log" 2>&1
  "$RESULT_DIR/replay" "$1" air 3612 > "$RESULT_DIR/ue-3612.log" 2>&1
fi
shasum -a 256 "$BUILD_DIR/lib/librenderdoc.dylib" > "$RESULT_DIR/library-hash.log"
echo 'PASS directed shader edit; logs distinguish small fixtures and real UE replacement'
