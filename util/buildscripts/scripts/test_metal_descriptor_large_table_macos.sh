#!/bin/bash
# Actual UE descriptor capacity with one-thread consumers and explicit identities.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-large-table.XXXXXX")"
echo "Large table logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_large_table_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_large_table_replay.mm -framework Foundation -framework Metal -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/open_probe"
run_bounded() {
 python3 - "$@" <<'PY'
import subprocess,sys
sys.exit(subprocess.run(sys.argv[1:],timeout=60).returncode)
PY
}
run_bounded env MTL_DEBUG_LAYER=1 "${LOG_DIR}/capture" >"${LOG_DIR}/native.log" 2>&1
run_bounded env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/table" "${LOG_DIR}/capture" >"${LOG_DIR}/capture.log" 2>&1
source_va="$(python3 - "${LOG_DIR}/capture.log" <<'PY'
import re,sys
from pathlib import Path
print(re.search(r'CAPTURED_VA=(\d+)',Path(sys.argv[1]).read_text())[1])
PY
)"
for suffix in '' '_2'; do
 run_bounded env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 "${LOG_DIR}/replay" "${LOG_DIR}/table_capture${suffix}.rdc" "$source_va" >"${LOG_DIR}/replay${suffix}.log" 2>&1
done
python3 util/test/metal/metal_descriptor_large_table_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/open_probe" "${LOG_DIR}/table_capture.rdc" "${LOG_DIR}/gate" >"${LOG_DIR}/gate.log" 2>&1
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS large typed tables: 2 captures, 16 dispatch seeks plus 8 EID0 resets, 786432 resource/4096 sampler slots, GPU122/161 and full metadata preservation'
