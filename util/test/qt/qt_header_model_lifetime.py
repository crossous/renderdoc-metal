#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build a CPU-only Qt header model/lifetime regression against repository sources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qmake', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    work = (args.work_dir or repo/'build-macos-debug/qt-header-lifetime-current').resolve()
    work.mkdir(parents=True, exist_ok=True)
    qmake = args.qmake
    if not qmake:
        match = re.search(r'^QMAKE\s*=\s*(.+)$',
                          (repo/'build-macos-debug/qrenderdoc/Makefile').read_text(), re.M)
        if not match: raise RuntimeError('No configured Qt qmake; provide --qmake')
        qmake = Path(match.group(1))
    header = repo/'qrenderdoc/Widgets/Extended/RDHeaderView.cpp'
    manifest = {'status':'RUNNING','header_sha256':hashlib.sha256(header.read_bytes()).hexdigest()}
    project = work/'probe.pro'
    project.write_text('''QT += widgets gui core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = qt_header_model_lifetime
DEFINES += RENDERDOC_PLATFORM_APPLE
INCLUDEPATH += "'''+str(repo)+'''" "'''+str(repo/'qrenderdoc')+'''" "'''+str(repo/'renderdoc/api/replay')+'''"
SOURCES += "'''+str(repo/'util/test/qt/qt_header_model_lifetime.cpp')+'''" "'''+str(header)+'''"
HEADERS += "'''+str(repo/'qrenderdoc/Widgets/Extended/RDHeaderView.h')+'''"
''')
    try:
        version = subprocess.check_output([str(qmake), '-query', 'QT_VERSION'], text=True).strip()
        manifest['qt_version'] = version
        if not version.startswith('5.'): raise RuntimeError('Use the Qt 5 runtime configured for qrenderdoc')
        for name, command in [('qmake',[str(qmake),str(project)]),('build',['make','-j2']),
                              ('probe',[str(work/'qt_header_model_lifetime')])]:
            env = os.environ.copy()
            if name == 'probe': env['QT_QPA_PLATFORM'] = 'offscreen'
            with (work/(name+'.log')).open('w') as log:
                result = subprocess.run(command,cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
            manifest[name+'_exit'] = result.returncode
            if result.returncode: raise RuntimeError(name+' failed; inspect '+str(work/(name+'.log')))
        if 'PASS retired/current models and header destruction' not in (work/'probe.log').read_text():
            raise RuntimeError('Missing lifecycle oracle')
        manifest['status'] = 'PASS'
    except Exception as error:
        manifest.update(status='FAIL',error=str(error)); raise
    finally:
        (work/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('PASS Qt header model/lifetime regression: '+str(work))


if __name__ == '__main__': main()
