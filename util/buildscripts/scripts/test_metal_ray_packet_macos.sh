#!/bin/bash
# Serial, bounded argument-buffer AS/IFT/VFT regression. No UE launch.
set -euo pipefail
cd "$(dirname "$0")/../../.."
python3 - <<'PY'
from pathlib import Path
import fcntl,hashlib,json,os,re,signal,subprocess
repo=Path.cwd();build=Path(os.environ.get('RENDERDOC_METAL_BUILD_DIR',repo/'build-macos-debug')).resolve()
out=Path(os.environ.get('RENDERDOC_METAL_RESULT_DIR',build/'metal-ray-b499')).resolve()
caps=Path(os.environ.get('RENDERDOC_METAL_CAPTURE_DIR',repo/'captures/metal-ray-b499')).resolve()
out.mkdir(parents=True,exist_ok=True);caps.mkdir(parents=True,exist_ok=True)
lib=build/'lib/librenderdoc.dylib';cli=build/'bin/renderdoccmd';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
m={'status':'RUNNING','checks':[],'UE':'NOT RUN','full_regression':'NOT RUN','GUI':'NOT RUN'}
def save():(out/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
def step(name,args,seconds=60,env=None):
    with (out/(name+'.log')).open('w') as log:
        proc=subprocess.Popen([str(a) for a in args],env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
        try:code=proc.wait(timeout=seconds)
        except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);proc.wait();code=124
    m['checks'].append({'name':name,'exit':code});save();print(name,code,flush=True)
    if code:raise RuntimeError(name+' failed: '+str(out/(name+'.log')))
try:
    for name in ('UnrealEditor','qrenderdoc'):
        if subprocess.run(['pgrep','-x',name],stdout=subprocess.DEVNULL).returncode==0:raise RuntimeError(name+' already running')
    if os.environ.get('RENDERDOC_METAL_SKIP_BUILD')!='1':step('build',['cmake','--build',build,'--target','renderdoccmd','build-qrenderdoc','-j2'],600)
    m['backend_sha256']=sha(lib);m['bundle_sha256']=sha(build/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib')
    if m['backend_sha256']!=m['bundle_sha256']:raise RuntimeError('backend/bundle mismatch')
    step('capture-build',['clang++','-std=c++17','-I.','util/test/metal/metal_ray_packet_capture.mm','-framework','Foundation','-framework','Metal','-framework','QuartzCore','-o',out/'capture'])
    link=['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I.','-L'+str(build/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(build/'lib')]
    step('replay-build',link+['util/test/metal/metal_ray_packet_replay.cpp','-o',out/'replay'])
    clean=os.environ.copy()
    for key in tuple(clean):
        if key.startswith('RENDERDOC_METAL_') or key=='DYLD_INSERT_LIBRARIES':clean.pop(key)
    clean['MTL_DEBUG_LAYER']='1'
    modes=['function','device','function-managed','device-managed','function-arrays','device-arrays-managed']
    tlas=os.environ.get('RENDERDOC_METAL_PACKET_TLAS')=='1'
    extended=os.environ.get('RENDERDOC_METAL_PACKET_EXTENDED_NEGATIVES')=='1'
    if tlas:modes += [mode+'-tlas' for mode in modes.copy()]
    rejected_modes=['function-frame','device-managed-frame']
    if tlas:rejected_modes += [mode+'-tlas' for mode in rejected_modes.copy()]
    selections=0
    for mode in modes+rejected_modes:
        step(mode+'-native',[out/'capture',caps/(mode+'-native'),mode],30,clean)
        env=clean.copy();trace=out/(mode+'-capture-renderdoc.log');env.update(DYLD_INSERT_LIBRARIES=str(lib),RENDERDOC_DEBUG_LOG_FILE=str(trace))
        with trace.open('w') as retained:
            fcntl.flock(retained.fileno(),fcntl.LOCK_SH);step(mode+'-capture',[out/'capture',caps/mode,mode],30,env)
        native=[s for s in (out/(mode+'-native.log')).read_text().splitlines() if s.startswith('PASS '+mode)]
        capture=[s for s in (out/(mode+'-capture.log')).read_text().splitlines() if s.startswith('PASS '+mode)]
        if len(native)!=1 or native!=capture:raise RuntimeError('native/capture output or layout differs')
        cap=caps/(mode+'_capture.rdc')
        if 'frame' in mode:
            with (out/(mode+'-rejected.log')).open('w') as log:
                result=subprocess.run([str(cli),'replay','--loops','1',str(cap)],stdout=log,stderr=subprocess.STDOUT,timeout=60)
            if result.returncode<=0 or 'MTLArgumentEncoder::unsupportedEncoding' not in (out/(mode+'-rejected.log')).read_text():
                raise RuntimeError('frame mutation was not cleanly rejected')
            m['checks'].append({'name':mode+'-expected-rejection','exit':0});save();continue
        step(mode+'-replay',[out/'replay',cap],60,clean)
        match=re.search(r'(\d+) events in three directions/EID0',(out/(mode+'-replay.log')).read_text())
        if not match:raise RuntimeError('missing event oracle')
        selections+=int(match.group(1))*3
        step(mode+'-cli',[cli,'replay','--loops','3',cap],60,clean)
        flags=(['--extended'] if extended else [])+(['--tlas'] if extended and 'tlas' in mode else [])
        step(mode+'-invalid',['python3','util/test/metal/metal_ray_packet_invalid.py',cli,cap,*flags],120,clean)
    m['event_selections']=selections
    oldenv=os.environ.copy();oldenv['RENDERDOC_METAL_CAPTURE_DIR']=str(repo/'captures/metal-smoke')
    step('old-arguments',['bash','util/buildscripts/scripts/test_metal_replay_targeted_macos.sh','--sentinel','t54','t55','t60','t119','t120','t126','t127','t128','t129'],180,oldenv)
    step('old-device-argument-invalid',['python3','util/test/metal/metal_device_argument_encoder_invalid.py',cli,repo/'captures/metal-smoke/t60_capture.rdc'],120)
    for variant in ('t126','t127'):
        step('old-visible-argument-invalid-'+variant,['python3','util/test/metal/metal_argument_visible_table_invalid.py',cli,repo/'captures/metal-smoke'/ (variant+'_capture.rdc')],120)
    step('lifecycle-build',link+['util/test/metal/metal_replay_lifecycle_smoke.mm','-framework','Foundation','-framework','Metal','-framework','QuartzCore','-o',out/'lifecycle'])
    step('lifecycle',[out/'lifecycle',repo/'captures/metal-smoke/t35_capture.rdc',*[caps/(s+'_capture.rdc') for s in modes],'10'],120,clean)
    m['backend_end_sha256']=sha(lib)
    if m['backend_end_sha256']!=m['backend_sha256']:raise RuntimeError('backend changed during validation')
    m['status']='PASS'
except Exception as error:m.update(status='FAIL',error=str(error));raise
finally:save()
PY
