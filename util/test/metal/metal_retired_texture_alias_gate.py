#!/usr/bin/env python3
"""A retired texture backing cannot bypass live sources or unfinished consumers."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

cmd, opener, capture, folder = [Path(p).resolve() for p in sys.argv[1:]]
folder.mkdir(parents=True, exist_ok=False)
env = {k:v for k,v in os.environ.items() if not k.startswith('RENDERDOC_METAL_')}
env.update(MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
           RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
def convert(source, output, fmt):
    subprocess.run([str(cmd), 'convert', '-f', str(source), '-o', str(output), '-c', fmt],
                   env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=60)
original = folder/'original.zip.xml'
convert(capture, original, 'zip.xml')
tree = ET.parse(original)
fields = lambda node: {c.get('name'):c for c in node}
for case in ('live_texture_descriptor', 'unsubmitted_texture_consumer', 'old_coverage'):
    changed=copy.deepcopy(tree); chunks=changed.getroot().find('chunks')
    retirement=next(c for c in chunks if c.get('id')=='1403' and fields(c)['event'].text=='1')
    birth=next(c for c in chunks if c.get('id')=='1317')
    if case=='live_texture_descriptor':
        chunks.remove(retirement)
    elif case=='unsubmitted_texture_consumer':
        commit=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::commit')
        chunks.remove(commit); chunks.insert(list(chunks).index(birth)+1,commit)
    else:
        coverage=next(c for c in chunks if c.get('id')=='1399')
        fields(coverage)['version'].text='64'
    xml=folder/(case+'.zip.xml'); changed.write(xml,encoding='utf-8',xml_declaration=True)
    os.link(folder/'original.zip',folder/(case+'.zip'))
    malformed=folder/(case+'.rdc'); convert(xml,malformed,'rdc')
    for name,command in [('api',[str(opener),str(malformed)]),
                         ('cli',[str(cmd),'replay','--loops','1',str(malformed)])]:
        result=subprocess.run(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                              text=True,timeout=60)
        (folder/(case+'-'+name+'.log')).write_text(result.stdout)
        if result.returncode<=0 or 'Metal replay wait begin' in result.stdout:
            raise RuntimeError(case+' was not rejected before frame GPU work: '+name)
print('PASS retired texture gates: live descriptor, unsubmitted consumer, old coverage; API/CLI pre-frame rejection')
