#!/usr/bin/env python3
"""T66: malformed tessellation state and direct patch draws fail before native use."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t66_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-tessellation-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't66.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        assert sum(n.get('id') == '1153' for n in chunks) == 1
        assert sum(n.get('id') == '1154' for n in chunks) == 1
        assert sum(n.get('id') == '1155' for n in chunks) == 1
        cases = []

        def edit(tag, kind, field, value):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == kind)
            child(node, field).text = str(value)
            cases.append((tag, variant))

        edit('factor-identity', '1153', 'buffer', 2**63)
        edit('factor-offset', '1153', 'offset', 256)
        edit('factor-stride', '1153', 'instanceStride', 2**32)
        edit('factor-scale', '1154', 'scale', -1)
        edit('patch-control-points-zero', '1155', 'controlPoints', 0)
        edit('patch-control-points-too-many', '1155', 'controlPoints', 33)
        edit('patch-count-zero', '1155', 'patchCount', 0)
        edit('patch-start-overflow', '1155', 'patchStart', 2**32)
        edit('patch-instance-zero', '1155', 'instanceCount', 0)
        edit('patch-index-identity', '1155', 'patchIndexBuffer', 2**63)
        edit('patch-index-offset-without-buffer', '1155', 'patchIndexBufferOffset', 1)

        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T66: {len(cases)} malformed tessellation inputs rejected without crash')


if __name__ == '__main__':
    main()
