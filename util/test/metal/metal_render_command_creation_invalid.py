#!/usr/bin/env python3
"""Reject malformed T34 render bindings and T35 command-creation parameters."""

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


def child(node, name):
    return next(element for element in node if element.get("name") == name)


def set_named(node, name, value):
    child(node, name).text = str(value)


def make_render_cases(tree):
    cases = []

    def add(tag, method, mutate):
        variant = copy.deepcopy(tree)
        mutate(chunk(variant.getroot(), f"MTLRenderCommandEncoder::{method}"))
        cases.append((tag, variant))

    add("vertex-range", "setVertexBuffers",
        lambda node: set_named(child(node, "range"), "location", 31))
    add("vertex-bound-mismatch", "setVertexBuffers",
        lambda node: child(node, "bound")[0].__setattr__("text", "0"))
    add("vertex-missing-buffer", "setVertexBuffers",
        lambda node: child(node, "buffers")[0].__setattr__("text", "999999"))
    add("vertex-invalid-offset", "setVertexBuffers",
        lambda node: child(node, "offsets")[0].__setattr__("text", "40"))
    add("vertex-offset-slot", "setVertexBufferOffset",
        lambda node: set_named(node, "index", 31))
    add("vertex-offset-value", "setVertexBufferOffset",
        lambda node: set_named(node, "offset", 304))
    add("vertex-bytes-slot", "setVertexBytes",
        lambda node: set_named(node, "index", 31))
    add("fragment-range", "setFragmentBuffers",
        lambda node: set_named(child(node, "range"), "location", 31))
    add("fragment-invalid-offset", "setFragmentBuffers",
        lambda node: child(node, "offsets")[1].__setattr__("text", "32"))
    add("fragment-bytes-slot", "setFragmentBytes",
        lambda node: set_named(node, "index", 31))
    return cases


def make_creation_cases(tree):
    cases = []

    def add(tag, name, field, value):
        variant = copy.deepcopy(tree)
        set_named(chunk(variant.getroot(), name), field, value)
        cases.append((tag, variant))

    add("queue-zero-limit", "MTLDevice::newCommandQueueWithMaxCommandBufferCount",
        "maxCommandBufferCount", 0)
    add("direct-dispatch-type", "MTLCommandBuffer::computeCommandEncoderWithDispatchType",
        "dispatchType", 2)
    add("descriptor-error-options", "MTLCommandQueue::commandBufferWithDescriptor",
        "errorOptions", 2)
    add("descriptor-dispatch-type", "MTLCommandBuffer::computeCommandEncoderWithDescriptor",
        "dispatchType", 2)
    return cases


def reject_cases(command, source, prefix, case_factory, directory):
    original = directory / f"{prefix}.zip.xml"
    run(command, "convert", "-f", source, "-o", original, "-c", "zip.xml")
    tree = ET.parse(original)
    count = 0
    for tag, variant in case_factory(tree):
        xml = directory / f"{prefix}-{tag}.zip.xml"
        variant.write(xml, encoding="unicode", xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(xml)[:-4])
        invalid = directory / f"{prefix}-{tag}.rdc"
        run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
        message = run(command, "replay", "--loops", "1", invalid, success=False)
        if not re.search(r"failed|invalid|unsupported|missing|Couldn't load", message, re.I):
            raise RuntimeError(f"missing diagnostic: {tag}: {message}")
        count += 1
    return count


def main():
    if len(sys.argv) != 4:
        raise SystemExit(f"usage: {sys.argv[0]} renderdoccmd t34_capture.rdc t35_capture.rdc")
    command, render_capture, creation_capture = map(pathlib.Path, sys.argv[1:4])
    with tempfile.TemporaryDirectory(prefix="metal-render-command-creation-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        render_count = reject_cases(command, render_capture, "t34", make_render_cases, directory)
        creation_count = reject_cases(command, creation_capture, "t35", make_creation_cases,
                                      directory)
    print(f"T34/T35 malformed captures rejected: {render_count + creation_count} cases")


if __name__ == "__main__":
    main()
