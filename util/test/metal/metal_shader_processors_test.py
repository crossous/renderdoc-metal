#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Exercise the shipped processors, actual native compiler, relocation and failure paths."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('app',type=Path)
parser.add_argument('result',type=Path)
parser.add_argument('--specialized-library',type=Path)
args=parser.parse_args()
root=args.app.resolve()/'Contents/Resources/shader-tools'
result=args.result.resolve();result.mkdir(parents=True,exist_ok=True)
env=dict(os.environ);env['PATH']='/usr/bin:/bin'

def call(mode,source,dest,entry,extra=(),expected=0,toolroot=root):
    run=subprocess.run(['/usr/bin/python3',str(toolroot/'metal_shader_processor.py'),mode,
                        str(source),str(dest),entry,*extra],env=env,capture_output=True,text=True,timeout=150)
    print(run.stdout+run.stderr,end='')
    assert run.returncode==expected,(mode,run.returncode,run.stderr)
    if expected==0:assert dest.exists() and dest.stat().st_size>0
    else:assert not dest.exists()
    print('PASS',mode,'expectedExit='+str(expected))

source=result/'literal.metal'
source.write_text('#include <metal_stdlib>\nusing namespace metal;\nfragment float4 literal_fs() {return float4(0.25,0.5,0.75,1);}\n')
call('compile-msl',source,result/'literal.metallib','literal_fs')
for mode,extension,options in [('air','ll',()),('msl','msl',('--editable',)),
                               ('hlsl','hlsl',('--view-only',)),('glsl','glsl',('--view-only',))]:
    call(mode,result/'literal.metallib',result/('literal.'+extension),'literal_fs',options)
call('compile-msl',result/'literal.msl',result/'compile-msl.metallib','literal_fs')
call('compile-air',result/'literal.ll',result/'compile-air.metallib','literal_fs')
assert 'fragment literal_fs_out literal_fs(' in (result/'literal.msl').read_text()
if args.specialized_library:
    call('msl',args.specialized_library.resolve(),result/'specialized-preview.msl','fs',('--view-only',))
    assert 'VIEW ONLY' in (result/'specialized-preview.msl').read_text()
    call('msl',args.specialized_library.resolve(),result/'specialized-edit.msl','fs',('--editable',),expected=1)
invalid=result/'invalid.metal';invalid.write_text('invalid source')
failed=result/'invalid.metallib';failed.write_bytes(b'stale output')
call('compile-msl',invalid,failed,'fs',expected=1)
# The shipped distribution must be self contained after relocation, including spaces in its path.
with tempfile.TemporaryDirectory(prefix='renderdoc shader tools relocated ') as tmp:
    relocated=Path(tmp)/'tools';shutil.copytree(root,relocated)
    call('msl',result/'literal.metallib',result/'relocated.msl','literal_fs',('--editable',),toolroot=relocated)
    assert (result/'relocated.msl').read_bytes()==(result/'literal.msl').read_bytes()
manifest=json.loads((root/'manifest.json').read_text())
for relative,digest in manifest['files'].items():
    assert hashlib.sha256((root/relative).read_bytes()).hexdigest()==digest,relative
# A subprocess deadline must kill the worker, then clean its private scratch directory.
sys.path.insert(0,str(root))
import metal_shader_processor as processor
run_tool = processor.run_tool
with tempfile.TemporaryDirectory() as temp:
    try:run_tool(['/bin/sleep','5'],Path(temp),timeout=0.1)
    except RuntimeError as error:assert 'second limit' in str(error)
    else:raise AssertionError('deadline was not enforced')
    processor.DEADLINE = time.monotonic() + 0.1
    try:run_tool(['/bin/sleep','5'],Path(temp),timeout=120)
    except RuntimeError as error:assert 'second limit' in str(error)
    else:raise AssertionError('total deadline was not enforced')
    processor.DEADLINE = None
print('PASS package hashes, relocation without Homebrew PATH, stale output guard and timeout')
