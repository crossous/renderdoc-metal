#!/usr/bin/env python3
"""Reject malformed compute batch bindings while preserving valid null entries."""
import copy
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result: {args}\n{result.stdout}")
    return result.stdout


def main():
    command, capture = map(pathlib.Path, sys.argv[1:3])
    cases = [
        ("texture-range", "setTextures", "range", "location", 128),
        ("texture-missing", "setTextures", "textures", 0, 999999),
        ("texture-null-mismatch", "setTextures", "bound", 1, 1),
        ("sampler-range", "setSamplerStates", "range", "location", 16),
        ("sampler-missing", "setSamplerStates", "samplers", 0, 999999),
        ("buffer-range", "setBuffers", "range", "location", 31),
        ("buffer-missing", "setBuffers", "buffers", 0, 999999),
        ("buffer-offset", "setBuffers", "offsets", 0, 304),
        ("buffer-null-offset", "setBuffers", "offsets", 1, 1),
    ]
    with tempfile.TemporaryDirectory(prefix="metal-compute-batch-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "t31.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        for tag, method, parent, child, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(node for node in variant.findall("./chunks/chunk")
                         if node.get("name") == f"MTLComputeCommandEncoder::{method}")
            element = next(node for node in chunk if node.get("name") == parent)
            if isinstance(child, int):
                element = element[child]
            else:
                element = next(node for node in element if node.get("name") == child)
            element.text = str(value)
            xml = directory / f"t31-{tag}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"t31-{tag}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            message = run(command, "replay", "--loops", "1", invalid, success=False)
            if not re.search(r"failed|invalid|unsupported|missing|Couldn't load", message, re.I):
                raise RuntimeError(f"missing diagnostic: {tag}: {message}")
    print(f"T31 invalid compute batch captures rejected: {len(cases)} cases")


if __name__ == "__main__":
    main()
