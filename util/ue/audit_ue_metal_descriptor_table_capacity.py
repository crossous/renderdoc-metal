#!/usr/bin/env python3
"""CPU-only census of declared table slots and frozen initial bytes."""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile

def field(c,name):return next((n for n in c if n.get('name')==name),None)
def value(c,name):return int(field(c,name).text)
def audit(xml):
    chunks=ET.parse(xml).find('./chunks');scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5')
    states,initial,creation,tables={},{},{},{}
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        def blob(n):return archive.read(f'{int(n.text):06d}')
        for i,c in enumerate(chunks):
            name=c.get('name','')
            if name in ('MTLHeap::newBuffer(offset)','MTLHeap::newBufferWithLength','MTLDevice::newBufferWithLength','MTLDevice::newBufferWithBytes'):
                creation[value(c,'Buffer')]={'chunk':i,'length':value(c,'length')}
                data=field(c,'initialData')
                if i<scope and data is not None and int(data.get('byteLength','0')):initial[value(c,'Buffer')]=blob(data)
            if name=='Internal::Initial Contents' and value(c,'type')==1:initial[value(c,'id')]=blob(field(c,'Contents'))
            if name=='MTLBuffer::DeclareDescriptorTable':tables[value(c,'buffer')]={n:value(c,n) for n in ('offset','count','stride','schema')}
            if i>=scope:continue
            if name=='MTLBuffer::DescriptorSlotEvent':
                key=(value(c,'buffer'),value(c,'offset'));event=value(c,'event')
                if event==0:states[key]={'live':True,'type':value(c,'descriptorType'),'generation':value(c,'generation'),'data':None}
                elif event==1:
                    if key in states:states[key]['live']=False
                elif event in (2,3):
                    if key in states:states[key]['data']=blob(field(c,'data'))
        reports=[]
        for rid,layout in tables.items():
            counts=Counter();examples=[];contents=initial.get(rid);birth=creation.get(rid,{})
            report={'buffer':rid,**layout,'logical_bytes':layout['count']*layout['stride'],'creation':birth,'initial_bytes':len(contents) if contents is not None else None}
            if birth.get('chunk',0)>scope:report['status']='frame-born';reports.append(report);continue
            if contents is None or layout['offset']+layout['count']*layout['stride']>len(contents):
                report['status']='missing-or-short-initial';reports.append(report);continue
            for entry in range(layout['count']):
                offset=layout['offset']+entry*layout['stride'];slot=states.get((rid,offset))
                data=contents[offset:offset+24]
                fields=(struct.unpack_from('<Q',data,0)[0],) if layout['schema']==2 else struct.unpack_from('<QQ',data,0)
                if slot and slot['live']:
                    counts['live']+=1
                    if data!=slot['data']:counts['live-byte-mismatch']+=1
                elif slot:
                    counts['known-freed']+=1
                    if any(fields):counts['known-freed-nonzero-fields']+=1
                elif any(fields):
                    counts['unknown-nonzero-fields']+=1
                    if len(examples)<16:examples.append({'offset':offset,'fields':fields,'data':data.hex()})
                else:counts['unknown-zero-fields']+=1
            report.update(status='audited',counts=dict(counts),unknown_nonzero_examples=examples);reports.append(report)
    return {'gpu_commands_submitted':0,'scope':scope,'tables':reports,
            'limitations':['Capacity/frozen-byte census does not prove shader accesses, source liveness or complete replay coverage.','Known-freed fields require validated retirement/temporal semantics, not raw-address inference.']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('xml',type=Path);p.add_argument('--output',type=Path);args=p.parse_args();d=audit(args.xml)
    text=json.dumps(d,indent=2);print(text)
    if args.output:args.output.write_text(text+'\n')
if __name__=='__main__':main()
