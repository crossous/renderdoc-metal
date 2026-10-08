#!/bin/bash
# Serial native/capture/seek and provenance gate for large compute visible function tables.
set -euo pipefail
cd "$(dirname "$0")/../../.."
python3 - <<'PY'
import hashlib, json, os, re, subprocess
from pathlib import Path
repo=Path.cwd()
build=Path(os.environ.get('RENDERDOC_METAL_BUILD_DIR',repo/'build-macos-debug')).resolve()
out=Path(os.environ.get('RENDERDOC_METAL_RESULT_DIR',build/'metal-ray-b485')).resolve()
captures=Path(os.environ.get('RENDERDOC_METAL_CAPTURE_DIR',repo/'captures/metal-ray-b485')).resolve()
out.mkdir(parents=True,exist_ok=True); captures.mkdir(parents=True,exist_ok=True)
lib=build/'lib/librenderdoc.dylib'; cli=build/'bin/renderdoccmd'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
report={'status':'RUNNING','checks':[],'full_regression':'NOT RUN','GUI':'NOT RUN'}
def step(name,args,timeout=180):
    with (out/(name+'.log')).open('w') as log:
        process=subprocess.run([str(x) for x in args],stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
    report['checks'].append({'name':name,'exit':process.returncode})
    (out/'visible-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(name,process.returncode,flush=True)
    if process.returncode: raise RuntimeError(name+' failed; '+str(out/(name+'.log')))
try:
    if os.environ.get('RENDERDOC_METAL_SKIP_BUILD')!='1':
        step('build',['cmake','--build',build,'--target','renderdoccmd','build-qrenderdoc','-j2'],600)
    report['backend_sha256']=sha(lib)
    report['bundle_sha256']=sha(build/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib')
    if report['bundle_sha256']!=report['backend_sha256']: raise RuntimeError('bundle/backend mismatch')
    step('capture-helper',['clang++','-std=c++17','-I.','util/test/metal/metal_ray_table_capture.mm',
                          '-framework','Foundation','-framework','Metal','-framework','QuartzCore','-o',out/'capture'])
    link=['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I.','-L'+str(build/'lib'),
          '-lrenderdoc','-Wl,-rpath,'+str(build/'lib')]
    step('replay-helper',link+['util/test/metal/metal_ray_table_replay.cpp','-o',out/'replay'])
    step('lifecycle-helper',link+['util/test/metal/metal_replay_lifecycle_smoke.mm',
                                 '-framework','Foundation','-framework','Metal','-framework','QuartzCore',
                                 '-o',out/'lifecycle'])
    modes=['large-visible-33','large-visible-256','large-visible-65536',
           'background-large-visible-256']
    selections=0
    for mode in modes:
        cap=captures/(mode+'_capture.rdc')
        step(mode+'-native',['env','MTL_DEBUG_LAYER=1',out/'capture',captures/(mode+'-native'),mode],60)
        step(mode+'-capture',['env','MTL_DEBUG_LAYER=1','DYLD_INSERT_LIBRARIES='+str(lib),
                              out/'capture',captures/mode,mode],60)
        step(mode+'-replay',['env','MTL_DEBUG_LAYER=1',out/'replay',cap,mode])
        match=re.search(r'(\d+) events in three directions',(out/(mode+'-replay.log')).read_text())
        if not match: raise RuntimeError('missing event oracle')
        selections+=int(match.group(1))*3
        step(mode+'-cli',[cli,'replay','--loops','3',cap])
        step(mode+'-invalid',['python3','util/test/metal/metal_large_visible_invalid.py',cli,cap],240)
    report['event_selections']=selections
    step('old-directed',['bash','util/buildscripts/scripts/test_metal_replay_targeted_macos.sh',
                         '--sentinel','t44','t120','t124','t125','t126','t130','t135','t140','t141','t144','t148'],600)
    step('old-compute-invalid',['python3','util/test/metal/metal_compute_visible_function_table_invalid.py',cli,repo/'captures/metal-smoke/t124_capture.rdc'],240)
    step('lifecycle',['env','MTL_DEBUG_LAYER=1',out/'lifecycle',repo/'captures/metal-smoke/t35_capture.rdc',
                      *[captures/(m+'_capture.rdc') for m in modes],'10'])
    report['backend_end_sha256']=sha(lib)
    if report['backend_end_sha256']!=report['backend_sha256']: raise RuntimeError('backend changed')
    report['status']='PASS'
except Exception as error:
    report.update(status='FAIL',error=str(error)); raise
finally:
    (out/'visible-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
PY
