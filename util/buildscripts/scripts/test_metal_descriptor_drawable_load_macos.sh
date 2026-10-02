#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-drawable-load.XXXXXX")"
echo "Drawable Load logs: ${LOG_DIR}"
cd "$REPO_ROOT"
cmake --build "$BUILD_DIR" --target renderdoc renderdoccmd -j2 >"$LOG_DIR/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_drawable_load_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "$LOG_DIR/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_drawable_load_replay.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/opener"
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-hash.log"
env MTL_DEBUG_LAYER=1 "$LOG_DIR/capture" >"$LOG_DIR/native.log" 2>&1
env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_DRAWABLE_INITIAL=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="$LOG_DIR/frame" "$LOG_DIR/capture" >"$LOG_DIR/capture.log" 2>&1
python3 - "$LOG_DIR/capture.log" <<'PYACQUIRED'
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text();rows=re.findall(r'Metal acquired drawable initial:.*format=90 preserved=1',s);assert len(rows)==2,(len(rows),s)
print('PASS both drawables acquired after capture start preserve Native initial pixels')
PYACQUIRED
for suffix in '' '_2';do
 read -r pixels native_sum < <(python3 - "$LOG_DIR/capture.log" "$suffix" <<'PYNATIVE'
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text();i=1 if sys.argv[2] else 0
m=re.search(r'capture='+str(i)+r' SUM=(\d+) PIXELS=([0-9a-f]+)',s);assert m,s;print(m[2],m[1])
PYNATIVE
 )
 env MTL_DEBUG_LAYER=1 "$LOG_DIR/replay" "$LOG_DIR/frame_capture${suffix}.rdc" "$pixels" "$native_sum" >"$LOG_DIR/replay${suffix}.log" 2>&1
 python3 util/test/metal/metal_descriptor_drawable_load_gate.py "$BUILD_DIR/bin/renderdoccmd" "$LOG_DIR/opener" "$LOG_DIR/frame_capture${suffix}.rdc" "$LOG_DIR/gate${suffix}" >"$LOG_DIR/gate${suffix}.log" 2>&1
done
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-end-hash.log"
cmp "$LOG_DIR/library-hash.log" "$LOG_DIR/library-end-hash.log"
echo 'PASS acquired RGB10A2 drawable: 2 captures/8 reset seeks, exact Native Load pixels and shader read sum, initial restore'
