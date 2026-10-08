#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Corrupt every compute creation API in the real six-entry ray capture."""
import copy
from pathlib import Path
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_ray_table_invalid import field, run

command, capture = map(Path, sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix="metal-ray-compile-invalid-") as tmp:
    root = Path(tmp)
    original = root / "original.zip.xml"
    run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
    tree = ET.parse(original)
    creations = [c for c in tree.findall("./chunks/chunk") if
                 c.get("name", "").startswith("MTLDevice::newComputePipelineStateWith")]
    if len(creations) != 6 or len({c.get("id") for c in creations}) != 6:
        raise RuntimeError("capture must include all six API chunks")
    rejected = 0
    for creation in creations:
        cases = [("zero-pipeline", "ComputePipelineState", "0"),
                 ("unknown-function", "computeFunction", "999999999"),
                 ("wrong-function-type", "computeFunction", "ComputePipelineState")]
        if any(n.get("name") == "optionsValue" for n in creation):
            cases.append(("invalid-options", "optionsValue", "16"))
        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            node = next(c for c in variant.findall("./chunks/chunk") if c.get("id") == creation.get("id"))
            target = next(n for n in node.iter() if n.get("name") == member)
            target.text = field(node, "ComputePipelineState").text if value == "ComputePipelineState" else value
            # Use a previously created buffer to test type confusion, rather than
            # this not-yet-created pipeline ID, which would only test missing IDs.
            if tag == "wrong-function-type":
                target.text = next(field(c, "Buffer").text for c in variant.findall("./chunks/chunk")
                                   if c.get("name") == "MTLDevice::newBufferWithBytes")
            path = root / (creation.get("id") + "-" + tag + ".zip.xml")
            variant.write(path, encoding="utf-8", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(path)[:-4])
            rdc = path.with_suffix(".rdc")
            run(command, "convert", "-f", path, "-o", rdc, "-c", "rdc")
            output = run(command, "replay", "--loops", "1", rdc, success=False)
            if "Failed to process Metal chunk " + creation.get("name") not in output:
                raise RuntimeError(f"{creation.get('id')} {tag} was not rejected: {output[-1000:]}")
            rejected += 1
            print("PASS reject", creation.get("id"), tag)
    print("PASS", rejected, "malformed compute creation chunks across six native ray APIs")
