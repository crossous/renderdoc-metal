#!/usr/bin/env python3
"""T37: malformed linear and ranged texture copies must fail cleanly before native encoding."""

import copy
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result {result.returncode}: {args}\n{result.stdout}")
    return result.stdout


def named(node, name):
    return next(child for child in node if child.get("name") == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {sys.argv[0]} renderdoccmd t37_capture.rdc")
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="metal-blit-transfer-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "t37.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        cases = []

        def change(tag, chunk_name, field, value):
            variant = copy.deepcopy(tree)
            display, discriminator = {
                "copyFromBuffer_toTexture_options": ("copyFromBuffer", "destinationTexture"),
                "copyFromTexture_toBuffer_options": ("copyFromTexture", "destinationBuffer"),
                "copyFromTexture_toTexture_slice_level_count": ("copyFromTexture", "sliceCount"),
                "copyFromTexture_toTexture": ("copyFromTexture_toTexture", "sourceTexture"),
            }[chunk_name]
            node = next(node for node in variant.findall("./chunks/chunk")
                        if node.get("name") == "MTLBlitCommandEncoder::" + display
                        and any(child.get("name") == discriminator for child in node))
            for part in field.split("."):
                node = named(node, part)
            node.text = str(value)
            cases.append((tag, variant))
            return node, variant

        for direction, name, offset, row, texture_slice, level, buffer, texture in [
                ("upload", "copyFromBuffer_toTexture_options", "sourceOffset",
                 "sourceBytesPerRow", "destinationSlice", "destinationLevel",
                 "sourceBuffer", "destinationTexture"),
                ("readback", "copyFromTexture_toBuffer_options", "destinationOffset",
                 "destinationBytesPerRow", "sourceSlice", "sourceLevel",
                 "destinationBuffer", "sourceTexture")]:
            for tag, field, value in [
                    ("unaligned-offset", offset, 1), ("offset-overflow", offset, 2**64 - 1),
                    ("short-buffer", offset, 4092), ("zero-pitch", row, 0),
                    ("short-pitch", row, 8), ("unaligned-pitch", row, 13),
                    ("pitch-overflow", row, 2**64 - 4), ("slice", texture_slice, 2),
                    ("mip", level, 2), ("empty-width", "sourceSize.width", 0),
                    ("region-overflow", "sourceSize.height", 2**64 - 1),
                    ("missing-buffer", buffer, 0), ("missing-texture", texture, 0),
                    ("unsupported-option", "options", 1)]:
                change(f"{direction}-{tag}", name, field, value)

        ranged = "copyFromTexture_toTexture_slice_level_count"
        for field, value in [("sliceCount", 0), ("levelCount", 0),
                             ("sliceCount", 2**64 - 1), ("levelCount", 2**64 - 1),
                             ("sourceSlice", 2), ("destinationSlice", 2),
                             ("sourceLevel", 2), ("destinationLevel", 2),
                             ("destinationLevel", 0), ("sourceTexture", 0),
                             ("destinationTexture", 0)]:
            change(f"range-{field}-{value}", ranged, field, value)
        for field in ("sourceTexture", "destinationTexture"):
            change(f"whole-{field}", "copyFromTexture_toTexture", field, 0)

        whole = next(node for node in tree.findall("./chunks/chunk")
                     if node.get("name") == "MTLBlitCommandEncoder::copyFromTexture_toTexture")
        change("whole-self-copy", "copyFromTexture_toTexture", "destinationTexture",
               named(whole, "sourceTexture").text)
        _, variant = change("range-self-overlap", ranged, "sourceSlice", 0)
        node = next(node for node in variant.findall("./chunks/chunk")
                    if any(child.get("name") == "sliceCount" for child in node))
        named(node, "destinationTexture").text = named(node, "sourceTexture").text

        for tag, variant in cases:
            xml = directory / f"{tag}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{tag}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            message = run(command, "replay", "--loops", "1", invalid, success=False)
            if not re.search(r"failed|invalid|unsupported|missing|Couldn't load", message, re.I):
                raise RuntimeError(f"missing diagnostic: {tag}: {message}")
    print(f"T37 malformed blit captures rejected without crash: {len(cases)} cases")


if __name__ == "__main__":
    main()
