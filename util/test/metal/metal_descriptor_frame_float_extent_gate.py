#!/usr/bin/env python3
"""Check v65 2D float extent boundaries without submitting captured GPU work."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')

    def run(label, command, reject=False):
        result = subprocess.run(list(map(str, command)), env=env, timeout=30,
                                capture_output=True, text=True)
        output = result.stdout + result.stderr
        (folder / (label + '.log')).write_text(output)
        if reject:
            assert result.returncode == 4 and 'failed' in output.lower(), (label, output)
            assert 'Metal replay wait begin' not in output, (label, output)
        else:
            assert result.returncode == 0, (label, output)

    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    cases = {'width-limit': ('width', '513'), 'height-limit': ('height', '513'),
             'zero-width': ('width', '0'), 'samples': ('sampleCount', '2'),
             '2d-depth': ('depth', '2'), '2d-array': ('arrayLength', '2'),
             'usage': ('usage', '1')}
    for case in [*cases, 'legacy64', 'heap-range', 'duplicate-birth']:
        tree = copy.deepcopy(original)
        chunks = tree.getroot().find('chunks')
        birth = next(c for c in chunks if c.get('name') == 'MTLHeap::newTexture(offset)')
        if case in cases:
            name, value = cases[case]
            birth.find("*[@name='descriptor']").find("*[@name='%s']" % name).text = value
        elif case == 'legacy64':
            next(c for c in chunks if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage').find("*[@name='version']").text = '64'
        elif case == 'heap-range':
            birth.find("*[@name='offset']").text = str(2**32)
        else:
            chunks.insert(list(chunks).index(birth), copy.deepcopy(birth))
        for chunk in chunks:
            chunk.set('length', '0')
        target = folder / (case + '.zip.xml')
        tree.write(target, encoding='utf-8', xml_declaration=True)
        # The original payload is immutable and shared by all descriptor mutations.
        os.link(xml.with_suffix(''), target.with_suffix(''))
        rdc = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        run(case + '-api', [opener, rdc], True)
    print('PASS 10 v65 frame float boundary/legacy/identity/heap negative cases; no GPU wait')


if __name__ == '__main__':
    main()
