#!/usr/bin/env python3
"""T65: malformed shared-event identity and GPU timeline must fail before native use."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t65_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-shared-event-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't65.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        creations = [n for n in chunks if n.get('id') == '1033']
        signals = [n for n in chunks if n.get('id') == '1063']
        waits = [n for n in chunks if n.get('id') == '1062']
        host_initial = [n for n in chunks if n.get('id') == '1283']
        assert len(creations) == len(host_initial) == 1
        assert len(signals) == 2 and len(waits) == 3
        first_value = int(child(signals[0], 'value').text)
        second_value = int(child(signals[1], 'value').text)
        cases = []

        def edit(tag, kind, ordinal, field, value):
            variant = copy.deepcopy(tree)
            nodes = [n for n in variant.findall('./chunks/chunk') if n.get('id') == kind]
            child(nodes[ordinal], field).text = str(value)
            cases.append((tag, variant))

        for identity in (0, 9):
            edit(f'creation-{identity}', '1033', 0, 'Event', identity)
        for identity in (0, 2**63):
            edit(f'host-event-{identity}', '1283', 0, 'event', identity)
        for identity in (0, 2**63):
            edit(f'signal-event-{identity}', '1063', 0, 'event', identity)
            edit(f'wait-event-{identity}', '1062', 0, 'event', identity)
        edit('signal-zero', '1063', 0, 'value', 0)
        edit('wait-future', '1062', 0, 'value', first_value + 1)
        edit('second-signal-not-increasing', '1063', 1, 'value', first_value)
        edit('second-wait-future', '1062', 2, 'value', second_value + 1)

        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T65: {len(cases)} malformed shared events rejected without crash')


if __name__ == '__main__':
    main()
