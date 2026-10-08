#!/usr/bin/env python3
"""Capture observations must not replace actual Native indirect execution counts.

Use the per-use sample's replay validator as opener to also verify complete
outputs and event resets. Structural evidence failures have a separate gate.
"""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5])
    replay_args = sys.argv[5:]  # e.g. "0" for the complete per-use output validator.
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
    for ordinal, observed in ((0, 0), (1, 2), (0, 262145)):
        tree = copy.deepcopy(original)
        nodes = tree.find('./chunks')
        proof = [c for c in nodes if c.get('name') ==
                 'MTLComputeCommandEncoder::CaptureIndirectArguments'][ordinal]
        groups = next(c for c in proof if c.get('name') == 'groups')
        groups[0].text = str(observed)  # No original command or GPU input is changed.
        for node in nodes:
            node.set('length', str(int(node.get('length', '0')) + 128))
        target = folder / f'ordinal-{ordinal}-observed-{observed}.zip.xml'
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in blobs.items():
                archive.writestr(name, data)
        rdc = target.with_suffix('').with_suffix('.rdc')
        converted = run([cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        assert converted.returncode == 0, converted.stdout + converted.stderr
        for label, args, code in (
                ('api', [opener, rdc, *replay_args], 0), ('cli', [cli, 'replay', '--loops', '1', rdc], 0)):
            result = run(args)
            output = result.stdout + result.stderr
            (folder / f'ordinal-{ordinal}-observed-{observed}-{label}.log').write_text(output)
            assert result.returncode == code, (result.returncode, output)
            assert 'actual=1,1,1' in output and 'actual=3,1,1' in output and 'actual=2,1,1' in output, output
            assert 'Metal compute indirect expected:' in output and 'Metal replay wait end' in output, output
    print('PASS ordinary capture observations differ; API+CLI retain original GPU 1/3/2 dispatches')


if __name__ == '__main__':
    main()
