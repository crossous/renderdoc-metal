#!/usr/bin/env python3
"""Two bounded malformed cases for frame-created Shared placement buffers."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=20)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result {result.returncode}: {args}\n{result.stdout}")
    return result.stdout


def child(chunk, name):
    return next(item for item in chunk if item.get("name") == name)


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="metal-frame-shared-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "source.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        source = ET.parse(original)
        creations = [c for c in source.findall("./chunks/chunk")
                     if c.get("name") == "MTLHeap::newBuffer(offset)"]
        if len(creations) != 1 or source.find("./chunks").get("version") != "16":
            raise RuntimeError("expected one v0x10 frame placement buffer")
        for case in ("missing-heap", "unaligned"):
            variant = copy.deepcopy(source)
            creation = next(c for c in variant.findall("./chunks/chunk")
                            if c.get("name") == "MTLHeap::newBuffer(offset)")
            child(creation, "Heap" if case == "missing-heap" else "offset").text = \
                "0" if case == "missing-heap" else "1"
            xml = directory / f"{case}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{case}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            output = run(command, "replay", "--loops", "1", invalid, success=False)
            if "Failed to replay Metal chunk MTLHeap::newBuffer(offset)" not in output:
                raise RuntimeError(f"unexpected rejection for {case}: {output}")
    print("frame Shared placement malformed captures rejected: 2")


if __name__ == "__main__":
    main()
