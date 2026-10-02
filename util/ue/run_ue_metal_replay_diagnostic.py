#!/usr/bin/env python3
"""Run one explicit Metal replay diagnostic, preserving the input capture and exact library hash."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

def main():
    root=Path(__file__).resolve().parents[2]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True)
    parser.add_argument('--mode',choices=('cpu','pre-submit','initial','prefix'),default='pre-submit')
    parser.add_argument('--commits',type=int)
    parser.add_argument('--build-dir',type=Path,default=root/'build-macos-debug')
    parser.add_argument('--log',type=Path,required=True)
    parser.add_argument('--timeout',type=int,default=240)
    args=parser.parse_args()
    if args.mode=='prefix' and (args.commits is None or not 1<=args.commits<=256):parser.error('prefix requires --commits between1 and256')
    if args.mode!='prefix' and args.commits is not None:parser.error('--commits only applies to prefix')
    if args.timeout<1:parser.error('--timeout must be positive')
    capture=args.capture.resolve();build=args.build_dir.resolve();log=args.log.resolve();log.parent.mkdir(parents=True,exist_ok=True)
    library=build/'lib/librenderdoc.dylib';opener=build/'local-m2-descriptor-replay/diagnostic-opener';opener.parent.mkdir(parents=True,exist_ok=True)
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    library_hash=sha(library);capture_hash=sha(capture)
    subprocess.run(['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I'+str(root),str(root/'util/ue/ue_capture_open_probe.cpp'),'-L'+str(library.parent),'-lrenderdoc','-Wl,-rpath,'+str(library.parent),'-o',str(opener)],check=True)
    flags=('RENDERDOC_METAL_CPU_METADATA_COVERAGE','RENDERDOC_METAL_PRE_SUBMIT_COVERAGE','RENDERDOC_METAL_INITIAL_UPLOAD_COVERAGE','RENDERDOC_METAL_FRAME_PREFIX_COVERAGE','RENDERDOC_METAL_FRAME_PREFIX_COMMITS')
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    for key in flags:env.pop(key,None)
    env[flags[{'cpu':0,'pre-submit':1,'initial':2,'prefix':3}[args.mode]]]='65'
    if args.mode=='prefix':env[flags[4]]=str(args.commits)
    if args.mode=='initial':env['RENDERDOC_METAL_TRACE_INITIAL_PRIVATE']='1'
    expected={'cpu':'Metal CPU-only metadata preflight accepted candidate coverage65',
              'pre-submit':'Metal pre-submit diagnostic accepted candidate coverage65',
              'initial':'initial GPU uploads completed and frame replay was not executed',
              'prefix':f'{args.commits} commits completed; capture loading stopped at commit boundary'}[args.mode]
    with log.open('w') as output:
        try:code=subprocess.run([str(opener),str(capture)],env=env,stdout=output,stderr=subprocess.STDOUT,timeout=args.timeout).returncode
        except subprocess.TimeoutExpired:code=124
    result=log.read_text(errors='replace')
    accepted=code==4 and expected in result and sha(library)==library_hash and sha(capture)==capture_hash
    if args.mode in ('cpu','pre-submit','initial'):accepted &= 'Metal replay wait begin' not in result
    manifest={'mode':args.mode,'commits':args.commits,'capture':str(capture),'capture_sha256':capture_hash,'library_sha256':library_hash,'exit_code':code,'accepted_forced_exit':accepted,'log':str(log)}
    log.with_suffix(log.suffix+'.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2));print(result[-3000:])
    return 0 if accepted else 1
if __name__=='__main__':sys.exit(main())
