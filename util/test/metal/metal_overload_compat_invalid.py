#!/usr/bin/env python3
"""T54 draw overload and pre-options T37 chunk compatibility, using the same serializers."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_compute_inline_invalid import child, run


def main():
    command, draw_capture, blit_capture, api = map(pathlib.Path, sys.argv[1:])
    count = 0
    with tempfile.TemporaryDirectory(prefix='metal-overload-') as tmp:
        directory = pathlib.Path(tmp)
        for label, capture in [('draw', draw_capture), ('blit', blit_capture)]:
            original = directory / (label + '.zip.xml')
            run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
            tree = ET.parse(original)
            if label == 'blit':
                for node in tree.findall('./chunks/chunk'):
                    if node.get('id') in ('1208', '1210'):
                        # The original enum places each no-options ID immediately before options.
                        node.set('id', str(int(node.get('id')) - 1))
                        node.remove(child(node, 'options'))
            methods = ['1147'] if label == 'draw' else ['1207', '1209']
            if label == 'draw':
                draw = next(n for n in tree.findall('./chunks/chunk') if n.get('id') == methods[0])
                assert not any(n.get('name') in ('baseVertex', 'baseInstance') for n in draw)
                assert child(draw, 'instanceCount').text == '2'
            def replay_variant(tag, variant, success):
                xml = directory / (tag + '.zip.xml')
                variant.write(xml, encoding='unicode', xml_declaration=True)
                shutil.copyfile(str(original)[:-4], str(xml)[:-4])
                rdc = directory / (tag + '.rdc')
                run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
                message = run(command, 'replay', '--loops', '3' if success else '1', rdc,
                              success=success)
                if not success:
                    assert re.search(r'failed|invalid|unsupported|missing', message, re.I), message
                elif label == 'blit':
                    run('env', 'MTL_DEBUG_LAYER=1', api, rdc, directory / 'legacy.ppm')
            replay_variant(label + '-valid', tree, True)
            for method in methods:
                fields = (['RenderCommandEncoder', 'indexBuffer'] if label == 'draw' else
                          ['BlitCommandEncoder', 'sourceBuffer', 'destinationTexture']
                          if method == '1207' else
                          ['BlitCommandEncoder', 'sourceTexture', 'destinationBuffer'])
                device = next(n for n in tree.findall('./chunks/chunk')
                              if n.get('name') == 'MTLCreateSystemDefaultDevice')
                device_id = child(device, 'Device').text
                edits = [(f, value) for f in fields for value in (0, device_id, 2**63)]
                edits += ([('indexBufferOffset', 1), ('indexBufferOffset', 2**64-1),
                           ('indexCount', 2**64-1), ('indexType', 17), ('primitiveType', 99),
                           ('instanceCount', 2**32)] if label == 'draw' else
                          [(f, 2**64-1) for f in
                           (['sourceOffset', 'sourceBytesPerRow', 'destinationLevel']
                            if method == '1207' else
                            ['destinationOffset', 'destinationBytesPerRow', 'sourceLevel'])])
                for field, value in edits:
                    variant = copy.deepcopy(tree)
                    node = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == method)
                    child(node, field).text = str(value)
                    replay_variant(f'{label}-{count}', variant, False)
                    count += 1
    print(f'T54/legacy blit: {count} malformed cases rejected; two legal variants passed (legacy API checked)')


if __name__ == '__main__':
    main()
