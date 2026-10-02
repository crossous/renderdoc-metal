#!/bin/bash
# Bounded frame color births, typed sources, full clear/rewind and native heap layout.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-frame-family-numeric.XXXXXX")"
echo "Frame family logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_frame_family_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_frame_family_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/open_probe"
run_bounded() {
 python3 - "$@" <<'PY'
import subprocess,sys
sys.exit(subprocess.run(sys.argv[1:],timeout=60).returncode)
PY
}
for kind in uint_array uint_image uint_atomic r8_image packed_volume; do
 capture_environment=(env MTL_DEBUG_LAYER=1)
 if [[ "$kind" == uint* ]];then capture_environment+=(RENDERDOC_METAL_FRAME_FAMILY_UINT=1);fi
 if [[ "$kind" == uint_array ]];then capture_environment+=(RENDERDOC_METAL_FRAME_FAMILY_ARRAY=1);fi
 if [[ "$kind" == uint_image || "$kind" == uint_atomic || "$kind" == r8_image ]];then capture_environment+=(RENDERDOC_METAL_FRAME_FAMILY_2D=1);fi
 if [[ "$kind" == uint_atomic ]];then capture_environment+=(RENDERDOC_METAL_FRAME_FAMILY_ATOMIC=1);fi
 if [[ "$kind" == r8_image ]];then capture_environment+=(RENDERDOC_METAL_FRAME_FAMILY_UNORM8=1);fi
 if [[ "$kind" == packed_volume ]];then capture_environment+=(RENDERDOC_METAL_FRAME_FAMILY_PACKED10=1);fi
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
 done
 python3 util/test/metal/metal_descriptor_frame_family_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/open_probe" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-gate" >"${LOG_DIR}/${kind}-gate.log" 2>&1
 done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS frame numeric sources: R32Uint array/2D/ShaderAtomic usage, R8Unorm512x512, RGB10A2 volume, 10 captures, 40 seeks, full raw and typed pixel checks, source usage, Native GPU ID and EID0 reset'
