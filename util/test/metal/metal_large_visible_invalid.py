#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject large compute VFT allocation and last-slot provenance violations."""
import copy
from pathlib import Path
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_ray_table_invalid import field, run


def main():
    command, capture = map(Path, sys.argv[1:3])
    create = "MTLComputePipelineState::newVisibleFunctionTableWithDescriptor"
    update = "MTLVisibleFunctionTable::setFunction"
    with tempfile.TemporaryDirectory(prefix="metal-large-visible-") as tmp:
        directory = Path(tmp)
        original = directory/"original.zip.xml"
        run(command,"convert","-f",capture,"-o",original,"-c","zip.xml")
        tree = ET.parse(original)
        def first(tree,name):
            return next(c for c in tree.findall("./chunks/chunk") if c.get("name")==name)
        count = int(field(first(tree,create),"count").text)
        intersection = first(tree,"MTLIntersectionFunctionTable::setFunction")
        wrong_handle = field(intersection,"function").text
        table_id = field(first(tree,create),"Table").text
        cases = [("count-zero",create,"count","0"),
                 ("count-budget",create,"count","65537"),
                 ("count-overflow",create,"count","4294967295"),
                 ("index-end",update,"index",str(count)),
                 ("index-overflow",update,"index","4294967295"),
                 ("handle-unknown",update,"function","999999999"),
                 ("handle-type",update,"function",table_id),
                 ("handle-intersection",update,"function",wrong_handle)]
        for tag,name,member,value in cases:
            variant = copy.deepcopy(tree)
            field(first(variant,name),member).text=value
            xml=directory/(tag+".zip.xml")
            variant.write(xml,encoding="unicode",xml_declaration=True)
            shutil.copyfile(str(original)[:-4],str(xml)[:-4])
            invalid=directory/(tag+".rdc")
            run(command,"convert","-f",xml,"-o",invalid,"-c","rdc")
            message=run(command,"replay","--loops","1",invalid,success=False)
            if not ("Failed to process Metal chunk" in message or
                    "Failed to replay Metal chunk" in message):
                raise RuntimeError(tag+": missing clean rejection: "+message)
        print("PASS 8 malformed large compute VFT captures")


if __name__=="__main__":
    main()
