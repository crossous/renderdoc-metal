#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-interleaved-producer.XXXXXX")"
echo "Interleaved producer logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 2 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_descriptor_sourced_compute_capture.mm -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -fobjc-arc -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_descriptor_sourced_gpu_replay.mm -L"${BUILD_DIR}/lib" -lrenderdoc -framework Foundation -framework Metal -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. util/ue/ue_capture_open_probe.cpp -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/opener"
for kind in retained unretained; do
  runtime=(env MTL_DEBUG_LAYER=1 RENDERDOC_METAL_INTERLEAVED_PRODUCER=1)
  if [[ "$kind" == unretained ]]; then runtime+=(RENDERDOC_METAL_UNRETAINED_SUBMISSIONS=1); fi
  "${runtime[@]}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
  "${runtime[@]}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
  read -r va_a va_b va_table < <(python3 -c 'import re,sys; from pathlib import Path; print(*re.search(r"VA_A=(\d+) VA_B=(\d+) VA_TABLE=(\d+)",Path(sys.argv[1]).read_text()).groups())' "${LOG_DIR}/${kind}-capture.log")
  "${runtime[@]}" "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture.rdc" "$va_a" "$va_b" "$va_table" 41 80 compute >"${LOG_DIR}/${kind}-replay.log" 2>&1
  "${runtime[@]}" "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture_2.rdc" "$va_a" "$va_b" "$va_table" 80 41 compute >"${LOG_DIR}/${kind}-replay2.log" 2>&1
  python3 util/test/metal/metal_interleaved_producer_gate.py "${BUILD_DIR}/bin/renderdoccmd" "${LOG_DIR}/opener" "${LOG_DIR}/${kind}_capture.rdc" "${LOG_DIR}/${kind}-gate" >"${LOG_DIR}/${kind}-gate.log" 2>&1
done
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
echo 'PASS retained/unretained independent submit amid GPU producer annotations: Native clear, producer/consumer outputs, EID0 resets, API/CLI fail-closed ownership gates'
