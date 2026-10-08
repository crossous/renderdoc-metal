#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Unknown compute function-table IDs must not become legal nil unbindings."""
import copy
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*command, success=True):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=45)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f'exit {result.returncode}: {command}\n{result.stdout}')
    return result.stdout


def field(node, name):
    return next(child for child in node if child.get('name') == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd capture.rdc')
    command, capture = map(Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-compute-table-binding-') as temporary:
        directory = Path(temporary)
        original = directory/'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        supported = ['MTLComputeCommandEncoder::set'+name for name in (
            'VisibleFunctionTable', 'VisibleFunctionTables',
            'IntersectionFunctionTable', 'IntersectionFunctionTables')]
        selected = [(i, chunk) for i, chunk in enumerate(chunks)
                    if chunk.get('name') in supported and any(
                        member.text != '0' for member in (
                            list(field(chunk, 'tables')) if chunk.get('name').endswith('Tables')
                            else [field(chunk, 'table')]))]
        if not selected:
            raise RuntimeError('Capture has no non-null compute table binding')
        index, binding = selected[0]
        array = binding.get('name').endswith('Tables')
        wrong = field(binding, 'ComputeCommandEncoder').text

        def table(node):
            return field(node, 'tables')[0] if array else field(node, 'table')

        def replay(variant, tag, success, loops):
            xml = directory/(tag+'.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory/(tag+'.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            output = run(command, 'replay', '--loops', str(loops), rdc, success=success)
            if not success and not ('Failed to process Metal chunk' in output or
                                    'Failed to replay Metal chunk' in output or
                    'Missing or wrong-type Metal ray binding object' in output):
                raise RuntimeError(tag+': missing clean Metal rejection: '+output)

        # Explicit nil is legal before a restored binding. Preserve dispatch's valid table.
        valid = copy.deepcopy(tree)
        parent = valid.find('./chunks')
        clear = copy.deepcopy(parent[index])
        if array:
            for member in field(clear, 'tables'): member.text = '0'
        else:
            field(clear, 'table').text = '0'
        parent.insert(index, clear)
        replay(valid, 'nil-clear-restore', True, 3)
        for tag, value in [('unknown', '999999999'), ('wrong-type', wrong)]:
            variant = copy.deepcopy(tree)
            table(variant.find('./chunks')[index]).text = value
            replay(variant, tag, False, 1)
        print(f'{capture.stem}: {binding.get("name")} nil clear/restore 3 loops PASS; 2 malformed IDs rejected')


if __name__ == '__main__':
    main()
