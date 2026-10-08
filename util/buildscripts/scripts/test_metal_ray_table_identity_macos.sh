#!/bin/bash
# Bounded, serial native/capture/replay gate for typed VFT/IFT GPU identities.
set -euo pipefail
cd "$(dirname "$0")/../../.."
python3 - <<'PY'
import fcntl, hashlib, json, os, re, subprocess, sys
from pathlib import Path
repo=Path.cwd()
sys.path.insert(0,str(repo/'util/ue'))
from metal_pipeline_compile_audit import audit_compile_trace
build=Path(os.environ.get('RENDERDOC_METAL_BUILD_DIR',repo/'build-macos-debug')).resolve()
out=Path(os.environ.get('RENDERDOC_METAL_RESULT_DIR',build/'metal-ray-b497')).resolve()
caps=Path(os.environ.get('RENDERDOC_METAL_CAPTURE_DIR',repo/'captures/metal-ray-b497')).resolve()
out.mkdir(parents=True,exist_ok=True);caps.mkdir(parents=True,exist_ok=True)
lib=build/'lib/librenderdoc.dylib';cli=build/'bin/renderdoccmd'
report={'status':'RUNNING','checks':[],'UE':'NOT RUN','full_regression':'NOT RUN','GUI':'NOT RUN'}
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def save(): (out/'table-identity-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
def step(name,args,timeout=60):
    with (out/(name+'.log')).open('w') as log:
        result=subprocess.run([str(a) for a in args],stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
    report['checks'].append({'name':name,'exit':result.returncode});save()
    print(name,result.returncode,flush=True)
    if result.returncode:raise RuntimeError(name+' failed: '+str(out/(name+'.log')))
try:
    for name in ('UnrealEditor','qrenderdoc'):
        if subprocess.run(['pgrep','-x',name],stdout=subprocess.DEVNULL).returncode==0:
            raise RuntimeError(name+' already running')
    if os.environ.get('RENDERDOC_METAL_SKIP_BUILD')!='1':
        step('build',['cmake','--build',build,'--target','renderdoccmd','build-qrenderdoc','-j2'],600)
    report['backend_sha256']=sha(lib)
    report['bundle_sha256']=sha(build/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib')
    if report['backend_sha256']!=report['bundle_sha256']:raise RuntimeError('library/bundle mismatch')
    step('capture-helper',['clang++','-std=c++17','-I.','util/test/metal/metal_ray_table_capture.mm',
         '-framework','Foundation','-framework','Metal','-framework','QuartzCore','-o',out/'capture'])
    link=['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I.','-L'+str(build/'lib'),
          '-lrenderdoc','-Wl,-rpath,'+str(build/'lib')]
    step('replay-helper',link+['util/test/metal/metal_ray_table_replay.cpp','-o',out/'replay'])
    modes=['table-identity-large-visible-33','background-table-identity-large-visible-33',
           'table-identity-frame-born-large-visible-33','background-table-identity-unused-large-visible-33']
    selections=0
    for mode in modes:
        cap=caps/(mode+'_capture.rdc')
        step(mode+'-native',['env','MTL_DEBUG_LAYER=1',out/'capture',caps/(mode+'-native'),mode],30)
        trace=out/(mode+'-renderdoc.log')
        # RenderDoc deletes its log on clean shutdown if no other process holds
        # the shared logfile lock. Keep evidence until the child has exited.
        with trace.open('w') as retained_log:
            fcntl.flock(retained_log.fileno(),fcntl.LOCK_SH)
            step(mode+'-capture',['env','MTL_DEBUG_LAYER=1','DYLD_INSERT_LIBRARIES='+str(lib),
                 'RENDERDOC_METAL_PIPELINE_COMPILE_TRACE=1','RENDERDOC_DEBUG_LOG_FILE='+str(trace),
                 out/'capture',caps/mode,mode],30)
        with trace.open(errors='replace') as lines:compile_audit=audit_compile_trace(lines)
        if compile_audit['status']!='COMPLETE TRACE' or compile_audit['begin_count']!=1 or \
           compile_audit['native_failed_count'] or compile_audit['completed'][0]['api']!='descriptor-sync':
            raise RuntimeError('native compile trace did not prove one successful descriptor call')
        completed=compile_audit['completed'][0]
        if not completed['begin_native_thread'] or \
           completed['begin_native_thread']!=completed['end_native_thread']:
            raise RuntimeError('synchronous compile trace lost native thread identity')
        (out/(mode+'-compile-trace.json')).write_text(json.dumps(compile_audit,indent=2)+'\n')
        step(mode+'-replay',['env','MTL_DEBUG_LAYER=1',out/'replay',cap,mode])
        match=re.search(r'(\d+) events in three directions',(out/(mode+'-replay.log')).read_text())
        if not match:raise RuntimeError('missing event oracle')
        selections+=int(match.group(1))*3
        step(mode+'-cli',[cli,'replay','--loops','3',cap])
        step(mode+'-invalid',['python3','util/test/metal/metal_ray_table_identity_invalid.py',cli,cap],180)
    report['event_selections']=selections
    old=[('old-AS-identity','captures/metal-ray-b484/background-multi-indexed-tlas-identity_capture.rdc',
          'background-multi-indexed-tlas-identity'),
         ('old-frame-private','captures/metal-ray-b494/background-indirect-private-same-cb-tlas-frame-rebuild_capture.rdc',
          'background-indirect-private-same-cb-tlas-frame-rebuild'),
         ('old-frame-alias','captures/metal-ray-b494/background-indirect-private-placement-input-same-cb-tlas-alias-gpu-mutated-inactive-frame_capture.rdc',
          'background-indirect-private-placement-input-same-cb-tlas-alias-gpu-mutated-inactive-frame')]
    for name,path,mode in old:step(name,['env','MTL_DEBUG_LAYER=1',out/'replay',repo/path,mode])
    step('old-function-tables',['bash','util/buildscripts/scripts/test_metal_replay_targeted_macos.sh',
         '--sentinel','t44','t120','t126','t130','t135','t140','t141','t144','t148'],180)
    step('lifecycle-helper',link+['util/test/metal/metal_replay_lifecycle_smoke.mm',
         '-framework','Foundation','-framework','Metal','-framework','QuartzCore','-o',out/'lifecycle'])
    step('lifecycle',['env','MTL_DEBUG_LAYER=1',out/'lifecycle',repo/'captures/metal-smoke/t35_capture.rdc',
                     *[caps/(m+'_capture.rdc') for m in modes],'10'],120)
    report['backend_end_sha256']=sha(lib)
    if report['backend_end_sha256']!=report['backend_sha256']:raise RuntimeError('backend changed during gate')
    report['status']='PASS'
except Exception as error:
    report.update(status='FAIL',error=str(error));raise
finally:save()
PY
