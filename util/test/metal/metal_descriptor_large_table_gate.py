#!/usr/bin/env python3
"""Reject malformed large typed tables before command encoding or GPU waits."""
import copy
import os
from pathlib import Path
import struct
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
        p = subprocess.run(list(map(str, args)), capture_output=True, text=True, env=env, timeout=40)
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
        blobs = {n: z.read(n) for n in z.namelist()}
    cases = ['legacy40', 'zero-count', 'count-limit', 'stride', 'table-range', 'table-schema',
             'unknown-buffer-address', 'unknown-texture-identity', 'unknown-sampler-identity',
             'live-buffer-byte-mismatch', 'live-texture-byte-mismatch', 'live-sampler-byte-mismatch',
             'slot-range', 'slot-overlap', 'slot-generation', 'missing-source', 'source-member',
             'missing-inline', 'inline-range', 'dispatch-too-wide', 'duplicate-layout']
    for case in cases:
        tree = copy.deepcopy(original)
        chunks = tree.find('./chunks')
        data = dict(blobs)
        main_layout = next(c for c in chunks if c.get('name') == 'MTLBuffer::DeclareDescriptorTable' and field(c, 'count').text == '786432')
        sampler_layout = next(c for c in chunks if c.get('name') == 'MTLBuffer::DeclareDescriptorTable' and field(c, 'schema').text == '2')
        rid, sid = field(main_layout, 'buffer').text, field(sampler_layout, 'buffer').text
        if case == 'legacy40':
            field(next(c for c in chunks if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage'), 'version').text = '40'
        elif case in ('zero-count', 'count-limit', 'stride', 'table-range', 'table-schema'):
            key, value = {'zero-count': ('count', '0'), 'count-limit': ('count', '786433'),
                          'stride': ('stride', '32'), 'table-range': ('offset', '24'), 'table-schema': ('schema', '3')}[case]
            field(main_layout, key).text = value
        elif case.startswith('unknown-') or case.startswith('live-'):
            sampling = 'sampler' in case
            initial = next(c for c in chunks if c.get('name') == 'Internal::Initial Contents' and field(c, 'id').text == (sid if sampling else rid))
            blob = f"{int(field(initial, 'Contents').text):06d}"
            offset = 0 if case.startswith('unknown-') else (4095 if sampling else 786431 if 'texture' in case else 786430) * 24
            if 'texture' in case:
                offset += 8
            payload = bytearray(data[blob])
            struct.pack_into('<Q', payload, offset, 0x123456789abcdef0)
            data[blob] = bytes(payload)
        elif case in ('slot-range', 'slot-overlap', 'slot-generation'):
            event = next(c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotEvent' and field(c, 'buffer').text == rid and field(c, 'event').text == '0')
            if case == 'slot-range':
                field(event, 'offset').text = str(786432 * 24)
            elif case == 'slot-generation':
                field(event, 'generation').text = '0'
            else:
                other = copy.deepcopy(event)
                field(other, 'offset').text = str(int(field(event, 'offset').text) + 8)
                chunks.insert(list(chunks).index(event) + 1, other)
        elif case in ('missing-source', 'source-member'):
            binding = next(c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotBinding' and field(c, 'buffer').text == rid and field(c, 'kind').text == '0')
            if case == 'missing-source':
                chunks.remove(binding)
            else:
                field(binding, 'memberOffset').text = '8'
        elif case in ('missing-inline', 'inline-range'):
            binding = next(c for c in chunks if c.get('name') == 'MTLCommandEncoder::DescriptorInlineBinding')
            if case == 'missing-inline':
                chunks.remove(binding)
            else:
                field(binding, 'memberOffset').text = str(786432 * 24)
        elif case == 'dispatch-too-wide':
            field(field(next(c for c in chunks if c.get('name') == 'MTLComputeCommandEncoder::dispatchThreadgroups'), 'groups'), 'width').text = '2'
        elif case == 'duplicate-layout':
            chunks.insert(list(chunks).index(main_layout), copy.deepcopy(main_layout))
        for c in chunks:
            c.set('length', '0')
        target = folder / (case + '.zip.xml')
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as z:
            for name, value in data.items():
                z.writestr(name, value)
        rdc = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        run(case + '-api', [opener, rdc], True)
        run(case + '-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS large typed table: {len(cases)} API+CLI negative groups')


if __name__ == '__main__':
    main()
