#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject malformed sample argument packets, ray bindings and AABB initial recipes."""
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
    with tempfile.TemporaryDirectory(prefix="metal-apple-ray-invalid-") as tmp:
        directory = Path(tmp)
        original = directory / "original.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        parent = tree.find("./chunks")
        with zipfile.ZipFile(str(original)[:-4]) as archive:
            payloads = {name: archive.read(name) for name in archive.namelist()}
        def chunk(name):
            return next(c for c in parent if c.get("name") == name)
        pointer = chunk("MTLArgumentEncoder::setBuffer")
        buffer_id = field(pointer, "buffer").text
        primitive = next(c for c in parent if c.get("name") == "Internal::Initial Contents"
                         and any(n.get("name") == "kind" and n.text in ("1", "2") for n in c))
        cases = []
        def edit(tag, node, value):
            cases.append((tag, node, str(value), None))
        descriptors = field(chunk("MTLDevice::newArgumentEncoderWithArguments"), "descriptors")
        for tag, member, value in [("argument-type",1,1),("argument-overlap",0,1),
                                   ("argument-array-limit",2,33),("argument-access",3,2),
                                   ("argument-alignment",5,16)]:
            edit(tag, descriptors[member], value)
        for tag, member, value in [("pointer-range","offset",2**64-1),
                                   ("pointer-member","index",32),
                                   ("pointer-identity","buffer",99999999)]:
            edit(tag, field(pointer, member), value)
        acceleration = chunk("MTLComputeCommandEncoder::setAccelerationStructure")
        for tag, value in [("ray-null-as",0),("ray-buffer-as",buffer_id),
                           ("ray-primitive-as",field(primitive,"id").text)]:
            edit(tag,field(acceleration,"structure"),value)
        residency = chunk("MTLComputeCommandEncoder::useResource")
        edit("residency-usage",field(residency,"usageValue"),0)
        edit("residency-unknown",field(residency,"resource"),99999999)
        box = next((c for c in parent if c.get("name") == "Internal::Initial Contents"
                    and any(n.get("name") == "kind" and n.text == "3" for n in c)), None)
        if box is not None:
            table_binding = chunk("MTLComputeCommandEncoder::setIntersectionFunctionTable")
            edit("intersection-table-unknown",field(table_binding,"table"),99999999)
            edit("intersection-table-buffer",field(table_binding,"table"),buffer_id)
            parameters = field(box,"parameters")
            for tag, index, value in [("offset",0,2),("stride-small",1,20),
                                       ("stride-large",1,1048577),("format",2,1),
                                       ("count-zero",3,0),("count-large",3,1000001),
                                       ("count-payload",3,int(parameters[3].text)+1),("table",4,32),
                                       ("opaque",5,2),("duplicate",6,2),("usage",7,2)]:
                edit("box-"+tag,parameters[index],value)
            payload = f'{int(field(box,"vertices").text):06d}'
            cases += [("box-nan",None,None,(payload,0,struct.pack("<I",0x7fc00000))),
                      ("box-inverted",None,None,(payload,0,struct.pack("<f",1e20)))]
        for tag, node, value, patch in cases:
            variant = copy.deepcopy(tree)
            if node is not None:
                # ElementTree nodes carry no parent/path; mirror their traversal position.
                index = list(tree.iter()).index(node)
                list(variant.iter())[index].text = value
            changed = dict(payloads)
            if patch:
                name, offset, replacement = patch
                data = bytearray(changed[name]);data[offset:offset+len(replacement)] = replacement
                changed[name] = bytes(data)
            xml = directory / (tag+".zip.xml")
            variant.write(xml,encoding="unicode",xml_declaration=True)
            with zipfile.ZipFile(str(xml)[:-4],"w") as archive:
                for name,data in changed.items():archive.writestr(name,data)
            invalid = directory / (tag+".rdc")
            run(command,"convert","-f",xml,"-o",invalid,"-c","rdc")
            message=run(command,"replay","--loops","1",invalid,success=False)
            assert ("Failed to process Metal chunk" in message or
                    "Failed to replay Metal chunk" in message or
                    "Invalid Metal initial CPU buffer data" in message), (tag,message)
            print("PASS rejected "+tag,flush=True)
        print(f"PASS {len(cases)} malformed official ray captures")


if __name__ == "__main__":
    main()
