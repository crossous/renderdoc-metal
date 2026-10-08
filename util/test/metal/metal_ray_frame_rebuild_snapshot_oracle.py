#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prove both execution-point packets survive a later source/AS rebuild."""
from pathlib import Path
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from metal_ray_table_invalid import field, run
command,capture=map(Path,sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='metal-frame-rebuild-') as tmp:
    path=Path(tmp)/'capture.zip.xml'
    run(command,'convert','-f',capture,'-o',path,'-c','zip.xml')
    nodes=[c for c in ET.parse(path).findall('./chunks/chunk') if
           c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances']
    if len(nodes)!=2:raise RuntimeError('expected two frame build snapshots')
    if field(nodes[0],'structure').text!=field(nodes[1],'structure').text:
        raise RuntimeError('expected the same rebuilt TLAS')
    identities=[]
    with zipfile.ZipFile(str(path)[:-4]) as archive:
        for i,node in enumerate(nodes):
            if len(field(node,'children'))!=1 or int(field(node,'parameters')[3].text)!=1:
                raise RuntimeError('wrong typed dependency/count')
            raw=archive.read(f"{int(field(node,'descriptorBytes').text):06d}")
            if len(raw)!=72 or struct.unpack_from('<I',raw,60)[0]!=73+i:
                raise RuntimeError('overwritten or missing execution-point userID')
            identities.append(struct.unpack_from('<Q',raw,64)[0])
    if not identities[0] or identities[0]!=identities[1]:raise RuntimeError('AS identity changed')
    print('PASS distinct frame packets: same TLAS/child, original userID73 then74')
