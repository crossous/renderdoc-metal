#!/usr/bin/env python3
"""Reject update identity, topology and disabled-refit initial states safely."""
import copy
import pathlib
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_ray_table_invalid import field, run
from metal_ray_initial_invalid import initial


def check(command, capture, indexed):
    with tempfile.TemporaryDirectory(prefix='metal-ray-refit-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        name = 'MTLAccelerationStructureCommandEncoder::' + ('refitIndexedTriangle' if indexed else 'refitTriangle')
        target = next(c for c in tree.find('./chunks') if c.get('name') == name)
        structure = field(initial(tree.find('./chunks')), 'id').text
        vertices = field(target, 'vertices').text
        cases = []
        identities = ['Encoder', 'source', 'destination', 'vertices', 'indices', 'scratch'] if indexed else ['Encoder', 'structure', 'vertices', 'scratch']
        for member in identities:
            wrong = vertices if member in ['structure', 'source', 'destination', 'Encoder'] else structure
            for tag, value in [('zero', '0'), ('unknown', '99999999'), ('wrong-type', wrong)]:
                cases.append((member+'-'+tag, member, value))
        cases += [('count-zero', 'triangleCount', '0'), ('count-changed', 'triangleCount', '2'),
                  ('disabled-refit-initial', 'usage', '0'), ('unsupported-initial-usage', 'usage', '2'),
                  ('refit-after-end', 'after-end', None)]
        if indexed:
            cases += [('vertex-offset-unaligned', 'vertexOffset', '1'),
                      ('index-offset-unaligned', 'indexOffset', '1'), ('index-type-invalid', 'indexType', '999'),
                      ('stride-changed', 'vertexStride', '16'), ('format-invalid', 'vertexFormat', '0'),
                      ('table-offset-changed', 'tableOffset', '1'), ('opaque-changed', 'opaque', 'true'),
                      ('duplicate-changed', 'allowDuplicate', 'false'), ('scratch-offset-unaligned', 'scratchOffset', '1')]
        for tag, member, value in cases:
            variant = copy.deepcopy(tree); parent = variant.find('./chunks')
            target = next(c for c in parent if c.get('name') == name)
            if member == 'usage':
                field(initial(parent), 'parameters')[7].text = value
            elif member == 'after-end':
                end = next(c for c in parent if c.get('name') == 'MTLAccelerationStructureCommandEncoder::endEncoding')
                parent.remove(target); parent.insert(list(parent).index(end)+1, target)
            else:
                field(target, member).text = value
            xml = directory / f'{tag}.zip.xml'; variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert 'Failed to process Metal chunk' in message or 'Failed to replay Metal chunk' in message, (tag, message)
            print(f'PASS rejected {"indexed-" if indexed else ""}{tag}')
        return len(cases)


def main():
    command, triangle, indexed = map(pathlib.Path, sys.argv[1:])
    count = check(command, triangle, False) + check(command, indexed, True)
    print(f'PASS {count} malformed refit captures')


if __name__ == '__main__':
    main()
