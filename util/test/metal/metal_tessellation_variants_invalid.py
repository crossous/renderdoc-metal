#!/usr/bin/env python3
"""T67: malformed indexed and indirect patch draws fail before native use."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET

from metal_compute_inline_invalid import child, run


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t67_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-tessellation-variants-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't67.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        for kind in ('1156', '1157', '1158'):
            assert sum(n.get('id') == kind for n in chunks) == 1
        cases = []

        def edit(tag, kind, field, value):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == kind)
            child(node, field).text = str(value)
            cases.append((tag, variant))

        for kind in ('1156', '1158'):
            edit(f'{kind}-indirect-identity', kind, 'indirectBuffer', 2**63)
            edit(f'{kind}-indirect-offset', kind, 'indirectBufferOffset', 64)
            edit(f'{kind}-control-points', kind, 'controlPoints', 0)
        edit('direct-control-identity', '1157', 'controlPointIndexBuffer', 2**63)
        edit('direct-control-offset', '1157', 'controlPointIndexBufferOffset', 64)
        edit('direct-patch-count', '1157', 'patchCount', 0)
        edit('direct-instance-count', '1157', 'instanceCount', 0)
        edit('indirect-control-identity', '1158', 'controlPointIndexBuffer', 2**63)
        edit('indirect-control-offset', '1158', 'controlPointIndexBufferOffset', 64)
        edit('indirect-patch-offset-without-buffer', '1156', 'patchIndexBufferOffset', 1)

        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T67: {len(cases)} malformed patch variants rejected without crash')


if __name__ == '__main__':
    main()
