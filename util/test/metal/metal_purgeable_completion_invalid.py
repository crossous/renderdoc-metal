#!/usr/bin/env python3
"""Reject a frame that references a buffer after its terminal Empty."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args):
    result = subprocess.run(args, text=True, capture_output=True, timeout=10)
    if result.returncode:
        raise RuntimeError(f'{args}: {result.returncode}\n{result.stdout}{result.stderr}')


def main():
    if len(sys.argv) != 4:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd api_probe terminal_capture.rdc')
    command, probe, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-terminal-empty-invalid-') as tmp:
        folder = pathlib.Path(tmp)
        xml = folder / 'source.zip.xml'
        run(command, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml')
        tree = ET.parse(xml)
        chunks = tree.getroot().find('chunks')
        originals = list(chunks)
        purges = [c for c in originals if c.get('name') == 'MTLBuffer::setPurgeableState'
                  and c.find("./uint[@name='State']").text == '4']
        if len(purges) != 1:
            raise RuntimeError(f'expected one terminal Empty, found {len(purges)}')
        extra = copy.deepcopy(purges[0])
        extra.find("./uint[@name='State']").text = '2'
        chunks.insert(originals.index(purges[0]) + 1, extra)
        for index, chunk in enumerate(chunks):
            chunk.set('chunkIndex', str(index))
        mutation = folder / 'after-empty.zip.xml'
        tree.write(mutation, encoding='unicode', xml_declaration=True)
        shutil.copyfile(folder / 'source.zip', folder / 'after-empty.zip')
        rdc = folder / 'after-empty.rdc'
        run(command, 'convert', '-f', mutation, '-o', rdc, '-c', 'rdc')
        result = subprocess.run([probe, rdc], text=True, capture_output=True, timeout=10)
        if result.returncode != 4:
            raise RuntimeError(f'expected OpenCapture rejection (4), got '
                               f'{result.returncode}\n{result.stdout}{result.stderr}')
    print('terminal Empty post-reference rejected before GPU replay')


if __name__ == '__main__':
    main()
