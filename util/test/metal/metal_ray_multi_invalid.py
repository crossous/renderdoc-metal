#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject malformed ordered geometry descriptions and frozen multi-geometry inputs."""
import copy
from pathlib import Path
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from metal_ray_table_invalid import field, run


def main():
    command, capture = map(Path, sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix="metal-ray-multi-invalid-") as tmp:
        directory = Path(tmp)
        original = directory/"original.zip.xml"
        run(command,"convert","-f",capture,"-o",original,"-c","zip.xml")
        tree = ET.parse(original)
        def target(tree):
            return next(c for c in tree.findall("./chunks/chunk") if
                c.get("name") == "MTLAccelerationStructureCommandEncoder::buildMultiIndexed" or
                (c.get("name") == "Internal::Initial Contents" and
                 any(v.get("name") == "kind" and v.text == "8" for v in c)))
        base = target(tree)
        initial = base.get("name") == "Internal::Initial Contents"
        vertex_field = "source" if initial else "vertices"
        index_field = "indexSource" if initial else "indices"
        as_field = "id" if initial else "structure"
        wrong = field(base,as_field).text
        cases=[]
        for name in (vertex_field,index_field):
            for tag,value in (("zero","0"),("unknown","999999999"),("wrong-type",wrong)):
                cases.append((name+"-"+tag,name,value))
        for index,value in ((10,"1"),(11,"0"),(11,"13"),(11,"1048580"),(12,"0"),
                            (12,"31"),(13,"0"),(13,"1000001"),(14,"32"),(15,"2"),
                            (16,"2"),(17,"2"),(18,"1"),(18,"18446744073709551612"),(19,"99")):
            cases.append((f"second-geometry-{index}-{value}",index,value))
        cases += [("partial-geometry","short",None),("too-few-geometries","single",None),
                  ("too-many-geometries","many",None)]
        if initial:
            cases += [("schema-zero","schema","0"),("schema-old","schema","4"),
                      ("schema-future","schema","6"),("kind-mismatch","kind","2"),
                      ("index-vertex-range","payload-index",None),
                      ("vertex-length","payload-vertex",None)]
        else:
            cases += [("scratch-zero","scratch","0"),("scratch-unknown","scratch","999999999"),
                      ("scratch-wrong-type","scratch",wrong),("scratch-align","scratchOffset","1"),
                      ("scratch-range","scratchOffset","18446744073709551360"),
                      ("encoder-wrong-type","Encoder",wrong)]
        with zipfile.ZipFile(str(original)[:-4]) as archive:
            payloads = {name:archive.read(name) for name in archive.namelist()}
        for tag,member,value in cases:
            variant=copy.deepcopy(tree); node=target(variant); changed=dict(payloads)
            parameters=field(node,"parameters")
            if isinstance(member,int): parameters[member].text=value
            elif member=="short": parameters.remove(parameters[-1])
            elif member=="single":
                for child in list(parameters)[10:]: parameters.remove(child)
            elif member=="many":
                for _ in range(63):
                    for child in list(parameters)[:10]: parameters.append(copy.deepcopy(child))
            elif member=="payload-index":
                name=f"{int(field(node,'indices').text):06d}"; data=bytearray(changed[name])
                # The second geometry is the one intersected by the oracle ray.
                index_size=2 if int(parameters[19].text)==0 else 4
                offset=int(parameters[18].text)
                data[offset:offset+index_size]=(65535 if index_size==2 else 0xffffffff).to_bytes(index_size,'little')
                changed[name]=bytes(data)
            elif member=="payload-vertex":
                name=f"{int(field(node,'vertices').text):06d}"; changed[name]=changed[name][:-1]
            else: field(node,member).text=value
            xml=directory/(tag+".zip.xml"); variant.write(xml,encoding="unicode",xml_declaration=True)
            with zipfile.ZipFile(str(xml)[:-4],"w") as archive:
                for name,data in changed.items(): archive.writestr(name,data)
            invalid=directory/(tag+".rdc")
            run(command,"convert","-f",xml,"-o",invalid,"-c","rdc")
            message=run(command,"replay","--loops","1",invalid,success=False)
            if not ("Failed to process Metal chunk" in message or "Failed to replay Metal chunk" in message or
                    "Reading invalid array or byte buffer" in message):
                raise RuntimeError(tag+": missing clean rejection: "+message)
        print(f"PASS {len(cases)} malformed multi-geometry {'initial' if initial else 'frame'} captures")


if __name__=="__main__": main()
