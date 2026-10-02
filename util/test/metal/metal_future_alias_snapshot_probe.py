#!/usr/bin/env python3
"""Validate partial tails using only the future alias's nonredundant CPU snapshot."""
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    cli, replay, capture, folder = map(Path, sys.argv[1:5])
    folder.mkdir(parents=True, exist_ok=True)
    def run(name, args):
        env=dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
        with (folder/(name+'.log')).open('w') as output:
            subprocess.run(list(map(str,args)), env=env, stdout=output, stderr=subprocess.STDOUT,
                           timeout=60, check=True)
    xml=folder/'source.zip.xml'
    run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    tree=ET.parse(xml); chunks=tree.find('./chunks')
    def field(c,n):return next(x for x in c if x.get('name')==n)
    scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5')
    frame=list(chunks)[scope+1:]
    aid=field(next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotBinding'
        and field(c,'kind').text=='0'),'resource').text
    commit=next(i for i,c in enumerate(frame) if c.get('name')=='MTLCommandBuffer::commit')
    births=[c for c in frame[:commit] if c.get('name')=='MTLHeap::newBuffer(offset)']
    assert births and field(births[-1],'Buffer').text!=aid
    bid=field(births[-1],'Buffer').text
    assert any(c.get('name')=='Internal_MTLBufferModifyCPUContents' and field(c,'Buffer').text==bid for c in frame[:commit])
    removed=0
    for c in frame[:commit]:
        if c.get('name')=='Internal_MTLBufferModifyCPUContents' and field(c,'Buffer').text==aid:
            chunks.remove(c); removed+=1
    assert removed, 'Expected the redundant A snapshot to be removable'
    for c in chunks:c.set('length','0')
    target=folder/'future-only.zip.xml'; tree.write(target, encoding='utf-8', xml_declaration=True)
    with zipfile.ZipFile(xml.with_suffix('')) as original, zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
        for name in original.namelist():archive.writestr(name,original.read(name))
    rdc=folder/'future-only.rdc'
    run('convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
    run('pixels-seeks',[replay,rdc,*sys.argv[5:],'future-alias-cpu'])
    print('PASS future-only alias snapshot: A snapshot removed, B writes 103 before first commit, partial GPU values 184/248 and final pixels 225/161')


if __name__=='__main__':main()
