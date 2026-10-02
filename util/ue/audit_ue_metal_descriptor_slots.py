#!/usr/bin/env python3
"""CPU-only audit of explicit UE descriptor allocation/update/free diagnostics.

Candidate identities are diagnostics, not permission to submit this capture to GPU.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile


def field(chunk, name):
    return next((n.text for n in chunk if n.get('name') == name), None)


def audit(xml):
    chunks = ET.parse(xml).find('./chunks')
    identities = {kind: defaultdict(set) for kind in range(3)}
    lengths, initial, declarations, identities_by_resource = {}, {}, {}, {}
    scope = next(int(c.get('chunkIndex')) for c in chunks if c.get('id') == '5')
    for chunk in chunks:
        name = chunk.get('name')
        if name == 'MTLResource::CaptureGPUIdentity':
            identities[int(field(chunk, 'kind'))][int(field(chunk, 'value'))].add(int(field(chunk, 'resource')))
            identities_by_resource[int(field(chunk, 'resource'))] = (int(field(chunk, 'kind')), int(field(chunk, 'value')))
        if name in ('MTLHeap::newBuffer(offset)', 'MTLHeap::newBufferWithLength',
                    'MTLDevice::newBufferWithLength', 'MTLDevice::newBufferWithBytes'):
            lengths[int(field(chunk, 'Buffer'))] = int(field(chunk, 'length'))
        if name == 'Internal::Initial Contents' and field(chunk, 'type') == '1':
            initial[int(field(chunk, 'id'))] = int(field(chunk, 'Contents'))
        if name == 'MTLBuffer::DeclareDescriptorTable':
            declarations[int(field(chunk, 'buffer'))] = {
                n: int(field(chunk, n)) for n in ('offset', 'count', 'stride', 'schema')}
    states, histories, issues = {}, Counter(), []
    snapshots = []
    epoch_counts, epoch_issues = Counter(), []
    inline_counts, inline_issues, pending_inline = Counter(), [], {}
    producer_counts, producer_issues, producers, dispatches = Counter(), [], {}, {}
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        initial_cache = {}
        def payload(c):
            element = next(n for n in c if n.get('name') == 'data')
            data = archive.read(f'{int(element.text):06d}')
            assert len(data) == int(element.get('byteLength'))
            return data
        def finish_value(key, slot):
            if not slot or slot['data'] is None or slot.get('checked'):
                return
            slot['checked'] = True
            if slot.get('producer_bindings') is not None and slot['producer_bindings'] != slot['bindings']:
                producer_issues.append({'chunk': slot['value_chunk'], 'key': key,
                    'reason': 'GPU destination source bindings disagree with producer payload'})
            epoch_counts['written_values'] += 1
            words = struct.unpack('<QQQ', slot['data'])
            fields = [(2, words[0])] if slot['type'] == 7 else [(0, words[0]), (1, words[1])]
            for kind, raw in fields:
                if not raw:
                    continue
                explicit = slot['bindings'].get(kind)
                identity = identities_by_resource.get(explicit[0]) if explicit else None
                expected = identity[1] + explicit[1] if identity and kind == 0 else identity[1] if identity else None
                reason = None
                if not explicit:
                    reason = 'missing source binding'
                elif not identity:
                    reason = 'source identity not retained in capture'
                elif not identity or identity[0] != kind or raw != expected:
                    reason = 'source identity/value mismatch'
                elif kind == 0 and explicit[1] >= lengths.get(explicit[0], 0):
                    reason = 'source offset out of bounds'
                if reason:
                    epoch_issues.append({'chunk': slot['value_chunk'], 'key': key, 'kind': kind,
                        'raw': raw, 'source': explicit, 'reason': reason})
                else:
                    epoch_counts[f'kind{kind}_explicit_match'] += 1
                epoch_counts[f'{"frame" if slot["value_chunk"] > scope else "background"}_fields'] += 1
        def snapshot(label):
            counts, details, binding_issues = Counter(), [], []
            buffers = Counter()
            for (buffer, offset), slot in states.items():
                if not slot['live']:
                    counts['freed_slots'] += 1
                    continue
                counts['live_slots'] += 1
                buffers[buffer] += 1
                data = slot['data']
                if data is None:
                    counts['live_without_value'] += 1
                    continue
                if label == 'frame_start':
                    index = initial.get(buffer)
                    if index is not None and index not in initial_cache:
                        initial_cache[index] = archive.read(f'{index:06d}')
                    contents = initial_cache.get(index)
                    reason = None
                    if contents is None:
                        reason = 'live descriptor backing has no initial contents'
                    elif offset + 24 > len(contents):
                        reason = 'live descriptor exceeds initial contents'
                    elif contents[offset:offset + 24] != data:
                        reason = 'initial bytes disagree with latest declared descriptor value'
                    if reason:
                        counts['initial_contents_mismatch'] += 1
                        binding_issues.append({'buffer': buffer, 'offset': offset,
                            'generation': slot['generation'], 'reason': reason,
                            'declared': data.hex(),
                            'initial': contents[offset:offset + 24].hex() if contents is not None else None})
                    else:
                        counts['initial_contents_match'] += 1
                words = struct.unpack('<QQQ', data)
                fields = [(2, words[0])] if slot['type'] == 7 else [(0, words[0]), (1, words[1])]
                for kind, raw in fields:
                    if not raw:
                        continue
                    explicit = slot['bindings'].get(kind)
                    if explicit:
                        resource, member_offset = explicit
                        identity = identities_by_resource.get(resource)
                        expected = identity[1] + member_offset if identity and kind == 0 else identity[1] if identity else None
                        if identity and identity[0] == kind and expected == raw:
                            counts[f'kind{kind}_explicit_match'] += 1
                        else:
                            counts[f'kind{kind}_explicit_mismatch'] += 1
                            binding_issues.append({'buffer': buffer, 'offset': offset, 'kind': kind,
                                'raw': raw, 'resource': resource, 'member_offset': member_offset,
                                'identity': identity, 'generation': slot['generation']})
                    else:
                        counts[f'kind{kind}_no_explicit_binding'] += 1
                    if kind == 0:
                        candidates = [(rid, raw - base) for base, ids in identities[0].items()
                                      for rid in ids if base <= raw < base + lengths.get(rid, 0)]
                    else:
                        candidates = [(rid, 0) for rid in identities[kind].get(raw, ())]
                    status = 'unique' if len(candidates) == 1 else 'missing' if not candidates else 'multiple'
                    counts[f'kind{kind}_{status}'] += 1
                    if status != 'unique':
                        details.append({'buffer': buffer, 'offset': offset, 'type': slot['type'],
                            'generation': slot['generation'], 'kind': kind, 'value': raw,
                            'candidates': candidates})
            return {'phase': label, 'counts': dict(counts), 'buffers': dict(buffers),
                    'unresolved_candidates': details, 'binding_issues': binding_issues}
        for chunk in chunks:
            name = chunk.get('name', '')
            number = int(chunk.get('chunkIndex'))
            if name == 'MTLComputeCommandEncoder::dispatchThreadgroups':
                dispatches[int(field(chunk, 'ComputeCommandEncoder'))] = number
            if name == 'MTLBuffer::DescriptorSlotProducer':
                key = (int(field(chunk, 'buffer')), int(field(chunk, 'offset')))
                source_key = (int(field(chunk, 'source')), int(field(chunk, 'sourceOffset')))
                encoder = int(field(chunk, 'encoder'))
                source_slot, destination_slot = states.get(source_key), states.get(key)
                reason = None
                if key in producers:
                    reason = 'duplicate pending GPU producer'
                elif encoder not in dispatches:
                    reason = 'producer encoder has no preceding dispatch'
                elif not source_slot or not source_slot['live'] or source_slot['data'] is None:
                    reason = 'producer source is not a live typed payload'
                elif not destination_slot or not destination_slot['live'] or source_slot['type'] != destination_slot['type']:
                    reason = 'producer destination/type disagrees with live slot'
                if reason:
                    producer_issues.append({'chunk': number, 'key': key, 'source': source_key, 'reason': reason})
                else:
                    producers[key] = {'data': source_slot['data'], 'bindings': dict(source_slot['bindings']),
                                      'chunk': number, 'encoder': encoder}
                    producer_counts['sourced_records'] += 1
                continue
            if name == 'MTLCommandEncoder::DescriptorInlineLayout':
                key = tuple(int(field(chunk, n)) for n in ('encoder', 'stage', 'index'))
                if key in pending_inline:
                    inline_issues.append({'chunk': number, 'reason': 'inline declaration overwritten before bytes', 'key': key})
                pending_inline[key] = {'count': int(field(chunk, 'count')),
                    'stride': int(field(chunk, 'stride')), 'bindings': {}, 'chunk': number}
                continue
            if name == 'MTLCommandEncoder::DescriptorInlineBinding':
                key = tuple(int(field(chunk, n)) for n in ('encoder', 'stage', 'index'))
                layout = pending_inline.get(key)
                entry = int(field(chunk, 'entry'))
                if not layout or entry >= layout['count'] or entry in layout['bindings']:
                    inline_issues.append({'chunk': number, 'reason': 'inline binding without unique declared entry', 'key': key})
                else:
                    layout['bindings'][entry] = tuple(int(field(chunk, n)) for n in ('resource', 'memberOffset'))
                continue
            stages = {'MTLComputeCommandEncoder::setBytes': 0,
                'MTLRenderCommandEncoder::setVertexBytes': 1,
                'MTLRenderCommandEncoder::setFragmentBytes': 2,
                'MTLRenderCommandEncoder::setObjectBytes': 3,
                'MTLRenderCommandEncoder::setMeshBytes': 4}
            if name in stages:
                stage = stages[name]
                encoder = int(field(chunk, 'ComputeCommandEncoder' if stage == 0 else 'RenderCommandEncoder'))
                index = int(field(chunk, 'index'))
                key = (encoder, stage, index)
                layout = pending_inline.pop(key, None)
                if layout:
                    data = bytes(int(n.text) for n in next(n for n in chunk if n.get('name') == 'data'))
                    inline_counts[f'stage{stage}_declared_calls'] += 1
                    expected_size = layout['count'] * layout['stride'] if layout['count'] else layout['stride']
                    if len(data) != expected_size:
                        inline_issues.append({'chunk': number, 'reason': 'inline byte length disagrees with layout', 'key': key})
                        continue
                    if not layout['count']:
                        inline_counts['ordinary_draw_constant_calls'] += 1
                        if stage != 1 or not (index == 4 and len(data) == 20 or index == 5 and len(data) in (2, 4)) or layout['bindings']:
                            inline_issues.append({'chunk': number, 'reason': 'invalid ordinary draw constant layout', 'key': key})
                        elif index == 5 and (int.from_bytes(data, 'little') > 2 if len(data) == 2 else any(data)):
                            inline_issues.append({'chunk': number, 'reason': 'invalid ordinary index kind', 'key': key})
                        continue
                    for entry in range(layout['count']):
                        raw = struct.unpack_from('<Q', data, entry * layout['stride'])[0]
                        binding = layout['bindings'].get(entry)
                        if not raw:
                            inline_counts['zero_fields'] += 1
                            if binding:
                                inline_issues.append({'chunk': number, 'reason': 'zero inline address carries binding', 'key': key, 'entry': entry})
                            continue
                        identity = identities_by_resource.get(binding[0]) if binding else None
                        if not binding or not identity or identity[0] != 0 or identity[1] + binding[1] != raw or binding[1] >= lengths.get(binding[0], 0):
                            inline_issues.append({'chunk': number, 'reason': 'inline source missing or disagrees with bytes', 'key': key,
                                'entry': entry, 'raw': raw, 'binding': binding, 'identity': identity})
                        else:
                            inline_counts['explicit_fields_match'] += 1
                else:
                    inline_counts[f'stage{stage}_index{index}_undeclared_calls'] += 1
                continue
            if chunk.get('id') == '5':
                snapshots.append(snapshot('frame_start'))
            if chunk.get('name') == 'MTLBuffer::DescriptorSlotBinding':
                key = (int(field(chunk, 'buffer')), int(field(chunk, 'offset')))
                slot = states.get(key)
                if not slot or not slot['live'] or slot['data'] is None:
                    issues.append({'chunk': int(chunk.get('chunkIndex')), 'reason': 'binding without live written slot', 'key': key})
                else:
                    slot['bindings'][int(field(chunk, 'kind'))] = (
                        int(field(chunk, 'resource')), int(field(chunk, 'memberOffset')))
                continue
            if chunk.get('name') != 'MTLBuffer::DescriptorSlotEvent':
                continue
            number = int(chunk.get('chunkIndex'))
            buffer, offset, generation, event, descriptor_type = (
                int(field(chunk, n)) for n in ('buffer', 'offset', 'generation', 'event', 'descriptorType'))
            histories[f'{"frame" if number > scope else "background"}_event{event}'] += 1
            key = (buffer, offset)
            previous = states.get(key)
            finish_value(key, previous)
            data = payload(chunk)
            if event == 0:
                if previous and (previous['live'] or generation <= previous['generation']):
                    issues.append({'chunk': number, 'reason': 'allocated live/reused generation', 'key': key})
                if not generation or data:
                    issues.append({'chunk': number, 'reason': 'invalid allocation payload/generation', 'key': key})
                states[key] = {'generation': generation, 'type': descriptor_type, 'live': True,
                               'data': None, 'bindings': {}}
            elif event in (1, 2):
                if not previous or not previous['live'] or previous['generation'] != generation or previous['type'] != descriptor_type:
                    issues.append({'chunk': number, 'reason': 'free/write without matching live generation', 'key': key})
                    continue
                if event == 1:
                    previous['live'] = False
                    if data:
                        issues.append({'chunk': number, 'reason': 'free carries payload', 'key': key})
                elif len(data) != 24:
                    issues.append({'chunk': number, 'reason': 'incomplete CPU entry', 'key': key})
                else:
                    previous['data'] = data
                    previous['producer_bindings'] = None
                    previous['bindings'] = {}
                    previous['checked'] = False
                    previous['value_chunk'] = number
            elif event == 3:
                if not previous or not previous['live'] or generation or len(data) != 24:
                    issues.append({'chunk': number, 'reason': 'GPU expected value without live slot/complete payload', 'key': key})
                    continue
                producer = producers.pop(key, None)
                if number > scope:
                    if producer and producer['data'] == data:
                        producer_counts['expected_values_match'] += 1
                    else:
                        producer_issues.append({'chunk': number, 'key': key,
                            'reason': 'frame GPU expected value lacks matching typed producer'})
                previous['data'] = data
                previous['producer_bindings'] = producer['bindings'] if producer else None
                previous['bindings'] = {}
                previous['checked'] = False
                previous['value_chunk'] = number
            else:
                issues.append({'chunk': number, 'reason': 'unknown event', 'key': key})
        for key, slot in states.items():
            finish_value(key, slot)
        snapshots.append(snapshot('frame_end'))
        for key, producer in producers.items():
            producer_issues.append({'chunk': producer['chunk'], 'key': key, 'reason': 'producer has no matching expected value'})
        for key, layout in pending_inline.items():
            inline_issues.append({'chunk': layout['chunk'], 'reason': 'inline declaration never consumed', 'key': key})
    # Report a very narrow retirement prefix separately while retaining every raw
    # source mismatch. No GPU submission or source-binding repair is authorized here.
    retirements = []
    scope_index = next(i for i, chunk in enumerate(chunks) if int(chunk.get('chunkIndex')) == scope)
    for chunk in list(chunks)[scope_index + 1:]:
        if chunk.get('name') == 'Internal::Beginning of Capture':
            continue
        if chunk.get('name') != 'MTLBuffer::DescriptorSlotEvent' or field(chunk, 'event') != '1':
            break
        retirements.append({'chunk': int(chunk.get('chunkIndex')),
                            'buffer': int(field(chunk, 'buffer')),
                            'offset': int(field(chunk, 'offset')),
                            'generation': int(field(chunk, 'generation'))})
    retired_keys = {(row['buffer'], row['offset'], row['generation']) for row in retirements}
    prelude_retired_issues = [issue for issue in snapshots[0]['binding_issues']
                            if (issue['buffer'], issue['offset'], issue['generation']) in retired_keys]
    return {'chunks': len(chunks), 'scope': scope, 'events': dict(histories),
            'declared_tables': declarations, 'lifetime_issues': issues, 'snapshots': snapshots,
            'value_epochs': dict(epoch_counts), 'value_epoch_issues': epoch_issues,
            'frame_value_epoch_issues': [issue for issue in epoch_issues if issue['chunk'] > scope],
            'inline_counts': dict(inline_counts), 'inline_issues': inline_issues,
            'producer_counts': dict(producer_counts), 'producer_issues': producer_issues,
            'leading_slot_retirements': retirements,
            'initial_binding_issues_retired_in_prefix': prelude_retired_issues,
            'limitations': ['Candidate lookup includes all captured identities, not execution-time liveness.',
                'Multiple sampler candidates are not yet compared for equivalent replay state.',
                'Does not prove temporal source slices, shader usage, heap aliases or GPU replay correctness.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.xml)
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps({key: result[key] for key in ('chunks', 'scope', 'events')}, indent=2))
    print('Lifetime issues:', len(result['lifetime_issues']))
    print('Value epochs:', result['value_epochs'], 'issues:', len(result['value_epoch_issues']))
    print('Inline:', result['inline_counts'], 'issues:', len(result['inline_issues']))
    print('GPU producers:', result['producer_counts'], 'issues:', len(result['producer_issues']))
    for entry in result['snapshots']:
        print(entry['phase'], json.dumps(entry['counts']), 'candidate details:', len(entry['unresolved_candidates']))


if __name__ == '__main__':
    main()
