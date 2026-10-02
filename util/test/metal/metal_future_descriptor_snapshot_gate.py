#!/usr/bin/env python3
"""Reject live or unsubmitted predecessors before a future typed backing alias."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

cmd, opener, capture, folder = [Path(p).resolve() for p in sys.argv[1:]]
folder.mkdir(parents=True, exist_ok=False)
env = os.environ.copy()
for key in tuple(env):
    if key.startswith('RENDERDOC_METAL_'):
        env.pop(key)
env.update(MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')

def convert(source, output, fmt):
    subprocess.run([str(cmd), 'convert', '-f', str(source), '-o', str(output), '-c', fmt],
                   env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=60)

original = folder / 'original.zip.xml'
convert(capture, original, 'zip.xml')
tree = ET.parse(original)
fields = lambda node: {child.get('name'): child.text for child in node}
for case in ('live_old_slot', 'unsubmitted_old_consumer'):
    changed = copy.deepcopy(tree); chunks = changed.getroot().find('chunks')
    old_free = next(c for c in chunks if c.get('id') == '1403' and fields(c).get('event') == '1')
    if case == 'live_old_slot':
        chunks.remove(old_free)
    else:
        commit = next(c for c in chunks if c.get('id') == '1048')
        wait = next(c for c in chunks if c.get('name') == 'MTLCommandBuffer::waitUntilCompleted')
        birth = next(c for c in list(chunks)[list(chunks).index(old_free) + 1:] if c.get('id') == '1317')
        chunks.remove(commit); chunks.remove(wait)
        position = list(chunks).index(birth) + 1
        chunks.insert(position, commit); chunks.insert(position + 1, wait)
    xml = folder / (case + '.zip.xml'); changed.write(xml, encoding='utf-8', xml_declaration=True)
    os.link(folder / 'original.zip', folder / (case + '.zip'))
    malformed = folder / (case + '.rdc'); convert(xml, malformed, 'rdc')
    for name, command in [('api', [str(opener), str(malformed)]),
                          ('cli', [str(cmd), 'replay', '--loops', '1', str(malformed)])]:
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=60, text=True)
        (folder / (case + '-' + name + '.log')).write_text(result.stdout)
        if result.returncode <= 0 or 'Metal replay wait begin' in result.stdout:
            raise RuntimeError(case + ' was not safely rejected before frame GPU execution: ' + name)
print('PASS future descriptor snapshot alias gates: live old slot and unsubmitted consumer, API/CLI pre-frame rejection')
