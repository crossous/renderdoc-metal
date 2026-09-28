#!/bin/bash
# Offline metallib loading, native failure handling and full terminal regression. No GUI QA.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
FIXTURE_DIR="${DEMO_BUILD_DIR}/metal-library-fixture"
OFFLINE_DIR="${FIXTURE_DIR}.offline"
FIXTURE_APP="${FIXTURE_DIR}/Binary Library.app"
DEMO="${FIXTURE_APP}/Contents/MacOS/demos_x64"
CMD="${BUILD_DIR}/bin/renderdoccmd"
# Recover our own fixture directory after an interrupted prior offline check; never overwrite it.
if [[ -e "$OFFLINE_DIR" ]]; then
  if [[ -e "$FIXTURE_DIR" ]]; then echo "Both fixture directories exist; refusing to overwrite" >&2; exit 2; fi
  mv "$OFFLINE_DIR" "$FIXTURE_DIR"
fi
restore_fixture() {
  if [[ -e "$OFFLINE_DIR" && ! -e "$FIXTURE_DIR" ]]; then mv "$OFFLINE_DIR" "$FIXTURE_DIR"; fi
}
trap restore_fixture EXIT
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"
"${SCRIPT_DIR}/build_metal_dev_macos.sh"
mkdir -p "${CAPTURE_DIR}" "${FIXTURE_APP}/Contents/MacOS" "${FIXTURE_APP}/Contents/Resources" \
  "${FIXTURE_DIR}/Test.bundle/Contents/Resources" "${FIXTURE_DIR}/Empty.bundle/Contents/Resources"
cp "${REPO_ROOT}/bin/demos_x64" "$DEMO"
for bundle in "$FIXTURE_APP" "${FIXTURE_DIR}/Test.bundle" "${FIXTURE_DIR}/Empty.bundle"; do
  cp "${REPO_ROOT}/util/test/demos/metal/metal_library_fixture.plist" "${bundle}/Contents/Info.plist"
done
xcrun -sdk macosx metal -std=macos-metal2.4 -mmacosx-version-min=12.0 \
  -c "${REPO_ROOT}/util/test/demos/metal/metal_binary_library.metal" -o "${FIXTURE_DIR}/library.air"
xcrun -sdk macosx metallib "${FIXTURE_DIR}/library.air" \
  -o "${FIXTURE_APP}/Contents/Resources/default.metallib"
cp "${FIXTURE_APP}/Contents/Resources/default.metallib" \
  "${FIXTURE_DIR}/Test.bundle/Contents/Resources/default.metallib"
MTL_DEBUG_LAYER=1 RENDERDOC_METAL_LIBRARY_FIXTURE="$FIXTURE_DIR" \
  "$DEMO" Metal_Binary_Library --frames 5
RENDERDOC_METAL_LIBRARY_FIXTURE="$FIXTURE_DIR" RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t48" \
  DYLD_INSERT_LIBRARIES="${BUILD_DIR}/lib/librenderdoc.dylib" \
  "$DEMO" Metal_Binary_Library --frames 8
# Hide every original library, including both bundles, before any replay.
mv "$FIXTURE_DIR" "$OFFLINE_DIR"
"${CMD}" convert -f "${CAPTURE_DIR}/t48_capture.rdc" -o "${CAPTURE_DIR}/t48.xml" -c xml
"${CMD}" convert -f "${CAPTURE_DIR}/t48_capture.rdc" -o "${CAPTURE_DIR}/t48.zip.xml" -c zip.xml
python3 - "${CAPTURE_DIR}/t48.zip.xml" "${OFFLINE_DIR}/Binary Library.app/Contents/Resources/default.metallib" <<'PY'
import pathlib
import sys
import xml.etree.ElementTree as ET
import zipfile
xml = pathlib.Path(sys.argv[1])
expected = pathlib.Path(sys.argv[2]).read_bytes()
chunks = ET.parse(xml).findall('./chunks/chunk')
def child(node, name):
    return next(item for item in node if item.get('name') == name)
libraries = []
with zipfile.ZipFile(str(xml)[:-4]) as archive:
    for chunk_id in range(1014, 1019):
        nodes = [n for n in chunks if int(n.get('id')) == chunk_id]
        assert len(nodes) == 1
        node = nodes[0]
        libraries.append(child(node, 'Library').text)
        data = child(node, 'data')
        assert int(data.get('byteLength')) == len(expected)
        assert archive.read(f'{int(data.text):06d}') == expected
        if chunk_id in (1015, 1016, 1017):
            assert not pathlib.Path(child(node, 'origin').text).exists()
assert len(set(libraries)) == 5
functions = [n for n in chunks if n.get('name') == 'MTLLibrary::newFunctionWithName']
assert len(functions) == 7
assert set(child(n, 'Library').text for n in functions) == set(libraries)
assert not any(n.get('name') == 'MTLDevice::newLibraryWithSource' for n in chunks)
print('T48 all five embedded payloads match original; failed creations absent; source paths unavailable')
PY
MTL_DEBUG_LAYER=1 "${CMD}" replay --loops 3 "${CAPTURE_DIR}/t48_capture.rdc"
if [[ "${RENDERDOC_METAL_SKIP_REGRESSION:-0}" != 1 ]]; then
  RENDERDOC_METAL_LAST_TEST=48 bash "${SCRIPT_DIR}/test_metal_replay_batch35_38_macos.sh"
  bash "${SCRIPT_DIR}/test_metal_source_library_compat_macos.sh"
fi
echo "BATCH48 native/capture/offline replay passed; GUI QA remains deferred."
