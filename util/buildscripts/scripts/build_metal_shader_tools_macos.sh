#!/bin/bash
# SPDX-License-Identifier: MIT
# Build-time dependencies: CMake, C++ compiler, Rust >= 1.87, Xcode command-line tools.
# End users only need Xcode's Metal tools. No Cargo/Homebrew dependency is shipped.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PREFIX="${1:-${HOME}/Library/Caches/renderdoc-metal/shader-tools/$(uname -m)}"
mkdir -p "${PREFIX}/bin" "${PREFIX}/licenses" "${PREFIX}/source" "${PREFIX}/build"
M2V_REV=43c46ac8a24adf1a6e872b8a52c706ec9614fad0
SDK_REV=vulkan-sdk-1.4.357.0
fetch() {
  local name="$1" url="$2" expected="$3" archive="${PREFIX}/source/$1.tar.gz"
  if [ ! -f "${archive}" ]; then curl --fail --location --retry 3 "${url}" -o "${archive}.tmp"; mv "${archive}.tmp" "${archive}"; fi
  actual="$(shasum -a 256 "${archive}" | cut -d ' ' -f1)"
  if [ "${actual}" != "${expected}" ]; then echo "Source checksum mismatch: ${name}" >&2; exit 1; fi
  if [ ! -d "${PREFIX}/build/${name}" ]; then
    mkdir -p "${PREFIX}/build/${name}"
    tar -xzf "${archive}" --strip-components=1 -C "${PREFIX}/build/${name}"
  fi
}
fetch metal2vulkan "https://codeload.github.com/steelbrain/metal2vulkan/tar.gz/${M2V_REV}" "2f723bf651871af475911f8566cf637e145a86f6fbda31d9db558b7ebb49b3fb"
fetch spirv-cross "https://codeload.github.com/KhronosGroup/SPIRV-Cross/tar.gz/${SDK_REV}" "97c910326afdd44d794ce8561326fa675fd1958b27142f03295403044d639639"
fetch spirv-tools "https://codeload.github.com/KhronosGroup/SPIRV-Tools/tar.gz/${SDK_REV}" "d31e7109b6ef3559067e53e520870eafed7c9534d00db9728814b6df03fa4a5e"
fetch spirv-headers "https://codeload.github.com/KhronosGroup/SPIRV-Headers/tar.gz/${SDK_REV}" "4d703067a7e06331ccb37bdfed3f9b7879cc61969a2689ae95c95db34a47ff07"
# Use the lockfile checked into this fork for reproducible dependency resolution.
cp "${REPO_ROOT}/util/shader_tools/metal2vulkan.Cargo.lock" "${PREFIX}/build/metal2vulkan/Cargo.lock"
cargo build --manifest-path "${PREFIX}/build/metal2vulkan/Cargo.toml" --locked --release --features serde -p metal2vulkan -j 2
cp "${PREFIX}/build/metal2vulkan/target/release/metal2vulkan" "${PREFIX}/bin/"
for name in spirv-cross spirv-tools; do
  extra=(-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF)
  target=spirv-cross
  if [ "${name}" = spirv-tools ]; then
    extra+=(-DSPIRV-Headers_SOURCE_DIR="${PREFIX}/build/spirv-headers" -DSPIRV_SKIP_TESTS=ON)
    target=spirv-val
  else
    extra+=(-DSPIRV_CROSS_ENABLE_TESTS=OFF)
  fi
  cmake -S "${PREFIX}/build/${name}" -B "${PREFIX}/build/${name}/rdoc-build" "${extra[@]}"
  cmake --build "${PREFIX}/build/${name}/rdoc-build" --target "${target}" -j 2
  binary="${PREFIX}/build/${name}/rdoc-build/${target}"
  if [ "${name}" = spirv-tools ]; then binary="${PREFIX}/build/${name}/rdoc-build/tools/spirv-val"; fi
  cp "${binary}" "${PREFIX}/bin/"
  cp "${PREFIX}/build/${name}/LICENSE" "${PREFIX}/licenses/${name}.txt"
done
cp "${PREFIX}/build/metal2vulkan/LICENSE" "${PREFIX}/licenses/metal2vulkan-LGPL-3.0.txt"
cp "${PREFIX}/build/spirv-headers/LICENSE" "${PREFIX}/licenses/spirv-headers.txt"
cp "${REPO_ROOT}/util/shader_tools/metal2vulkan.Cargo.lock" "${PREFIX}/source/Cargo.lock"
cp "${REPO_ROOT}/util/shader_tools/THIRD_PARTY.md" "${PREFIX}/licenses/THIRD_PARTY.md"
cp "${REPO_ROOT}/util/shader_tools/GPL-3.0.txt" "${PREFIX}/licenses/"
python3 "${REPO_ROOT}/util/shader_tools/collect_tool_licenses.py" "${PREFIX}" "${CARGO_HOME:-${HOME}/.cargo}"
python3 "${REPO_ROOT}/util/shader_tools/write_manifest.py" "${PREFIX}"
echo "Built pinned native Metal shader tools: ${PREFIX}"
