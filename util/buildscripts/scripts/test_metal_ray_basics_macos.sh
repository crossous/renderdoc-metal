#!/bin/bash
# Serial, bounded native/capture/replay checks for ray tables, sync and AS initial state.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-$REPO_ROOT/build-macos-debug}"
RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-$BUILD_DIR/metal-ray-b473}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-$REPO_ROOT/captures/metal-ray-b473}"
cd "$REPO_ROOT"
mkdir -p "$RESULT_DIR" "$CAPTURE_DIR"
run() {
  local name="$1" timeout="$2"
  shift 2
  if ! python3 - "$timeout" "$@" > "$RESULT_DIR/$name.log" 2>&1 <<'PY'
import os, signal, subprocess, sys
p = subprocess.Popen(sys.argv[2:], start_new_session=True)
try:
    code = p.wait(timeout=int(sys.argv[1]))
except subprocess.TimeoutExpired:
    os.killpg(p.pid, signal.SIGKILL)
    p.wait()
    sys.exit(124)
sys.exit(code if code >= 0 else 1)
PY
  then tail -40 "$RESULT_DIR/$name.log" >&2; return 1; fi
  echo "PASS $name"
}
run build 600 cmake --build "$BUILD_DIR" --target renderdoccmd build-qrenderdoc -j2
cp -p "$BUILD_DIR/lib/librenderdoc.dylib" "$BUILD_DIR/bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib"
run capture-helper 120 clang++ -std=c++17 -I. util/test/metal/metal_ray_table_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "$RESULT_DIR/capture"
link=(-std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. -L"$BUILD_DIR/lib" -lrenderdoc "-Wl,-rpath,$BUILD_DIR/lib")
run replay-helper 120 clang++ "${link[@]}" util/test/metal/metal_ray_table_replay.cpp -o "$RESULT_DIR/replay"
run lifecycle-helper 120 clang++ "${link[@]}" util/test/metal/metal_replay_lifecycle_smoke.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "$RESULT_DIR/lifecycle"
run native 60 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/capture" "$CAPTURE_DIR/native"
run capture 60 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
  "$RESULT_DIR/capture" "$CAPTURE_DIR/table"
run inspection 120 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/replay" "$CAPTURE_DIR/table_capture.rdc"
run cli 120 "$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/table_capture.rdc"
run invalid 600 python3 util/test/metal/metal_ray_table_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/table_capture.rdc"
run fences-native 60 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/capture" "$CAPTURE_DIR/fences-native" fences
run fences-capture 60 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
  "$RESULT_DIR/capture" "$CAPTURE_DIR/fences" fences
run fences-inspection 120 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/replay" "$CAPTURE_DIR/fences_capture.rdc"
run fences-cli 120 "$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/fences_capture.rdc"
run fences-invalid 600 python3 util/test/metal/metal_ray_table_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/fences_capture.rdc"
for mode in background background-mutated background-late background-tlas \
    background-tlas-repeated background-tlas-mutated background-tlas-late background-tlas-frame \
    background-indexed background-indexed-u32 background-indexed-mutated background-indexed-late \
    background-tlas-indexed-repeated background-formatted array-bindings background-array-bindings; do
  run "$mode-native" 60 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/capture" "$CAPTURE_DIR/$mode-native" "$mode"
  run "$mode-capture" 60 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
    "$RESULT_DIR/capture" "$CAPTURE_DIR/$mode" "$mode"
  run "$mode-inspection" 120 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/replay" "$CAPTURE_DIR/${mode}_capture.rdc" "$mode"
  run "$mode-cli" 120 "$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/${mode}_capture.rdc"
done
run capability-gate 120 python3 util/test/metal/metal_ray_capability_gate.py --build-dir "$BUILD_DIR"
for capture in "$REPO_ROOT/captures/metal-smoke/t124_capture.rdc" \
    "$REPO_ROOT/captures/metal-smoke/t125_capture.rdc" "$CAPTURE_DIR/table_capture.rdc" \
    "$CAPTURE_DIR/array-bindings_capture.rdc"; do
  run "binding-identity-$(basename "$capture" .rdc)" 180 \
    python3 util/test/metal/metal_compute_table_binding_invalid.py \
    "$BUILD_DIR/bin/renderdoccmd" "$capture"
