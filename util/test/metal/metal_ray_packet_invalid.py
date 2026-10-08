#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject bad typed ray argument fields, including a table from another PSO."""
import copy
import argparse
from pathlib import Path
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_ray_table_invalid import field, run

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('command',type=Path)
parser.add_argument('capture',type=Path)
parser.add_argument('--extended',action='store_true',help='Fixture includes an unbuilt AS')
parser.add_argument('--tlas',action='store_true',help='Fixture also retains primitive AS children')
args=parser.parse_args();command,capture=args.command,args.capture
with tempfile.TemporaryDirectory(prefix='metal-ray-packet-invalid-') as temp:
    root=Path(temp);original=root/'original.zip.xml'
    run(command,'convert','-f',capture,'-o',original,'-c','zip.xml')
    tree=ET.parse(original);chunks=tree.findall('./chunks/chunk')
    bad_buffer=next(field(c,'Buffer').text for c in chunks if c.get('name')=='MTLDevice::newBufferWithBytes')
    keys={'setAccelerationStructure':'resource','setIntersectionFunctionTable':'resource','setVisibleFunctionTable':'table'}
    rejected=0
    for method,member in keys.items():
        name='MTLArgumentEncoder::'+method
        node=next(c for c in chunks if c.get('name')==name)
        foreign=None
        if method!='setAccelerationStructure':
            creator='MTLComputePipelineState::new'+('Intersection' if 'Intersection' in method else 'Visible')+'FunctionTableWithDescriptor'
            creations=[c for c in chunks if c.get('name')==creator]
            foreign=field(creations[-1],'Table').text
        cases=[('unknown-resource',member,'999999999'),('wrong-resource-type',member,bad_buffer),
               ('wrong-member','index','1' if method=='setAccelerationStructure' else '0'),('member-limit','index','32'),
               ('unknown-encoder','ArgumentEncoder','999999999'),('wrong-encoder-type','ArgumentEncoder',bad_buffer)]
        if foreign:cases.append(('foreign-pipeline',member,foreign))
        if args.extended and method=='setAccelerationStructure':
            creations=[c for c in chunks if c.get('name')=='MTLDevice::newAccelerationStructureWithSize']
            cases.append(('unbuilt-AS',member,field(creations[-1],'Structure').text))
            if args.tlas:
                assert len(creations)==5,'TLAS fixture must retain two children, two instances and unbuilt AS'
                cases.append(('primitive-as-instance',member,field(creations[0],'Structure').text))
        if args.extended:cases.append(('missing-member',None,None))
        for tag,key,value in cases:
            variant=copy.deepcopy(tree);target=next(c for c in variant.findall('./chunks/chunk') if c.get('name')==name)
            if key is None:variant.find('./chunks').remove(target)
            else:field(target,key).text=value
            path=root/(method+'-'+tag+'.zip.xml');variant.write(path,encoding='utf-8',xml_declaration=True)
            shutil.copyfile(str(original)[:-4],str(path)[:-4]);rdc=path.with_suffix('.rdc')
            run(command,'convert','-f',path,'-o',rdc,'-c','rdc')
            output=run(command,'replay','--loops','1',rdc,success=False)
            dispatch_failure=tag in ('foreign-pipeline','unbuilt-AS','primitive-as-instance','missing-member')
            expected='MTLComputeCommandEncoder::dispatchThreads' if dispatch_failure else 'Failed to process Metal chunk '+name
            if expected not in output:raise RuntimeError(tag+' failed for an unrelated reason: '+output[-1200:])
            rejected+=1;print('PASS reject',method,tag)
    print('PASS',rejected,'malformed typed ray argument packets')
