#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Malformed Temporal operations must fail before native MetalFX encoding."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

def run(*args, success=True):
    result=subprocess.run([str(a) for a in args],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
    if result.returncode<0 or (result.returncode==0)!=success:
        raise RuntimeError(f'{result.returncode}: {args}\n{result.stdout}')
    return result.stdout

def field(node,name):return next(c for c in node if c.get('name')==name)
command,capture=map(pathlib.Path,sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='metal-temporal-invalid-') as tmp:
    root=pathlib.Path(tmp);original=root/'original.zip.xml'
    run(command,'convert','-f',capture,'-o',original,'-c','zip.xml')
    tree=ET.parse(original);name='MTLCommandBuffer::encodeMetalFXTemporal'
    fx=next(c for c in tree.findall('./chunks/chunk') if c.get('name')==name)
    color=list(field(fx,'inputs'))[0].text
    cases=[('zero-command','CommandBuffer',None,'0'),('zero-scaler','scaler',None,'0'),
           ('scaler-aliases-texture','scaler',None,color),('zero-color','inputs',0,'0'),('zero-depth','inputs',1,'0'),('zero-motion','inputs',2,'0'),
           ('zero-reactive','inputs',4,'0'),('zero-output','output',None,'0'),
           ('aliased-output','output',None,color),('wrong-fence','fence',None,color),
           ('zero-width','parameters',0,'0'),('oversize','parameters',2,'4294967295'),
           ('wrong-color-format','parameters',4,'80'),('wrong-depth-format','parameters',5,'80'),
           ('wrong-motion-format','parameters',6,'80'),('wrong-output-format','parameters',7,'80'),
           ('content-zero','parameters',10,'0'),('content-too-large','parameters',11,'65'),
           ('bad-reset','parameters',12,'2'),('bad-reversed','parameters',13,'2'),
           ('bad-reactive','parameters',14,'2'),('wrong-reactive-format','parameters',15,'80'),
           ('bad-preexposure','values',4,'0'),('nan-jitter','values',0,'nan'),
           ('missing-inputs','inputs',None,None),('missing-parameters','parameters',None,None),
           ('missing-values','values',None,None)]
    for case,key,index,value in cases:
        mutated=copy.deepcopy(tree);chunk=next(c for c in mutated.findall('./chunks/chunk') if c.get('name')==name)
        node=field(chunk,key)
        if value is None:node.remove(list(node)[-1])
        elif index is not None:list(node)[index].text=value
        else:node.text=value
        path=root/f'{case}.zip.xml';mutated.write(path,encoding='utf-8',xml_declaration=True)
        shutil.copyfile(original.with_suffix(''),path.with_suffix(''));out=root/f'{case}.rdc'
        run(command,'convert','-f',path,'-o',out,'-c','rdc')
        message=run(command,'replay','--loops','1',out,success=False)
        if "Couldn't load and replay" not in message:raise RuntimeError(f'Not a clean rejection: {case}\n{message}')
        print(f'PASS reject {case}')
