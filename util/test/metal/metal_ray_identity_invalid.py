#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check AS identity provenance, deduplication and malformed capture rejection."""
import copy
from pathlib import Path
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_ray_table_invalid import field, run


def main():
    command, capture = map(Path, sys.argv[1:3])
    name = "MTLAccelerationStructure::CaptureGPUIdentity"
    with tempfile.TemporaryDirectory(prefix="metal-ray-identity-") as tmp:
        directory = Path(tmp)
        original = directory/"original.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        chunks = tree.findall("./chunks/chunk")
        identities = [c for c in chunks if c.get("name") == name]
        if not identities:
            raise RuntimeError("missing AS identity metadata")
        values = {}
        for node in identities:
            resource = field(node, "resource").text
            value = int(field(node, "value").text)
            if not value or (resource in values and values[resource] != value):
                raise RuntimeError("inconsistent AS identity")
            values[resource] = value
        # Repeated getters emit at most one record plus one active-frame copy.
        if any(sum(field(c, "resource").text == r for c in identities) > 2 for r in values):
            raise RuntimeError("getter metadata was not deduplicated")
        wrong = next(field(c, "Buffer").text for c in chunks if
                     c.get("name") in ("MTLDevice::newBufferWithBytes", "MTLDevice::newBufferWithLength"))
        cases = [("zero-resource", "resource", "0"),
                 ("unknown-resource", "resource", "999999999"),
                 ("wrong-type", "resource", wrong), ("zero-value", "value", "0"),
                 ("conflict", "conflict", None), ("before-birth", "before-birth", None),
                 ("identical-duplicate", "duplicate", None)]
        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            parent = variant.find("./chunks")
            node = next(c for c in parent if c.get("name") == name)
            if member in ("conflict", "duplicate"):
                extra = copy.deepcopy(node)
                if member == "conflict":
                    field(extra, "value").text = str(int(field(extra, "value").text) ^ 1)
                parent.insert(list(parent).index(node)+1, extra)
            elif member == "before-birth":
                parent.remove(node)
                # Preserve DriverInit so this probes AS provenance, rather than
                # the file format's required first chunk.
                parent.insert(1, node)
            else:
                field(node, member).text = value
            xml = directory/(tag+".zip.xml")
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            output = directory/(tag+".rdc")
            run(command, "convert", "-f", xml, "-o", output, "-c", "rdc")
            success = member == "duplicate"
            message = run(command, "replay", "--loops", "3", output, success=success)
            if not success and not ("Failed to process Metal chunk" in message or
                                    "Failed to replay Metal chunk" in message):
                raise RuntimeError(tag+": missing clean rejection: "+message)
            print("PASS", tag)
        print("PASS 6 malformed AS identities, identical duplicate and getter deduplication")


if __name__ == "__main__":
    main()
