#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify the original complete masked-null packet, even after source GPU erase."""
from pathlib import Path
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from metal_ray_table_invalid import field, run

command,capture=map(Path,sys.argv[1:3]);mode=sys.argv[3]
count=3 if 'repeated' in mode else 1
userid=0xf0000049 if 'high' in mode else 73
with tempfile.TemporaryDirectory(prefix='metal-inactive-snapshot-') as tmp:
    path=Path(tmp)/'capture.zip.xml'
    run(command,'convert','-f',capture,'-o',path,'-c','zip.xml')
    chunks=ET.parse(path).findall('./chunks/chunk')
    nodes=[c for c in chunks if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances' or
           (c.get('name')=='Internal::Initial Contents' and
            any(v.get('name')=='kind' and v.text=='11' for v in c))]
    if len(nodes)!=1:raise RuntimeError('expected exactly one inactive build recipe')
    node=nodes[0];initial=node.get('name')=='Internal::Initial Contents'
    if len(field(node,'children')) or int(field(node,'parameters')[3].text)!=count:
        raise RuntimeError('inactive packet has dependencies or wrong count')
    if initial and field(node,'schema').text!='9':raise RuntimeError('inactive schema mismatch')
    key=f"{int(field(node,'vertices' if initial else 'descriptorBytes').text):06d}"
    with zipfile.ZipFile(str(path)[:-4]) as archive:raw=archive.read(key)
    if len(raw)!=count*72:raise RuntimeError('inactive payload size mismatch')
    for i in range(count):
        packet=raw[i*72:(i+1)*72]
        floats=struct.unpack_from('<12f',packet)
        expected=[1,0,0,0,1,0,0,0,1,0 if i==0 else 100,0,0]
        if list(floats)!=expected or struct.unpack_from('<4IQ',packet,48)!=(0,0,0,userid+i,0):
            raise RuntimeError('inactive packet differs from the original executed input')
    print(f'PASS original {count} inactive packets; zero dependencies, preserved transforms/userID')
