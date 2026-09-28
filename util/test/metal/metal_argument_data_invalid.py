#!/usr/bin/env python3
"""T57: packet/member/constants validation and poisoned capture-address relocation."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from metal_compute_inline_invalid import child, run


def nodes(tree, cid):
    return [n for n in tree.findall('./chunks/chunk') if n.get('id') == str(cid)]


def main():
    command, capture, api = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-argument-data-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'source.zip.xml'
        run(command,'convert','-f',capture,'-o',original,'-c','zip.xml')
        tree = ET.parse(original)
        selection = nodes(tree,1274)
        writes = nodes(tree,1275)
        constants = nodes(tree,1276)
        assert len(selection) == 2 and len(writes) == 16 and len(constants) == 4
        assert [child(n,'arrayElement').text for n in selection] == ['0','1']
        assert [child(n,'index').text for n in writes] == ['0','0','2','3','2','3','3','3'] * 2
        assert [child(n,'index').text for n in constants] == ['8','9'] * 2
        outer = child(selection[0],'argumentBuffer').text
        encoder = child(selection[0],'ArgumentEncoder').text
        texture = child(nodes(tree,1234)[0],'texture').text
        sampler = child(nodes(tree,1235)[0],'sampler').text
        cases = []
        def edit(tag,cid,field,value,occurrence=0):
            variant = copy.deepcopy(tree)
            child(nodes(variant,cid)[occurrence],field).text = str(value)
            cases.append((tag,variant))
            return variant
        for occurrence in (0,1):
            for field, values in (
                    ('ArgumentEncoder',(0,2**63,outer)),
                    ('argumentBuffer',(0,2**63,encoder,texture)),
                    ('startOffset',(1,621,2**64-1)),
                    ('arrayElement',(2,2**64-1))):
                for value in values:
                    edit(f'select-{occurrence}-{field}-{value}',1274,field,value,occurrence)
        # A different, aligned packet start overlapping the first selection.
        edit('overlap',1274,'startOffset',176,1)
        for occurrence in (1,4,7,9,12,15):
            for field, values in (
                    ('ArgumentEncoder',(0,2**63,outer)),
                    ('buffer',(2**63,encoder,texture,sampler)),
                    ('index',(1,4,6,8,32,2**64-1)),
                    ('offset',(1,112 if occurrence in (1,9) else 208,2**64-1))):
                for value in values:
                    edit(f'member-{occurrence}-{field}-{value}',1275,field,value,occurrence)
        for occurrence in (0,3):
            for field, values in (
                    ('ArgumentEncoder',(0,2**63,outer)),
                    ('index',(0,4,6,31,2**64-1))):
                for value in values:
                    edit(f'constant-{occurrence}-{field}-{value}',1276,field,value,occurrence)
        for occurrence in (1,7,9,15):
            variant = edit(f'missing-member-{occurrence}',1275,'buffer',0,occurrence)
            child(nodes(variant,1275)[occurrence],'offset').text = '0'
        for occurrence in (0,1):
            variant = copy.deepcopy(tree)
            variant.find('./chunks').remove(nodes(variant,1274)[occurrence])
            cases.append((f'missing-selection-{occurrence}',variant))
        # Fragment packet offsets must be exact, and native argument-buffer calls must be bounded.
        for method in ('setFragmentBuffer','setFragmentBufferOffset'):
            node = next(n for n in tree.findall('./chunks/chunk')
                        if n.get('name') == 'MTLRenderCommandEncoder::'+method)
            cid = node.get('id')
            for field, values in (('index',(31,2**64-1)),('offset',(1,272,620,2**64-1)),
                                  ('RenderCommandEncoder',(0,2**63,outer))):
                for value in values:
                    edit(method+'-'+field+'-'+str(value),cid,field,value)
        def emit(tag,variant,blobs=None):
            xml = directory / (tag+'.zip.xml')
            variant.write(xml,encoding='unicode',xml_declaration=True)
            if blobs is None:
                shutil.copyfile(str(original)[:-4],str(xml)[:-4])
            else:
                with zipfile.ZipFile(str(xml)[:-4],'w') as archive:
                    for name,data in blobs.items(): archive.writestr(name,data)
            rdc = directory / (tag+'.rdc')
            run(command,'convert','-f',xml,'-o',rdc,'-c','rdc')
            return rdc
        for tag,variant in cases:
            message = run(command,'replay','--loops','1',emit(tag,variant),success=False)
            assert re.search(r'failed|invalid|unsupported|missing',message,re.I), message
        # Destroy every captured GPU address, including addresses spanned by CPU delta updates.
        # The metadata must rebuild them without changing constants or surrounding padding.
        with zipfile.ZipFile(str(original)[:-4]) as archive:
            blobs = {name:archive.read(name) for name in archive.namelist()}
        for node in tree.findall('./chunks/chunk'):
            if node.get('id') == '3' and child(node,'id').text == outer:
                payload, start = child(node,'Contents'), 0
            elif node.get('id') == '1198' and child(node,'Buffer').text == outer:
                payload, start = child(node,'data'), int(child(node,'start').text)
            else:
                continue
            name = f'{int(payload.text):06d}'
            data = bytearray(blobs[name])
            for offset in (256,352):
                for absolute in range(max(start,offset),min(start+len(data),offset+64)):
                    data[absolute-start] = 0
            blobs[name] = data
        positive = emit('poisoned-gpu-addresses',tree,blobs)
        run('env','MTL_DEBUG_LAYER=1',api,positive,directory/'poisoned.ppm')
        run(command,'replay','--loops','3',positive)
    print(f'T57: {len(cases)} malformed cases rejected; poisoned-address API/CLI variant passed')


if __name__ == '__main__':
    main()
