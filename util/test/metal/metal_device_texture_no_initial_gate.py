#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject actual initial-state/layout/identity violations before GPU submission.

Usage: cli probe capture output-dir targets-file
The caller owns the shared build/GPU lock. No missing ordinary producer is
classified as malformed; undefined application pixels have no byte oracle.
"""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

cli, probe, capture, folder, targets = map(Path, sys.argv[1:6])
folder.mkdir(parents=True, exist_ok=False)
env = {k: v for k, v in os.environ.items() if not k.startswith(('DYLD_', 'RENDERDOC_'))}
env.update(RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',
           RENDERDOC_METAL_TRACE_REPLAY_WAITS='1', MTL_DEBUG_LAYER='1')
checks = []

def run(label, args, reject=False):
    env['RENDERDOC_DEBUG_LOG_FILE'] = str((folder / (label + '-renderdoc.log')).resolve())
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True,
                            env=env, timeout=20)
    text = result.stdout + result.stderr
    log = folder / (label + '-renderdoc.log')
    if log.exists():
        text += log.read_text(errors='replace')
    (folder / (label + '.log')).write_text(text)
    bad = [s for s in ('Assertion failed', 'failed assertion', 'ForceCrash',
                       'OVERRUNNING CHUNK', 'm_ResourceMap.empty') if s in text]
    wait = 'Metal replay wait begin' in text
    checks.append(dict(label=label, exit=result.returncode, GPU_wait_observed=wait,
                       strict=bad))
    assert result.returncode == (4 if reject else 0) and not bad, checks[-1]
    if reject:
        assert not wait, checks[-1]

def field(node, name):
    return next(child for child in node if child.get('name') == name)

xml = folder / 'source.zip.xml'
manifest = dict(execution={'GPU': 'NOT_RUN'}, output_comparison={'status': 'NOT_APPLICABLE'},
                overall_acceptance={'status': 'INCOMPLETE'}, checks=checks)
try:
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}
    for case in ('missing-required-initial', 'duplicate-false-cannot-hide-required',
                 'frame-birth-falsely-requires-initial', 'unknown-copy-source',
                 'copy-mip-out-of-range', 'copy-region-overflow', 'CPU-read-row-too-short'):
        tree = copy.deepcopy(original)
        chunks = tree.find('./chunks')
        transfer = next(c for c in chunks if c.get('id') == '1212')
        source = field(transfer, 'sourceTexture').text
        destination = field(transfer, 'destinationTexture').text
        initial = next(c for c in chunks if c.get('id') == '3' and field(c, 'id').text == source)
        needed = field(next(c for c in chunks if c.get('id') == '2'), 'NeededInitials')
        if case in ('missing-required-initial', 'duplicate-false-cannot-hide-required'):
            chunks.remove(initial)
            if case.startswith('duplicate'):
                record = copy.deepcopy(next(c for c in needed if field(c, 'id').text == source))
                field(record, 'written').text = 'false'
                needed.append(record)
        elif case == 'frame-birth-falsely-requires-initial':
            field(next(c for c in needed if field(c, 'id').text == destination), 'written').text = 'true'
        elif case == 'unknown-copy-source':
            field(transfer, 'sourceTexture').text = '9999999'
        elif case == 'copy-mip-out-of-range':
            field(transfer, 'sourceLevel').text = '64'
        elif case == 'copy-region-overflow':
            field(field(transfer, 'sourceOrigin'), 'x').text = str(2**64 - 1)
        else:
            read = next(c for c in chunks if c.get('name') == 'MTLTexture::getBytes')
            field(read, 'bytesPerRow').text = '1'
        changed = folder / (case + '.zip.xml')
        tree.write(changed, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(changed.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in blobs.items():
                archive.writestr(name, data)
        output = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', changed, '-o', output, '-c', 'rdc'])
        run(case + '-replay', [probe, output, folder, targets, '--whole-only'], reject=True)
    manifest['execution']['preflight'] = 'REJECTED_BEFORE_GPU'
    manifest['scope_result'] = 'EXPECTED_CONTRACT_VIOLATIONS_REJECTED'
finally:
    (folder / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('Expected initial-state/layout/identity violations rejected before GPU; overall RT acceptance pending')
