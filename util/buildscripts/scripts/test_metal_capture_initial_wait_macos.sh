#!/bin/bash
# Tiny two-queue GPU completion, blocked reservations, automatic capture and seeks.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-initial-wait.XXXXXX")"
echo "Initial wait logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target renderdoc renderdoccmd -j 4 >"${LOG_DIR}/build.log" 2>&1
clang++ -std=c++17 -fobjc-arc -mmacosx-version-min=13.0 -I. \
  util/test/metal/metal_capture_initial_wait.mm -framework Foundation -framework Metal \
  -framework QuartzCore -o "${LOG_DIR}/capture"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. \
  util/test/metal/metal_capture_initial_wait_replay.cpp -L"${BUILD_DIR}/lib" -lrenderdoc \
  -Wl,-rpath,"${BUILD_DIR}/lib" -o "${LOG_DIR}/replay"
for kind in normal blocked; do
  blocked=0
  if [[ "$kind" == blocked ]]; then blocked=1; fi
  if [[ "$blocked" == 1 ]]; then export RENDERDOC_METAL_TEST_BLOCKED_RESERVATION=1; else unset RENDERDOC_METAL_TEST_BLOCKED_RESERVATION; fi
  MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
    RENDERDOC_METAL_TRACE_CAPTURE_WAITS=1 RENDERDOC_METAL_CAPTURE_PATH="${LOG_DIR}/${kind}" \
    "${LOG_DIR}/capture" >"${LOG_DIR}/${kind}-capture.log" 2>&1
  for suffix in capture automatic; do
    capture="${LOG_DIR}/${kind}_capture.rdc"
    if [[ "$suffix" == automatic ]]; then
      auto_files=("${LOG_DIR}/${kind}"_frame*.rdc)
      capture="${auto_files[0]}"
    fi
    if [[ "$suffix" == automatic ]]; then
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${capture}" auto >"${LOG_DIR}/${kind}-${suffix}-api.log" 2>&1
    else
      MTL_DEBUG_LAYER=1 "${LOG_DIR}/replay" "${capture}" >"${LOG_DIR}/${kind}-${suffix}-api.log" 2>&1
    fi
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/bin/renderdoccmd" replay --loops 1 "${capture}" >"${LOG_DIR}/${kind}-${suffix}-cli.log" 2>&1
  done
  "${BUILD_DIR}/bin/renderdoccmd" convert -f "${LOG_DIR}/${kind}_capture.rdc" \
    -o "${LOG_DIR}/${kind}.zip.xml" -c zip.xml >"${LOG_DIR}/${kind}-export.log" 2>&1
  python3 - "${LOG_DIR}/${kind}.zip.xml" <<'PY'
from pathlib import Path
import struct, sys, xml.etree.ElementTree as ET, zipfile
xml = Path(sys.argv[1]); chunks = ET.parse(xml).find('./chunks')
assert not any('dispatch' in c.get('name', '').lower() for c in chunks), 'Background dispatch entered capture'
values = []
with zipfile.ZipFile(xml.with_suffix('')) as archive:
    for chunk in chunks:
        if chunk.get('name') != 'Internal::Initial Contents': continue
        fields = {n.get('name'): n for n in chunk}
        if fields['type'].text == '1':
            data = archive.read(f'{int(fields["Contents"].text):06d}')
            assert len(data) == 4
            values.append(struct.unpack('<I', data)[0])
assert sorted(values) == [111, 222], values
print('PASS frozen initial bytes [111,222]; background dispatch excluded')
PY
  echo "PASS ${kind}: two-queue snapshot, completion callback, automatic Start/End, API/CLI replay and seeks"
done
unset RENDERDOC_METAL_TEST_BLOCKED_RESERVATION
echo "PASS tiny capture initial wait; no full UE GPU or UI acceptance"
