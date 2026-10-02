#!/usr/bin/env python3
"""Reject depth-only plans, absent-stage bindings and incompatible pipeline targets."""
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
        result = subprocess.run(list(map(str, args)), capture_output=True, text=True,
                                env=env, timeout=30)
        output = result.stdout + result.stderr
        (folder / (label + '.log')).write_text(output)
        assert result.returncode in ((1, 4) if reject else (0,)), (label, result.returncode, output)
        if reject:
            assert 'failed' in output.lower() and 'Metal replay wait begin' not in output, (label, output)
    def field(node, name):
        return next(child for child in node if child.get('name') == name)
    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}
    chunks = original.find('./chunks')
    def depth_only(chunks):
        for c in chunks:
            if c.get('name') in ('MTLCommandBuffer::renderCommandEncoderWithDescriptor', 'MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor'):
                d=field(c,'descriptor')
                if field(field(d,'depthAttachment'),'texture').text!='0' and all(field(a,'texture').text=='0' for a in field(d,'colorAttachments')):
                    return c
        raise AssertionError('No depth-only pass')
    first=depth_only(chunks)
    owner=next(x.text for x in first if x.get('name') in ('RenderCommandEncoder','ParallelRenderCommandEncoder'))
    if first.get('name').startswith('MTLCommandBuffer::parallel'):
        child=next(c for c in chunks if c.get('name')=='MTLParallelRenderCommandEncoder::renderCommandEncoder' and field(c,'ParallelRenderCommandEncoder').text==owner)
        owner=field(child,'RenderCommandEncoder').text
    pipeline=next(field(c,'pipelineState').text for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setRenderPipelineState' and field(c,'RenderCommandEncoder').text==owner)
    cases=['no-depth','depth-level','depth-slice','depth-resolve','depth-clear-invalid','no-vertex','pipeline-depth-mismatch','pipeline-color-added','unexpected-fragment-binding','old-coverage']
    if field(field(first,'descriptor'),'stencilAttachment').find("*[@name='texture']").text!='0':cases+=['stencil-mismatch','stencil-clear-invalid']
    for case in cases:
        tree = copy.deepcopy(original)
        chunks = tree.find('./chunks')
        first=depth_only(chunks);d=field(first,'descriptor');dep=field(d,'depthAttachment')
        if case in ('no-depth','depth-level','depth-slice','depth-resolve','depth-clear-invalid'):
            name,value={'no-depth':('texture','0'),'depth-level':('level','1'),'depth-slice':('slice','1'),'depth-resolve':('resolveTexture',field(dep,'texture').text),'depth-clear-invalid':('clearDepth','2')}[case]
            field(dep,name).text=value
        elif case in ('no-vertex','pipeline-depth-mismatch','pipeline-color-added'):
            c=next(c for c in chunks if 'newRenderPipeline' in c.get('name','') and any(x.get('name')=='RenderPipelineState' and x.text==pipeline for x in c))
            pd=field(c,'descriptor')
            if case=='no-vertex':field(pd,'vertexFunction').text='0'
            elif case=='pipeline-depth-mismatch':field(pd,'depthAttachmentPixelFormat').text='0'
            else:
                colors=field(pd,'colorAttachments')
                template=next(field(field(c,'descriptor'),'colorAttachments')[0] for c in chunks if 'newRenderPipeline' in c.get('name','') and any(x.get('name')=='descriptor' for x in c) and len(field(field(c,'descriptor'),'colorAttachments')))
                colors.append(copy.deepcopy(template));field(colors[0],'pixelFormat').text='80'
        elif case=='unexpected-fragment-binding':
            c=next(c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setVertexBytes' and field(c,'RenderCommandEncoder').text==owner)
            fragment_id=next(c.get('id') for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setFragmentBytes')
            c.set('name','MTLRenderCommandEncoder::setFragmentBytes');c.set('id',fragment_id)
        elif case=='old-coverage':
            c=next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage');field(c,'version').text='55'
        else:
            field(field(d,'stencilAttachment'),'texture' if case=='stencil-mismatch' else 'clearStencil').text='0' if case=='stencil-mismatch' else '256'
        for c in chunks:
            c.set('length', '0')
        target = folder / (case + '.zip.xml')
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in blobs.items():
                archive.writestr(name, data)
        rdc = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        run(case + '-api', [opener, rdc], True)
        run(case + '-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS depth-only: {len(cases)} API+CLI negative groups; attachments, absent fragment bindings, PSO and coverage')


if __name__ == '__main__':
    main()
