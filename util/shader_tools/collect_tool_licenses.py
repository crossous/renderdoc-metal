#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Collect notices for the exact Rust dependency graph compiled into the separate tool."""
import re
import shutil
import subprocess
import sys
from pathlib import Path
prefix, cargo_home = map(Path, sys.argv[1:3])
manifest = prefix / 'build/metal2vulkan/Cargo.toml'
text = subprocess.check_output(['cargo','tree','--offline','--locked','--manifest-path',str(manifest),
    '-p','metal2vulkan','--features','serde','--edges','normal,build','--prefix','none','--format','{p}'], text=True)
packages = sorted(set(re.findall(r'^([\w-]+) v([^\s]+)',text,re.M)))
sysroot=Path(subprocess.check_output(['rustc','--print','sysroot'],text=True).strip())
std=sysroot/'share/doc/rust'
notices = prefix / 'licenses/rust-dependencies'
notices.mkdir(parents=True,exist_ok=True)
for name,version in packages:
    if name=='metal2vulkan':continue
    matches=list((cargo_home/'registry/src').glob('*/'+name+'-'+version))
    if len(matches)!=1:raise SystemExit('Missing exact Rust dependency source: '+name+' '+version)
    source=matches[0];dest=notices/(name+'-'+version);dest.mkdir(exist_ok=True)
    files=[p for p in source.iterdir() if p.is_file() and
           p.name.upper().startswith(('LICENSE','COPYING','COPYRIGHT','NOTICE'))]
    if not files:
        # spirv's published crate omits its root Apache license file. Preserve its
        # author/license/repository notice and provide the complete standard license.
        if name != 'spirv' or 'license = "Apache-2.0"' not in (source/'Cargo.toml').read_text():
            raise SystemExit('Missing Rust dependency notices: '+name)
        shutil.copy2(std/'licenses/Apache-2.0.txt',dest/'LICENSE-APACHE')
        files=[source/'Cargo.toml',source/'README.md']
    for p in files:shutil.copy2(p,dest/p.name)
(prefix/'licenses/rust-dependencies.txt').write_text('\n'.join(name+' '+version for name,version in packages)+'\n')
for p in [std/'COPYRIGHT-library.html', std/'COPYRIGHT.html']:
    if not p.exists():raise SystemExit('Rust standard library notices unavailable')
    shutil.copy2(p,prefix/'licenses'/('rust-'+p.name))
shutil.copytree(std/'licenses',prefix/'licenses/rust-runtime',dirs_exist_ok=True)
(prefix/'licenses/rustc-version.txt').write_text(subprocess.check_output(['rustc','-Vv'],text=True))
