#!/usr/bin/env python3
"""Reject malformed opaque MetalFX work without entering MetalFX on invalid resources/shape."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

def run(*args, success=True):
    result = subprocess.run([str(arg) for arg in args], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=30)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f'{result.returncode}: {args}\n{result.stdout}')
    return result.stdout

def field(node, name):
    return next(child for child in node if child.get('name') == name)

command, capture = map(pathlib.Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='metal-fx-invalid-') as tmp:
    root = pathlib.Path(tmp)
    original = root / 'original.zip.xml'
    run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
    tree = ET.parse(original)
    fx = next(chunk for chunk in tree.findall('./chunks/chunk') if chunk.get('name') == 'MTLCommandBuffer::encodeMetalFXSpatial')
    for name, key, index, value in [
        ('zero-command','CommandBuffer',None,'0'), ('zero-input','colour',None,'0'),
        ('zero-output','output',None,'0'), ('same-texture','output',None,field(fx,'colour').text),
        ('wrong-fence','fence',None,field(fx,'colour').text),
        ('zero-width','parameters',0,'0'), ('oversize','parameters',2,'4294967295'),
        ('downscale','parameters',2,'8'), ('colour-mode','parameters',6,'99'),
        ('content-zero','parameters',7,'0'), ('content-too-large','parameters',8,'17'),
        ('wrong-input-format','parameters',4,'80'), ('wrong-output-format','parameters',5,'80'),
        ('wrong-input-extent','parameters',0,'15'), ('wrong-output-extent','parameters',2,'33'),
        ('missing-parameters','parameters',None,None),
    ]:
        mutated = copy.deepcopy(tree)
        chunk = next(c for c in mutated.findall('./chunks/chunk') if c.get('name') == fx.get('name'))
        node = field(chunk,key)
        if value is None: node.remove(list(node)[-1])
        elif index is not None: list(node)[index].text = value
        else: node.text = value
        path = root / f'{name}.zip.xml'
        mutated.write(path,encoding='utf-8',xml_declaration=True)
        shutil.copyfile(original.with_suffix(''),path.with_suffix(''))
        out = root / f'{name}.rdc'
        run(command,'convert','-f',path,'-o',out,'-c','rdc')
        message=run(command,'replay','--loops','1',out,success=False)
        if 'Couldn\'t load and replay' not in message:
            raise RuntimeError(f'Expected clean replay rejection: {name}\n{message}')
        print(f'PASS reject {name}')
