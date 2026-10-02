#!/bin/bash
# Native GPU regression: a retired descriptor upload range becomes ordinary constants.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-retired-uniform.XXXXXX")"
echo "Frame color logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_frame_color_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_frame_color_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/open_probe"
run_bounded() {
 python3 - "$@" <<'PY'
import subprocess,sys
sys.exit(subprocess.run(sys.argv[1:],timeout=60).returncode)
PY
}
for kind in r11; do
 capture_environment=(env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_RETIRED_UNIFORM_ARENA=1)
 if [[ "$kind" == r16* ]];then capture_environment+=(RENDERDOC_METAL_FRAME_COLOR_R16=1);fi
 if [[ "$kind" == *_view ]];then capture_environment+=(RENDERDOC_METAL_FRAME_COLOR_VIEW=1);fi
 run_bounded "${capture_environment[@]}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
 run_bounded "${capture_environment[@]}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
 for suffix in '' '_2';do
  index=0;if [[ -n "$suffix" ]];then index=1;fi
  texture_id="$(python3 - "${LOG_DIR}/${kind}-capture.log" "$index" <<'PY'
import re,sys
from pathlib import Path
print(re.search(r'FRAME_IDS=(\d+),(\d+)',Path(sys.argv[1]).read_text())[int(sys.argv[2])+1])
PY
)"
  run_bounded env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" "$texture_id" >"${LOG_DIR}/${kind}-replay${suffix}.log" 2>&1
  if [[ -n "${RENDERDOC_METAL_BASELINE_LIBRARY_DIR:-}" ]];then
   python3 - "${RENDERDOC_METAL_BASELINE_LIBRARY_DIR}" "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" "$texture_id" "${LOG_DIR}/${kind}-baseline${suffix}.log" <<'PY'
import os,subprocess,sys
from pathlib import Path
env=dict(os.environ,DYLD_LIBRARY_PATH=sys.argv[1],MTL_DEBUG_LAYER='1')
p=subprocess.run(sys.argv[2:5],env=env,capture_output=True,text=True,timeout=60)
out=p.stdout+p.stderr;Path(sys.argv[5]).write_text(out)
assert p.returncode==9 and 'marker=00000bad' in out,(p.returncode,out)
PY
  fi
 done

 done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS retired descriptor bytes reused as ordinary constants:2 captures/8 seeks, all6 words, actual GPU consumer and EID0 restoration'
