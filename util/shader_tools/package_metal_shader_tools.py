#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Copy the pinned native processor distribution into a relocatable macOS app."""
import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('prefix', type=Path)
parser.add_argument('app', type=Path)
args = parser.parse_args()
root = args.app / 'Contents/Resources/shader-tools'
manifest = json.loads((args.prefix / 'manifest.json').read_text())
if manifest['schema'] != 1 or manifest['architecture'] != subprocess.check_output(
        ['/usr/bin/uname', '-m'], text=True).strip():
    raise SystemExit('Shader tools manifest architecture/schema mismatch')
required = ['bin/metal2vulkan', 'bin/spirv-cross', 'bin/spirv-val',
            'licenses/metal2vulkan-LGPL-3.0.txt', 'licenses/GPL-3.0.txt',
            'licenses/spirv-cross.txt', 'licenses/spirv-tools.txt', 'licenses/spirv-headers.txt',
            'licenses/rust-dependencies.txt', 'licenses/rust-COPYRIGHT-library.html',
            'licenses/rustc-version.txt', 'licenses/THIRD_PARTY.md',
            'source/metal2vulkan.tar.gz', 'source/Cargo.lock', 'source/spirv-cross.tar.gz',
            'source/spirv-tools.tar.gz', 'source/spirv-headers.tar.gz']
for relative in required:
    if relative not in manifest['files']:
        raise SystemExit('Incomplete Metal shader processor distribution: ' + relative)
for relative, expected in manifest['files'].items():
    if hashlib.sha256((args.prefix / relative).read_bytes()).hexdigest() != expected:
        raise SystemExit('Shader tools hash mismatch: ' + relative)
root.mkdir(parents=True, exist_ok=True)
for directory in ['bin', 'lib', 'licenses', 'source']:
    if (args.prefix / directory).exists():
        shutil.copytree(args.prefix / directory, root / directory, dirs_exist_ok=True)
for script in ['metal_air_processor.py', 'metal_shader_processor.py']:
    shutil.copy2(Path(__file__).parent / script, root / script)
shutil.copy2(args.prefix / 'manifest.json', root / 'manifest.json')
# Static tools and the validator's @loader_path/../lib rpath may only load bundled/system libs.
for executable in list((root / 'bin').iterdir()) + list((root / 'lib').glob('*.dylib')):
    linked = subprocess.check_output(['/usr/bin/otool', '-L', str(executable)], text=True)
    for line in linked.splitlines()[1:]:
        dependency = line.strip().split(' (')[0]
        if not dependency.startswith(('/usr/lib/', '/System/Library/', '@rpath/', '@loader_path/')):
            raise SystemExit('Unbundled dependency in {}: {}'.format(executable.name, dependency))
    subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', str(executable)], check=True,
                   capture_output=True)
# Record the installed files after ad-hoc signing, including the fork's helper scripts.
manifest['files'] = {relative: hashlib.sha256((root / relative).read_bytes()).hexdigest()
                     for relative in manifest['files']}
for script in ['metal_air_processor.py', 'metal_shader_processor.py']:
    manifest['files'][script] = hashlib.sha256((root / script).read_bytes()).hexdigest()
(root / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
subprocess.run([str(root / 'bin/spirv-val'), '--version'], check=True, capture_output=True)
print('Bundled Metal shader processors: ' + str(root))
