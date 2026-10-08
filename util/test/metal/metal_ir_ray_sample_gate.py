#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Finite converted DXR sample: native/capture/replay and complete ABI evidence."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, default=Path('/Users/Shared/Epic Games/UE_5.8/Engine'))
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--mode', default='default', choices=['default','ue-pad','pattern-pad',
        'null-hit','null-miss','ue-null-both','ue-any-hit','ue-any-hit-no-closest','ue-any-hit-null-miss','local-root','ue-local-root-any-hit','ue-local-root-null-hit',
        'ue-local-root-null-miss','ue-local-root-any-hit-no-closest','ue-local-root-six-samplers',
        'global-root','ue-global-root-any-hit','ue-global-root-null-miss',
        'ue-global-local-root-six-samplers','ue-global-local-root-six-samplers-any-hit',
        'descriptor-heaps','ue-descriptor-heaps-any-hit','ue-descriptor-heaps-null-hit','ue-descriptor-heaps-null-miss',
        'ue-descriptor-heaps-local-root-six-samplers','ue-descriptor-heaps-local-root-six-samplers-any-hit',
        'ue-global-descriptor-heaps-local-root-six-samplers',
        'descriptor-heaps-heap-as','ue-descriptor-heaps-heap-as-any-hit','ue-descriptor-heaps-heap-as-null-hit',
        'ue-descriptor-heaps-heap-as-null-miss','ue-descriptor-heaps-heap-as-local-root-six-samplers',
        'ue-descriptor-heaps-heap-as-local-root-six-samplers-any-hit',
        'ue-global-descriptor-heaps-heap-as-local-root-six-samplers',
        'descriptor-heaps-heap-as-only','ue-descriptor-heaps-heap-as-only-local-root-six-samplers','ue-indirect-tlas','ue-indirect-tlas-empty','ue-indirect-tlas-masked','ue-indirect-tlas-private','ue-indirect-tlas-private-masked','ue-descriptor-heaps-heap-as-only-indirect-tlas','ue-descriptor-heaps-heap-as-only-indirect-tlas-empty','ue-descriptor-heaps-heap-as-only-indirect-tlas-masked','ue-descriptor-heaps-heap-as-only-indirect-tlas-private','ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private','ue-descriptor-heaps-heap-as-only-local-root-six-samplers-indirect-tlas','ue-descriptor-heaps-heap-as-only-indirect-tlas-any-hit','ue-indirect-tlas-any-hit','ue-indirect-tlas-null-hit','ue-indirect-tlas-null-miss','ue-indirect-tlas-private-frame-build','ue-indirect-tlas-private-empty-frame-build','ue-indirect-tlas-private-masked-frame-build','ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build','ue-descriptor-heaps-heap-as-only-indirect-tlas-private-empty-frame-build','ue-descriptor-heaps-heap-as-only-local-root-six-samplers-indirect-tlas-private-frame-build','ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build','ue-indirect-tlas-scratch-offset','ue-indirect-tlas-private-scratch-offset','ue-indirect-tlas-empty-scratch-offset','ue-indirect-tlas-private-masked-scratch-offset','ue-indirect-tlas-private-frame-build-scratch-offset','ue-indirect-tlas-private-empty-frame-build-scratch-offset','ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build-scratch-offset','ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build-scratch-offset','ue-indirect-tlas-private-frame-build-geometry','ue-indirect-tlas-private-frame-build-geometry-indexed','ue-indirect-tlas-private-frame-build-geometry-new-target','ue-indirect-tlas-private-frame-build-geometry-new-target-indexed','ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build-geometry-new-target','ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build-geometry-indexed','ue-indirect-tlas-private-frame-build-geometry-multi-indexed','ue-indirect-tlas-private-frame-build-geometry-new-target-multi-indexed','ue-indirect-tlas-private-frame-build-geometry-multi64-indexed','ue-indirect-tlas-private-frame-build-geometry-new-target-multi64-indexed','ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build-geometry-new-target-multi-indexed','ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build-geometry-multi-indexed'])
    parser.add_argument('--typed', action='store_true', help='Require typed IR replay, with API event oracle')
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--capture-dir', type=Path)
    args = parser.parse_args()
    local = 'local-root' in args.mode
    global_root = 'global-' in args.mode
    heaps = 'descriptor-heaps' in args.mode
    heap_as = 'heap-as' in args.mode
    heap_only = 'heap-as-only' in args.mode
    indirect = 'indirect-tlas' in args.mode
    inactive = indirect and any(x in args.mode for x in ('-empty','-masked'))
    frame = 'frame-build' in args.mode
    identity_count = (3 if heap_as else 2 if indirect and (frame or not inactive) else 1) + (1 if "geometry-new-target" in args.mode else 0)
    repo = Path(__file__).resolve().parents[3]
    out = (args.work_dir or repo/'build-macos-debug/metal-ray-b500/ir-native').resolve()
    caps = (args.capture_dir or repo/'captures/metal-ray-b500').resolve()
    out.mkdir(parents=True, exist_ok=True); caps.mkdir(parents=True, exist_ok=True)
    engine = args.engine.resolve()
    dxc = engine/'Binaries/ThirdParty/ShaderConductor/Mac'
    converter = engine/'Binaries/ThirdParty/Apple/MetalShaderConverter/Mac'
    dxc_include = engine/'Source/ThirdParty/ShaderConductor/ShaderConductor/External/DirectXShaderCompiler/include'
    ir_include = engine/'Source/ThirdParty/Apple/MetalShaderConverter/include'
    build = (args.build_dir or repo/'build-macos-debug').resolve()
    library = build/'lib/librenderdoc.dylib'
    cli = build/'bin/renderdoccmd'
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    manifest = dict(status='RUNNING', replay='REQUIRED' if args.typed else 'EXPECTED UNSUPPORTED', checks=[], hashes={})
    env = os.environ.copy()
    for key in tuple(env):
        if key.startswith('RENDERDOC_METAL_') or key == 'DYLD_INSERT_LIBRARIES': env.pop(key)
    env['MTL_DEBUG_LAYER'] = '1'
    env['RENDERDOC_IR_RAY_MODE'] = args.mode
    env.pop('RENDERDOC_IR_RAY_TYPED', None)
    if args.typed: env['RENDERDOC_IR_RAY_TYPED'] = '1'

    def run(name, command, seconds, current_env=env, expected=None):
        effective_env=current_env.copy()
        effective_env.setdefault("RENDERDOC_DEBUG_LOG_FILE",str(out/(name+"-renderdoc.log")))
        # POSIX RenderDoc deletes a log at shutdown when no other shared lock
        # exists. Keep our reference until the child exits so diagnostics survive.
        with Path(effective_env['RENDERDOC_DEBUG_LOG_FILE']).open('a') as retained, (out/(name+'.log')).open('w') as log:
            fcntl.flock(retained.fileno(),fcntl.LOCK_SH)
            process = subprocess.Popen([str(x) for x in command], cwd=repo, env=effective_env,
                stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try: code = process.wait(timeout=seconds)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL); process.wait(); code = 124
        text = (out/(name+'.log')).read_text(errors='replace')
        trace_path=Path(effective_env['RENDERDOC_DEBUG_LOG_FILE'])
        diagnostics=text+(trace_path.read_text(errors='replace') if trace_path.exists() else '')
        ok = code == 0 if expected is None else 0 < code < 124 and expected in text
        ok &= not any(marker in diagnostics for marker in ('Assertion failed','OVERRUNNING CHUNK',
            'failed assertion','Unexpected Metal resource type','m_ResourceMap.empty',
            'Metal frame geometry snapshot missing'))
        manifest['checks'].append(dict(name=name, exit=code, passed=ok, expected=expected))
        (out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        print(('PASS ' if ok else 'FAIL ')+name, flush=True)
        if not ok: raise RuntimeError(name+': '+diagnostics[-2500:])
        return text

    try:
        manifest['engine_version'] = json.loads((engine/'Build/Build.version').read_text())
        for name in ('UnrealEditor', 'qrenderdoc'):
            if subprocess.run(['pgrep', '-x', name], stdout=subprocess.DEVNULL).returncode == 0:
                raise RuntimeError(name+' running')
        dependencies = [engine/'Build/Build.version', dxc/'libdxcompiler.dylib', converter/'libmetalirconverter.dylib',
            dxc_include/'dxc/dxcapi.h', ir_include/'metal_irconverter/metal_irconverter.h',
            ir_include/'metal_irconverter_runtime/metal_irconverter_runtime.h',
            ir_include/'metal_irconverter_runtime/ir_raytracing.h', library,
            repo/'util/test/metal/metal_ir_ray_shaders.hlsl', repo/'util/test/metal/metal_ir_ray_convert.cpp',
            repo/'util/test/metal/metal_ir_ray_native.mm', repo/'util/test/metal/metal_ir_ray_replay.cpp',
            repo/'util/test/metal/metal_ir_ray_audit.py', repo/'util/test/metal/metal_ir_ray_invalid.py']
        manifest['hashes'] = {str(p): sha(p) for p in dependencies}
        run('convert-build', ['clang++', '-std=c++17', '-I'+str(dxc_include), '-I'+str(ir_include),
            repo/'util/test/metal/metal_ir_ray_convert.cpp', '-L'+str(dxc), '-ldxcompiler',
            '-L'+str(converter), '-lmetalirconverter', '-Wl,-rpath,'+str(dxc), '-Wl,-rpath,'+str(converter),
            '-o', out/'convert'], 60)
        variant = ('multi-geometry-' if 'geometry' in args.mode and 'multi' in args.mode else '') + ('indirect-tlas-' if indirect else '') + ('global-' if global_root else '') + ('descriptor-heaps-' if heaps else '') + ('heap-as-only-' if heap_only else 'heap-as-' if heap_as else '') + ('local-root-six-samplers' if 'six-samplers' in args.mode else 'local-root' if local else 'root')
        run('convert', [out/'convert', repo/'util/test/metal/metal_ir_ray_shaders.hlsl', out] + ([variant] if local or global_root or heaps or indirect else []), 30)
        if global_root:
            expected_layout = [(0,8,'SRV'),(8,8,'UAV'),(16,8,'CBV'),(24,16,'Constant'),(40,8,'SRV'),(48,8,'Table'),(56,8,'Table')]
            reflection = json.loads((out/'raygen.reflection.json').read_text())
            actual_layout = [(e['EltOffset'],e['Size'],e['Type']) for e in reflection['TopLevelArgumentBuffer']]
            root = json.loads((out/'global-root.json').read_text())['RootSignature']
            if actual_layout != expected_layout or reflection['EntryPoint'] != 'raygen' or reflection['ShaderType'] != 'Raygeneration' or root['NumParameters'] != 6 or root['NumStaticSamplers'] != 6:
                raise RuntimeError('SDK global reflection differs from fixture declaration')
            manifest['SDK_global_layout'] = actual_layout
        run('native-build', ['clang++', '-std=c++17', '-fobjc-arc', '-I'+str(repo), '-I'+str(ir_include),
            repo/'util/test/metal/metal_ir_ray_native.mm', '-framework', 'Foundation', '-framework', 'Metal',
            '-framework', 'QuartzCore', '-o', out/'native'], 60)
        manifest['hashes'].update({str(p): sha(p) for p in [out/'pipeline.dxil', *out.glob('*.metallib'), *out.glob('*.reflection.json'), *out.glob('*-root.json'), out/'native', out/'convert']})
        native = run('native', [out/'native', out], 20)
        injected = env.copy(); trace = out/'capture-renderdoc.log'
        injected.update(DYLD_INSERT_LIBRARIES=str(library), RENDERDOC_DEBUG_LOG_FILE=str(trace),
            RENDERDOC_METAL_RAYTRACING_PROBE='1', RENDERDOC_METAL_PIPELINE_COMPILE_TRACE='1')
        with trace.open('w') as retained:
            fcntl.flock(retained.fileno(), fcntl.LOCK_SH)
            capture = run('capture', [out/'native', out, caps/'ir-runtime'], 20, injected)
        null_hit = any(x in args.mode for x in ('null-hit','null-both','no-closest'))
        null_miss = any(x in args.mode for x in ('null-miss','null-both'))
        want_miss = 7 if null_miss else 243 if local else 11
        want_hit = want_miss if 'any-hit' in args.mode else 7 if null_hit else (131 if local else 73)+((74 if frame else 73) if indirect else 0)
        if 'geometry' in args.mode and 'multi' in args.mode:want_hit+=17*(63 if 'multi64' in args.mode else 1)
        if inactive and not frame: want_hit=want_miss
        elif heap_as != ("geometry" in args.mode): want_hit,want_miss=want_miss,want_hit
        if global_root: want_hit += 1211; want_miss += 1211
        if heaps: want_hit += 23; want_miss += 37
        want = f'PASS converted TraceRay hit/miss={want_hit}/{want_miss} dispatchArgument=152 shaderID=32 ASHeader=64'
        if want not in native or want not in capture: raise RuntimeError('native/capture TraceRay oracle differs')
        if 'scratch-offset' in args.mode and ('PASS scratchOffset=256 preserves prefix guard' not in native or 'PASS scratchOffset=256 preserves prefix guard' not in capture):raise RuntimeError('scratch offset native/capture guard differs')
        if 'geometry' in args.mode and ('PASS Private/GPU frame geometry cleared after BLAS' not in native or 'PASS Private/GPU frame geometry cleared after BLAS' not in capture):raise RuntimeError('Private geometry consumption/clear marker differs')
        capture_file = caps/'ir-runtime_capture.rdc'
        manifest['hashes'][str(capture_file)] = sha(capture_file)
        capture_log=(out/'capture-renderdoc.log').read_text(errors='replace')
        if any(marker in capture_log for marker in ('Assertion failed','OVERRUNNING CHUNK',
            'Unexpected Metal resource type','m_ResourceMap.empty')):
            raise RuntimeError('capture serialization or shutdown diagnostic')
        run('structured-export', [cli, 'convert', '-f', capture_file, '-o', out/'capture.zip.xml', '-c', 'zip.xml'], 30)
        tree = ET.parse(out/'capture.zip.xml'); names = [c.get('name') for c in tree.findall('./chunks/chunk')]
        counts = {name: names.count(name) for name in (
            'MTLComputeCommandEncoder::dispatchThreadgroups', 'MTLResource::CaptureGPUIdentity',
            'MTLAccelerationStructure::CaptureGPUIdentity', 'MTLFunctionTable::CaptureGPUIdentity')}
        manifest['chunk_counts'] = counts
        if args.typed:
            roles = [c for c in tree.findall('./chunks/chunk') if c.get('name') == 'MTLFunctionHandle::DeclareRayIRShaderRole']
            if len(roles) != 4 or sorted(int(c.find('./*[@name="role"]').text) for c in roles) != [0,1,2,3]:
                raise RuntimeError('missing immutable shader roles or duplicate idempotent declaration')
            roots = [c for c in tree.findall('./chunks/chunk') if c.get('name') == 'MTLFunctionHandle::DeclareRayIRLocalRoot']
            if len(roots) != (15 if local else 0): raise RuntimeError('local root declaration coverage differs')
            global_roots = [c for c in tree.findall('./chunks/chunk') if c.get('name') == 'MTLComputePipelineState::DeclareRayIRGlobalRoot']
            if len(global_roots) != (7 if global_root else 2 if heap_only else 0): raise RuntimeError('global root declaration coverage differs')
            heap_entries = [c for c in tree.findall('./chunks/chunk') if c.get('name') == 'MTLComputePipelineState::DeclareRayIRHeapEntry']
            if len(heap_entries) != (5 if heap_as else 4 if heaps else 0): raise RuntimeError('heap entry declaration coverage differs')
        if counts['MTLComputeCommandEncoder::dispatchThreadgroups'] != (2 if frame else 1) or \
            counts['MTLResource::CaptureGPUIdentity'] < 5 or \
            counts['MTLAccelerationStructure::CaptureGPUIdentity'] != identity_count or \
            counts['MTLFunctionTable::CaptureGPUIdentity'] != 2:
            raise RuntimeError('missing converted RT resource identity or dispatch evidence')
        run('ABI-audit', ['python3', repo/'util/test/metal/metal_ir_ray_audit.py', out/'capture.zip.xml', args.mode], 10)
        manifest['ABI_audit'] = json.loads((out/'ABI-audit.log').read_text())
        run('compile-audit', ['python3', repo/'util/ue/metal_pipeline_compile_audit.py', trace,
            '--output', out/'compile-trace.json'], 10)
        compile_trace = json.loads((out/'compile-trace.json').read_text())
        if compile_trace['status'] != 'COMPLETE TRACE' or compile_trace['begin_count'] != 1 or \
            compile_trace['completed_count'] != 1 or compile_trace['native_failed_count'] or \
            compile_trace['completed'][0]['function_name'] != 'RaygenIndirection':
            raise RuntimeError('converted kernel compile trace incomplete')
        manifest['compile_trace'] = compile_trace
        if args.typed:
            run('replay-build', ['clang++', '-std=c++17', '-DRENDERDOC_PLATFORM_APPLE', '-I'+str(repo),
                '-L'+str(library.parent), '-lrenderdoc', '-Wl,-rpath,'+str(library.parent),
                repo/'util/test/metal/metal_ir_ray_replay.cpp', '-framework', 'Foundation', '-framework', 'Metal',
                '-framework', 'QuartzCore', '-o', out/'replay'], 60)
            manifest['hashes'][str(out/'replay')] = sha(out/'replay')
            run('API-replay', [out/'replay', capture_file, args.mode], 30)
            run('CLI-replay', [cli, 'replay', '--loops', '3', capture_file], 30)
        else:
            expected = 'descriptor relocation is unsupported'
            run('replay-rejected', [cli, 'replay', '--loops', '1', capture_file], 30, expected=expected)
        if sha(library) != manifest['hashes'][str(library)]: raise RuntimeError('backend changed')
        manifest.update(status='PASS NATIVE/CAPTURE/REPLAY' if args.typed else 'PASS NATIVE/CAPTURE; REPLAY UNSUPPORTED',
            mode=args.mode, native_output=[want_hit,want_miss], dispatch_argument_size=152, shader_identifier_size=32, AS_header_size=64,
            UE='NOT RUN', production_RT='NOT CERTIFIED BY THIS INDIVIDUAL GATE',
            execution={'Native_GPU':'COMPLETED', 'capture_GPU':'COMPLETED',
                       'replay_GPU':'COMPLETED' if args.typed else 'NOT_RUN'},
            output_comparison={'status':'MATCH',
                               'scope':'Native/capture/replay defined output' if args.typed else 'Native/capture defined output only'},
            overall_acceptance={'status':'INCOMPLETE',
                                'reason':'Device capability and UE/release acceptance are separate'})
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        manifest.update(status='FAIL', error=str(error)); print(str(error), flush=True); return 1
    finally:
        (out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    return 0


if __name__ == '__main__':
    # All finite IR gates share one process-lifetime lock. Independent gate
    # invocations must not overlap native, capture, replay or compilation work.
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as gpu_lock:
        fcntl.flock(gpu_lock.fileno(), fcntl.LOCK_EX)
        raise SystemExit(main())
