#!/usr/bin/env python3
"""T56: canonical batch layout and identity/member/range validation before native encoding."""
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
    with tempfile.TemporaryDirectory(prefix='metal-argument-batch-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'source.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        def nodes(tree, cid):
            return [n for n in tree.findall('./chunks/chunk') if n.get('id') == str(cid)]
        creation = nodes(tree, 1043)[0]
        selection = nodes(tree, 1233)[0]
        textures = nodes(tree, 1234)
        samplers = nodes(tree, 1235)
        assert len(textures) == len(samplers) == 6
        assert [child(n, 'index').text for n in textures] == ['2','3','2','3','3','3']
        assert [child(n, 'index').text for n in samplers] == ['6','7','6','7','7','7']
        for group, field in ((textures,'texture'),(samplers,'sampler')):
            assert [child(group[i], field).text for i in (0,1,4)] == ['0','0','0']
            assert child(group[2], field).text != child(group[3], field).text
            assert child(group[3], field).text == child(group[5], field).text
        cases = []
        def edit(tag, cid, field, value, occurrence=0):
            variant = copy.deepcopy(tree)
            child(nodes(variant, cid)[occurrence], field).text = str(value)
            cases.append((tag, variant))
        buffer_id = child(selection, 'argumentBuffer').text
        encoder_id = child(creation, 'ArgumentEncoder').text
        function_id = child(creation, 'Function').text
        for field, values in (
                ('Function', (0, 2**63, buffer_id)),
                ('ArgumentEncoder', (0, buffer_id, function_id)),
                ('bufferIndex', (31, 2**64-1))):
            for value in values:
                edit('create-'+field+'-'+str(value),1043,field,value)
        for field, values in (
                ('ArgumentEncoder', (0,2**63,buffer_id)),
                ('argumentBuffer', (0,2**63,encoder_id)),
                ('offset',(1,2**32,2**64-1))):
            for value in values:
                edit('select-'+field+'-'+str(value),1233,field,value)
        for cid, field, wrong_index in ((1234,'texture',6),(1235,'sampler',2)):
            for value in (0,2**63,buffer_id):
                edit(str(cid)+'-encoder-'+str(value),cid,'ArgumentEncoder',value)
            for value in (2**63,buffer_id,encoder_id):
                edit(str(cid)+'-resource-'+str(value),cid,field,value,2)
            for value in (0,wrong_index,32,2**64-1):
                edit(str(cid)+'-index-'+str(value),cid,'index',value)
        variant = copy.deepcopy(tree)
        variant.find('./chunks').remove(nodes(variant,1233)[0])
        cases.append(('missing-selection',variant))
        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml,encoding='unicode',xml_declaration=True)
            shutil.copyfile(str(original)[:-4],str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command,'convert','-f',xml,'-o',rdc,'-c','rdc')
            message = run(command,'replay','--loops','1',rdc,success=False)
            assert re.search(r'failed|invalid|unsupported|missing',message,re.I), message
    print(f'T56 canonical batch layout passed; {len(cases)} malformed captures rejected')


if __name__ == '__main__':
    main()
