#!/usr/bin/env python3
"""Measure captured placement ranges using device layout queries, without GPU work.

The probe only creates an MTLDevice and asks heapSizeAndAlign. It does not create
heaps, textures, buffers, command queues or command buffers. An overlap is evidence
of physical sharing, not proof of legal lifetime or GPU completion.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET


def fields(node):
    result = {}
    for child in node:
        name = child.get('name')
        if name and child.text and child.text.strip():
            text = child.text.strip()
            if text in ('true', 'false'):
                result[name] = text == 'true'
            else:
                try:
                    result[name] = int(text)
                except ValueError:
                    pass
    return result


def extract(path):
    placements, heaps = [], {}
    scope, frame_min, frame_max = None, None, None
    for _, chunk in ET.iterparse(path, events=('end',)):
        if chunk.tag != 'chunk':
            continue
        name = chunk.get('name', '')
        index = int(chunk.get('chunkIndex'))
        timestamp = int(chunk.get('timestamp', '0'))
        if name == 'Internal::Frame Metadata':
            scope = index
        elif scope is not None and not name.startswith('Internal::') and timestamp:
            frame_min = timestamp if frame_min is None else min(frame_min, timestamp)
            frame_max = timestamp if frame_max is None else max(frame_max, timestamp)
        data = fields(chunk)
        if name == 'MTLDevice::newHeapWithDescriptor':
            heaps[str(data['Heap'])] = data
        elif name in ('MTLHeap::newBuffer(offset)', 'MTLHeap::newTexture(offset)'):
            texture = name == 'MTLHeap::newTexture(offset)'
            entry = {'kind': 'texture' if texture else 'buffer',
                     'resource': data['Texture' if texture else 'Buffer'],
                     'heap': data['Heap'], 'offset': data['offset'], 'chunk': index,
                     'timestamp': timestamp, 'frame': scope is not None}
            if texture:
                descriptor = next(n for n in chunk if n.get('name') == 'descriptor')
                entry['descriptor'] = fields(descriptor)
            else:
                entry.update(length=data['length'], options=data['options'])
            placements.append(entry)
        chunk.clear()
    if scope is None:
        raise ValueError('Capture has no frame metadata')
    return {'scope': scope, 'heaps': heaps, 'placements': placements,
            'frame_call_timestamp_range': [frame_min, frame_max]}


def analyse(source, layouts):
    if layouts['gpu_commands_submitted'] != 0:
        raise ValueError('Expected query-only layout probe')
    measured = layouts['placements']
    expected = source['placements']
    if len(measured) != len(expected):
        raise ValueError('Layout query count mismatch')
    ranges, issues, overlaps = defaultdict(list), [], []
    for original, entry in zip(expected, measured):
        if any(entry.get(key) != value for key, value in original.items()):
            raise ValueError('Layout results do not match captured inputs')
        size, align, offset = entry['native_size'], entry['native_align'], entry['offset']
        heap = source['heaps'].get(str(entry['heap']))
        if not heap or size <= 0 or align <= 0 or offset % align or offset + size > heap['size']:
            issues.append(entry)
            continue
        for old in ranges[entry['heap']]:
            if offset < old['offset'] + old['native_size'] and old['offset'] < offset + size:
                overlaps.append({'heap': entry['heap'], 'old': old, 'new': entry})
        ranges[entry['heap']].append(entry)
    counts = Counter(('frame' if row['old']['frame'] else 'background') + '_' + row['old']['kind'] +
                     '_to_' + ('frame' if row['new']['frame'] else 'background') + '_' + row['new']['kind']
                     for row in overlaps)
    first, last = source['frame_call_timestamp_range']
    hoisted = [entry for entry in measured if entry['kind'] == 'texture' and not entry['frame']
               and first is not None and first <= entry['timestamp'] <= last]
    return {'device': layouts['device'], 'scope': source['scope'], 'placements': len(measured),
            'recommended_max_working_set': layouts['recommended_max_working_set'],
            'range_issues': issues, 'overlap_count': len(overlaps), 'overlap_counts': dict(counts),
            'overlaps': overlaps, 'background_texture_births_inside_frame_timing': hoisted,
            'frame_call_timestamp_range': [first, last], 'gpu_commands_submitted': 0,
            'limitations': ['Native sizes are device-specific allocation footprints, not measured residency.',
                            'Overlaps retain historical ranges and do not prove live-resource conflicts.',
                            'CPU API order and timestamps do not prove GPU completion or logical retirement.',
                            'Timestamp-in-frame findings need source and dependency validation.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml', type=Path)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = extract(args.xml)
    query_input = args.output.with_suffix('.layout-input.json')
    query_output = args.output.with_suffix('.native-layouts.json')
    query_input.write_text(json.dumps(source, indent=2))
    subprocess.run([str(args.probe.resolve()), str(query_input.resolve()), str(query_output.resolve())],
                   check=True, timeout=60)
    result = analyse(source, json.loads(query_output.read_text()))
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps({key: result[key] for key in ('device', 'placements', 'overlap_count', 'overlap_counts')}, indent=2))
    print('Range issues:', len(result['range_issues']))
    print('Background texture births inside frame timing:', len(result['background_texture_births_inside_frame_timing']))


if __name__ == '__main__':
    main()
