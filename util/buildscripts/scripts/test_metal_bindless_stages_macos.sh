#!/bin/bash
# Directed, serial GPU checks. The UE capture is optional; this is not full regression/UI QA.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="${RENDERDOC_METAL_BINDLESS_LOG_DIR:-$(mktemp -d "${BUILD_DIR}/metal-bindless-stages.XXXXXX")}"
mkdir -p "$LOG_DIR"
cd "$REPO_ROOT"
echo "Directed bindless stage logs: $LOG_DIR"
link=(-DRENDERDOC_PLATFORM_APPLE -I. -L"$BUILD_DIR/lib" -lrenderdoc "-Wl,-rpath,$BUILD_DIR/lib")
clang++ -std=c++17 -I. util/test/metal/metal_air_access_test.cpp -o "$LOG_DIR/air-test"
"$LOG_DIR/air-test" >"$LOG_DIR/parser.log"
clang++ -std=c++17 "${link[@]}" util/test/metal/metal_bindless_write_replay.cpp -o "$LOG_DIR/compute-replay"
clang++ -std=c++17 "${link[@]}" util/test/metal/metal_stage_inspection_replay.cpp -o "$LOG_DIR/stage-replay"
for fixture in write stages; do
  xcrun -sdk macosx metal -std=metal3.0 -c "util/test/metal/metal_bindless_${fixture}.metal" -o "$LOG_DIR/$fixture.air"
  xcrun -sdk macosx metallib "$LOG_DIR/$fixture.air" -o "$LOG_DIR/$fixture.metallib"
  clang++ -std=c++17 -fobjc-arc -I. "util/test/metal/metal_bindless_${fixture}_capture.mm" \
    -framework Foundation -framework Metal -framework QuartzCore -o "$LOG_DIR/$fixture-capture"
  MTL_DEBUG_LAYER=1 "$LOG_DIR/$fixture-capture" "$LOG_DIR/$fixture.metallib" >"$LOG_DIR/$fixture-native.log" 2>&1
  MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="$LOG_DIR/$fixture" "$LOG_DIR/$fixture-capture" "$LOG_DIR/$fixture.metallib" \
    >"$LOG_DIR/$fixture-capture.log" 2>&1
 done
MTL_DEBUG_LAYER=1 "$LOG_DIR/compute-replay" "$LOG_DIR/write_capture.rdc" >"$LOG_DIR/compute-replay.log" 2>&1
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TEST_LATE_UNIFORM_WRITE=1 \
  DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
  RENDERDOC_METAL_CAPTURE_PATH="$LOG_DIR/late-uniform" \
  "$LOG_DIR/write-capture" "$LOG_DIR/write.metallib" >"$LOG_DIR/late-uniform-capture.log" 2>&1
MTL_DEBUG_LAYER=1 "$LOG_DIR/compute-replay" "$LOG_DIR/late-uniform_capture.rdc" \
  >"$LOG_DIR/late-uniform-replay.log" 2>&1
MTL_DEBUG_LAYER=1 "$LOG_DIR/stage-replay" "$LOG_DIR/stages_capture.rdc" bindless >"$LOG_DIR/stage-replay.log" 2>&1
python3 util/test/metal/metal_bindless_stages_invalid.py "$BUILD_DIR/bin/renderdoccmd" "$LOG_DIR/stages_capture.rdc" >"$LOG_DIR/invalid.log" 2>&1
for fixture in t79 t80 t83 t85; do
  MTL_DEBUG_LAYER=1 "$LOG_DIR/stage-replay" "captures/metal-smoke/${fixture}_capture.rdc" mesh >"$LOG_DIR/$fixture.log" 2>&1
done
for fixture in t144 t244 t247 t250; do
  MTL_DEBUG_LAYER=1 "$LOG_DIR/stage-replay" "captures/metal-smoke/${fixture}_capture.rdc" ray >"$LOG_DIR/$fixture.log" 2>&1
done
if (( $# )); then
  clang++ -std=c++17 "${link[@]}" util/ue/ue_metal_replay_event_probe.cpp -o "$LOG_DIR/event-probe"
  "$LOG_DIR/event-probe" "$1" "$LOG_DIR" 3509 3596 3612 3928 --inspect-shaders --texture=12323 --usage=12323 >"$LOG_DIR/ue.log" 2>&1
fi
shasum -a 256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-hash.log"
echo 'PASS directed CS/Task/Mesh bindless, RT AS inputs and explicit unused residency checks'
