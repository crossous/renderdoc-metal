#!/usr/bin/env python3
"""Create a separate coverage65 replay candidate only after strict pre-submit validation.

The original capture is immutable. All original chunks, binary payloads and the
capture thumbnail must survive the XML/ZIP round trip. This does not claim that
GPU replay or image comparison has passed; those are separate validation steps.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def canonical(node, chunk=False):
    attrs = dict(node.attrib)
    if chunk:
        for key in ('chunkIndex', 'length'):
            attrs.pop(key, None)
    text = node.text or ''
    if len(node) and not text.strip():
        text = ''
    return (node.tag, sorted(attrs.items()), text,
            [canonical(child) for child in node])


def chunk_hash(node):
    return hashlib.sha256(json.dumps(canonical(node, True), ensure_ascii=False).encode()).hexdigest()


def zip_hashes(path):
    result = {}
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(set(names)) != len(names):
            raise RuntimeError('Duplicate ZIP member names')
        for name in names:
            digest = hashlib.sha256()
            with archive.open(name) as source:
                for block in iter(lambda: source.read(1024 * 1024), b''):
                    digest.update(block)
            result[name] = digest.hexdigest()
    return result


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--audit-dir', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, default=root / 'build-macos-debug')
    args = parser.parse_args()
    source, output, audit, build = [p.resolve() for p in
                                 (args.capture, args.output, args.audit_dir, args.build_dir)]
    if output == source or output.exists() or audit.exists():
        parser.error('Output and audit directory must be new; original capture cannot be overwritten')
    audit.mkdir(parents=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    library = build / 'lib/librenderdoc.dylib'
    original_sha, library_sha = sha(source), sha(library)
    cmd = build / 'bin/renderdoccmd'
    manifest = {'original': str(source), 'original_sha256': original_sha,
                'library_sha256': library_sha, 'output': str(output),
                'gpu_replay_validated': False, 'accepted': False}
    try:
        # Existing strict diagnostic refuses malformed provenance/order/lifetime
        # before initial GPU uploads and before any captured GPU submission.
        subprocess.run([sys.executable, str(root / 'util/ue/run_ue_metal_replay_diagnostic.py'),
                        '--capture', str(source), '--mode', 'pre-submit',
                        '--build-dir', str(build), '--log', str(audit / 'pre-submit.log')], check=True)
        env = os.environ.copy()
        for key in tuple(env):
            if key.startswith('RENDERDOC_METAL_'):
                env.pop(key)
        def convert(src, dst, fmt, log):
            with (audit / log).open('w') as stream:
                subprocess.run([str(cmd), 'convert', '-f', str(src), '-o', str(dst),
                                '-c', fmt], env=env, stdout=stream,
                               stderr=subprocess.STDOUT, check=True, timeout=240)
        original_xml = audit / 'original.zip.xml'
        convert(source, original_xml, 'zip.xml', 'export.log')
        tree = ET.parse(original_xml)
        chunks = tree.getroot().find('chunks')
        if chunks is None or not len(chunks):
            raise RuntimeError('Missing original chunk stream')
        if any(c.get('id') == '1399' for c in chunks):
            raise RuntimeError('Original already declares coverage; no candidate promotion needed')
        original_chunks = [chunk_hash(c) for c in chunks]
        original_header = canonical(tree.getroot().find('header'))
        binaries = zip_hashes(audit / 'original.zip')
        declaration = ET.Element('chunk', id='1399', name='MTLDevice::DeclareDescriptorCoverage',
                                 length='4', duration='0')
        ET.SubElement(declaration, 'uint', name='version', typename='uint32_t',
                      width='4', important='true').text = '65'
        # Declaration is device-level metadata. Put it after device creation,
        # before background resource metadata, as in native explicit fixtures.
        position = next(i + 1 for i, c in enumerate(chunks) if c.get('id') == '1000')
        chunks.insert(position, declaration)
        staged_xml = audit / 'candidate.zip.xml'
        tree.write(staged_xml, encoding='utf-8', xml_declaration=True)
        os.link(audit / 'original.zip', audit / 'candidate.zip')
        staged = audit / 'candidate.rdc'
        convert(staged_xml, staged, 'rdc', 'import.log')
        exported = audit / 'roundtrip.zip.xml'
        convert(staged, exported, 'zip.xml', 'roundtrip.log')
        replay_tree = ET.parse(exported)
        replay_chunks = list(replay_tree.getroot().find('chunks'))
        added = [c for c in replay_chunks if c.get('id') == '1399']
        if len(added) != 1 or chunk_hash(added[0]) != chunk_hash(declaration):
            raise RuntimeError('Coverage declaration changed during conversion')
        if [chunk_hash(c) for c in replay_chunks if c.get('id') != '1399'] != original_chunks:
            raise RuntimeError('Original command or metadata fields changed during conversion')
        if canonical(replay_tree.getroot().find('header')) != original_header:
            raise RuntimeError('Original capture header changed during conversion')
        if zip_hashes(audit / 'roundtrip.zip') != binaries:
            raise RuntimeError('Original binary payload or thumbnail changed during conversion')
        if sha(source) != original_sha or sha(library) != library_sha:
            raise RuntimeError('Original capture or library changed during validation')
        # Publish only the audited copy. Do not replace an existing output.
        os.link(staged, output)
        manifest.update(accepted=True, candidate_sha256=sha(output),
                        original_chunks=len(original_chunks), candidate_chunks=len(replay_chunks),
                        binary_members=len(binaries), preserved_binary_payloads_and_thumbnail=True,
                        preserved_commands_and_metadata=True,
                        pre_submit_log=str(audit / 'pre-submit.log'))
        print(json.dumps(manifest, indent=2))
        return 0
    finally:
        (audit / 'candidate-audit.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    sys.exit(main())
