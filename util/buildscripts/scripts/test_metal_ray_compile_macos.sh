#!/bin/bash
# Finite six-entry native RT compile/capture/replay diagnostic. No UE launch.
set -euo pipefail
cd "$(dirname "$0")/../../.."
python3 - <<'PY'
import fcntl, hashlib, json, os, re, signal, subprocess, sys
from pathlib import Path
repo=Path.cwd();sys.path.insert(0,str(repo/'util/ue'))
from metal_pipeline_compile_audit import audit_compile_trace, APIS, MARKER
build=Path(os.environ.get('RENDERDOC_METAL_BUILD_DIR',repo/'build-macos-debug')).resolve()
out=Path(os.environ.get('RENDERDOC_METAL_RESULT_DIR',build/'metal-ray-b498')).resolve()
caps=Path(os.environ.get('RENDERDOC_METAL_CAPTURE_DIR',repo/'captures/metal-ray-b498')).resolve()
out.mkdir(parents=True,exist_ok=True);caps.mkdir(parents=True,exist_ok=True)
lib=build/'lib/librenderdoc.dylib';cli=build/'bin/renderdoccmd'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report={'status':'RUNNING','checks':[],'UE':'NOT RUN','full_regression':'NOT RUN','GUI':'NOT RUN'}
def save():(out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
def step(name,args,seconds=60,env=None):
    with (out/(name+'.log')).open('w') as log:
        process=subprocess.Popen([str(a) for a in args],env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
        try:code=process.wait(timeout=seconds)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid,signal.SIGKILL);process.wait();code=124
    report['checks'].append({'name':name,'exit':code});save();print(name,code,flush=True)
    if code:raise RuntimeError(name+' failed; see '+str(out/(name+'.log')))
def rays(name):return [s for s in (out/(name+'.log')).read_text().splitlines() if s.startswith('PASS ray query ')]
try:
    for name in ('UnrealEditor','qrenderdoc'):
        if subprocess.run(['pgrep','-x',name],stdout=subprocess.DEVNULL).returncode==0:raise RuntimeError(name+' already running')
    report['backend_sha256']=sha(lib);report['bundle_sha256']=sha(build/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib')
    if report['backend_sha256']!=report['bundle_sha256']:raise RuntimeError('backend/bundle mismatch')
    step('capture-build',['clang++','-std=c++17','-I.','util/test/metal/metal_ray_compile_capture.mm',
                         '-framework','Foundation','-framework','Metal','-framework','QuartzCore','-o',out/'capture'])
    link=['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I.','-L'+str(build/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(build/'lib')]
    step('replay-build',link+['util/test/metal/metal_ray_compile_replay.cpp','-o',out/'replay'])
    clean=os.environ.copy()
    for key in tuple(clean):
        if key.startswith('RENDERDOC_METAL_') or key=='DYLD_INSERT_LIBRARIES':clean.pop(key)
    clean['MTL_DEBUG_LAYER']='1'
    step('native',[out/'capture',out/'native'],30,clean)
    expected=rays('native')
    if len(expected)!=6 or any(not s.endswith('hit/miss=1/0') for s in expected):raise RuntimeError('native ray oracle failed')
    for case,flag in [('enabled','1'),('default',None),('disabled','0'),('invalid-text','true'),('empty','')]:
        env=clean.copy();trace=out/(case+'-renderdoc.log');env.update(DYLD_INSERT_LIBRARIES=str(lib),RENDERDOC_DEBUG_LOG_FILE=str(trace))
        if flag is not None:env['RENDERDOC_METAL_PIPELINE_COMPILE_TRACE']=flag
        with trace.open('w') as retained:
            fcntl.flock(retained.fileno(),fcntl.LOCK_SH)
            step(case+'-capture',[out/'capture',caps/case],30,env)
        if rays(case+'-capture')!=expected:raise RuntimeError(case+' changed native ray output')
        with trace.open() as lines:audit=audit_compile_trace(lines)
        (out/(case+'-compile-trace.json')).write_text(json.dumps(audit,indent=2)+'\n')
        if flag!='1':
            if audit['status']!='NOT OBSERVED':raise RuntimeError(case+' unexpectedly enabled trace')
            continue
        if audit['status']!='COMPLETE TRACE' or audit['begin_count']!=6 or audit['completed_count']!=6 or audit['native_failed_count']:
            raise RuntimeError('six native calls did not complete')
        if {c['api'] for c in audit['completed']}!=APIS:raise RuntimeError('not all six APIs traced')
        records=[json.loads(line.split(MARKER,1)[1]) for line in trace.read_text().splitlines() if MARKER in line]
        begins={r['token']:r for r in records if r['phase']=='begin'}
        submitted=[r for r in records if r['phase']=='submitted']
        if len(submitted)!=3 or len({r['token'] for r in submitted})!=3 or any(not begins[r['token']]['api'].endswith('async') for r in submitted):
            raise RuntimeError('async submission trace count/identity mismatch')
        order=['function-sync','function-options-sync','descriptor-sync','function-async','function-options-async','descriptor-async']
        for c in audit['completed']:
            b=begins[c['token']];i=order.index(c['api'])
            if c['function_name']!='trace'+str(i) or b['options']!=(0 if i in (0,3) else 1) or \
               b['linked_functions'] or b['binary_functions'] or b['private_functions'] or \
               not c['begin_native_thread'] or not c['end_native_thread']:
                raise RuntimeError('native descriptor/function/options/thread identity mismatch')
            if not c['api'].endswith('async') and c['begin_native_thread']!=c['end_native_thread']:
                raise RuntimeError('sync native thread changed')
        report['native_compile_trace']=audit;report['async_submissions']=3
    capture=caps/'enabled_capture.rdc'
    step('replay',[out/'replay',capture],60,clean)
    match=re.search(r'(\d+) events in three directions/EID0',(out/'replay.log').read_text())
    if not match:raise RuntimeError('missing event oracle')
    report['event_selections']=int(match.group(1))*3
    step('cli',[cli,'replay','--loops','3',capture],60,clean)
    step('invalid',['python3','util/test/metal/metal_ray_compile_invalid.py',cli,capture],120,clean)
    step('old-pipelines',['bash','util/buildscripts/scripts/test_metal_replay_targeted_macos.sh','--sentinel','t36','t37','t38','t39','t40','t41','t47','t48','t51','t52'],180)
    step('lifecycle-build',link+['util/test/metal/metal_replay_lifecycle_smoke.mm','-framework','Foundation','-framework','Metal','-framework','QuartzCore','-o',out/'lifecycle'])
    step('lifecycle',[out/'lifecycle',repo/'captures/metal-smoke/t35_capture.rdc',
                     repo/'captures/metal-ray-b497/table-identity-frame-born-large-visible-33_capture.rdc',capture,'10'],120,clean)
    include=Path(os.environ.get('UE_METAL_IR_INCLUDE_DIR','/Users/Shared/Epic Games/UE_5.8/Engine/Source/ThirdParty/Apple/MetalShaderConverter/include'))
    header=include/'metal_irconverter_runtime/ir_raytracing.h'
    if header.exists():
        step('ir-layout-build',['clang++','-std=c++17','-I'+str(include),'util/test/metal/metal_ir_ray_layout.mm','-framework','Foundation','-framework','Metal','-o',out/'ir-layout'])
        step('ir-layout',[out/'ir-layout'],10)
        report['UE_IR_layout']=json.loads((out/'ir-layout.log').read_text());report['UE_IR_header_sha256']=sha(header)
    else:report['UE_IR_layout']='NOT RUN: matching UE IR include not present'
    report['backend_end_sha256']=sha(lib)
    if report['backend_sha256']!=report['backend_end_sha256']:raise RuntimeError('backend changed during tests')
    report['status']='PASS'
except Exception as error:report.update(status='FAIL',error=str(error));raise
finally:save()
PY
