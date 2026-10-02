#!/usr/bin/env python3
"""Reject well-formed capture evidence that differs from actual Native execution-point data."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_INDIRECT_REPLAY='1',
               RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')

    def run(args):
        return subprocess.run(list(map(str, args)), env=env, capture_output=True, text=True, timeout=60)

    xml = folder / 'source.zip.xml'
    exported = run([cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    assert exported.returncode == 0, exported.stdout + exported.stderr
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}
    for ordinal in (0, 1):
        tree = copy.deepcopy(original)
        nodes = tree.find('./chunks')
        proof = [c for c in nodes if c.get('name') ==
                 'MTLComputeCommandEncoder::CaptureIndirectArguments'][ordinal]
        groups = next(c for c in proof if c.get('name') == 'groups')
        groups[0].text = '2'  # Original Native dispatch is 1 or 3; no command is changed.
        for node in nodes:
            node.set('length', '0')
        target = folder / f'ordinal-{ordinal}.zip.xml'
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in blobs.items():
                archive.writestr(name, data)
        rdc = target.with_suffix('').with_suffix('.rdc')
        converted = run([cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        assert converted.returncode == 0, converted.stdout + converted.stderr
        for label, args, code in (
                ('api', [opener, rdc], 4), ('cli', [cli, 'replay', '--loops', '1', rdc], 1)):
            result = run(args)
            output = result.stdout + result.stderr
            (folder / f'ordinal-{ordinal}-{label}.log').write_text(output)
            assert result.returncode == code, (result.returncode, output)
            assert 'Metal compute indirect execution-point arguments do not match capture' in output, output
            assert 'Metal compute indirect expected:' in output and 'Metal replay wait end' in output, output
    print('PASS 2 Native execution mismatch groups, API+CLI, original 1/3/2 dispatches retained')


if __name__ == '__main__':
    main()
