#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Scoped placement initial-state and ordinary-copy recovery checks on an explicit backend."""
import argparse
import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backend', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    work = args.work_dir.resolve()
    work.mkdir(exist_ok=False, parents=True)
    backend = args.backend.resolve()
    cli = backend.parent.parent / 'bin/renderdoccmd'
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    manifest = dict(test_source_sha256=sha(Path(__file__)), backend=str(backend), backend_sha256=sha(backend), checks=[],
                    GPU_execution='NOT_COMPLETED', output_comparison='NOT_RUN', overall='INCOMPLETE',
                    scope='Targeted API recovery; not full UE/output/performance/RT acceptance')

    def run(tag, command, capture=False, expected=0, extra=None):
        env = {k: v for k, v in os.environ.items()
               if not k.startswith(('RENDERDOC_', 'DYLD_', 'MTL_DEBUG'))}
        env.update(MTL_DEBUG_LAYER='1', DYLD_LIBRARY_PATH=str(backend.parent),
                   RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
        if capture:
            env['DYLD_INSERT_LIBRARIES'] = str(backend)
        env.update(extra or {})
        start = time.monotonic()
        peak, stopped = 0, ''
        with (work / (tag + '.log')).open('w') as out:
            child = subprocess.Popen(list(map(str, command)), env=env, stdout=out,
                                     stderr=subprocess.STDOUT, start_new_session=True)
            while child.poll() is None:
                rss = subprocess.run(['ps', '-o', 'rss=', '-p', str(child.pid)],
                                     capture_output=True, text=True).stdout.strip()
                if rss:
                    peak = max(peak, int(rss) * 1024)
                if time.monotonic() - start > 60 or peak > 4 * 1024**3:
                    stopped = 'timeout or RSS'
                    os.killpg(child.pid, signal.SIGKILL)
                    break
                time.sleep(.1)
            code = child.wait()
        text = (work / (tag + '.log')).read_text(errors='replace')
        hits = [x for x in ['Assertion failed', 'failed assertion', 'ForceCrash', 'OVERRUNNING CHUNK']
                if x in text]
        # Real range/identity negatives must fail before replay GPU work.
        if expected and 'Metal replay wait begin' in text:
            hits.append('unexpected replay GPU execution')
        passed = code == expected and not stopped and not hits
        manifest['checks'].append(dict(tag=tag, exit=code, seconds=time.monotonic()-start,
                                       peak_RSS_bytes=peak, stopped=stopped, hits=hits, passed=passed))
        (work / 'manifest.json').write_text(json.dumps(manifest, indent=2))
        print(tag, code, flush=True)
        if not passed:
            raise RuntimeError(tag + '\n' + text[-4000:])

    def build(source, output, native=False):
        command = ['clang++', '-O2', '-std=c++17', '-I'+str(repo), repo/'util/test/metal'/source]
        if native:
            command += ['-fobjc-arc', '-framework', 'Foundation', '-framework', 'Metal']
        else:
            command += ['-DRENDERDOC_PLATFORM_APPLE', '-L'+str(backend.parent), '-lrenderdoc',
                        '-Wl,-rpath,'+str(backend.parent)]
        run(output+'-build', command+['-o', work/output])

    try:
        with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a+') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            for app in ['UnrealEditor', 'qrenderdoc']:
                assert subprocess.run(['pgrep', '-x', app], stdout=subprocess.DEVNULL).returncode != 0
            build('metal_placement_buffer_alias_capture.mm', 'placement-native', True)
            build('metal_placement_buffer_alias_replay.mm', 'placement-replay')
            build('../../ue/ue_metal_open_profile.cpp', 'opener')
            for size, offset in [(98304, 196608), (327680, 393216)]:
                folder = work / f'placement-{size}-{offset}'
                folder.mkdir()
                params = [size, 'background', offset]
                run(folder.name+'-native', [work/'placement-native', folder/'native', *params])
                run(folder.name+'-capture', [work/'placement-native', folder/'capture', *params], True)
                cap = next(folder.glob('capture*.rdc'))
                run(folder.name+'-API', [work/'placement-replay', cap, *params])
                manifest.setdefault('captures', []).append(dict(path=str(cap), sha256=sha(cap)))
            # Existing frame-created overlap still uses its original lifecycle contract.
            folder = work/'placement-frame'
            folder.mkdir()
            run('frame-native', [work/'placement-native', folder/'native', 229376])
            run('frame-capture', [work/'placement-native', folder/'capture', 229376], True)
            run('frame-API', [work/'placement-replay', next(folder.glob('capture*.rdc')), 229376])

            base = work/'placement-98304-196608'
            run('placement-export', [cli, 'convert', '-f', next(base.glob('capture*.rdc')),
                                     '-o', base/'original.zip.xml', '-c', 'zip.xml'])
            original = ET.parse(base/'original.zip.xml')
            with zipfile.ZipFile(base/'original.zip') as archive:
                blobs = {name: archive.read(name) for name in archive.namelist()}
            field = lambda c, n: c.find('./*[@name="'+n+'"]')
            for tag in ['unaligned-offset', 'outside-heap', 'copy-outside']:
                tree = copy.deepcopy(original)
                chunks = tree.find('./chunks')
                alias = next(c for c in chunks if c.get('name') == 'MTLHeap::newBuffer(offset)'
                             and field(c, 'offset').text == '196608')
                if tag == 'unaligned-offset':
                    field(alias, 'offset').text = '1'
                elif tag == 'outside-heap':
                    field(alias, 'offset').text = str(2*1024*1024)
                else:
                    target = field(alias, 'Buffer').text
                    writer = next(c for c in chunks if c.get('name') == 'MTLBlitCommandEncoder::copyFromBuffer'
                                  and field(c, 'destinationBuffer').text == target)
                    field(writer, 'size').text = '98305'
                for c in chunks:
                    c.set('length', str(int(c.get('length', '0'))+128))
                xml = work/(tag+'.zip.xml')
                tree.write(xml, encoding='utf-8', xml_declaration=True)
                with zipfile.ZipFile(xml.with_suffix(''), 'w', zipfile.ZIP_DEFLATED) as archive:
                    for name, data in blobs.items():
                        archive.writestr(name, data)
                cap = work/(tag+'.rdc')
                run(tag+'-import', [cli, 'convert', '-f', xml, '-o', cap, '-c', 'rdc'])
                run(tag+'-API', [work/'opener', cap, work], expected=4)

            build('metal_runtime_heap_consumer_capture.mm', 'ordinary-native', True)
            build('metal_runtime_heap_consumer_replay.cpp', 'ordinary-replay')
            run('ordinary-shader', ['xcrun', '-sdk', 'macosx', 'metal', '-c',
                                    repo/'util/test/metal/metal_runtime_heap_consumer.metal', '-o', work/'ordinary.air'])
            run('ordinary-metallib', ['xcrun', '-sdk', 'macosx', 'metallib', work/'ordinary.air', '-o', work/'ordinary.metallib'])
            for size, count in [(3145728, 12), (2359296, 16)]:
                folder = work/f'ordinary-{size}-{count}'
                folder.mkdir()
                extra = dict(RENDERDOC_METAL_TEST_ORDINARY_COPY_BYTES=str(size),
                             RENDERDOC_METAL_TEST_ORDINARY_COPY_COUNT=str(count))
                command = [work/'ordinary-native', folder/'native', work/'ordinary.metallib']
                run(folder.name+'-native', command, extra=extra)
                command[1] = folder/'capture'
                run(folder.name+'-capture', command, True, extra=extra)
                cap = next(folder.glob('capture*.rdc'))
                run(folder.name+'-API', [work/'ordinary-replay', cap], extra=extra)
                manifest.setdefault('captures', []).append(dict(path=str(cap), sha256=sha(cap)))
            # CPU cannot evaluate this GPU-selected ordinary writer. The
            # original shader still executes against real relocated tables;
            # exact access feedback is optional, full Native output is not.
            run('dynamic-shader', ['xcrun', '-sdk', 'macosx', 'metal', '-c',
                                   repo/'util/test/metal/metal_dynamic_namespace_writer.metal',
                                   '-o', work/'dynamic.air'])
            run('dynamic-metallib', ['xcrun', '-sdk', 'macosx', 'metallib',
                                     work/'dynamic.air', '-o', work/'dynamic.metallib'])
            for size, offset in [(65536, 64), (1048576, 4096)]:
                folder = work/f'dynamic-{size}-{offset}'
                folder.mkdir()
                extra = dict(RENDERDOC_METAL_RUNTIME_SCALAR_ROOT_BYTES=str(size),
                             RENDERDOC_METAL_RUNTIME_SCALAR_ROOT_OFFSET=str(offset),
                             RENDERDOC_METAL_RUNTIME_UNKNOWN_TARGET='1')
                run(folder.name+'-native', [work/'ordinary-native', folder/'native',
                                           work/'dynamic.metallib'], extra=extra)
                run(folder.name+'-capture', [work/'ordinary-native', folder/'capture',
                                            work/'dynamic.metallib'], True, extra=extra)
                cap = next(folder.glob('capture*.rdc'))
                run(folder.name+'-API', [work/'ordinary-replay', cap], extra=extra)
                manifest.setdefault('captures', []).append(dict(path=str(cap), sha256=sha(cap)))
            base = work/'dynamic-65536-64'
            run('dynamic-export', [cli, 'convert', '-f', next(base.glob('capture*.rdc')),
                                   '-o', base/'original.zip.xml', '-c', 'zip.xml'])
            original = ET.parse(base/'original.zip.xml')
            with zipfile.ZipFile(base/'original.zip') as archive:
                blobs = {name: archive.read(name) for name in archive.namelist()}
            for tag in ['missing-typed-source', 'typed-source-outside']:
                tree = copy.deepcopy(original)
                chunks = tree.find('./chunks')
                source = next(c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotBinding'
                              and field(c, 'offset').text == '24' and field(c, 'kind').text == '0')
                if tag == 'missing-typed-source':
                    chunks.remove(source)
                else:
                    field(source, 'memberOffset').text = '257'
                for c in chunks:
                    c.set('length', str(int(c.get('length', '0'))+128))
                xml = work/(tag+'.zip.xml')
                tree.write(xml, encoding='utf-8', xml_declaration=True)
                with zipfile.ZipFile(xml.with_suffix(''), 'w', zipfile.ZIP_DEFLATED) as archive:
                    for name, data in blobs.items():
                        archive.writestr(name, data)
                cap = work/(tag+'.rdc')
                run(tag+'-import', [cli, 'convert', '-f', xml, '-o', cap, '-c', 'rdc'])
                run(tag+'-API', [work/'opener', cap, work], expected=4)
            assert sha(backend) == manifest['backend_sha256']
            manifest.update(GPU_execution='COMPLETED', output_comparison='DEFINED_BYTES_PASS',
                            overall='TARGETED_PASS; STAGE_INCOMPLETE')
    except Exception as error:
        manifest['error'] = repr(error)
        raise
    finally:
        (work/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')


if __name__ == '__main__':
    main()
