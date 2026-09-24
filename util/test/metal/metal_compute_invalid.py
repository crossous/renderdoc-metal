#!/usr/bin/env python3
"""Reject malformed thread grids and compute buffer bindings."""

import argparse
import copy
import pathlib
import re
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result: {args}\n{result.stdout}")
    return result.stdout


def check(command, capture, tag, cases, directory):
    original = directory / f"{tag}.zip.xml"
    run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
    tree = ET.parse(original)
    for name, edits in cases:
        variant = copy.deepcopy(tree)
        for chunk_name, occurrence, field, value, *field_occurrence in edits:
            chunks = [chunk for chunk in variant.findall("./chunks/chunk")
                      if chunk.get("name") == chunk_name]
            item = [child for child in chunks[occurrence].iter()
                    if child.get("name") == field][field_occurrence[0] if field_occurrence else 0]
            item.text = str(value)
        xml = directory / f"{tag}-{name}.zip.xml"
        variant.write(xml, encoding="unicode", xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(xml)[:-4])
        invalid = directory / f"{tag}-{name}.rdc"
        run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
        message = run(command, "replay", "--loops", "1", invalid, success=False)
        if not re.search(r"failed|invalid|unsupported|missing|Couldn't load", message, re.I):
            raise RuntimeError(f"missing diagnostic: {name}: {message}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("renderdoccmd", type=pathlib.Path)
    parser.add_argument("t28_capture", type=pathlib.Path)
    parser.add_argument("t29_capture", type=pathlib.Path)
    args = parser.parse_args()
    dispatch = "MTLComputeCommandEncoder::dispatchThreads"
    texture = "MTLComputeCommandEncoder::setTexture"
    bind = "MTLComputeCommandEncoder::setBuffer"
    groups = "MTLComputeCommandEncoder::dispatchThreadgroups"
    t28 = [
        ("zero-grid", [(dispatch, 0, "width", 0)]),
        ("past-texture-grid", [(dispatch, 0, "width", 11)]),
        ("zero-group", [(dispatch, 0, "height", 0, 1)]),
        ("oversized-group", [(dispatch, 0, "width", 1025, 1)]),
        ("missing-source", [(texture, 0, "texture", 999999)]),
    ]
    t29 = [
        ("invalid-slot", [(bind, 0, "index", 31)]),
        ("missing-input", [(bind, 0, "buffer", 999999)]),
        ("past-input-offset", [(bind, 0, "offset", 304)]),
        ("short-output-range", [(bind, 1, "offset", 81)]),
        ("incomplete-grid", [(groups, 0, "width", 1)]),
    ]
    with tempfile.TemporaryDirectory(prefix="metal-compute-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        check(args.renderdoccmd, args.t28_capture, "t28", t28, directory)
        check(args.renderdoccmd, args.t29_capture, "t29", t29, directory)
    print(f"T28/T29 invalid compute captures rejected: {len(t28) + len(t29)} cases")


if __name__ == "__main__":
    main()
