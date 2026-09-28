#!/bin/bash
# Non-overlapping heap child makeAliasable, buffer and texture. Terminal-only.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
cmake --build "${REPO_ROOT}/build-metal-demos" --target demos -j 8
cmake --build "${BUILD_DIR}" --target renderdoccmd -j 8
mkdir -p "${CAPTURE_DIR}"
for fixture in 131 132; do
  if [[ "$fixture" == 131 ]]; then
    feature=RENDERDOC_METAL_T131_ALIAS_BUFFER
    demo=Metal_Private_Buffer
  else
    feature=RENDERDOC_METAL_T132_ALIAS_TEXTURE
    demo=Metal_Shared_Texture
  fi
  for trial in 1 2 3; do
    env MTL_DEBUG_LAYER=1 "${feature}=1" \
      "${REPO_ROOT}/bin/demos_x64" "$demo" --frames 3
  done
  env MTL_DEBUG_LAYER=1 "${feature}=1" \
    RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t${fixture}" \
    DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    "${REPO_ROOT}/bin/demos_x64" "$demo" --frames 3
  "${CMD}" convert -f "${CAPTURE_DIR}/t${fixture}_capture.rdc" \
    -o "${CAPTURE_DIR}/t${fixture}.zip.xml" -c zip.xml
  MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t${fixture}_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_heap_alias_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t${fixture}_capture.rdc"
done
bash "${SCRIPT_DIR}/test_metal_replay_targeted_macos.sh" \
  t12 t39 t64 t71 t72 t73 t116 t117 t130 t131 t132
echo "T131/T132 non-overlapping heap aliasable targeted terminal validation passed."
