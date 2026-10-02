#!/usr/bin/env python3
"""CPU-only correspondence and bounded workload audit of captured render indirect proof."""
import argparse, json, xml.etree.ElementTree as ET
from pathlib import Path

def field(node,name):return next(x for x in node if x.get('name')==name)
def audit(path):
    chunks=ET.parse(path).find('./chunks'); records={}; owners={}; passes={}; ordinals={};calls=[]; header=None;frame=False
    for c in chunks:
        name=c.get('name','')
        if c.get('id')=='5':frame=True
        if name=='MTLDevice::CaptureRenderIndirectArgumentsCount':
            assert not frame and header is None;header=int(field(c,'count').text);assert 0<=header<=512
        elif name=='MTLRenderCommandEncoder::CaptureIndirectArguments':
            assert not frame and header is not None
            r={k:int(field(c,k).text) for k in ('command','encoder','pass','ordinal','buffer','offset','wordCount')}
            r['arguments']=[int(v.text) for v in field(c,'arguments')]
            assert field(c,'writesDeclared').text=='true'
            assert all(r[k]>0 for k in ('command','encoder','pass','buffer')) and r['offset']%4==0 and 0<=r['ordinal']<512
            assert r['wordCount'] in (4,5) and len(r['arguments'])==r['wordCount']
            key=r['encoder'],r['ordinal'];assert key not in records;records[key]=r
        elif frame and (name.startswith('MTLCommandBuffer::renderCommandEncoder') or name.startswith('MTLCommandBuffer::parallelRenderCommandEncoder')):
            encoder=int(field(c,'ParallelRenderCommandEncoder' if 'parallelRender' in name else 'RenderCommandEncoder').text)
            assert encoder not in owners;owners[encoder]=int(field(c,'CommandBuffer').text);passes[encoder]=encoder
        elif frame and name=='MTLParallelRenderCommandEncoder::renderCommandEncoder':
            parent=int(field(c,'ParallelRenderCommandEncoder').text);encoder=int(field(c,'RenderCommandEncoder').text)
            assert encoder not in owners;owners[encoder]=owners[parent];passes[encoder]=passes[parent]
        elif frame and name.startswith(('MTLRenderCommandEncoder::drawPrimitives','MTLRenderCommandEncoder::drawIndexedPrimitives')) and any(v.get('name')=='indirectBuffer' for v in c):
            encoder=int(field(c,'RenderCommandEncoder').text);ordinal=ordinals.get(encoder,0);ordinals[encoder]=ordinal+1
            call=dict(command=owners[encoder],encoder=encoder,pass_=passes[encoder],ordinal=ordinal,buffer=int(field(c,'indirectBuffer').text),offset=int(field(c,'indirectBufferOffset').text),wordCount=5 if 'drawIndexed' in name else 4)
            record=records[encoder,ordinal]
            assert all(record[k]==call[k] for k in ('command','buffer','offset','wordCount')) and record['pass']==call.pop('pass_')
            call['pass']=record['pass'];call['arguments']=record['arguments']
            call['vertices_or_indices']=record['arguments'][0]*record['arguments'][1]
            assert 0<=call['vertices_or_indices']<=1024*1024
            calls.append(call)
    assert header is not None and header==len(records)==len(calls)
    total=sum(c['vertices_or_indices'] for c in calls);assert total<=8*1024*1024
    return dict(source=str(Path(path).resolve()),count=header,total_vertices_or_indices=total,max_vertices_or_indices=max((c['vertices_or_indices'] for c in calls),default=0),zero_calls=sum(c['vertices_or_indices']==0 for c in calls),calls=calls)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('xml',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    r=audit(a.xml);a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:v for k,v in r.items() if k!='calls'}))
if __name__=='__main__':main()
