#!/bin/bash
# Bounded pipeline inspection checks. Close qrenderdoc/UE before running GPU work.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-${BUILD_DIR}/metal-ui-alignment}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
if pgrep -x qrenderdoc >/dev/null || pgrep -x UnrealEditor >/dev/null; then
  echo 'Close qrenderdoc/UnrealEditor before this serial GPU probe.' >&2
  exit 2
fi
mkdir -p "$RESULT_DIR"
if [[ "${RENDERDOC_METAL_SKIP_BUILD:-0}" != 1 ]]; then
  cmake --build "$BUILD_DIR" --target build-qrenderdoc renderdoccmd -j2
fi
export DYLD_LIBRARY_PATH="${BUILD_DIR}/lib"
for probe in pipeline_inspection stage_inspection; do
  clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I"$REPO_ROOT" \
    "$REPO_ROOT/util/test/metal/metal_${probe}_replay.cpp" -L"$BUILD_DIR/lib" -lrenderdoc \
    -Wl,-rpath,"$BUILD_DIR/lib" -o "$RESULT_DIR/$probe"
done
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"$REPO_ROOT" "$REPO_ROOT/util/test/metal/metal_replay_output_smoke.mm" \
  -L"$BUILD_DIR/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"$BUILD_DIR/lib" -o "$RESULT_DIR/output-smoke"
shasum -a 256 "$BUILD_DIR/lib/librenderdoc.dylib" > "$RESULT_DIR/start-hash.log"
for fixture in 36 66 67; do
  mode=tess; if [[ "$fixture" == 36 ]]; then mode=dynamic; fi
  "$RESULT_DIR/pipeline_inspection" "$CAPTURE_DIR/t${fixture}_capture.rdc" "$mode" > "$RESULT_DIR/inspection-t${fixture}.log" 2>&1
  "$RESULT_DIR/output-smoke" "$CAPTURE_DIR/t${fixture}_capture.rdc" "$RESULT_DIR/t${fixture}.ppm" > "$RESULT_DIR/t${fixture}.log" 2>&1
done
for fixture in 79 80 83 85; do
  "$RESULT_DIR/stage_inspection" "$CAPTURE_DIR/t${fixture}_capture.rdc" mesh > "$RESULT_DIR/t${fixture}.log" 2>&1
done
if [[ -n "${RENDERDOC_METAL_VIEWPORT_CAPTURE_DIR:-}" ]]; then
  for mode in viewport-array viewport-reset; do
    "$RESULT_DIR/pipeline_inspection" "$RENDERDOC_METAL_VIEWPORT_CAPTURE_DIR/${mode}_capture.rdc" "$mode" > "$RESULT_DIR/${mode}.log" 2>&1
  done
fi
shasum -a 256 "$BUILD_DIR/lib/librenderdoc.dylib" > "$RESULT_DIR/end-hash.log"
cmp "$RESULT_DIR/start-hash.log" "$RESULT_DIR/end-hash.log"
echo 'PASS directed dynamic state, patch variants, mesh stages and GPU pixels/resets'
