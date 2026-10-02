#!/bin/bash
# Bounded frame color births, typed sources, full clear/rewind and native heap layout.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-large-plain-copy.XXXXXX")"
echo "Large frame buffer logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_large_frame_buffer_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_large_frame_buffer_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/open_probe"
run_bounded() {
 python3 - "$@" <<'PY'
import subprocess,sys
sys.exit(subprocess.run(sys.argv[1:],timeout=60).returncode)
PY
}
for kind in heap_private heap_shared standalone; do
 capture_environment=(env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_LARGE_COPY=1)
 if [[ "$kind" == heap* ]];then capture_environment+=(RENDERDOC_METAL_LARGE_FRAME_HEAP=1);fi
 if [[ "$kind" == heap_private ]];then capture_environment+=(RENDERDOC_METAL_LARGE_FRAME_PRIVATE=1);fi
 run_bounded "${capture_environment[@]}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
 run_bounded "${capture_environment[@]}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
 for suffix in '' '_2';do
  index=0;if [[ -n "$suffix" ]];then index=1;fi
  source_va="$(python3 - "${LOG_DIR}/${kind}-capture.log" "$index" <<'PY'
import re,sys
from pathlib import Path
print(re.search(r'FRAME_VAS=(\d+),(\d+)',Path(sys.argv[1]).read_text())[int(sys.argv[2])+1])
PY
)"
  run_bounded env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" "$source_va" >"${LOG_DIR}/${kind}-replay${suffix}.log" 2>&1
 done
 python3 util/test/metal/metal_descriptor_large_frame_buffer_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/open_probe" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-gate" >"${LOG_DIR}/${kind}-gate.log" 2>&1
 done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo "PASS sourced ordinary blit: 128KiB heap Shared/Private, 1MiB standalone transfer, $((${RENDERDOC_METAL_LARGE_COPY_COUNT:-128}+1)) copies per frame, 6 captures/24 seek cycles/full bytes and EID0"
# echo 'PASS large frame sources: Private/Shared placement128KiB and standalone Shared4MiB, 6 captures, 24 seeks, GPU54/80, complete Shared data, partial upload/overwrite and EID0 reset'
