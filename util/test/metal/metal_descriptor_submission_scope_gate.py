#!/usr/bin/env python3
"""A delayed unrelated binding must not waive a used descriptor's source proof."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

cli, opener, capture, folder = [Path(p).resolve() for p in sys.argv[1:]]
folder.mkdir(parents=True, exist_ok=False)
env = {k: v for k, v in os.environ.items() if not k.startswith('RENDERDOC_METAL_')}
env.update(MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')


def convert(source, target, fmt):
    subprocess.run([str(cli), 'convert', '-f', str(source), '-o', str(target), '-c', fmt],
                   env=env, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.PIPE, timeout=30)


original = folder / 'source.zip.xml'
convert(capture, original, 'zip.xml')
tree = ET.parse(original)
chunks = tree.getroot().find('chunks')
begin = next(i for i, c in enumerate(chunks) if c.get('name') == 'Internal::Beginning of Capture')
binding_index = next(i for i, c in enumerate(chunks) if i > begin and
                     c.get('name') == 'MTLBuffer::DescriptorSlotBinding' and
                     c.find("*[@name='offset']").text == '0')
assert chunks[binding_index - 1].get('name') == 'MTLBuffer::DescriptorSlotEvent'
assert chunks[binding_index - 1].find("*[@name='generation']").text == '2'
for case in ('missing_consumed_source', 'null_consumed_source'):
    changed = copy.deepcopy(tree)
    changed_chunks = changed.getroot().find('chunks')
    binding = changed_chunks[binding_index]
    if case == 'missing_consumed_source':
        changed_chunks.remove(binding)
    else:
        binding.find("*[@name='resource']").text = '0'
    xml = folder / (case + '.zip.xml')
    changed.write(xml, encoding='utf-8', xml_declaration=True)
    os.link(folder / 'source.zip', folder / (case + '.zip'))
    malformed = folder / (case + '.rdc')
    convert(xml, malformed, 'rdc')
    for name, command in [('api', [str(opener), str(malformed)]),
                          ('cli', [str(cli), 'replay', '--loops', '1', str(malformed)])]:
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=30)
        (folder / (case + '-' + name + '.log')).write_text(result.stdout)
        if result.returncode <= 0 or 'Metal replay wait begin' in result.stdout:
            raise RuntimeError(case + ' not refused before frame GPU execution: ' + name)
print('PASS submission descriptor scope: missing/null consumed source, API/CLI refusal before GPU waits')
