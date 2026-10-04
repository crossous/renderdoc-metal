#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject malformed sourced Task/Mesh bindings before replaying GPU work."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    p = subprocess.run([str(a) for a in args], text=True, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=45)
    if p.returncode < 0 or (p.returncode == 0) != success:
        raise RuntimeError(f'Unexpected exit {p.returncode}: {args}\n{p.stdout}')
    return p.stdout


def child(node, name):
    return next(c for c in node if c.get('name') == name)


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-bindless-stages-') as tmp:
        root = pathlib.Path(tmp)
        original = root / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        layout = next(c for c in chunks if c.get('name') == 'MTLCommandEncoder::DescriptorInlineLayout')
        binding = next(c for c in chunks if c.get('name') == 'MTLCommandEncoder::DescriptorInlineBinding')
        draw = next(c for c in chunks if c.get('name') == 'MTLRenderCommandEncoder::drawMeshThreadgroups')
        declaration = next(c for c in chunks if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage')
        cases = [
            ('unknown-coverage', declaration, 'version', '67'),
            ('unknown-stage', layout, 'stage', '5'),
            ('layout-count', layout, 'count', '2'),
            ('wrong-stage-binding', binding, 'stage', '2'),
            ('missing-root-source', binding, 'resource', '0'),
            ('root-out-of-bounds', binding, 'memberOffset', '4096'),
            ('zero-grid', draw, 'threadgroupsPerGrid.width', '0'),
            ('grid-work-budget', draw, 'threadgroupsPerGrid.width', '1048576'),
        ]
        for tag, chunk, field, value in cases:
            variant = copy.deepcopy(tree)
            node = variant.findall('./chunks/chunk')[chunks.index(chunk)]
            for name in field.split('.'):
                node = child(node, name)
            node.text = value
            xml = root / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = root / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', rdc, '-i', 'zip.xml')
            run(command, 'replay', '--loops', '1', rdc, success=False)
            print(f'PASS reject {tag}')


if __name__ == '__main__':
    main()
