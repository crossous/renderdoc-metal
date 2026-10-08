#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject malformed masked null-ID TLAS recipes and frame builds."""
import copy
from pathlib import Path
import sys
import struct
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from metal_ray_table_invalid import field, run


def main():
    command, capture = map(Path, sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix='metal-ray-inactive-instance-invalid-') as tmp:
        directory = Path(tmp)
        original = directory/'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        def target(tree):
            return next(c for c in tree.findall('./chunks/chunk') if
                c.get('name') == 'MTLAccelerationStructureCommandEncoder::buildIndirectInstances' or
                (c.get('name') == 'Internal::Initial Contents' and
                 any(v.get('name') == 'schema' and v.text == '9' for v in c)))
        base = target(tree)
        initial = base.get('name') == 'Internal::Initial Contents'
        source = 'source' if initial else 'instances'
        as_field = 'id' if initial else 'structure'
        if int(field(base, 'parameters')[3].text) < 1 or len(field(base, 'children')):
            raise RuntimeError('fixture is not a masked null-ID build')
        cases = []
        for index, value in ((0,1),
                             (0,67108864),
                             (0,18446744073709551608), (1,64), (1,70),
                             (1,1048584), (2,0), (2,1), (2,2), (3,0), (3,int(field(base,'parameters')[3].text)+1), (3,65537),
                             (4,1), (5,1), (6,1), (7,1)):
            cases.append((f'layout-{index}-{value}', index, str(value)))
        for name in (source, as_field):
            for tag, value in (('zero','0'), ('unknown','999999999')):
                cases.append((name+'-'+tag, name, value))
        cases += [('source-wrong-type',source,field(base,as_field).text),
                  ('target-wrong-type',as_field,field(base,source).text),
                  ('child-zero','child','0'), ('child-self','child',field(base,as_field).text),
                  ('child-wrong-type','child',field(base,source).text),
                  ('layout-short','layout-short',None), ('payload-short','payload-short',None),
                  ('payload-missing','payload-missing',None)]
        for tag,offset,value in [('nan',0,struct.pack('<I',0x7fc00000)),
                                 ('mask-enabled',52,struct.pack('<I',1)),
                                 ('flags',48,struct.pack('<I',16)),
                                 ('conflicting-flags',48,struct.pack('<I',12)),
                                 ('table-offset',56,struct.pack('<I',32)),
                                 ('nonzero-AS-ID',64,struct.pack('<Q',0xffffffffffffffff))]:
            cases.append((tag,'payload',(offset,value)))
        if initial:
            cases += [(f'schema-{v}','schema',str(v)) for v in (0,4,5,6,7,8,10)]
            cases += [('kind-mismatch','kind','10'), ('compacted','compacted','true'),
                      ('size-source','sizeSource',field(base,source).text)]
        else:
            cases += [('scratch-zero','scratch','0'),
                      ('scratch-wrong-type','scratch',field(base,as_field).text),
                      ('encoder-wrong-type','Encoder',field(base,as_field).text)]
        with zipfile.ZipFile(str(original)[:-4]) as archive:
            payloads = {n:archive.read(n) for n in archive.namelist()}
        for tag, member, value in cases:
            variant = copy.deepcopy(tree); node = target(variant); changed = dict(payloads)
            if isinstance(member,int): field(node,'parameters')[member].text=value
            elif member == 'child':
                child=copy.deepcopy(field(node,as_field)); child.set('name','[0]'); child.text=value
                field(node,'children').append(child)
            elif member == 'layout-short': field(node,'parameters').remove(field(node,'parameters')[-1])
            elif member in ('payload-short','payload','payload-missing'):
                data=field(node,'vertices' if initial else 'descriptorBytes')
                key=f"{int(data.text):06d}"; raw=bytearray(changed[key])
                if member=='payload-missing': raw=bytearray()
                elif member=='payload-short': raw=raw[:-1]
                else:
                    offset,replacement=value;raw[offset:offset+len(replacement)]=replacement
                changed[key]=bytes(raw)
            else: field(node,member).text=value
            xml=directory/(tag+'.zip.xml'); variant.write(xml,encoding='unicode',xml_declaration=True)
            with zipfile.ZipFile(str(xml)[:-4],'w') as archive:
                for name,data in changed.items(): archive.writestr(name,data)
            invalid=directory/(tag+'.rdc')
            run(command,'convert','-f',xml,'-o',invalid,'-c','rdc')
            message=run(command,'replay','--loops','1',invalid,success=False)
            if not ('Failed to process Metal chunk' in message or 'Failed to replay Metal chunk' in message or
                    'Invalid Metal initial CPU buffer data' in message):
                raise RuntimeError(tag+': missing clean rejection: '+message)
        print(f'PASS {len(cases)} malformed inactive {"initial" if initial else "frame"} captures')


if __name__ == '__main__': main()
