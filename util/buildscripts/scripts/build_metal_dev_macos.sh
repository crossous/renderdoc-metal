#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SOURCE_DIR="${RENDERDOC_METAL_SOURCE_DIR:-${REPO_ROOT}}"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"

# Apple's linker splits -force_load paths containing spaces. Keep the build in
# the repository, but address it through a stable path without spaces.
if [[ "${BUILD_DIR}" == *" "* ]]; then
  BUILD_LINK="/tmp/renderdoc-metal-$(printf '%s' "${REPO_ROOT}" | shasum -a 256 | cut -c1-12)"
  if [ ! -e "${BUILD_LINK}" ]; then
    ln -s "${REPO_ROOT}" "${BUILD_LINK}"
  fi
  BUILD_DIR="${BUILD_LINK}/$(basename "${BUILD_DIR}")"
fi

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required to locate the Qt 5 development tools." >&2
  exit 1
fi

QT5_PREFIX="$(brew --prefix qt@5)"
BISON_PREFIX="$(brew --prefix bison)"
QMAKE="${QMAKE_QT5_COMMAND:-${QT5_PREFIX}/bin/qmake}"

if [ ! -x "${QMAKE}" ]; then
  echo "Qt 5 qmake was not found at ${QMAKE}." >&2
  echo "Install dependencies with: brew install qt@5 autoconf automake pcre bison" >&2
  exit 1
fi

export PATH="${QT5_PREFIX}/bin:${BISON_PREFIX}/bin:${PATH}"

CACHED_TOOLS_ROOT=""
if [ -f "${BUILD_DIR}/CMakeCache.txt" ]; then
  CACHED_TOOLS_ROOT="$(sed -n 's/^METAL_SHADER_TOOLS_ROOT:PATH=//p' "${BUILD_DIR}/CMakeCache.txt")"
fi
TOOLS_ROOT="${RENDERDOC_METAL_SHADER_TOOLS_ROOT:-${CACHED_TOOLS_ROOT:-${HOME}/Library/Caches/renderdoc-metal/shader-tools/$(uname -m)}}"
if [ ! -f "${TOOLS_ROOT}/manifest.json" ]; then
  "${SCRIPT_DIR}/build_metal_shader_tools_macos.sh" "${TOOLS_ROOT}"
fi

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DMETAL_SHADER_TOOLS_ROOT="${TOOLS_ROOT}" \
  -DQMAKE_QT5_COMMAND="${QMAKE}" \
  -DENABLE_METAL=ON \
  -DENABLE_GL=OFF \
  -DENABLE_GLES=OFF \
  -DENABLE_EGL=OFF \
  -DENABLE_VULKAN=OFF \
  -DENABLE_PYRENDERDOC=OFF \
  -DENABLE_RENDERDOCCMD=ON \
  -DCMAKE_REQUIRED_INCLUDES="$(brew --prefix pcre)/include"

# Keep the default build within the local M2's memory budget. Larger machines
# can explicitly increase the number of jobs.
cmake --build "${BUILD_DIR}" --target build-qrenderdoc renderdoccmd -j "${RENDERDOC_METAL_BUILD_JOBS:-2}"

APP_PATH="${BUILD_DIR}/bin/qrenderdoc.app"
APP_RENDERDOC_LIB="${APP_PATH}/Contents/lib/librenderdoc.dylib"
# qmake only refreshes the bundle copy when qrenderdoc itself relinks. A replay-only change can
# leave the app loading an older library than renderdoccmd, so keep the bundle copy in sync.
if ! cmp -s "${BUILD_DIR}/lib/librenderdoc.dylib" "${APP_RENDERDOC_LIB}"; then
  mkdir -p "${APP_PATH}/Contents/lib"
  cp -p "${BUILD_DIR}/lib/librenderdoc.dylib" "${APP_RENDERDOC_LIB}"
fi
echo "Built ${APP_PATH}"
echo "Built ${BUILD_DIR}/bin/renderdoccmd"

if [ "${1:-}" = "--run" ]; then
  open "${APP_PATH}"
fi
