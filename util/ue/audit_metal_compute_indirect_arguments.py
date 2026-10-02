#!/usr/bin/env python3
"""CPU-only correspondence and work audit of captured per-use indirect evidence."""
import argparse
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET


def field(node, name):
    return next(child for child in node if child.get('name') == name)


def audit(path):
    chunks = ET.parse(path).find('./chunks')
    assert chunks is not None
    records, calls, owners, ordinals = {}, [], {}, {}
    header, frame = None, False
    for chunk in chunks:
        name = chunk.get('name', '')
        if chunk.get('id') == '5':
            frame = True
        if name == 'MTLDevice::CaptureComputeIndirectArgumentsCount':
            assert not frame and header is None
            header = int(field(chunk, 'count').text)
            assert 0 <= header <= 1024
        elif name == 'MTLComputeCommandEncoder::CaptureIndirectArguments':
            assert not frame and header is not None
            record = {key: int(field(chunk, key).text) for key in
                      ('command', 'encoder', 'ordinal', 'buffer', 'offset')}
            record['groups'] = [int(child.text) for child in field(chunk, 'groups')]
            key = record['encoder'], record['ordinal']
            assert key not in records and len(record['groups']) == 3
            assert all(record[key] > 0 for key in ('command', 'encoder', 'buffer'))
            assert record['offset'] % 4 == 0 and 0 <= record['ordinal'] < 1024
            records[key] = record
        elif frame and name.startswith('MTLCommandBuffer::computeCommandEncoder'):
            encoder = int(field(chunk, 'ComputeCommandEncoder').text)
            assert encoder not in owners
            owners[encoder] = int(field(chunk, 'CommandBuffer').text)
        elif frame and name.startswith('MTLComputeCommandEncoder::dispatchThreadgroups') and any(
                child.get('name') == 'indirectBuffer' for child in chunk):
            encoder = int(field(chunk, 'ComputeCommandEncoder').text)
            ordinal = ordinals.get(encoder, 0)
            ordinals[encoder] = ordinal + 1
            call = dict(command=owners[encoder], encoder=encoder, ordinal=ordinal,
                        buffer=int(field(chunk, 'indirectBuffer').text),
                        offset=int(field(chunk, 'indirectBufferOffset').text),
                        threads=[int(child.text) for child in field(chunk, 'threadsPerGroup')])
            record = records[encoder, ordinal]
            assert all(record[key] == call[key] for key in ('command', 'buffer', 'offset'))
            call['groups'] = record['groups']
            assert len(call['threads']) == 3 and all(0 < value <= 1024 for value in call['threads'])
            assert all(0 <= value <= 262144 for value in call['groups'])
            call['threads_total'] = math.prod(call['groups']) * math.prod(call['threads'])
            assert call['threads_total'] <= 262144
            calls.append(call)
    assert header is not None and header == len(records) == len(calls)
    total = sum(call['threads_total'] for call in calls)
    assert total <= 8 * 1024 * 1024
    return dict(source=str(Path(path).resolve()), count=header,
                max_threads=max((call['threads_total'] for call in calls), default=0),
                total_threads=total, zero_calls=sum(call['threads_total'] == 0 for call in calls),
                calls=calls)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.xml)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: value for key, value in report.items() if key != 'calls'}))


if __name__ == '__main__':
    main()
