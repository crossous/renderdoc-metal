#!/usr/bin/env python3
"""Refuse invalid creation/declaration/source order for frame-born CPU descriptor payloads."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5]); folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1', RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label, args, refuse=False):
        r = subprocess.run(list(map(str,args)), capture_output=True, text=True, env=env, timeout=20)
        output=r.stdout+r.stderr; (folder/(label+'.log')).write_text(output)
        assert r.returncode in ((1,4) if refuse else (0,)), (label,r.returncode,output)
        if refuse: assert 'failed' in output.lower() and 'Metal replay wait begin' not in output, (label,output)
    def f(c,n):return next(x for x in c if x.get('name')==n)
    run('positive-api',[opener,capture]);run('positive-cli',[cli,'replay','--loops','1',capture])
    xml=folder/'source.zip.xml';run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml']);original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:blobs={n:archive.read(n) for n in archive.namelist()}
    cases=['v7','missing-payload-creation','missing-input-creation','duplicate-payload','duplicate-input',
           'payload-length','input-length','private-payload','private-input','table-before-creation',
           'slot-before-table','table-count','table-schema','missing-table','slot-after-consumer',
           'source-before-creation','identity-before-creation','identity-conflict','creation-after-consumer']
    for case in cases:
        tree,data=copy.deepcopy(original),dict(blobs);chunks=tree.find('./chunks');scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5');frame=list(chunks)[scope+1:]
        births=[c for c in frame if c.get('name')=='MTLDevice::newBufferWithBytes'];payload,source=births
        table=next(c for c in frame if c.get('name')=='MTLBuffer::DeclareDescriptorTable')
        slot=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and f(c,'buffer').text==f(payload,'Buffer').text and f(c,'event').text=='0')
        binding=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and f(c,'resource').text==f(source,'Buffer').text)
        identity=next(c for c in frame if c.get('name')=='MTLResource::CaptureGPUIdentity' and f(c,'resource').text==f(source,'Buffer').text)
        consumer=next(c for c in frame if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups')
        def move(c,before,after=False):chunks.remove(c);chunks.insert(list(chunks).index(before)+int(after),c)
        if case=='v7':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='7'
        elif case=='missing-payload-creation':chunks.remove(payload)
        elif case=='missing-input-creation':chunks.remove(source)
        elif case=='duplicate-payload':chunks.insert(list(chunks).index(payload),copy.deepcopy(payload))
        elif case=='duplicate-input':chunks.insert(list(chunks).index(source),copy.deepcopy(source))
        elif case=='payload-length':f(payload,'length').text='65537'
        elif case=='input-length':f(source,'length').text='65537'
        elif case=='private-payload':f(payload,'options').text='32'
        elif case=='private-input':f(source,'options').text='32'
        elif case=='table-before-creation':move(table,payload)
        elif case=='slot-before-table':move(slot,table)
        elif case=='table-count':f(table,'count').text='2'
        elif case=='table-schema':f(table,'schema').text='0'
        elif case=='missing-table':chunks.remove(table)
        elif case=='slot-after-consumer':move(slot,consumer,True)
        elif case=='source-before-creation':move(binding,source)
        elif case=='identity-before-creation':move(identity,source)
        elif case=='identity-conflict':
            bad=copy.deepcopy(identity);f(bad,'value').text=str(int(f(bad,'value').text)+8);chunks.insert(list(chunks).index(identity),bad)
        elif case=='creation-after-consumer':move(payload,consumer,True)
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for n,v in data.items():archive.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc']);run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS frame sourced tables: {len(cases)} API+CLI negative creation/declaration/source order groups')


if __name__=='__main__':main()
