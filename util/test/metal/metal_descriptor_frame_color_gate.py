#!/usr/bin/env python3
"""Bounded frame color inputs refuse malformed sources before GPU work."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
               RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label, args, reject=False):
        p = subprocess.run(list(map(str, args)), env=env, timeout=30, capture_output=True, text=True)
        out = p.stdout + p.stderr
        (folder / (label + '.log')).write_text(out)
        assert p.returncode in ((1, 4) if reject else (0,)), (label, p.returncode, out)
        if reject:
            assert 'failed' in out.lower() and 'Metal replay wait begin' not in out, (label, out)
    def field(c, name):
        return next(n for n in c if n.get('name') == name)
    run('positive-api', [opener, capture])
    run('positive-cli', [cli, 'replay', '--loops', '1', capture])
    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as z:
        blobs = {n:z.read(n) for n in z.namelist()}
    cases = ['legacy37', 'zero-width', 'width-limit', 'height-limit', 'depth', 'mips',
             'array', 'samples', 'unknown-format', 'depth-format', 'storage', 'cache',
             'hazards', 'options', 'usage', 'optimized', 'unaligned-offset', 'heap-range',
             'unknown-heap', 'duplicate-birth', 'unknown-source', 'source-member',
             'birth-after-binding', 'render-width', 'render-height', 'load-before-clear']
    has_view = any('newTextureView' in c.get('name','') for c in original.find('./chunks'))
    if has_view: cases += ['missing-view', 'view-parent', 'view-format', 'view-level', 'view-slice']
    for case in cases:
        tree = copy.deepcopy(original);chunks=tree.find('./chunks')
        births=[c for c in chunks if c.get('name')=='MTLHeap::newTexture(offset)']
        assert len(births)==2
        birth=births[0];d=field(birth,'descriptor')
        binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'kind').text=='1')
        view=next((c for c in chunks if 'newTextureView' in c.get('name','')),None)
        render=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::renderCommandEncoderWithDescriptor')
        if case=='legacy37':field(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='37'
        elif case in ('zero-width','width-limit','height-limit','depth','mips','array','samples','unknown-format','depth-format','storage','cache','hazards','options','usage','optimized'):
            name,value={'zero-width':('width','0'),'width-limit':('width','513'),'height-limit':('height','513'),
                'depth':('depth','2'),'mips':('mipmapLevelCount','2'),'array':('arrayLength','2'),
                'samples':('sampleCount','2'),'unknown-format':('pixelFormat','0'),'depth-format':('pixelFormat','252'),
                'storage':('storageMode','0'),'cache':('cpuCacheMode','1'),'hazards':('hazardTrackingMode','1'),
                'options':('resourceOptions','32'),'usage':('usage','1'),'optimized':('allowGPUOptimizedContents','false')}[case]
            field(d,name).text=value
        elif case in ('unaligned-offset','heap-range'):field(birth,'offset').text='1' if case=='unaligned-offset' else str(2**32)
        elif case=='unknown-heap':field(birth,'Heap').text='9999999'
        elif case=='duplicate-birth':chunks.insert(list(chunks).index(birth),copy.deepcopy(birth))
        elif case=='unknown-source':field(binding,'resource').text='9999999'
        elif case=='source-member':field(binding,'memberOffset').text='1'
        elif case=='birth-after-binding':chunks.remove(birth);chunks.insert(list(chunks).index(binding)+1,birth)
        elif case in ('render-width','render-height'):field(field(render,'descriptor'),'renderTargetWidth' if case=='render-width' else 'renderTargetHeight').text='513'
        elif case=='load-before-clear':field(field(field(render,'descriptor'),'colorAttachments')[0],'loadAction').text='1'
        elif case=='missing-view':chunks.remove(view)
        elif case=='view-parent':field(view,'Source').text='9999999'
        elif case=='view-format':field(view,'format').text='70'
        elif case in ('view-level','view-slice'):field(field(view,'levels' if case=='view-level' else 'slices'),'location').text='1'
        for c in chunks:c.set('length','0')
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
            for n,b in blobs.items():z.writestr(n,b)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS frame color: {len(cases)} API+CLI negative groups')
if __name__=='__main__':main()