done
run initial-invalid 600 python3 util/test/metal/metal_ray_initial_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/background_capture.rdc"
run instance-initial-invalid 600 python3 util/test/metal/metal_ray_instance_initial_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/background-tlas-repeated_capture.rdc"
run indexed-initial-invalid 600 python3 util/test/metal/metal_ray_indexed_initial_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/background-indexed_capture.rdc"
run formatted-initial-invalid 120 python3 util/test/metal/metal_ray_initial_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/background-formatted_capture.rdc" --formatted
run background-tlas-stale-capture 60 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
  "$RESULT_DIR/capture" "$CAPTURE_DIR/background-tlas-stale" background-tlas-stale
run background-tlas-stale-reject 60 python3 util/test/metal/metal_ray_table_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/background-tlas-stale_capture.rdc" --background
run background-fences-capture 60 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
  "$RESULT_DIR/capture" "$CAPTURE_DIR/background-fences" background-fences
run background-fences-reject 60 python3 util/test/metal/metal_ray_table_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/background-fences_capture.rdc" --background
run background-alias-native 60 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/capture" "$CAPTURE_DIR/background-alias-native" background-alias
run background-alias-capture 60 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
  "$RESULT_DIR/capture" "$CAPTURE_DIR/background-alias" background-alias
run background-alias-reject 60 python3 util/test/metal/metal_ray_table_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/background-alias_capture.rdc" --background
run directed 600 env RENDERDOC_METAL_CAPTURE_DIR="$REPO_ROOT/captures/metal-smoke" \
  bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh \
  --sentinel t44 t120 t126 t130 t135 t140 t141 t144 t148 t151 t156 t159 t161 t218 t220 t244 t247 t250 t257 t299 t300
run visible-invalid 600 python3 util/test/metal/metal_visible_function_table_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" captures/metal-smoke/t120_capture.rdc
run render-invalid 600 python3 util/test/metal/metal_intersection_function_table_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" captures/metal-smoke/t148_capture.rdc
run compute-invalid 600 python3 util/test/metal/metal_compute_intersection_table_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" captures/metal-smoke/t218_capture.rdc
run lifecycle 180 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/lifecycle" \
  captures/metal-smoke/t35_capture.rdc "$CAPTURE_DIR/table_capture.rdc" "$CAPTURE_DIR/fences_capture.rdc" \
  "$CAPTURE_DIR/background_capture.rdc" "$CAPTURE_DIR/background-mutated_capture.rdc" \
  "$CAPTURE_DIR/background-late_capture.rdc" "$CAPTURE_DIR/background-tlas_capture.rdc" \
  "$CAPTURE_DIR/background-tlas-repeated_capture.rdc" "$CAPTURE_DIR/background-tlas-mutated_capture.rdc" \
  "$CAPTURE_DIR/background-tlas-late_capture.rdc" "$CAPTURE_DIR/background-tlas-frame_capture.rdc" \
  "$CAPTURE_DIR/background-indexed_capture.rdc" "$CAPTURE_DIR/background-indexed-u32_capture.rdc" \
  "$CAPTURE_DIR/background-indexed-mutated_capture.rdc" "$CAPTURE_DIR/background-indexed-late_capture.rdc" \
  "$CAPTURE_DIR/background-tlas-indexed-repeated_capture.rdc" \
  "$CAPTURE_DIR/background-formatted_capture.rdc" \
  "$CAPTURE_DIR/array-bindings_capture.rdc" "$CAPTURE_DIR/background-array-bindings_capture.rdc" \
  captures/metal-smoke/t300_capture.rdc 10
shasum -a 256 "$BUILD_DIR/lib/librenderdoc.dylib" "$BUILD_DIR/bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib" \
  "$CAPTURE_DIR/table_capture.rdc" "$CAPTURE_DIR/fences_capture.rdc" > "$RESULT_DIR/hashes.log"
echo "PASS ray basics directed gate; logs $RESULT_DIR; not full regression or GUI acceptance"
