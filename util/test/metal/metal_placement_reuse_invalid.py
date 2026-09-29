#!/usr/bin/env python3
"""Short malformed replay checks for a frame-created placement buffer."""
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
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result {result.returncode}: {args}\n{result.stdout}")
    return result.stdout


def child(chunk, name):
    return next(item for item in chunk if item.get("name") == name)


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="metal-placement-reuse-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "source.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        source = ET.parse(original)
        creations = [c for c in source.findall("./chunks/chunk")
                     if c.get("name") == "MTLHeap::newBuffer(offset)"]
        if len(creations) != 2 or source.find("./chunks").get("version") != "15":
            raise RuntimeError("expected one v0xF placement reuse capture")
        cases = ("duplicate-id", "missing-heap", "unaligned", "legacy-version")
        for case in cases:
            variant = copy.deepcopy(source)
            chunks = variant.find("./chunks")
            first, second = [c for c in chunks if c.get("name") == "MTLHeap::newBuffer(offset)"]
            if case == "duplicate-id":
                child(second, "Buffer").text = child(first, "Buffer").text
            elif case == "missing-heap":
                child(second, "Heap").text = "0"
            elif case == "unaligned":
                child(second, "offset").text = "1"
            else:
                chunks.set("version", "14")
            xml = directory / f"{case}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{case}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            output = run(command, "replay", "--loops", "1", invalid, success=False)
            if "Failed to replay Metal chunk MTLHeap::newBuffer(offset)" not in output:
                raise RuntimeError(f"unexpected rejection for {case}: {output}")
    print(f"Placement reuse malformed captures rejected: {len(cases)}")


if __name__ == "__main__":
    main()
