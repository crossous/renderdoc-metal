#!/usr/bin/env python3
"""CPU provenance of direct UE index reads; does not submit GPU work or approve replay."""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile


def analyse(path):
    chunks = list(ET.parse(path).find('./chunks'))
    f = lambda c: {n.get('name'): n.text for n in c}
    scope = next(i for i, c in enumerate(chunks) if c.get('id') == '5')
    creations = {f(c)['Buffer']: (i, c) for i, c in enumerate(chunks)
                 if 'newBuffer' in c.get('name', '') and 'Buffer' in f(c)}
    initials = {f(c)['id']: c for c in chunks if c.get('name') == 'Internal::Initial Contents'}
    draws = [(i, c) for i, c in enumerate(chunks) if i > scope and
             c.get('name') == 'MTLRenderCommandEncoder::drawIndexedPrimitives' and
             'indirectBuffer' not in f(c)]
    indices = {f(c)['indexBuffer'] for _, c in draws}
    future = {r for r in indices if creations[r][0] > scope}
    owners, pending, encoder_commands = {}, [], {}
    with zipfile.ZipFile(path.with_suffix('')) as archive:
        def blob(c, name):
            node = next(n for n in c if n.get('name') == name)
            return archive.read(f'{int(node.text):06d}')
        for i, c in enumerate(chunks[scope+1:], scope+1):
            d, name = f(c), c.get('name')
            if name == 'Internal_MTLBufferModifyCPUContents':
                data = blob(c, 'data')
                assert len(data) == int(d['size']), (i, 'snapshot-size')
                pending.append((d['Buffer'], int(d['start']), data))
            elif name == 'MTLCommandBuffer::commit':
                owners[d['CommandBuffer']] = pending
                pending = []
            elif 'CommandBuffer' in d:
                for key in ('ComputeCommandEncoder', 'RenderCommandEncoder', 'BlitCommandEncoder', 'ParallelRenderCommandEncoder'):
                    if key in d: encoder_commands[d[key]] = d['CommandBuffer']
            elif name == 'MTLParallelRenderCommandEncoder::renderCommandEncoder':
                encoder_commands[d['RenderCommandEncoder']] = encoder_commands[d['ParallelRenderCommandEncoder']]
        issues, copied, reads, committed, baseline = [], {}, [], [], {}
        def original(resource):
            if resource not in baseline:
                _, creation = creations[resource]
                d = f(creation)
                if resource in initials: baseline[resource] = blob(initials[resource], 'Contents')
                elif creation.get('name') == 'MTLDevice::newBufferWithBytes': baseline[resource] = blob(creation, 'initialData')
                else: baseline[resource] = b''
            return baseline[resource]
        def source_range(resource, command, offset, size):
            base = original(resource)
            data, known = bytearray(size), bytearray(size)
            if offset <= len(base) and size <= len(base)-offset:
                data[:] = base[offset:offset+size]; known[:] = b'\1'*size
            for owner in committed+[command]:
                for r, start, update in owners.get(owner, []):
                    if r != resource: continue
                    begin, end = max(offset, start), min(offset+size, start+len(update))
                    if begin < end:
                        data[begin-offset:end-offset] = update[begin-start:end-start]
                        known[begin-offset:end-offset] = b'\1'*(end-begin)
            return data, known
        command_ops = {}
        submitted_ops = []
        for i, c in enumerate(chunks[scope+1:], scope+1):
            d, name = f(c), c.get('name')
            if name == 'MTLCommandBuffer::commit':
                submitted_ops.extend(command_ops.get(d['CommandBuffer'], []))
                submitted_ops.append((i, c))
            elif name == 'MTLBlitCommandEncoder::copyFromBuffer' and d.get('destinationBuffer') in future:
                command_ops.setdefault(encoder_commands[d['BlitCommandEncoder']], []).append((i, c))
            elif name == 'MTLRenderCommandEncoder::drawIndexedPrimitives' and 'indirectBuffer' not in d:
                command_ops.setdefault(encoder_commands[d['RenderCommandEncoder']], []).append((i, c))
        for i, c in submitted_ops:
            d, name = f(c), c.get('name')
            if name == 'MTLCommandBuffer::commit': committed.append(d['CommandBuffer'])
            elif name == 'MTLBlitCommandEncoder::copyFromBuffer' and d.get('destinationBuffer') in future:
                target, source = d['destinationBuffer'], d['sourceBuffer']
                command = encoder_commands[d['BlitCommandEncoder']]
                size, off, src = int(d['size']), int(d['destinationOffset']), int(d['sourceOffset'])
                length = int(f(creations[target][1])['length'])
                assert off <= length and size <= length-off and size <= 65536
                data, known = source_range(source, command, src, size)
                if not all(known): issues.append({'chunk': i, 'kind': 'unknown-upload-source', 'source': source})
                if target not in copied: copied[target] = (bytearray(length), bytearray(length), set())
                content, mask, producers = copied[target]
                content[off:off+size] = data; mask[off:off+size] = known; producers.add(command)
            elif name == 'MTLRenderCommandEncoder::drawIndexedPrimitives' and 'indirectBuffer' not in d:
                target = d['indexBuffer']; count, off = int(d['indexCount']), int(d['indexBufferOffset'])
                stride = 2 if d['indexType'] == '0' else 4
                command = encoder_commands[d['RenderCommandEncoder']]
                if target in future: content, mask, producers = copied.get(target, (b'', b'', set()))
                else: content = original(target); mask = b'\1'*len(content); producers = set()
                known = off <= len(content) and count*stride <= len(content)-off and all(mask[off:off+count*stride])
                ordered = all(p == command or p in committed for p in producers)
                if not known or not ordered: issues.append({'chunk': i, 'kind': 'unknown-or-unsubmitted-index-read', 'buffer': target})
                values = [v[0]+int(d.get('baseVertex', 0)) for v in struct.iter_unpack('<H' if stride == 2 else '<I', content[off:off+count*stride])] if known else []
                bounds = bool(values) and min(values) >= 0 and max(values) < 65535
                if not bounds: issues.append({'chunk': i, 'kind': 'effective-index-bound', 'buffer': target})
                reads.append({'chunk': i, 'buffer': target, 'frame_buffer': target in future, 'count': count,
                              'known_bytes': known, 'submitted_or_same_command': ordered,
                              'min_effective': min(values) if values else None, 'max_effective': max(values) if values else None})
    return {'gpu_commands_submitted': False, 'complete_replay_validated': False,
            'direct_indexed_draws': len(reads), 'frame_indexed_draws': sum(r['frame_buffer'] for r in reads),
            'frame_index_buffers': sorted(future, key=int), 'reads_per_buffer': dict(Counter(r['buffer'] for r in reads)),
            'issues': issues, 'reads': reads,
            'limitations': ['CPU byte provenance and queue ownership only; Native residency/completion and shader memory safety are not proven.',
                            'Indirect indexed draws are excluded; shader writes, alias transitions and source layouts require driver preflight.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml', type=Path); parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(); result = analyse(args.xml)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: result[k] for k in ('gpu_commands_submitted','direct_indexed_draws','frame_indexed_draws','frame_index_buffers','issues')}, indent=2))


if __name__ == '__main__': main()
