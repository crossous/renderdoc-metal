#!/usr/bin/env python3
"""Malformed generic Private initial subresources must fail before GPU uploads."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

cli, opener, capture, folder=map(Path,sys.argv[1:5]);folder.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_INITIAL_PRIVATE='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
def run(label,args,refuse=False):
    result=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=40)
    text=result.stdout+result.stderr;(folder/(label+'.log')).write_text(text)
    assert result.returncode in ((1,4) if refuse else (0,)),(label,result.returncode,text)
    if refuse:
        assert 'failed' in text.lower(),(label,text)
        for marker in ('Private texture initial contents upload','Metal texture initial contents upload','Metal Private initial contents:','Private initial contents upload','Metal replay wait begin'):
            assert marker not in text,(label,text)
def field(node,name):return next(c for c in node if c.get('name')==name)
xml=folder/'source.zip.xml';run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
original=ET.parse(xml)
with zipfile.ZipFile(xml.with_suffix('')) as archive:blobs={name:archive.read(name) for name in archive.namelist()}
targets=[(70,2),(70,3),(70,5),(70,6),(70,7),(260,2),(260,3),(135,6),(53,7),(115,3)]
available={(int(field(field(c,'descriptor'),'pixelFormat').text),int(field(field(c,'descriptor'),'textureType').text)) for c in original.find('./chunks') if c.get('name') in ('MTLDevice::newTextureWithDescriptor','MTLHeap::newTexture(offset)')}
targets=[target for target in targets if target in available]
count=0
for fmt,kind in targets:
    heap_target=any(c.get('name')=='MTLHeap::newTexture(offset)' and int(field(field(c,'descriptor'),'pixelFormat').text)==fmt and int(field(field(c,'descriptor'),'textureType').text)==kind for c in original.find('./chunks'))
    for case in ('short','long','duplicate','unknown-resource','mips') + ((('heap-storage-mismatch' if heap_target else 'short-shared'),) if fmt==70 else ()):
        tree,data=copy.deepcopy(original),dict(blobs);chunks=tree.find('./chunks')
        creation=next(c for c in chunks if c.get('name') in ('MTLDevice::newTextureWithDescriptor','MTLHeap::newTexture(offset)') and
            field(field(c,'descriptor'),'pixelFormat').text==str(fmt) and field(field(c,'descriptor'),'textureType').text==str(kind))
        identity=field(creation,'Texture').text;desc=field(creation,'descriptor')
        initial=next(c for c in chunks if c.get('id')=='3' and field(c,'id').text==identity)
        if case in ('short','long','short-shared'):
            node=field(initial,'Contents');key=f'{int(node.text):06d}'
            data[key]=data[key][:-1] if case in ('short','short-shared') else data[key]+b'\x01';node.set('byteLength',str(len(data[key])))
            if case=='short-shared' and creation.get('name')=='MTLDevice::newTextureWithDescriptor':field(desc,'storageMode').text='0';field(desc,'resourceOptions').text='0'
        elif case=='duplicate':chunks.insert(list(chunks).index(initial),copy.deepcopy(initial))
        elif case=='unknown-resource':field(initial,'id').text='9999999'
        elif case=='heap-storage-mismatch':
            storage=2 if field(desc,'storageMode').text=='0' else 0
            field(desc,'storageMode').text=str(storage);field(desc,'resourceOptions').text=str(storage<<4)
        elif case=='mips':field(desc,'mipmapLevelCount').text='1'
        label=f'{fmt}-{kind}-{case}';target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,value in data.items():archive.writestr(name,value)
        rdc=folder/(label+'.rdc');run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True);count+=1
print(f'PASS {count} API+CLI negative groups, no GPU initial uploads/waits')
coverage=next((c for c in original.find('./chunks') if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),None)
if coverage is not None and field(coverage,'version').text=='36':
    cases=('legacy-v35','missing-binding','duplicate-binding','unknown-source','source-offset',
           'stencil-parent','stencil-level-range','stencil-format','cube-array-shape','volume-array-shape')
    for case in cases:
        tree=copy.deepcopy(original);chunks=tree.find('./chunks')
        binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding')
        view=next(c for c in chunks if c.get('name','').startswith('MTLTexture::newTextureView') and field(c,'format').text=='261')
        if case=='legacy-v35':field(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='35'
        elif case=='missing-binding':chunks.remove(binding)
        elif case=='duplicate-binding':chunks.insert(list(chunks).index(binding),copy.deepcopy(binding))
        elif case=='unknown-source':field(binding,'resource').text='9999999'
        elif case=='source-offset':field(binding,'memberOffset').text='1'
        elif case=='stencil-parent':field(view,'Source').text='9999999'
        # A shorter stencil mip range is a legal aspect view, not malformed.
        elif case=='stencil-level-range':field(field(view,'levels'),'length').text='4'
        elif case=='stencil-format':field(view,'format').text='13'
        else:
            kind=6 if case=='cube-array-shape' else 7
            creation=next(c for c in chunks if c.get('name')=='MTLDevice::newTextureWithDescriptor' and field(field(c,'descriptor'),'textureType').text==str(kind))
            desc=field(creation,'descriptor');field(desc,'height' if kind==6 else 'arrayLength').text='2'
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,value in blobs.items():archive.writestr(name,value)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS sourced generic texture: {len(cases)} extra API+CLI binding/view/type groups')
