#!/usr/bin/env python3
"""Compile only MetalRHI using cached response files, with isolated outputs.

Never calls UBT, installs a library, or modifies the engine. UE source is supplied
locally by the caller; this script does not redistribute Epic source.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True,
                        help='Isolated MetalRHI source directory with Private/Public')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--prepare-only', action='store_true')
    options = parser.parse_args()
    engine, source, output = (p.resolve() for p in
                              (options.engine, options.source, options.output))
    if sys.platform != 'darwin':
        parser.error('Requires macOS/Xcode')
    if source.is_relative_to(engine) or output.is_relative_to(engine):
        parser.error('Source copy and all outputs must be outside the installed engine')
    if not (source / 'Private/MetalBindlessDescriptors.cpp').is_file():
        parser.error('MetalRHI source copy is missing')
    intermediate = engine / 'Intermediate/Build/Mac/arm64/UnrealEditor/Development/MetalRHI'
    source_root = engine / 'Source'
    compiler = subprocess.check_output(['xcrun', '--find', 'clang++'], text=True).strip()
    output.mkdir(parents=True, exist_ok=True)
    jobs = []
    # Remove PCH dependence: the installed shared UnrealEd PCH can be unavailable.
    # Include and compile the same three unity units sequentially to limit RAM.
    units = sorted(intermediate.glob('Module.MetalRHI.*.cpp.o.rsp'))
    if len(units) != 3:
        parser.error('Expected three cached MetalRHI compile response files')
    for response in units:
        args = shlex.split(response.read_text())
        original_unit = next((a for a in args if a.endswith('.cpp')), None)
        if original_unit is None:
            parser.error(f'Missing unity input in {response}')
        original = (source_root / original_unit).resolve()
        unity = output / original.name
        content = original.read_text()
        def include(match):
            path = match.group(1)
            prefix = 'Runtime/Apple/MetalRHI/'
            resolved = source / path[len(prefix):] if path.startswith(prefix) else source_root / path
            return '#include "' + str(resolved.resolve()) + '"'
        unity.write_text(re.sub(r'#include "([^"]+)"', include, content))
        rewritten = []
        skip = False
        for index, arg in enumerate(args):
            if skip:
                skip = False
                continue
            if arg == '-include-pch':
                skip = True
                continue
            if arg == original_unit:
                arg = str(unity)
            elif index and args[index - 1] == '-o':
                arg = str(output / Path(arg).name)
            elif arg.startswith('-MF'):
                arg = '-MF' + str(output / (original.name + '.d'))
            elif arg.startswith('-I'):
                include_path = arg[2:]
                prefix = 'Runtime/Apple/MetalRHI/'
                if include_path.startswith(prefix):
                    arg = '-I' + str(source / include_path[len(prefix):])
            rewritten.append(arg)
        jobs.append({'compiler': compiler, 'args': rewritten, 'cwd': str(source_root),
                     'name': original.name})
        (output / response.name).write_text(shlex.join(rewritten) + '\n')
    link_response = intermediate / 'libUnrealEditor-MetalRHI.dylib.rsp'
    link = shlex.split(link_response.read_text())
    for index, arg in enumerate(link):
        if arg.endswith('.cpp.o'):
            link[index] = str(output / Path(arg).name)
        elif index and link[index - 1] == '-o':
            link[index] = str(output / 'libUnrealEditor-MetalRHI.dylib')
    # Refuse any remaining output under Engine, even if cached flags change.
    for job in jobs + [{'args': link}]:
        for index, arg in enumerate(job['args']):
            if (index and job['args'][index - 1] == '-o') or arg.startswith('-MF'):
                target = arg[3:] if arg.startswith('-MF') else arg
                if not Path(target).resolve().is_relative_to(output):
                    parser.error(f'Output escapes isolated directory: {target}')
    (output / 'jobs.json').write_text(json.dumps(jobs, indent=2))
    (output / 'link.json').write_text(json.dumps(link, indent=2))
    original_library = engine / 'Binaries/Mac/libUnrealEditor-MetalRHI.dylib'
    original_hash = digest(original_library)
    if options.prepare_only:
        print(f'Prepared three compile jobs and one link job in {output}')
        return
    for job in jobs:
        print('Compiling', job['name'], flush=True)
        with (output / (job['name'] + '.log')).open('w') as log:
            result = subprocess.run([compiler, *job['args']], cwd=source_root,
                                    stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            sys.exit(result.returncode)
    with (output / 'link.log').open('w') as log:
        result = subprocess.run([compiler, *link], cwd=source_root,
                                stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        sys.exit(result.returncode)
    library = output / 'libUnrealEditor-MetalRHI.dylib'
    def exports(path):
        return set(subprocess.check_output(['nm', '-gjU', str(path)], text=True).splitlines())
    old, new = exports(original_library), exports(library)
    report = {'original_sha256': original_hash, 'output_sha256': digest(library),
              'original_exports': len(old), 'output_exports': len(new),
              'missing_exports': sorted(old - new), 'added_exports': sorted(new - old),
              'source_files': {str(p.relative_to(source)): digest(p) for p in
                               sorted(source.rglob('*')) if p.is_file()}}
    (output / 'build-manifest.json').write_text(json.dumps(report, indent=2))
    if digest(original_library) != original_hash or old - new:
        sys.exit('Original library changed or required exports missing')
    print(f'Built isolated module: {library}\nSHA256: {report["output_sha256"]}')


if __name__ == '__main__':
    main()
