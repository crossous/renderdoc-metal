#!/usr/bin/env python3
"""Reject malformed compute heap declaration captures using short isolated replays."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30)
    if (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result {result.returncode}: {args}\n{result.stdout}")
    return result.stdout


def named(parent, name):
    return next(item for item in parent if item.get("name") == name)


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    cases = [
        ("single-zero", "MTLComputeCommandEncoder::useHeap", "heap", "0"),
        ("single-wrong-type", "MTLComputeCommandEncoder::useHeap", "heap", "32"),
        ("single-unknown", "MTLComputeCommandEncoder::useHeap", "heap", "999999"),
        ("single-wrong-variant", "MTLComputeCommandEncoder::useHeap", "variant", "true"),
        ("array-zero", "MTLComputeCommandEncoder::useHeaps", "heap", "0"),
        ("array-wrong-type", "MTLComputeCommandEncoder::useHeaps", "heap", "32"),
        ("array-wrong-variant", "MTLComputeCommandEncoder::useHeaps", "variant", "false"),
    ]
    with tempfile.TemporaryDirectory(prefix="metal-ue-compute-heap-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "source.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        if any(c.get("name") == "MTLComputeCommandEncoder::useResources"
               for c in tree.findall("./chunks/chunk")):
            cases.extend([
                ("compute-resource-zero", "MTLComputeCommandEncoder::useResources", "resources", "0"),
                ("compute-resource-wrong-type", "MTLComputeCommandEncoder::useResources", "resources", "15"),
                ("render-resource-unknown", "MTLRenderCommandEncoder::useResources", "resources", "999999"),
                ("compute-single-resource-zero", "MTLComputeCommandEncoder::useResource", "resource", "0"),
            ])
        for tag, chunk_name, field, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall("./chunks/chunk")
                         if item.get("name") == chunk_name)
            if field == "variant":
                node = named(chunk, "arrayVariant")
            elif field == "resources":
                node = named(chunk, "resources")[0]
            elif field == "resource":
                node = named(chunk, "resource")
            else:
                node = named(chunk, "heaps")[0]
            node.text = value
            xml = directory / f"{tag}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{tag}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            output = run(command, "replay", "--loops", "1", invalid, success=False)
            assert "Failed to process Metal chunk" in output or "Failed to replay Metal chunk" in output, (tag, output)
    print(f"UE compute heap malformed captures rejected: {len(cases)}")


if __name__ == "__main__":
    main()
