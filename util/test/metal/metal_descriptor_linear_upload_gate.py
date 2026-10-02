#!/usr/bin/env python3
"""Validate frame color Load only after an original compute dispatch with a live sourced UAV binding."""
import copy
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5])
    folder.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
    def run(label, args, reject=False):
        result=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=30)
        output=result.stdout+result.stderr
        (folder/(label+'.log')).write_text(output)
        assert result.returncode in ((1,4) if reject else (0,)),(label,result.returncode,output)
        if reject:
            assert 'failed' in output.lower() and 'Metal replay wait begin' not in output,(label,output)
    def field(node,name):return next(x for x in node if x.get('name')==name)
    xml=folder/'source.zip.xml'
    run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs={name:archive.read(name) for name in archive.namelist()}
    cases=['legacy-v64','unknown-source','source-type','unknown-target','target-type','row-short','row-misaligned','offset-misaligned','source-overrun','width-overrun','origin-overrun','slice-overrun','mip-overrun','zero-width','depth-overrun','unsupported-options','unknown-encoder','copy-after-end','missing-initial-target']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks');data=dict(blobs)
        copychunk=next(c for c in nodes if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationTexture') is not None)
        source=field(copychunk,'sourceBuffer');target=field(copychunk,'destinationTexture')
        if label=='legacy-v64':field(next(c for c in nodes if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
        elif label=='unknown-source':source.text='999999'
        elif label=='source-type':source.text=target.text
        elif label=='unknown-target':target.text='999999'
        elif label=='target-type':target.text=source.text
        elif label=='row-short':field(copychunk,'sourceBytesPerRow').text='0'
        elif label=='row-misaligned':field(copychunk,'sourceBytesPerRow').text='1'
        elif label=='offset-misaligned':field(copychunk,'sourceOffset').text='1056'
        elif label=='source-overrun':field(copychunk,'sourceOffset').text='1052'
        elif label=='width-overrun':field(field(copychunk,'sourceSize'),'width').text='9'
        elif label=='origin-overrun':field(field(copychunk,'destinationOrigin'),'x').text='8'
        elif label=='slice-overrun':field(copychunk,'destinationSlice').text='1'
        elif label=='mip-overrun':field(copychunk,'destinationLevel').text='1'
        elif label=='zero-width':field(field(copychunk,'sourceSize'),'width').text='0'
        elif label=='depth-overrun':field(field(copychunk,'sourceSize'),'depth').text='2'
        elif label=='unsupported-options':field(copychunk,'options').text='1'
        elif label=='unknown-encoder':field(copychunk,'BlitCommandEncoder').text='999999'
        elif label=='copy-after-end':
            end=next(c for c in nodes if c.get('name')=='MTLBlitCommandEncoder::endEncoding');nodes.remove(copychunk);nodes.insert(list(nodes).index(end)+1,copychunk)
        elif label=='missing-initial-target':
            nodes.remove(next(c for c in nodes if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==target.text))
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS sourced linear upload layout/lifecycle/options: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
