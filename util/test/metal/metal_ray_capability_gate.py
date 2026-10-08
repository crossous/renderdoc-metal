#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify native, default and process-local RT probe capability queries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--expect-compute-enabled', action='store_true',
                        help='Require the published compute predicate to match the Native device in every injected case')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    build = (args.build_dir or repo/'build-macos-debug').resolve()
    work = (args.work_dir or build/'ray-capability-gate').resolve()
    work.mkdir(parents=True, exist_ok=True)
    library = build/'lib/librenderdoc.dylib'
    identity = sha(library)
    manifest = dict(status='RUNNING', backend_sha256=identity, cases=[])
    env = os.environ.copy()
    for key in tuple(env):
        if key.startswith('RENDERDOC_METAL_') or key == 'DYLD_INSERT_LIBRARIES':
            env.pop(key)
    executable = work/'probe'
    try:
        compile_command = ['clang++', '-fobjc-arc', str(repo/'util/test/metal/metal_ray_capability_probe.mm'),
                           '-framework', 'Foundation', '-framework', 'Metal', '-o', str(executable)]
        with (work/'compile.log').open('w') as log:
            subprocess.run(compile_command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60)
        native = None
        cases = [('native', None, None), ('default', library, None),
                 ('probe', library, '1'), ('invalid-text', library, 'true'),
                 ('disabled', library, '0'), ('empty', library, '')]
        for name, injection, flag in cases:
            current = env.copy()
            if injection:
                current['DYLD_INSERT_LIBRARIES'] = str(injection)
                current['RENDERDOC_DEBUG_LOG_FILE'] = str(work/(name+'-renderdoc.log'))
            if flag is not None:
                current['RENDERDOC_METAL_RAYTRACING_PROBE'] = flag
            result = subprocess.run([str(executable)], env=current, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, timeout=30)
            (work/(name+'.log')).write_text(result.stdout)
            if result.returncode:
                raise RuntimeError(f'{name}: process exit {result.returncode}')
            match = re.search(r'^compute=([01]) render=([01])$', result.stdout, re.M)
            if not match:
                raise RuntimeError(f'{name}: missing capability result')
            observed = tuple(map(int, match.groups()))
            if name == 'native':
                native = observed
            expected = native if name == 'native' else (
                (native[0], 0) if args.expect_compute_enabled or name == 'probe' else (0, 0))
            manifest['cases'].append(dict(name=name, observed=observed, expected=expected))
            if observed != expected:
                raise RuntimeError(f'{name}: observed {observed}, expected {expected}')
            print(f'{name}: compute={observed[0]} render={observed[1]} PASS', flush=True)
        if sha(library) != identity:
            raise RuntimeError('Backend changed during capability gate')
        manifest.update(status='INCOMPLETE',
                        execution={'GPU':'NOT_SUBMITTED', 'device_queries':'COMPLETED'},
                        output_comparison={'status':'MATCH', 'scope':'Native versus published compute/render device predicates'},
                        overall_acceptance={'status':'INCOMPLETE'},
                        expected_compute_enabled=args.expect_compute_enabled,
                        unsupported_physical_device_path='NOT_RUN_ON_THIS_NATIVE_SUPPORTED_DEVICE')
        return 0
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        manifest.update(status='FAIL', error=str(error))
        print(str(error), flush=True)
        return 1
    finally:
        (work/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')


if __name__ == '__main__':
    raise SystemExit(main())
