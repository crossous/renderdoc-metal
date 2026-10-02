#!/usr/bin/env python3
"""Reject malformed sourced depth/stencil attachment plans before frame GPU work."""
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
    first = next(field(c, 'descriptor') for c in chunks
                 if c.get('name') == 'MTLCommandBuffer::renderCommandEncoderWithDescriptor')
    depth_id = field(field(first, 'depthAttachment'), 'texture').text
    stencil = field(field(first, 'stencilAttachment'), 'texture').text != '0'
    # Infer birth location instead of relying on a presentation name for heap chunks.
    scope = next(i for i, c in enumerate(chunks) if c.get('id') == '5')
    frame = any(i > scope and 'newTexture' in c.get('name', '') and
                any(n.get('name') == 'Texture' and n.text == depth_id for n in c)
                for i, c in enumerate(chunks))
    mutations = {
        'missing-depth': ('texture', '0'), 'depth-resolve': ('resolveTexture', depth_id),
        'depth-level': ('level', '1'), 'depth-slice': ('slice', '1'),
        'depth-plane': ('depthPlane', '1'), 'depth-clear-range': ('clearDepth', '2'),
        'depth-clear-negative': ('clearDepth', '-1'), 'depth-clear-nan': ('clearDepth', 'nan'),
        'depth-clear-inf': ('clearDepth', 'inf'), 'depth-load-undefined': ('loadAction', '0'), 'depth-discard': ('storeAction', '0'),
        'depth-store-unknown': ('storeAction', '4'), 'depth-store-options': ('storeActionOptions', '1'),
        'depth-resolve-filter': ('depthResolveFilter', '1')}
    cases = list(mutations) + ['pipeline-depth-format']
    if frame:
        mutations['future-depth-without-clear'] = ('loadAction', '1')
        cases.append('future-depth-without-clear')
    if stencil:
        cases += ['missing-stencil', 'stencil-clear-range', 'stencil-other-texture',
                  'stencil-resolve-filter', 'pipeline-stencil-format']
        if frame:
            cases.append('future-stencil-without-clear')
    for case in cases:
        tree = copy.deepcopy(original)
        chunks = tree.find('./chunks')
        first = next(field(c, 'descriptor') for c in chunks
                     if c.get('name') == 'MTLCommandBuffer::renderCommandEncoderWithDescriptor')
        if case in mutations:
            name, value = mutations[case]
            field(field(first, 'depthAttachment'), name).text = value
        elif case.startswith('pipeline-'):
            descriptor = next(field(c, 'descriptor') for c in chunks if 'newRenderPipeline' in c.get('name', '')
                              and any(n.get('name') == 'descriptor' for n in c))
            field(descriptor, 'depthAttachmentPixelFormat' if case == 'pipeline-depth-format'
                  else 'stencilAttachmentPixelFormat').text = '0'
        else:
            name, value = {
                'missing-stencil': ('texture', '0'), 'stencil-clear-range': ('clearStencil', '256'),
                'stencil-other-texture': ('texture', field(field(first, 'colorAttachments')[0], 'texture').text),
                'stencil-resolve-filter': ('stencilResolveFilter', '1'),
                'future-stencil-without-clear': ('loadAction', '1')}[case]
            field(field(first, 'stencilAttachment'), name).text = value
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
    print(f'PASS depth/stencil: {len(cases)} API+CLI negative groups; per-aspect clear provenance, format, bounds and store policy')


if __name__ == '__main__':
    main()
