#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-bounded-diagnostics.XXXXXX")"
CAPTURE="${RENDERDOC_METAL_DIAGNOSTIC_CAPTURE:-${BUILD_DIR}/metal-linear-upload.Bx59i7/frame-10_capture.rdc}"
echo "Bounded replay diagnostic logs: ${LOG_DIR}"
cd "$REPO_ROOT"
cmake --build "$BUILD_DIR" --target renderdoc renderdoccmd -j2 >"$LOG_DIR/build.log" 2>&1
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/opener"
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-hash.log"
python3 util/test/metal/metal_bounded_replay_diagnostics_gate.py "$BUILD_DIR/bin/renderdoccmd" "$LOG_DIR/opener" "$CAPTURE" "$LOG_DIR/gate" >"$LOG_DIR/gate.log" 2>&1
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-end-hash.log"
cmp "$LOG_DIR/library-hash.log" "$LOG_DIR/library-end-hash.log"
echo 'PASS bounded initial uploads and quiescent Native prefix diagnostics'
