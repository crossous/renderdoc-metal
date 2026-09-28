#!/usr/bin/env python3
"""Reject malformed nested argument-encoder identities, reflection and members."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f'unexpected result {result.returncode}: {args}\n{result.stdout}')
    return result.stdout


def field(node, name):
    return next(child for child in node if child.get('name') == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t118_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-nested-argument-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't118.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        parent = next(c for c in chunks if c.get('name') ==
                      'MTLArgumentEncoder::newArgumentEncoderForBufferAtIndex')
        parent_id = field(parent, 'ParentEncoder').text
        child_id = field(parent, 'Encoder').text
        outer = next(c for c in chunks if c.get('name') == 'MTLArgumentEncoder::setBuffer')
        inner_id = field(outer, 'buffer').text
        texture = next(c for c in chunks if c.get('name') == 'MTLArgumentEncoder::setTexture')
        texture_id = field(texture, 'texture').text
        selects = [c for c in chunks if c.get('name') == 'MTLArgumentEncoder::setArgumentBuffer']
        assert len(selects) == 2
        edits = [
            ('parent-zero', parent.get('name'), 0, 'ParentEncoder', '0'),
            ('parent-wrong-type', parent.get('name'), 0, 'ParentEncoder', inner_id),
            ('child-zero', parent.get('name'), 0, 'Encoder', '0'),
            ('child-duplicate', parent.get('name'), 0, 'Encoder', parent_id),
            ('nested-index', parent.get('name'), 0, 'index', '1'),
            ('nested-index-huge', parent.get('name'), 0, 'index', '32'),
            ('nested-length', parent.get('name'), 0, 'encodedLength', '8'),
            ('nested-alignment', parent.get('name'), 0, 'alignment', '16'),
            ('nested-unsupported', parent.get('name'), 0, 'supported', 'false'),
            ('outer-buffer-type', outer.get('name'), 0, 'buffer', texture_id),
            ('outer-buffer-offset', outer.get('name'), 0, 'offset', '1'),
            ('outer-buffer-index', outer.get('name'), 0, 'index', '1'),
            ('child-select-offset', selects[0].get('name'), 1, 'offset', '1'),
            ('child-select-type', selects[0].get('name'), 1, 'argumentBuffer', texture_id),
            ('nested-texture-index', texture.get('name'), 0, 'index', '1'),
            ('nested-texture-type', texture.get('name'), 0, 'texture', inner_id),
            ('nested-sampler-index', 'MTLArgumentEncoder::setSamplerState', 0, 'index', '0'),
            ('nested-sampler-type', 'MTLArgumentEncoder::setSamplerState', 0, 'sampler', inner_id),
        ]
        for tag, chunk_name, occurrence, member, value in edits:
            variant = copy.deepcopy(tree)
            candidates = [c for c in variant.findall('./chunks/chunk')
                          if c.get('name') == chunk_name]
            field(candidates[occurrence], member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'T118 nested argument malformed captures rejected without crash: {len(edits)} cases')


if __name__ == '__main__':
    main()
