#!/usr/bin/env python3
"""Reject malformed parallel render parent, child, store and counter identities."""
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


def chunk(tree, name):
    return next(item for item in tree.findall("./chunks/chunk") if item.get("name") == name)


def mutate(tree, tag):
    begin = chunk(tree, "MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor")
    child = chunk(tree, "MTLParallelRenderCommandEncoder::renderCommandEncoder")
    end = chunk(tree, "MTLParallelRenderCommandEncoder::endEncoding")
    store = chunk(tree, "MTLParallelRenderCommandEncoder::setColorStoreAction")
    if tag == "parent-zero":
        named(begin, "ParallelRenderCommandEncoder").text = "0"
    elif tag == "parent-wrong-type":
        named(begin, "ParallelRenderCommandEncoder").text = "35"
    elif tag == "child-unknown-parent":
        named(child, "ParallelRenderCommandEncoder").text = "999999"
    elif tag == "child-wrong-type":
        named(child, "RenderCommandEncoder").text = "35"
    elif tag == "end-unknown-parent":
        named(end, "ParallelRenderCommandEncoder").text = "999999"
    elif tag == "store-unknown-parent":
        named(store, "ParallelRenderCommandEncoder").text = "999999"
    elif tag == "store-invalid-action":
        named(store, "storeAction").text = "999"
    elif tag == "counter-id-zero":
        first = named(named(begin, "descriptor"), "sampleBufferAttachments")[0]
        named(first, "sampleBufferId").text = "0"
    elif tag == "counter-index-oob":
        first = named(named(begin, "descriptor"), "sampleBufferAttachments")[0]
        named(first, "endOfFragmentSampleIndex").text = "4"


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    cases = ["parent-zero", "parent-wrong-type", "child-unknown-parent",
             "child-wrong-type", "end-unknown-parent", "store-unknown-parent",
             "store-invalid-action", "counter-id-zero", "counter-index-oob"]
    with tempfile.TemporaryDirectory(prefix="metal-ue-parallel-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "source.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        for tag in cases:
            variant = copy.deepcopy(tree)
            mutate(variant, tag)
            xml = directory / f"{tag}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{tag}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            output = run(command, "replay", "--loops", "1", invalid, success=False)
            assert "Failed to process Metal chunk" in output or "Failed to replay Metal chunk" in output, (tag, output)
    print(f"UE parallel render malformed captures rejected: {len(cases)}")


if __name__ == "__main__":
    main()
