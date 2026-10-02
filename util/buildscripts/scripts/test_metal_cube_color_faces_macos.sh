#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-cube-color-faces.XXXXXX")"
echo "Background color logs: ${LOG_DIR}"
cd "$REPO_ROOT"
cmake --build "$BUILD_DIR" --target renderdoc renderdoccmd -j2 >"$LOG_DIR/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_cube_color_faces_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "$LOG_DIR/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_cube_color_faces_replay.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"$BUILD_DIR/lib" -lrenderdoc -Wl,-rpath,"$BUILD_DIR/lib" -o "$LOG_DIR/opener"
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-hash.log"
for format in 92;do
 export RENDERDOC_METAL_BACKGROUND_COLOR_FORMAT="$format"
 env MTL_DEBUG_LAYER=1 "$LOG_DIR/capture" >"$LOG_DIR/$format-native.log" 2>&1
 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="$LOG_DIR/$format" "$LOG_DIR/capture" >"$LOG_DIR/$format-capture.log" 2>&1
 read -r before after < <(python3 - "$LOG_DIR/$format-capture.log" <<'PY'
import re,sys
from pathlib import Path
values=re.findall(r'FIRST_PIXEL=([0-9a-f]+)',Path(sys.argv[1]).read_text());assert len(values)==2;print(*values)
PY
 )
 initial=$(python3 - "$before" <<'PY'
import sys
print('0'*len(sys.argv[1]))
PY
 )
 env MTL_DEBUG_LAYER=1 "$LOG_DIR/replay" "$LOG_DIR/${format}_capture.rdc" "$before" "$initial" 0 4 >"$LOG_DIR/$format-replay.log" 2>&1
 env MTL_DEBUG_LAYER=1 "$LOG_DIR/replay" "$LOG_DIR/${format}_capture_2.rdc" "$after" "$initial" 1 5 >"$LOG_DIR/$format-replay_2.log" 2>&1
 python3 util/test/metal/metal_cube_color_faces_gate.py "$BUILD_DIR/bin/renderdoccmd" "$LOG_DIR/opener" "$LOG_DIR/${format}_capture.rdc" "$LOG_DIR/$format-gate" >"$LOG_DIR/$format-gate.log" 2>&1
done
shasum -a256 "$BUILD_DIR/lib/librenderdoc.dylib" >"$LOG_DIR/library-end-hash.log"
cmp "$LOG_DIR/library-hash.log" "$LOG_DIR/library-end-hash.log"
echo 'PASS BG cube faces/mips: 2 captures/8 reset seeks, face4 mip0/face5 mip1, Native exact face pixels, untouched face0/initial restore and malformed gates'
