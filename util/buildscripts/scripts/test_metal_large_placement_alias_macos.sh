#!/bin/bash
# Reuse the existing asynchronous alias fixture with larger placement buffers.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-large-alias.XXXXXX")"
echo "Large placement alias logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_alias_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_graphics_replay.mm -L"${BUILD_DIR}/lib" -lrenderdoc -framework Foundation -framework Metal -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/open_probe"
run_bounded() {
 python3 - "$@" <<'PY'
import subprocess,sys
sys.exit(subprocess.run(sys.argv[1:],timeout=60).returncode)
PY
}
for kind in shared private shared_background private_background shared_both private_both shared_both_background private_both_background; do
 capture_environment=(env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_LARGE_BUFFER_ALIAS=1 RENDERDOC_METAL_IMPLICIT_BUFFER_ALIAS=1 RENDERDOC_METAL_ASYNC_BUFFER_ALIAS=1 RENDERDOC_METAL_IMPLICIT_ALIAS_LENGTH=131072 RENDERDOC_METAL_UNRETAINED_SUBMISSIONS=1)
 if [[ "$kind" == private* ]];then capture_environment+=(RENDERDOC_METAL_PRIVATE_BUFFER_ALIAS=1);fi
 if [[ "$kind" == *background ]];then capture_environment+=(RENDERDOC_METAL_BACKGROUND_BUFFER_ALIAS=1);fi
 if [[ "$kind" == *both* ]];then capture_environment+=(RENDERDOC_METAL_LARGE_ALIAS_BOTH=1);fi
 run_bounded "${capture_environment[@]}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
 run_bounded "${capture_environment[@]}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
 read -r va_a va_table tex_id sampler_id < <(python3 - "${LOG_DIR}/${kind}-capture.log" <<'PY'
import re,sys
from pathlib import Path
t=Path(sys.argv[1]).read_text()
assert 'captures=2' in t
print(*(re.search(k+r'=(\d+)',t)[1] for k in ('VA_A','VA_TABLE','TEX','SAMP')))
PY
 )
 for suffix in '' '_2';do
  before=122;pixel=225;if [[ -n "$suffix" ]];then before=186;pixel=161;fi
  replay_args=("$va_a" "$va_table" "$tex_id" "$sampler_id" "$before" "$pixel")
  if [[ "$kind" == private* && "$kind" != *background ]];then replay_args+=(private-frame);fi
  run_bounded env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_REPLAY_WAITS=1 RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT=1 "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" "${replay_args[@]}" >"${LOG_DIR}/${kind}-replay${suffix}.log" 2>&1
 done
 python3 util/test/metal/metal_descriptor_async_alias_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/open_probe" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-gate" >"${LOG_DIR}/${kind}-gate.log" 2>&1
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS large placement aliases: Shared/Private, background/frame source, 12B/128KiB original sharing 128KiB replacement, unretained async submissions, 16 captures and 64 seeks'
