#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-frame-heap-texture.XXXXXX")"
echo "Frame heap texture logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. util/test/metal/metal_frame_heap_texture_capture.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=13.0 -DRENDERDOC_PLATFORM_APPLE -I. \
  util/test/metal/metal_frame_heap_texture_replay.cpp -L"${BUILD_DIR}/lib" -lrenderdoc \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
run_bounded() {
  python3 - "$@" <<'PY'
import subprocess,sys
sys.exit(subprocess.run(sys.argv[1:],timeout=60).returncode)
PY
}
for kind in plain view; do
  capture_environment=(env MTL_DEBUG_LAYER=1)
  if [[ "$kind" == view ]];then capture_environment+=(RENDERDOC_METAL_FRAME_TEXTURE_VIEW=1);fi
  run_bounded "${capture_environment[@]}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-native.log" 2>&1
  run_bounded "${capture_environment[@]}" DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
  for suffix in '' '_2'; do
    pixel=41;if [[ -n "${suffix}" ]];then pixel=173;fi
    run_bounded env MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${LOG_DIR}/${kind}_capture${suffix}.rdc" "$pixel" >"${LOG_DIR}/${kind}-replay${suffix}.log" 2>&1
    "${BUILD_DIR}/bin/renderdoccmd" convert -f "${LOG_DIR}/${kind}_capture${suffix}.rdc" -o "${LOG_DIR}/${kind}${suffix}.zip.xml" -c zip.xml
    python3 - "${LOG_DIR}/${kind}${suffix}.zip.xml" "$kind" <<'PY'
import sys,xml.etree.ElementTree as ET
chunks=ET.parse(sys.argv[1]).findall('./chunks/chunk')
births=[i for i,c in enumerate(chunks) if c.get('name')=='MTLHeap::newTexture(offset)']
scope=next(i for i,c in enumerate(chunks) if c.get('name')=='Internal::Frame Metadata')
render=next(i for i,c in enumerate(chunks) if c.get('name')=='MTLCommandBuffer::renderCommandEncoderWithDescriptor')
assert len(births)==1 and scope<births[0]<render,(scope,births,render)
views=[i for i,c in enumerate(chunks) if 'newTextureView' in c.get('name','')]
if sys.argv[2]=='view':assert len(views)==1 and births[0]<views[0]<render,views
else:assert not views,views
print('PASS unique frame texture/view birth before first encoder')
PY
  done
 done
echo "PASS four frame placement/view captures, 24 seeks, GPU pixels and frame birth order"
