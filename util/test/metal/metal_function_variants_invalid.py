#!/usr/bin/env python3
"""Validate all four specialized Function creation paths before entering the native compiler."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_compute_inline_invalid import child, run


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    ids = ('1040', '1041', '1272', '1273')
    with tempfile.TemporaryDirectory(prefix='metal-function-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'source.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        cases = []
        for chunk_id in ids:
            def edit(tag, field, value):
                variant = copy.deepcopy(tree)
                node = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == chunk_id)
                target = child(node, field)
                if callable(value):
                    value(target)
                else:
                    target.text = str(value)
                cases.append((chunk_id + '-' + tag, variant))
            node = next(n for n in tree.findall('./chunks/chunk') if n.get('id') == chunk_id)
            assert child(node, 'supported').text == 'true'
            assert len(child(node, 'constantValues')) >= 4
            for field in ('Library', 'Function'):
                for value in (0, 2**63, child(node, 'Library').text):
                    if field == 'Library' and str(value) == child(node, 'Library').text:
                        value = child(node, 'Function').text
                    edit(f'{field}-{value}', field, value)
            for field, value in [('supported','false'),('functionName',''),('functionName','missing'),
                                 ('options',1),('options',2**64-1)]:
                edit(field + str(value),field,value)
            for field in ('constantNames','constantIndices','constantTypes','constantValues'):
                edit(field + '-count',field,lambda n: n.remove(n[-1]))
            edit('index-overflow','constantIndices',lambda n: setattr(n[0],'text','65536'))
            edit('type-unknown','constantTypes',lambda n: setattr(n[0],'text','999999'))
            edit('value-size','constantValues',lambda n: n[0].remove(n[0][-1]))
            if chunk_id in ('1040','1272'):
                edit('unexpected-specialized-name','specializedName','not_descriptor')
        intersection = next(n for n in tree.findall('./chunks/chunk') if n.get('id') == '1042')
        assert child(intersection, 'functionName').text == 'is_function_variant'
        def edit_intersection(tag, field, value):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == '1042')
            child(node, field).text = str(value)
            cases.append(('intersection-' + tag, variant))
        for field, value in [('supported', 'false'), ('functionName', ''),
                             ('functionName', 'missing_intersection'), ('options', 1),
                             ('Library', 0), ('Function', 0),
                             ('Function', child(intersection, 'Library').text)]:
            edit_intersection(field + '-' + str(value), field, value)
        def emit(tag, variant):
            # Array/string edits change the serialized payload size. Ask the existing
            # structured-file writer to recompute lengths rather than retaining the
            # original upper bound and corrupting the following chunk.
            for node in variant.findall('./chunks/chunk'):
                node.set('length', '0')
            xml = directory / (tag + '.zip.xml')
            variant.write(xml,encoding='unicode',xml_declaration=True)
            shutil.copyfile(str(original)[:-4],str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command,'convert','-f',xml,'-o',rdc,'-c','rdc')
            return rdc
        for tag, variant in cases:
            message = run(command,'replay','--loops','1',emit(tag,variant),success=False)
            assert re.search(r'failed|invalid|unsupported|missing',message,re.I),message
        variant = copy.deepcopy(tree)
        for node in variant.findall('./chunks/chunk'):
            if node.get('id') in ids:
                for field in ('constantNames','constantIndices','constantTypes','constantValues'):
                    array = child(node,field)
                    for element in list(array): array.append(copy.deepcopy(element))
        run(command,'replay','--loops','3',emit('duplicate-writes',variant))
    print(f'T55: {len(cases)} malformed function snapshots rejected; repeated-write variant passed')


if __name__ == '__main__':
    main()
