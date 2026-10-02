#!/usr/bin/env python3
"""Keep unfinished own/ended/unknown GPU producer proofs fail-closed."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

cmd, opener, capture, folder = [Path(p).resolve() for p in sys.argv[1:]]
folder.mkdir(parents=True, exist_ok=False)
env = {k: v for k, v in os.environ.items() if not k.startswith('RENDERDOC_METAL_')}
env.update(MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',
           RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')

def convert(source, output, fmt):
    subprocess.run([str(cmd), 'convert', '-f', str(source), '-o', str(output), '-c', fmt],
                   env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=60)

original = folder / 'original.zip.xml'
convert(capture, original, 'zip.xml')
tree = ET.parse(original)
fields = lambda node: {child.get('name'): child for child in node}
for case in ('own_command', 'ended_encoder', 'missing_gpu_value', 'unknown_encoder', 'old_coverage'):
    changed = copy.deepcopy(tree)
    chunks = changed.getroot().find('chunks')
    producer = next(c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotProducer')
    encoder = fields(producer)['encoder'].text
    position = list(chunks).index(producer)
    commit = next(c for c in list(chunks)[position + 1:] if c.get('name') == 'MTLCommandBuffer::commit')
    end = next(c for c in list(chunks)[position + 1:]
               if c.get('name') == 'MTLComputeCommandEncoder::endEncoding'
               and fields(c)['ComputeCommandEncoder'].text == encoder)
    if case in ('own_command', 'ended_encoder'):
        chunks.remove(end)
        chunks.insert(list(chunks).index(commit), end)
        if case == 'own_command':
            creation = next(c for c in chunks if c.get('name') == 'MTLCommandBuffer::computeCommandEncoder'
                            and fields(c)['ComputeCommandEncoder'].text == encoder)
            fields(commit)['CommandBuffer'].text = fields(creation)['CommandBuffer'].text
    elif case == 'missing_gpu_value':
        value = next(c for c in list(chunks)[position + 1:]
                     if c.get('name') == 'MTLBuffer::DescriptorSlotEvent' and fields(c)['event'].text == '3')
        chunks.remove(value)
    elif case == 'unknown_encoder':
        fields(producer)['encoder'].text = '999999'
    else:
        coverage = next(c for c in chunks if c.get('id') == '1399')
        fields(coverage)['version'].text = '64'
    xml = folder / (case + '.zip.xml')
    changed.write(xml, encoding='utf-8', xml_declaration=True)
    os.link(folder / 'original.zip', folder / (case + '.zip'))
    malformed = folder / (case + '.rdc')
    convert(xml, malformed, 'rdc')
    for name, command in [('api', [str(opener), str(malformed)]),
                          ('cli', [str(cmd), 'replay', '--loops', '1', str(malformed)])]:
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=60, text=True)
        (folder / (case + '-' + name + '.log')).write_text(result.stdout)
        if result.returncode <= 0 or 'Metal replay wait begin' in result.stdout:
            raise RuntimeError(case + ' was not rejected before frame GPU execution: ' + name)
print('PASS pending GPU producer: own/ended/unknown/missing/old-coverage API and CLI pre-frame gates')
