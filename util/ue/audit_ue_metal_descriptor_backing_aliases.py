#!/usr/bin/env python3
"""Audit typed-table placement reuse in captured API order, without GPU work.

Committed submissions are not completed submissions. This report establishes a
CPU ordering prerequisite for replay's existing wait-before-CPU-restore path;
it never authorizes submitting a capture or infers identity from a raw address.
"""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import xml.etree.ElementTree as ET

from audit_ue_metal_native_heap_layouts import fields


def audit(xml, native):
    slots, tables, owners, commits = {}, {}, {}, {}
    consumers = defaultdict(dict)
    pending = defaultdict(set)
    rows = []
    births = defaultdict(list)
    frame = False
    for overlap in native['overlaps']:
        if overlap['new']['frame']:
            births[overlap['new']['chunk']].append(overlap)

    def record(resource, encoder, index, reason):
        if resource:
            pending[encoder].add(resource)
            # Record declarations conservatively, including bindings overwritten
            # before a dispatch. Direct CBV root tables are not inline bindings.
            consumers[resource][owners.get(encoder, 0)] = (index, reason)

    for _, chunk in ET.iterparse(xml, events=('end',)):
        if chunk.tag != 'chunk':
            continue
        name = chunk.get('name', '')
        index = int(chunk.get('chunkIndex'))
        data = fields(chunk)
        if name == 'Internal::Frame Metadata':
            frame = True
        if name == 'MTLBuffer::DeclareDescriptorTable':
            tables[data['buffer']] = data
        elif name == 'MTLBuffer::DescriptorSlotEvent':
            key = (data['buffer'], data['offset'])
            if data['event'] == 0:
                slots[key] = {'live': True, 'generation': data['generation'],
                              'type': data['descriptorType'], 'sources': {}, 'chunk': index}
            elif data['event'] == 1 and key in slots:
                slots[key]['live'] = False
                slots[key]['chunk'] = index
            elif data['event'] in (2, 3) and key in slots:
                slots[key]['sources'].clear()
        elif name == 'MTLBuffer::DescriptorSlotBinding' and (data['buffer'], data['offset']) in slots:
            slots[(data['buffer'], data['offset'])]['sources'][data['kind']] = data['resource']
        elif frame:
            command = data.get('CommandBuffer')
            for key in ('ComputeCommandEncoder', 'RenderCommandEncoder',
                        'ParallelRenderCommandEncoder', 'BlitCommandEncoder'):
                if command and data.get(key):
                    owners[data[key]] = command
            if name == 'MTLParallelRenderCommandEncoder::renderCommandEncoder':
                owners[data['RenderCommandEncoder']] = owners.get(data['ParallelRenderCommandEncoder'], 0)
            encoder = data.get('ComputeCommandEncoder', data.get('RenderCommandEncoder', data.get('encoder')))
            if name == 'MTLCommandEncoder::DescriptorInlineBinding':
                record(data['resource'], encoder, index, 'inline source')
            elif '::setBuffer' in name or '::setVertexBuffer' in name or '::setFragmentBuffer' in name:
                record(data.get('buffer'), encoder, index, 'direct binding')
            elif '::useResource' in name:
                record(data.get('resource'), encoder, index, 'resource declaration')
                for array in chunk:
                    if array.get('name') == 'resources':
                        for value in array:
                            record(int(value.text), encoder, index, 'resource declaration')
            elif name == 'MTLBuffer::DescriptorSlotProducer':
                record(data.get('buffer'), data['encoder'], index, 'GPU descriptor destination')
                record(data.get('source'), data['encoder'], index, 'GPU descriptor source')
            if '::dispatch' in name or '::draw' in name:
                # Include descriptors of explicitly bound tables. Global bindless
                # tables are conservative: every live sourced slot is a dependency.
                for key, slot in slots.items():
                    if slot['live'] and key[0] in pending[encoder]:
                        for resource in slot['sources'].values():
                            record(resource, encoder, index, 'live table source at GPU work')
            if name == 'MTLCommandBuffer::commit':
                commits[data['CommandBuffer']] = index
        for overlap in births[index]:
            old, new = overlap['old']['resource'], overlap['new']['resource']
            # Declarations can follow the new birth; use all known declarations
            # gathered on a second pass below to select these rows.
            old_slots = [s for (buffer, _), s in slots.items() if buffer == old]
            prior = [{'command': cmd, 'last_reference_chunk': use[0], 'reason': use[1],
                      'commit_chunk': commits.get(cmd)} for cmd, use in consumers[old].items()]
            rows.append({'old': old, 'new': new, 'birth_chunk': index,
                         'old_known_slots': len(old_slots),
                         'old_live_slots': sum(s['live'] for s in old_slots),
                         'old_last_slot_event': max((s['chunk'] for s in old_slots), default=None),
                         'old_references': prior,
                         'old_uncommitted_reference_commands': [p['command'] for p in prior if p['commit_chunk'] is None]})
        chunk.clear()
    result = [r for r in rows if r['old'] in tables or r['new'] in tables]
    for row in result:
        row['old_table'] = row['old'] in tables
        row['new_table'] = row['new'] in tables
        row['slot_retirement_proven'] = not row['old_table'] or bool(row['old_known_slots']) and row['old_live_slots'] == 0
        row['prior_consumers_committed'] = not row['old_uncommitted_reference_commands']
        row['later_old_references'] = [dict(command=cmd, chunk=use[0], reason=use[1])
                                       for cmd, use in consumers[row['old']].items() if use[0] > row['birth_chunk']]
    return {'gpu_commands_submitted': 0, 'typed_table_overlap_count': len(result), 'overlaps': result,
            'limitations': ['This is conservative API-order evidence, not GPU completion proof.',
                            'Heap residency declarations alone do not prove all indirect GPU consumers.',
                            'Historical table/slot identity remains distinct from physical backing storage.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml', type=Path)
    parser.add_argument('--native-audit', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result = audit(args.xml, json.loads(args.native_audit.read_text()))
    args.output.write_text(json.dumps(result, indent=2))
    for row in result['overlaps']:
        print(f"{row['old']} -> {row['new']} at {row['birth_chunk']}: "
              f"retired={row['slot_retirement_proven']} committed={row['prior_consumers_committed']} "
              f"later references={len(row['later_old_references'])}")
    print('GPU commands submitted: 0')


if __name__ == '__main__':
    main()
