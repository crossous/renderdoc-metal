#!/usr/bin/env python3
"""Reject malformed indirect compute arguments and writer bindings."""
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


def child(node, name):
    return next(element for element in node if element.get("name") == name)


def main():
    command, capture = map(pathlib.Path, sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix="metal-compute-indirect-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "capture.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        chunks = tree.findall("./chunks/chunk")
        indirect = next(node for node in chunks if node.get("name") ==
                        "MTLComputeCommandEncoder::dispatchThreadgroups(indirect)")
        writer = next((node for node in chunks if node.get("name") ==
                       "MTLComputeCommandEncoder::setBuffer" and
                       child(node, "index").text == "0"), None)
        cases = [
            ("missing-buffer", "indirectBuffer", None, "999999"),
            ("misaligned-offset", "indirectBufferOffset", None, "17"),
            ("out-of-range-offset", "indirectBufferOffset", None, "36"),
            ("zero-threadgroup", "threadsPerGroup", "width", "0"),
            ("oversize-threadgroup", "threadsPerGroup", "width", "2048"),
        ]
        if writer is not None:
            cases.append(("writer-offset", "writer", "offset", "44"))

        for tag, field, nested, value in cases:
            variant = copy.deepcopy(tree)
            variant_chunks = variant.findall("./chunks/chunk")
            node = next(item for item in variant_chunks if item.get("chunkIndex") ==
                        (writer if field == "writer" else indirect).get("chunkIndex"))
            element = child(node, nested if field == "writer" else field)
            if field != "writer" and nested is not None:
                element = child(element, nested)
            element.text = value
            xml = directory / f"{tag}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{tag}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            message = run(command, "replay", "--loops", "1", invalid, success=False)
            if not re.search(r"failed|invalid|unsupported|missing|Couldn't load", message, re.I):
                raise RuntimeError(f"missing diagnostic: {tag}: {message}")
    print(f"indirect compute invalid captures rejected: {len(cases)} cases")


if __name__ == "__main__":
    main()
