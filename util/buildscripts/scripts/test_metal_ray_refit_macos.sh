#!/bin/bash
# Bounded serial update/copy snapshot checks, including the previous ray basics gate.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-$REPO_ROOT/build-macos-debug}"
RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-$BUILD_DIR/metal-ray-b474}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-$REPO_ROOT/captures/metal-ray-b474}"
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
run basics 1800 env RENDERDOC_METAL_RESULT_DIR="$RESULT_DIR/basics" \
  RENDERDOC_METAL_CAPTURE_DIR="$CAPTURE_DIR/basics" \
  bash util/buildscripts/scripts/test_metal_ray_basics_macos.sh
run refit-capture-helper 120 clang++ -std=c++17 -I. util/test/metal/metal_ray_refit_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "$RESULT_DIR/refit-capture"
run refit-replay-helper 120 clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. \
  -L"$BUILD_DIR/lib" -lrenderdoc "-Wl,-rpath,$BUILD_DIR/lib" \
  util/test/metal/metal_ray_refit_replay.cpp -o "$RESULT_DIR/refit-replay"
modes=(refittable refittable-separate refittable-noduplicate refittable-formatted \
  refittable-formatted-noduplicate-separate refittable-indexed \
  refittable-indexed-u32-formatted-noduplicate-separate initial-refit initial-refit-separate \
  initial-refit-formatted-noduplicate initial-refit-indexed \
  initial-refit-indexed-u32-formatted-noduplicate-separate copy initial-refit-copy \
  initial-refit-indexed-copy initial-refit-indexed-u32-formatted-noduplicate-copy-separate
  refittable-nil initial-refit-formatted-noduplicate-nil initial-refit-indexed-copy-nil)
for mode in "${modes[@]}"; do
  run "$mode-native" 60 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/refit-capture" "$CAPTURE_DIR/$mode-native" "$mode"
  run "$mode-capture" 60 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
    "$RESULT_DIR/refit-capture" "$CAPTURE_DIR/$mode" "$mode"
  run "$mode-inspection" 120 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/refit-replay" "$CAPTURE_DIR/${mode}_capture.rdc" "$mode"
  run "$mode-cli" 120 "$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/${mode}_capture.rdc"
done
run copy-late-native 60 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/refit-capture" "$CAPTURE_DIR/copy-late-native" copy-late
run copy-late-capture 60 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
  "$RESULT_DIR/refit-capture" "$CAPTURE_DIR/copy-late" copy-late
run copy-late-reject 60 python3 util/test/metal/metal_ray_table_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/copy-late_capture.rdc" --background
run refit-initial-invalid 600 python3 util/test/metal/metal_ray_initial_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/initial-refit_capture.rdc"
run refit-indexed-initial-invalid 600 python3 util/test/metal/metal_ray_indexed_initial_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/initial-refit-indexed_capture.rdc"
run refit-invalid 600 python3 util/test/metal/metal_ray_refit_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/refittable_capture.rdc" "$CAPTURE_DIR/refittable-indexed_capture.rdc"
files=(); for mode in "${modes[@]}"; do files+=("$CAPTURE_DIR/${mode}_capture.rdc"); done
run lifecycle 180 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/basics/lifecycle" "${files[@]}" 10
shasum -a 256 "$BUILD_DIR/lib/librenderdoc.dylib" "$BUILD_DIR/bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib" \
  "$CAPTURE_DIR/initial-refit_capture.rdc" "$CAPTURE_DIR/initial-refit-indexed-copy_capture.rdc" > "$RESULT_DIR/hashes.log"
echo "PASS refit/copy directed gate; logs $RESULT_DIR; not full regression or GUI acceptance"
