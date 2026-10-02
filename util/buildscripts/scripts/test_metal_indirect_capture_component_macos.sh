#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
LOG_DIR="$(mktemp -d "${BUILD_DIR}/metal-indirect-component.XXXXXX")"
echo "Native indirect capture component logs: ${LOG_DIR}"
cd "${REPO_ROOT}"
python3 - "${BUILD_DIR}" "${LOG_DIR}" >"${LOG_DIR}/build.log" 2>&1 <<'PY'
import shlex, subprocess, sys
from pathlib import Path
build,folder=map(lambda p:Path(p).resolve(),sys.argv[1:3])
r=subprocess.run(['ninja','-C',build,'-t','commands','renderdoc/driver/metal/CMakeFiles/rdoc_metal.dir/metal_compute_command_encoder.cpp.o'],capture_output=True,text=True,check=True)
a=shlex.split(next(c for c in r.stdout.splitlines() if 'metal_compute_command_encoder.cpp' in c and ' -c ' in c));base=[];i=0
while i<len(a):
    if a[i] in ('-o','-c','-MT','-MF'):i+=2;continue
    if a[i] in ('-MD','-DRENDERDOC_EXPORTS'):i+=1;continue
    base.append(a[i]);i+=1
base.append('-I'+str(Path('.').resolve()))
objects=[]
for file,name in [('util/test/metal/metal_indirect_capture_component.cpp','fixture.o'),('renderdoc/driver/metal/metal_indirect_readback.cpp','readback.o'),('renderdoc/driver/metal/official/metal-cpp.cpp','metalcpp.o')]:
    obj=folder/name;objects.append(str(obj))
    subprocess.run(base+['-c',str(Path(file).resolve()),'-o',str(obj)],cwd=build,check=True)
subprocess.run([base[0],*objects,'-framework','Metal','-framework','Foundation','-L'+str(build/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(build/'lib'),'-o',str(folder/'component')],check=True)
PY
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${LOG_DIR}/library-hash.log"
if [[ "${1:-}" == --build-only ]]; then exit 0; fi
if [[ -n "${1:-}" ]]; then echo "Expected optional --build-only" >&2; exit 2; fi
env MTL_DEBUG_LAYER=1 "${LOG_DIR}/component" >"${LOG_DIR}/native.log" 2>&1
cat "${LOG_DIR}/native.log"
