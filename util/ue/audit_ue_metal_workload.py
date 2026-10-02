#!/usr/bin/env python3
"""CPU-only workload inventory for a captured UE Metal frame; no replay acceptance claim."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET


def fields(node):
    return {n.get('name'): n.text for n in node if n.get('name')}


def analyse(path):
    chunks = list(ET.parse(path).find('./chunks'))
    scope = next(i for i, c in enumerate(chunks) if c.get('id') == '5')
    frame = chunks[scope + 1:]
    direct, indirect, copies, texture_copies, draws = [], [], [], [], []
    producers, layouts, heaps = Counter(), set(), []
    for c in chunks:
        name, data = c.get('name', ''), fields(c)
        if name == 'MTLBuffer::DeclareDescriptorTable': layouts.add(data['buffer'])
        if name == 'MTLDevice::newHeapWithDescriptor':
            heaps.append({'heap': int(data['Heap']), 'size': int(data['size'])})
    for c in frame:
        name, data = c.get('name', ''), fields(c)
        if name == 'MTLComputeCommandEncoder::dispatchThreadgroups':
            sizes = {n.get('name'): [int(v.text) for v in n]
                     for n in c if n.get('name') in ('groups', 'threadsPerGroup')}
            direct.append({'encoder': int(data['ComputeCommandEncoder']), **sizes,
                           'threads': math.prod(sizes['groups'] + sizes['threadsPerGroup'])})
        elif 'indirectBuffer' in data and ('dispatch' in name or 'draw' in name):
            indirect.append({'chunk': name, 'buffer': int(data['indirectBuffer']),
                             'offset': int(data['indirectBufferOffset']),
                             'kind': 'indexed-draw' if 'indexBuffer' in data else
                                 'draw' if 'draw' in name else 'dispatch'})
        elif name == 'MTLBlitCommandEncoder::copyFromBuffer':
            if 'size' in data:
                copies.append({k: int(data[k]) for k in
                               ('sourceBuffer', 'sourceOffset', 'destinationBuffer', 'destinationOffset', 'size')})
            else:
                texture_copies.append({'chunk': name, 'source': int(data['sourceBuffer'])})
        elif name.startswith('MTLRenderCommandEncoder::draw'):
            count = int(data.get('indexCount', data.get('vertexCount', 0)))
            instances = int(data.get('instanceCount', 1))
            draws.append({'chunk': name, 'count': count, 'instances': instances,
                          'work': count * instances, 'primitive': int(data.get('primitiveType', -1)),
                          'indexBuffer': int(data.get('indexBuffer', 0))})
        elif name == 'MTLBuffer::DescriptorSlotProducer': producers[data['encoder']] += 1
    copy_bytes = sum(c['size'] for c in copies)
    total_threads = sum(d['threads'] for d in direct)
    return {
        'xml': str(path.resolve()), 'gpu_commands_submitted': 0,
        'complete_replay_validated': False,
        'direct_dispatch': {'count': len(direct), 'threads': total_threads,
                            'max_threads': max((d['threads'] for d in direct), default=0),
                            'largest': sorted(direct, key=lambda d: d['threads'], reverse=True)[:12]},
        'producer': {'count': sum(producers.values()), 'batch_sizes': sorted(producers.values())},
        'buffer_copy': {'count': len(copies), 'bytes': copy_bytes,
                        'max_bytes': max((c['size'] for c in copies), default=0),
                        'declared_table_backing_touches': [c for c in copies if
                            str(c['sourceBuffer']) in layouts or str(c['destinationBuffer']) in layouts],
                        'largest': sorted(copies, key=lambda c: c['size'], reverse=True)[:12]},
        'buffer_to_texture_copies': texture_copies,
        'indirect': {'count': len(indirect), 'kinds': dict(Counter(d['kind'] for d in indirect)),
                     'buffers': dict(Counter(d['buffer'] for d in indirect)), 'commands': indirect},
        'direct_draw': {'count': len(draws), 'work': sum(d['work'] for d in draws),
                        'max_work': max((d['work'] for d in draws), default=0),
                        'primitives': dict(Counter(d['primitive'] for d in draws)),
                        'index_buffers': dict(Counter(d['indexBuffer'] for d in draws if d['indexBuffer']))},
        'placement_heaps': {'count': len(heaps), 'bytes': sum(h['size'] for h in heaps), 'heaps': heaps},
        'limitations': ['Counts and CPU ranges do not prove GPU completion, shader memory safety or residency.',
                        'GPU-produced indirect arguments are not evaluated by this inventory.',
                        'Indirect classification uses the serialized buffer field; legacy indexed-indirect chunk labels omit the suffix.',
                        'Only candidate workload capacity is described; resource/source/alias preflight is still required.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = analyse(args.xml)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ('gpu_commands_submitted', 'complete_replay_validated', 'producer')}, indent=2))


if __name__ == '__main__':
    main()
