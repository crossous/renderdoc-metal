#!/usr/bin/env python3
"""Read structured capture chunks without GPU replay; resolve nested encoder owners."""
import argparse
import json
import xml.etree.ElementTree as ET
p = argparse.ArgumentParser()
p.add_argument('xml')
p.add_argument('--events', nargs='+', type=int, required=True)
a = p.parse_args()
chunks = list(ET.parse(a.xml).getroot().find('chunks'))
start = next(i for i, c in enumerate(chunks) if c.get('id') == '4')
owners, queues, events, submissions = {}, {}, {}, []
def value(c, name):
    e = c.find("./*[@name='%s']" % name)
    return e.text if e is not None else None
for eid, c in enumerate(chunks[start + 1:], 1):
    cb = value(c, 'CommandBuffer')
    enc = value(c, 'RenderCommandEncoder')
    par = value(c, 'ParallelRenderCommandEncoder')
    if cb:
        for x in (enc, par):
            if x: owners[x] = cb
        if value(c, 'CommandQueue'): queues[cb] = value(c, 'CommandQueue')
    if par and enc and par in owners: owners[enc] = owners[par]
    owner = cb or owners.get(enc) or owners.get(par)
    if eid in a.events:
        events[eid] = dict(eid=eid, api=c.get('name'), encoder=enc or par,
                           command_buffer=owner, queue=queues.get(owner),
                           encoding_thread=c.get('threadID'))
    if c.get('name') in ('MTLCommandBuffer::enqueue', 'MTLCommandBuffer::commit'):
        submissions.append(dict(eid=eid, api=c.get('name'), command_buffer=cb,
                                queue=queues.get(cb)))
assert set(events) == set(a.events), 'Some requested EIDs are absent'
assert all(e['command_buffer'] for e in events.values()), 'Encoder ownership is incomplete'
selected = {e['command_buffer'] for e in events.values()}
print(json.dumps(dict(events=list(events.values()),
                     submissions=[s for s in submissions if s['command_buffer'] in selected]), indent=2))
