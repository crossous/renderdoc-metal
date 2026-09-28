#!/usr/bin/env python3
"""Reject malformed T36 render dynamic-state captures."""

import copy
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=30)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result: {args}\n{result.stdout}")
    return result.stdout


def chunk(root, name):
    return next(node for node in root.findall("./chunks/chunk") if node.get("name") == name)


def named(node, name):
    return next(element for element in node if element.get("name") == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {sys.argv[0]} renderdoccmd t36_capture.rdc")
    command, capture = map(pathlib.Path, sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix="metal-render-dynamic-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "t36.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        cases = []

        variant = copy.deepcopy(tree)
        named(chunk(variant.getroot(), "MTLRenderCommandEncoder::setViewports"),
              "viewports").clear()
        cases.append(("empty-viewports", variant))

        variant = copy.deepcopy(tree)
        viewports = named(chunk(variant.getroot(), "MTLRenderCommandEncoder::setViewports"),
                          "viewports")
        template = copy.deepcopy(viewports[0])
        for _ in range(16):
            viewports.append(copy.deepcopy(template))
        cases.append(("too-many-viewports", variant))

        variant = copy.deepcopy(tree)
        named(chunk(variant.getroot(), "MTLRenderCommandEncoder::setScissorRects"),
              "scissors").clear()
        cases.append(("empty-scissors", variant))

        for tag, chunk_name, field in [
                ("depth-clip", "MTLRenderCommandEncoder::setDepthClipMode", "depthClipMode"),
                ("triangle-fill", "MTLRenderCommandEncoder::setTriangleFillMode", "fillMode")]:
            variant = copy.deepcopy(tree)
            named(chunk(variant.getroot(), chunk_name), field).text = "2"
            cases.append((tag, variant))

        for tag, chunk_name, field, value in [
                ("visibility-mode", "MTLRenderCommandEncoder::setVisibilityResultMode",
                 "mode", "3"),
                ("visibility-offset", "MTLRenderCommandEncoder::setVisibilityResultMode",
                 "offset", "4"),
                ("visibility-out-of-bounds", "MTLRenderCommandEncoder::setVisibilityResultMode",
                 "offset", "8"),
                ("color-store-action", "MTLRenderCommandEncoder::setColorStoreAction",
                 "storeAction", "4"),
                ("color-attachment", "MTLRenderCommandEncoder::setColorStoreAction",
                 "colorAttachmentIndex", "8"),
                ("color-store-options", "MTLRenderCommandEncoder::setColorStoreActionOptions",
                 "storeActionOptions", "2"),
                ("depth-store-action", "MTLRenderCommandEncoder::setDepthStoreAction",
                 "storeAction", "6"),
                ("stencil-store-options",
                 "MTLRenderCommandEncoder::setStencilStoreActionOptions",
                 "storeActionOptions", "2")]:
            variant = copy.deepcopy(tree)
            named(chunk(variant.getroot(), chunk_name), field).text = value
            cases.append((tag, variant))

        variant = copy.deepcopy(tree)
        buffer = next(node for node in variant.getroot().iter()
                      if node.get("name") == "visibilityResultBuffer")
        buffer.text = "0"
        cases.append(("missing-visibility-buffer", variant))

        variant = copy.deepcopy(tree)
        sequence = variant.getroot().find("./chunks")
        barrier = chunk(variant.getroot(), "MTLRenderCommandEncoder::textureBarrier")
        draw = chunk(variant.getroot(), "MTLRenderCommandEncoder::drawPrimitives")
        sequence.remove(barrier)
        sequence.insert(list(sequence).index(draw) + 1, barrier)
        cases.append(("barrier-after-draw", variant))

        for tag, variant in cases:
            xml = directory / f"t36-{tag}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"t36-{tag}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            message = run(command, "replay", "--loops", "1", invalid, success=False)
            if tag == "barrier-after-draw" and \
                    "Failed to replay Metal chunk MTLRenderCommandEncoder::textureBarrier" not in message:
                raise RuntimeError(f"missing safe barrier rejection: {message}")
            if not re.search(r"failed|invalid|unsupported|missing|Couldn't load", message, re.I):
                raise RuntimeError(f"missing diagnostic: {tag}: {message}")
    print(f"T36 malformed render dynamic-state captures rejected: {len(cases)} cases")


if __name__ == "__main__":
    main()
