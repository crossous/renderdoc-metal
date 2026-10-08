#!/usr/bin/env python3
"""Reject invalid attachmentless sourced indirect passes before frame submission."""
import copy, os, subprocess, sys, xml.etree.ElementTree as ET, zipfile
from pathlib import Path

def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5]); folder.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
    def run(label,args,reject=False):
        r=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=30)
        output=r.stdout+r.stderr;(folder/(label+'.log')).write_text(output)
        assert r.returncode in ((1,4) if reject else (0,)),(label,r.returncode,output)
        if reject: assert 'failed' in output.lower() and 'Metal replay wait begin' not in output,(label,output)
    def field(node,name): return next(x for x in node if x.get('name')==name)
    xml=folder/'source.zip.xml';run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as z: blobs={n:z.read(n) for n in z.namelist()}
    header_name='MTLDevice::CaptureRenderIndirectArgumentsCount';record_name='MTLRenderCommandEncoder::CaptureIndirectArguments'
    def records(tree):
        nodes=tree.find('./chunks');return nodes,next(c for c in nodes if c.get('name')==header_name),[c for c in nodes if c.get('name')==record_name]
    _,header,proof=records(original)
    assert field(header,'count').text=='2' and len(proof)==2
    assert [int(field(c,'ordinal').text) for c in proof]==[0,1]
    kind=int(field(proof[0],'wordCount').text)
    assert kind in (4,5)
    assert [[int(x.text) for x in field(c,'arguments')] for c in proof]==([[3,1,0,2],[6,1,0,2]] if kind==4 else [[3,1,0,0,2],[6,1,0,0,2]])
    cases=['count-zero','count-short','count-long','count-over-limit','header-missing','header-duplicate','record-missing','records-all-missing','record-duplicate','arguments-short','arguments-long','command-zero','command-other','encoder-zero','encoder-other','pass-zero','pass-other','buffer-zero','buffer-other','offset-unaligned','offset-other','ordinal-over-limit','ordinal-duplicate','record-in-frame','kind-wrong','writes-undeclared','count-work-huge','instance-work-huge','work-product-huge','vertex-start-overflow','instance-start-overflow']
    cases+=['width-zero','height-zero','width-over-limit','height-over-limit','sample-zero','sample-two','array-two','rasterization-disabled','fragment-missing','write-declaration-missing','source-write-alias']
    for label in cases:
        tree=copy.deepcopy(original);nodes,header,proof=records(tree);first=proof[0]
        if label in ('count-zero','count-short','count-long','count-over-limit'):field(header,'count').text={'count-zero':'0','count-short':'1','count-long':'3','count-over-limit':'513'}[label]
        elif label=='header-missing':nodes.remove(header)
        elif label=='header-duplicate':nodes.insert(list(nodes).index(header)+1,copy.deepcopy(header))
        elif label=='record-missing':nodes.remove(first)
        elif label=='records-all-missing':
            for c in proof:nodes.remove(c)
        elif label=='record-duplicate':nodes.insert(list(nodes).index(first)+1,copy.deepcopy(first))
        elif label=='arguments-short':field(first,'arguments').remove(field(first,'arguments')[-1])
        elif label=='arguments-long':field(first,'arguments').append(copy.deepcopy(field(first,'arguments')[0]))
        elif label in ('count-work-huge','instance-work-huge','work-product-huge','vertex-start-overflow','instance-start-overflow'):
            args=field(first,'arguments')
            if label=='count-work-huge':args[0].text='1048577'
            elif label=='instance-work-huge':args[1].text='1048577'
            elif label=='work-product-huge':args[0].text='2048';args[1].text='2048'
            elif label=='vertex-start-overflow':args[2].text='4294967295'
            else:args[-1].text='4294967295'
        elif label in ('width-zero','height-zero','width-over-limit','height-over-limit','sample-zero','sample-two','array-two'):
            call=next(c for c in nodes if c.get('name') in ('MTLCommandBuffer::renderCommandEncoderWithDescriptor','MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor'))
            name,value={'width-zero':('renderTargetWidth','0'),'height-zero':('renderTargetHeight','0'),'width-over-limit':('renderTargetWidth','16385'),'height-over-limit':('renderTargetHeight','16385'),'sample-zero':('defaultRasterSampleCount','0'),'sample-two':('defaultRasterSampleCount','2'),'array-two':('renderTargetArrayLength','2')}[label]
            field(field(call,'descriptor'),name).text=value
        elif label in ('rasterization-disabled','fragment-missing'):
            call=next(c for c in nodes if c.get('name','').startswith('MTLDevice::newRenderPipelineState'))
            descriptor=field(call,'descriptor')
            field(descriptor,'rasterizationEnabled' if label=='rasterization-disabled' else 'fragmentFunction').text='false' if label=='rasterization-disabled' else '0'
        elif label in ('write-declaration-missing','source-write-alias'):
            call=next(c for c in nodes if c.get('name','').startswith('MTLRenderCommandEncoder::useResource') and field(c,'usageValue').text=='3')
            if label=='write-declaration-missing':field(call,'usageValue').text='1'
            else:field(call,'resource').text=field(first,'buffer').text
        elif label=='record-in-frame':
            scope=next(c for c in nodes if c.get('id')=='5');nodes.remove(first);nodes.insert(list(nodes).index(scope)+1,first)
        else:
            name,value={'command-zero':('command','0'),'command-other':('command','999999'),'encoder-zero':('encoder','0'),'encoder-other':('encoder','999999'),'pass-zero':('pass','0'),'pass-other':('pass','999999'),'buffer-zero':('buffer','0'),'buffer-other':('buffer','999999'),'offset-unaligned':('offset','17'),'offset-other':('offset','20'),'ordinal-over-limit':('ordinal','512'),'ordinal-duplicate':('ordinal','1'),'kind-wrong':('wordCount','5' if kind==4 else '4'),'writes-undeclared':('writesDeclared','false')}[label]
            field(first,name).text=value
        # Preserve a nonzero chunk estimate. The zero-length scratch writer
        # asserts on its inherited chunkID; that is a converter fault, not a
        # malformed Metal/API contract. 32 bytes also cover the longer record.
        for c in nodes:c.set('length',str(int(c.get('length','0'))+32))
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
            for n,data in blobs.items():z.writestr(n,data)
        rdc=folder/(label+'.rdc');run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS render indirect per-use 3/6: {len(cases)} API+CLI negative groups; exact command/encoder/pass/source/offset/ordinal/word count and declaration')
if __name__=='__main__':main()
