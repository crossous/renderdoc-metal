#!/bin/bash
# GPU-only shared event plus explicit rejection of CPU host mutation.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
cmake --build "${BUILD_DIR}" --target renderdoccmd -j "$(sysctl -n hw.ncpu)"
mkdir -p "${CAPTURE_DIR}"
MTL_DEBUG_LAYER=1 "${REPO_ROOT}/bin/demos_x64" Metal_Shared_Event --frames 4
RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t65" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "${REPO_ROOT}/bin/demos_x64" Metal_Shared_Event --frames 4
NEGATIVE_DIR="$(mktemp -d -t metal-shared-event-negative.XXXXXX)"
for mode in HOST_MUTATION; do
  env "RENDERDOC_METAL_SHARED_EVENT_${mode}=1" \
    "RENDERDOC_METAL_CAPTURE_PATH=${NEGATIVE_DIR}/${mode}" \
    "DYLD_INSERT_LIBRARIES=${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" Metal_Shared_Event --frames 4
  if "${BUILD_DIR}/bin/renderdoccmd" replay --loops 1 \
      "${NEGATIVE_DIR}/${mode}_capture.rdc" >"${NEGATIVE_DIR}/${mode}.log" 2>&1; then
    echo "Unsupported shared-event ${mode} unexpectedly replayed" >&2
    exit 1
  fi
  if ! rg -q 'unsupportedHostMutation' "${NEGATIVE_DIR}/${mode}.log"; then
    echo "Unsupported shared-event ${mode} failed without expected diagnostic" >&2
    exit 1
  fi
done
echo "T65 unsupported-host logs: ${NEGATIVE_DIR}"
RENDERDOC_METAL_LAST_TEST=65 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
