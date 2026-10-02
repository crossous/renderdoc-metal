#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-extended-heap.XXXXXX")"
echo "Extended Shared heap logs: ${LOG_DIR}"
cd "$REPO_ROOT"
cmake --build "$BUILD_DIR" --target renderdoc renderdoccmd -j2 >"$LOG_DIR/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_large_frame_buffer_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "$LOG_DIR/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_large_frame_buffer_replay.mm -framework Foundation -framework Metal -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/opener"
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-hash.log"
export RENDERDOC_METAL_LARGE_FRAME_HEAP=1 RENDERDOC_METAL_LARGE_COPY=1 RENDERDOC_METAL_LARGE_COPY_COUNT=1
for size in 786432 1048576;do
 export RENDERDOC_METAL_EXTENDED_FRAME_HEAP_BYTES="$size"
 env MTL_DEBUG_LAYER=1 "$LOG_DIR/capture" >"$LOG_DIR/$size-native.log" 2>&1
 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="$LOG_DIR/$size" "$LOG_DIR/capture" >"$LOG_DIR/$size-capture.log" 2>&1
 for suffix in '' '_2';do
  index=0;if [[ -n "$suffix" ]];then index=1;fi
  source_va="$(python3 - "$LOG_DIR/$size-capture.log" "$index" <<'PY'
import re,sys
from pathlib import Path
print(re.search(r'FRAME_VAS=(\d+),(\d+)',Path(sys.argv[1]).read_text())[int(sys.argv[2])+1])
PY
)"
  env MTL_DEBUG_LAYER=1 "$LOG_DIR/replay" "$LOG_DIR/${size}_capture${suffix}.rdc" "$source_va" >"$LOG_DIR/$size-replay$suffix.log" 2>&1
  python3 util/test/metal/metal_descriptor_large_frame_buffer_gate.py "$BUILD_DIR/bin/renderdoccmd" "$LOG_DIR/opener" "$LOG_DIR/${size}_capture${suffix}.rdc" "$LOG_DIR/$size-gate$suffix" >"$LOG_DIR/$size-gate$suffix.log" 2>&1
 done
done
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-end-hash.log"
cmp "$LOG_DIR/library-hash.log" "$LOG_DIR/library-end-hash.log"
echo 'PASS extended Shared placement: 4 captures/16 reset seeks, full768KiB/1MiB Native copies and CPU snapshots, typed Native addresses, 124 API+CLI rejection groups'
