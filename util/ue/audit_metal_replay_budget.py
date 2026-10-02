#!/usr/bin/env python3
"""Measure replay allocation costs without creating resources or submitting GPU work.

Placement children share the recorded heap allocation. Initial CPU snapshots and
the largest serialized texture upload are reported separately; these values do
not prove residency, source validity, or permission to replay an entire frame.
"""
import argparse
import json
import subprocess
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path
from audit_ue_metal_native_heap_layouts import fields


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml', type=Path)
    parser.add_argument('--probe', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    heaps, queries, initial, textures = {}, [], [], {}
    counts = Counter()
    heap_buffer_payload = 0
    snapshot_bytes = 0
    scope = None
    for _, chunk in ET.iterparse(args.xml, events=('end',)):
        if chunk.tag != 'chunk':
            continue
        name, data = chunk.get('name', ''), fields(chunk)
        if name == 'Internal::Frame Metadata':
            scope = int(chunk.get('chunkIndex'))
        if name == 'MTLDevice::newHeapWithDescriptor':
            heaps[str(data['Heap'])] = data
        if name in ('MTLDevice::newBufferWithLength', 'MTLDevice::newBufferWithBytes'):
            queries.append(dict(kind='buffer', resource=data['Buffer'], length=data['length'],
                                options=data['options'], heap=0, offset=0))
            counts['standalone_buffers'] += 1
        elif name.startswith('MTLHeap::newBuffer'):
            heap_buffer_payload += data['length']
            counts['heap_buffers'] += 1
        elif name in ('MTLDevice::newTextureWithDescriptor', '[CAMetalLayer nextDrawable]'):
            descriptor = fields(next(n for n in chunk if n.get('name') == 'descriptor'))
            queries.append(dict(kind='texture', resource=data['Texture'], descriptor=descriptor,
                                heap=0, offset=0))
            counts['standalone_textures'] += 1
            textures[data['Texture']] = queries[-1]
        elif name == 'MTLHeap::newTexture(offset)':
            descriptor = fields(next(n for n in chunk if n.get('name') == 'descriptor'))
            textures[data['Texture']] = dict(kind='texture', resource=data['Texture'], descriptor=descriptor,
                                             heap=data['Heap'], offset=data['offset'])
        if name in ('Internal_MTLBufferModifyCPUContents', 'MTLBuffer::DescriptorCPUWrite'):
            payload = next((n for n in chunk if n.get('name') == 'data'), None)
            if payload is not None:
                snapshot_bytes += int(payload.get('byteLength'))
        if name == 'Internal::Initial Contents':
            contents = next((n for n in chunk if n.get('name') == 'Contents'), None)
            if contents is not None:
                initial.append(dict(resource=data['id'], type=data['type'],
                                    bytes=int(contents.get('byteLength'))))
        chunk.clear()
    # Same packed mip rows used by the replay uploader. Ask Native Metal only for
    # linear color alignment; BC and depth have dedicated upload rules.
    sizes = {10:1,11:1,12:1,13:1,20:2,23:2,25:2,30:2,40:4,53:4,55:4,60:4,63:4,65:4,
             70:4,71:4,73:4,80:4,81:4,90:4,92:4,103:8,105:8,110:8,115:8,123:16,125:16,
             250:2,252:4,260:5}
    bc = {130:8,131:8,132:16,133:16,134:16,135:16,140:8,141:8,142:16,143:16,
          150:16,151:16,152:16,153:16}
    for state in initial:
        if state['type'] != 9:
            continue
        texture = textures.get(state['resource'])
        if not texture:
            raise ValueError('Missing initial texture allocation descriptor')
        desc = texture['descriptor']
        fmt, typ = desc['pixelFormat'], desc['textureType']
        slices = desc['arrayLength'] * (6 if typ in (5,6) else 1)
        rows = []
        for mip in range(desc['mipmapLevelCount']):
            width, height = max(1,desc['width']>>mip), max(1,desc['height']>>mip)
            depth = max(1,desc['depth']>>mip) if typ == 7 else 1
            planes = (4,1) if fmt == 260 else (bc.get(fmt,sizes.get(fmt)),)
            if planes[0] is None:
                raise ValueError('Unsupported upload format '+str(fmt))
            for size in planes:
                block = 4 if fmt in bc else 1
                rows.append(dict(row_bytes=((width+block-1)//block)*size,
                                 rows=((height+block-1)//block)*slices, depth=depth))
        packed = sum(r['row_bytes']*r['rows']*r['depth'] for r in rows)
        if packed != state['bytes']:
            raise ValueError('Packed initial texture size mismatch')
        texture.update(upload_rows=rows,upload_depth=fmt in (250,252,260),
                       upload_linear=fmt not in bc and fmt not in (250,252,260))
        if texture['heap']:
            queries.append(texture)
    query_input = args.output.with_suffix('.query-input.json')
    query_output = args.output.with_suffix('.query-output.json')
    query_input.write_text(json.dumps(dict(heaps=heaps, placements=queries, scope=scope), indent=2))
    subprocess.run([str(args.probe.resolve()), str(query_input.resolve()), str(query_output.resolve())],
                   check=True, timeout=60)
    native = json.loads(query_output.read_text())
    assert native['gpu_commands_submitted'] == 0
    heap_bytes = sum(h['size'] for h in heaps.values())
    standalone_bytes = sum(q['native_size'] for q in native['placements'] if not q['heap'])
    initial_bytes = sum(q['bytes'] for q in initial)
    largest_initial = max((q['bytes'] for q in initial), default=0)
    largest_texture_upload = max((q.get('upload_staging_bytes',0) for q in native['placements']),default=0)
    upload_peak = max(16*1024*1024, largest_texture_upload)
    report = dict(device=native['device'], recommended_max_working_set=native['recommended_max_working_set'],
                  heaps=len(heaps), heap_bytes=heap_bytes, counts=dict(counts),
                  heap_buffer_payload_bytes_not_extra_allocation=heap_buffer_payload,
                  standalone_native_bytes=standalone_bytes,
                  resource_allocation_bytes=heap_bytes+standalone_bytes,
                  retained_initial_cpu_bytes=initial_bytes, largest_initial_bytes=largest_initial,
                  largest_texture_upload_bytes=largest_texture_upload,
                  serial_upload_peak_bytes=upload_peak,
                  resource_plus_serial_upload_bytes=heap_bytes+standalone_bytes+upload_peak,
                  resource_plus_initial_bytes=heap_bytes+standalone_bytes+initial_bytes,
                  captured_snapshot_bytes=snapshot_bytes,
                  conservative_replay_bytes=heap_bytes+standalone_bytes+2*initial_bytes+snapshot_bytes+192*1024*1024,
                  gpu_commands_submitted=0,
                  limitations=['Heap layout queries estimate physical allocation, not measured residency.',
                               'Descriptor shadows, structured metadata and driver overhead require additional accounting.',
                               'All historical frame standalone allocations are conservatively accumulated.',
                               'This audit does not validate source closure or authorize GPU replay.'])
    args.output.write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
