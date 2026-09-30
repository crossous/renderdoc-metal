#!/usr/bin/env python3
"""Make a diagnostic RDC with frame-created buffer views after their parents.

The original capture is never changed. This only repairs the specific old capture
ordering defect; it does not make unsupported GPU features work.
"""
import argparse
import collections
import pathlib
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def command(*args):
    subprocess.run(args, check=True, timeout=30)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('renderdoccmd', type=pathlib.Path)
    parser.add_argument('capture', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    args = parser.parse_args()
    source = args.capture.resolve(strict=True)
    target = args.output.resolve()
    if source == target or target.exists():
        parser.error('output must be a new path, separate from the original capture')
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ue-metal-view-order-') as scratch:
        folder = pathlib.Path(scratch)
        original = folder / 'original.zip.xml'
        command(args.renderdoccmd, 'convert', '-f', source, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunk_list = tree.getroot().find('chunks')
        chunks = list(chunk_list)
        parent_position = {}
        frame_start = next(i for i, c in enumerate(chunks)
                           if c.get('name') == 'Internal::Beginning of Capture')
        for position, chunk in enumerate(chunks):
            name = chunk.get('name', '')
            if name.startswith(('MTLDevice::newBuffer', 'MTLHeap::newBuffer')):
                field = chunk.find("./ResourceId[@name='Buffer']")
                if field is not None:
                    parent_position[field.text] = position
        moves = collections.defaultdict(list)
        misplaced = []
        for position, chunk in enumerate(chunks):
            if chunk.get('name') != 'MTLBuffer::newTextureWithDescriptor':
                continue
            parent = chunk.find("./ResourceId[@name='Buffer']")
            view = chunk.find("./ResourceId[@name='Texture']")
            if parent is None or view is None:
                raise RuntimeError('malformed buffer texture view')
            parent_at = parent_position.get(parent.text)
            if parent_at is None:
                raise RuntimeError(f'buffer view {view.text} has no parent creation')
            if parent_at > position:
                if position >= frame_start or parent_at <= frame_start:
                    raise RuntimeError(f'unsupported view move: {view.text}')
                moves[parent_at].append((position, chunk))
                misplaced.append((position, parent_at, view.text, parent.text))
        if not misplaced:
            raise RuntimeError('no pre-frame views with frame-created parents found')
        if len(misplaced) != 15:
            raise RuntimeError(f'expected 15 known UE views, found {len(misplaced)}')

        # A misplaced view must have no other pre-parent use. Keep every unrelated chunk
        # in order, including the parent's allocation and any frame GPU work.
        for position, parent_at, view, _ in misplaced:
            for i in range(position + 1, parent_at):
                if any(field.text == view for field in chunks[i].iter('ResourceId')):
                    raise RuntimeError(f'view {view} is referenced before parent creation')
        remove = {position for position, _, _, _ in misplaced}
        ordered = []
        for position, chunk in enumerate(chunks):
            if position not in remove:
                ordered.append(chunk)
            ordered.extend(view for _, view in moves.get(position, []))
        if len(ordered) != len(chunks):
            raise RuntimeError('chunk count changed during reorder')
        chunk_list[:] = ordered
        for index, chunk in enumerate(ordered):
            chunk.set('chunkIndex', str(index))
        patched = folder / 'patched.zip.xml'
        tree.write(patched, encoding='unicode', xml_declaration=True)
        shutil.copyfile(folder / 'original.zip', folder / 'patched.zip')
        command(args.renderdoccmd, 'convert', '-f', patched, '-o', target, '-c', 'rdc')
        print(f'created diagnostic capture: {target} moved_views={len(misplaced)} '
              f'chunks={len(ordered)}')


if __name__ == '__main__':
    main()
