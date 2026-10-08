#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject frame aliasing before the frozen build and outside its exact range."""
import copy
from pathlib import Path
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from metal_ray_table_invalid import field, run
command,capture=map(Path,sys.argv[1:3]);helper=Path(sys.argv[3]);mode=sys.argv[4]
with tempfile.TemporaryDirectory(prefix='metal-as-frame-alias-') as tmp:
    directory=Path(tmp);original=directory/'original.zip.xml'
    run(command,'convert','-f',capture,'-o',original,'-c','zip.xml')
    tree=ET.parse(original)
    def nodes(tree):
        chunks=tree.find('./chunks')
        build=next(c for c in chunks if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances')
        allocations=[c for c in chunks if c.get('name')=='MTLHeap::newBuffer(offset)']
        if len(allocations)!=2:raise RuntimeError('expected input and exact-range alias allocations')
        return chunks,build,allocations[-1]
    with zipfile.ZipFile(str(original)[:-4]) as archive:payloads={n:archive.read(n) for n in archive.namelist()}
    for tag in ['alias-before-build','alias-before-encoder-end','alias-without-build','partial-overlap']:
        variant=copy.deepcopy(tree);chunks,build,alias=nodes(variant)
        if tag=='alias-before-build':
            chunks.remove(alias);chunks.insert(list(chunks).index(build),alias)
        elif tag=='alias-before-encoder-end':
            # Valid packet is already encoded; its consuming encoder is still open.
            chunks.remove(alias);chunks.insert(list(chunks).index(build)+1,alias)
        elif tag=='alias-without-build':chunks.remove(build)
        else:field(alias,'length').text=str(int(field(alias,'length').text)+8)
        xml=directory/(tag+'.zip.xml');variant.write(xml,encoding='unicode',xml_declaration=True)
        with zipfile.ZipFile(str(xml)[:-4],'w') as archive:
            for name,data in payloads.items():archive.writestr(name,data)
        invalid=directory/(tag+'.rdc');run(command,'convert','-f',xml,'-o',invalid,'-c','rdc')
        if tag=='alias-before-encoder-end':
            # CPU allocation does not access the range. Once the build owns its
            # validated staging packet, native replay may close the encoder at
            # the CPU boundary before the recorded EndEncoding chunk.
            run(command,'replay','--loops','3',invalid)
            summary=run("env","MTL_DEBUG_LAYER=1",helper,invalid,mode)
            print(summary.splitlines()[-1])
            continue
        message=run(command,'replay','--loops','1',invalid,success=False)
        if 'Failed to replay Metal chunk MTLHeap::newBuffer(offset)' not in message:
            raise RuntimeError(tag+': missing placement alias rejection: '+message)
    print('PASS 3 malformed AS-input aliases rejected; allocation-before-recorded-end control API/CLI passed')
