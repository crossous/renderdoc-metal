#!/usr/bin/env python3
"""Extract exact captured graphics pipeline compiler inputs; CPU only, no replay."""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
import zipfile


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('xml', type=Path)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--pipelines', type=int, nargs='+', required=True)
    args = ap.parse_args()
    chunks = list(ET.parse(args.xml).find('./chunks'))
    def field(n, k): return next(x for x in n if x.get('name') == k)
    def value(n, k): return field(n, k).text
    def decode(n):
        if n.tag == 'array': return [decode(x) for x in n]
        if n.tag == 'struct': return {x.get('name'): decode(x) for x in n}
        if n.tag in ('uint', 'int', 'enum', 'ResourceId'): return int(n.text)
        if n.tag == 'bool': return n.text == 'true'
        if n.tag == 'float': return float(n.text)
        return n.text or ''
    pipelines = []
    functions = {}
    libraries = {}
    args.output.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.xml.with_suffix('')) as archive:
        def add_function(id):
            if not id or str(id) in functions: return
            c = next(c for c in chunks if c.get('name', '').startswith('MTLLibrary::newFunction') and
                     any(x.get('name') == 'Function' and x.text == str(id) for x in c))
            lib = int(value(c, 'Library'))
            name = value(c, 'FunctionName' if c.get('name') == 'MTLLibrary::newFunctionWithName' else 'functionName')
            for key in ('constantNames', 'constantIndices', 'constantTypes', 'constantValues'):
                entries = [x for x in c if x.get('name') == key]
                assert not entries or not list(entries[0]), (id, key, 'specialization not supported by this probe')
            assert not any(x.get('name') == 'supported' and x.text != 'true' for x in c)
            options = next((int(x.text) for x in c if x.get('name') == 'options'), 0)
            functions[str(id)] = {'library': lib, 'name': name, 'options': options}
            if str(lib) not in libraries:
                lc = next(c for c in chunks if c.get('name') == 'MTLDevice::newLibraryWithData' and value(c, 'Library') == str(lib))
                data = archive.read(f'{int(value(lc, "data")):06d}')
                assert data[:4] == b'MTLB'
                path = f'library-{lib}.metallib'
                (args.output / path).write_bytes(data)
                libraries[str(lib)] = {'path': path, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        for id in args.pipelines:
            c = next(c for c in chunks if c.get('name', '').startswith('MTLDevice::newRenderPipelineState') and
                     any(x.get('name') == 'RenderPipelineState' and x.text == str(id) for x in c))
            d = decode(field(c, 'descriptor'))
            assert not d['vertexDescriptor']['attributes'] and not d['vertexDescriptor']['layouts']
            assert not d['binaryArchives'] and not d['vertexPreloadedLibraries'] and not d['fragmentPreloadedLibraries']
            for stage in ('vertex', 'fragment'):
                add_function(d[stage + 'Function'])
                linked = d[stage + 'LinkedFunctions']
                assert not linked['groups'] and not linked['binaryFunctions'] and not linked['privateFunctions']
                for function in linked['functions']: add_function(function)
            pipelines.append({'id': id, 'descriptor': d, 'options': int(value(c, 'optionsValue'))})
    result = {'capture_xml': str(args.xml.resolve()), 'mode': 'compiler inputs only; no GPU submission or full replay proof',
              'pipelines': pipelines, 'functions': functions, 'libraries': libraries}
    (args.output / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    print(f'Extracted {len(pipelines)} exact pipelines, {len(functions)} functions, {len(libraries)} libraries; CPU only')

if __name__ == '__main__': main()
