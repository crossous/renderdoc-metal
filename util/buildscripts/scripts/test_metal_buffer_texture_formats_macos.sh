#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-buffer-formats.XXXXXX")"
echo "Buffer texture format logs: ${LOG_DIR}"
exec python3 "${REPO_ROOT}/util/test/metal/metal_buffer_texture_formats_sample_gate.py" --build --build-dir "${BUILD_DIR}" --work-dir "${LOG_DIR}"
