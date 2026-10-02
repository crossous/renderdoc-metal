#!/usr/bin/env python3
"""CPU-only placement-buffer overlap and descriptor lifetime audit.

Lengths here are logical buffer lengths, not native heapSizeAndAlign sizes.
This reports evidence; it never grants permission to submit a capture to the GPU.
"""
import argparse
from bisect import bisect_right
from collections import Counter, defaultdict
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def field(chunk, name, default=None):
    return next((n.text for n in chunk if n.get('name') == name), default)


def audit(path):
    chunks = ET.parse(path).find('./chunks')
    scope = next(int(c.get('chunkIndex')) for c in chunks if c.get('id') == '5')
    parents, references, identities = {}, defaultdict(list), {}
    ignored_refs = {'MTLResource::CaptureGPUIdentity', 'Internal::Initial Contents'}
    for c in chunks:
        name, index = c.get('name'), int(c.get('chunkIndex'))
        if name == 'MTLBuffer::newTextureWithDescriptor':
            parents[int(field(c, 'Texture'))] = int(field(c, 'Buffer'))
        elif name == 'MTLTexture::newTextureViewWithPixelFormat':
            parents[int(field(c, 'View'))] = int(field(c, 'Source'))
        elif name == 'MTLResource::CaptureGPUIdentity':
            identities[int(field(c, 'resource'))] = (int(field(c, 'kind')), int(field(c, 'value')))
        if index > scope and name not in ignored_refs:
            for n in c.iter('ResourceId'):
                rid = int(n.text or '0')
                if rid:
                    references[rid].append(index)

    def ancestors(resource):
        seen = set()
        while resource and resource not in seen:
            seen.add(resource)
            resource = parents.get(resource, 0)
        return seen

    # Views also keep their parent allocation relevant. Include nested array ResourceIds.
    dependent_refs = defaultdict(list)
    for rid, refs in references.items():
        for parent in ancestors(rid):
            dependent_refs[parent].extend(refs)
    for refs in dependent_refs.values():
        refs.sort()

    ranges, slots = defaultdict(list), {}
    overlaps, totals = [], Counter()
    for c in chunks:
        name, index = c.get('name'), int(c.get('chunkIndex'))
        if name == 'MTLBuffer::DescriptorSlotEvent':
            key = (int(field(c, 'buffer')), int(field(c, 'offset')))
            event, generation = int(field(c, 'event')), int(field(c, 'generation'))
            if event == 0:
                slots[key] = {'generation': generation, 'live': True, 'sources': {}}
            elif key in slots:
                if event == 1:
                    slots[key]['live'] = False
                elif event in (2, 3):
                    slots[key]['sources'] = {}
        elif name == 'MTLBuffer::DescriptorSlotBinding':
            key = (int(field(c, 'buffer')), int(field(c, 'offset')))
            if key in slots:
                slots[key]['sources'][int(field(c, 'kind'))] = int(field(c, 'resource'))
        elif name == 'MTLHeap::newBuffer(offset)':
            heap, rid = int(field(c, 'Heap')), int(field(c, 'Buffer'))
            offset, length = int(field(c, 'offset')), int(field(c, 'length'))
            new = {'chunk': index, 'resource': rid, 'offset': offset, 'length': length,
                   'options': int(field(c, 'options')), 'frame': index > scope}
            for old in ranges[heap]:
                if offset >= old['offset'] + old['length'] or old['offset'] >= offset + length:
                    continue
                if index <= scope:
                    totals['background_background_overlaps'] += 1
                    continue
                live = []
                for key, slot in slots.items():
                    if slot['live'] and any(old['resource'] in ancestors(src) for src in slot['sources'].values()):
                        live.append({'buffer': key[0], 'offset': key[1], 'generation': slot['generation']})
                refs = dependent_refs.get(old['resource'], [])
                later = refs[bisect_right(refs, index):]
                old_identity, new_identity = identities.get(old['resource']), identities.get(rid)
                equal_va = bool(old_identity and new_identity and old_identity[0] == new_identity[0] == 0
                                and old_identity[1] == new_identity[1])
                item = {'heap': heap, 'old': old, 'new': new, 'live_descriptor_sources': live,
                        'later_reference_count': len(later), 'first_later_references': later[:8],
                        'equal_captured_base_va': equal_va}
                overlaps.append(item)
                totals['frame_old' if old['frame'] else 'background_old'] += 1
                totals['live_source_overlap' if live else 'no_live_source_overlap'] += 1
                if equal_va:
                    totals['equal_captured_base_va'] += 1
            ranges[heap].append(new)
    return {'xml': str(path), 'scope': scope, 'counts': dict(totals),
            'buffer_overlap_count': len(overlaps), 'overlaps': overlaps,
            'limitations': ['Logical lengths underestimate padded native allocation ranges.',
                            'Texture placement ranges are not measured by this audit.',
                            'Resource references are CPU API evidence, not GPU completion.',
                            'Absent logical references do not prove allocation retirement.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.xml)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'overlaps'}, indent=2))


if __name__ == '__main__':
    main()
