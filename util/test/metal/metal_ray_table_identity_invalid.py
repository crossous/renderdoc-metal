#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Typed function-table GPU identity provenance and corruption rejection."""
import copy
import fcntl
import os
from pathlib import Path
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_ray_table_invalid import field, run


def main():
    command, capture = map(Path, sys.argv[1:3])
    name = "MTLFunctionTable::CaptureGPUIdentity"
    with tempfile.TemporaryDirectory(prefix="metal-table-identity-") as tmp:
        directory = Path(tmp)
        original = directory/"original.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        chunks = tree.findall("./chunks/chunk")
        identities = [c for c in chunks if c.get("name") == name]
        values = {}
        for node in identities:
            resource = field(node,"resource").text
            pair = int(field(node,"kind").text), int(field(node,"value").text)
            if pair[0] not in (0,1) or not pair[1] or (resource in values and values[resource] != pair):
                raise RuntimeError("inconsistent table identity")
            values[resource] = pair
        if {p[0] for p in values.values()} != {0,1}:
            raise RuntimeError("missing visible or intersection table identity")
        if any(sum(field(c,"resource").text == r for c in identities) > 2 for r in values):
            raise RuntimeError("table getter metadata was not deduplicated")
        wrong = next(field(c,"Buffer").text for c in chunks if
                     c.get("name") in ("MTLDevice::newBufferWithBytes","MTLDevice::newBufferWithLength"))
        failures, controls = 0, 0
        for kind in (0,1):
            target = next(c for c in identities if int(field(c,"kind").text) == kind)
            other = next(r for r,p in values.items() if p[0] != kind)
            same = [r for r,p in values.items() if p[0] == kind]
            cases = [("zero-resource","resource","0"), ("unknown-resource","resource","999999999"),
                     ("wrong-resource-type","resource",wrong), ("opposite-table","resource",other),
                     ("invalid-kind","kind","2"), ("flipped-kind","kind",str(1-kind)),
                     ("zero-value","value","0"), ("conflict","conflict",None),
                     ("before-birth","before-birth",None), ("identical-duplicate","duplicate",None)]
            if len(same)>1:
                cases.append(("duplicate-ownership","ownership",same[1]))
            for tag, member, value in cases:
                variant = copy.deepcopy(tree)
                parent = variant.find("./chunks")
                node = next(c for c in parent if c.get("name") == name and
                            int(field(c,"kind").text) == kind)
                if member in ("conflict","duplicate"):
                    extra = copy.deepcopy(node)
                    if member == "conflict":
                        field(extra,"value").text = str(int(field(extra,"value").text)+1)
                    parent.insert(list(parent).index(node)+1,extra)
                elif member == "before-birth":
                    parent.remove(node);parent.insert(1,node)
                elif member == "ownership":
                    existing = next(c for c in parent if c.get("name") == name and
                                    field(c,"resource").text == value)
                    field(existing,"value").text = field(node,"value").text
                else:
                    field(node,member).text = value
                xml = directory/(str(kind)+"-"+tag+".zip.xml")
                variant.write(xml,encoding="unicode",xml_declaration=True)
                shutil.copyfile(str(original)[:-4],str(xml)[:-4])
                output = directory/(str(kind)+"-"+tag+".rdc")
                run(command,"convert","-f",xml,"-o",output,"-c","rdc")
                success = member == "duplicate"
                diagnostic = directory/(str(kind)+"-"+tag+".log")
                previous_log = os.environ.get("RENDERDOC_DEBUG_LOG_FILE")
                try:
                    os.environ["RENDERDOC_DEBUG_LOG_FILE"] = str(diagnostic)
                    with diagnostic.open("w") as retained_log:
                        fcntl.flock(retained_log.fileno(),fcntl.LOCK_SH)
                        message = run(command,"replay","--loops","3",output,success=success)
                    message += diagnostic.read_text(errors="replace")
                finally:
                    if previous_log is None:
                        os.environ.pop("RENDERDOC_DEBUG_LOG_FILE",None)
                    else:
                        os.environ["RENDERDOC_DEBUG_LOG_FILE"] = previous_log
                if not success:
                    expected = ("Conflicting Metal function-table GPU identity ownership" if member == "ownership" else
                                "Conflicting Metal function-table GPU identity metadata" if member == "conflict" else
                                "Invalid Metal function-table GPU identity resource/kind")
                    if expected not in message:
                        raise RuntimeError(tag+": missing precise clean rejection: "+message)
                    failures += 1
                else:
                    controls += 1
                print("PASS",kind,tag)
        print(f"PASS {failures} malformed function-table identities, {controls} identical duplicates and getter deduplication")


if __name__ == "__main__":
    main()
