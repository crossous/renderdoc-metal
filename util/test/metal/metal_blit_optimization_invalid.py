#!/usr/bin/env python3
"""T41: clean failures for invalid blit descriptors and texture optimization hints."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_compute_inline_invalid import child, run


def make_cases(tree):
    cases = []
    def edit(tag, name, field, value, subresource=None):
        variant = copy.deepcopy(tree)
        node = next(node for node in variant.findall('./chunks/chunk')
                    if node.get('name') == name and (subresource is None or
                       any(item.get('name') == 'slice' for item in node) == subresource))
        child(node, field).text = str(value)
        cases.append((tag, variant))
    for field, value in [('CommandBuffer', 0), ('BlitCommandEncoder', 0), ('hasSampleBuffers', 'true')]:
        edit('descriptor-' + field, 'MTLCommandBuffer::blitCommandEncoderWithDescriptor', field, value)
    for access in ('GPU', 'CPU'):
        for suffix in ('', '_slice_level'):
            name = f'MTLBlitCommandEncoder::optimizeContentsFor{access}Access'
            for field, value in [('texture', 0), ('texture', 999999), ('BlitCommandEncoder', 0)]:
                edit(f'{access}{suffix}-{field}-{value}', name, field, value, bool(suffix))
            if suffix:
                for field in ('slice', 'level'):
                    for value in (2, 2**64 - 1):
                        edit(f'{access}{suffix}-{field}-{value}', name, field, value, True)
    return cases


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t41_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-blit-optimization-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't41.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        cases = make_cases(ET.parse(original))
        for tag, variant in cases:
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            if not re.search(r'failed|invalid|unsupported|missing|Couldn.t load', message, re.I):
                raise RuntimeError(f'missing diagnostic: {tag}: {message}')
    print(f'T41 malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
