#!/usr/bin/env python3
"""Short malformed replay checks for frame-created placement buffer views."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run([str(arg) for arg in args], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=30)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result {result.returncode}: {args}\n{result.stdout}")
    return result.stdout


def child(chunk, name):
    return next(item for item in chunk if item.get("name") == name)


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="metal-typed-view-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "source.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        source = ET.parse(original)
        chunks = [c for c in source.findall("./chunks/chunk")
                  if c.get("name") == "MTLBuffer::newTextureWithDescriptor"]
        if len(chunks) != 3:
            raise RuntimeError("expected three Private typed view creations")
        frame = next(i for i, c in enumerate(source.findall("./chunks/chunk"))
                     if c.get("name") == "Internal::Beginning of Capture")
        if any(source.findall("./chunks/chunk").index(c) <= frame for c in chunks):
            raise RuntimeError("typed views must follow CaptureBegin")
        cases = ("unknown-parent", "unknown-format", "short-row", "outside-buffer")
        for case in cases:
            variant = copy.deepcopy(source)
            target = next(c for c in variant.findall("./chunks/chunk")
                          if c.get("name") == "MTLBuffer::newTextureWithDescriptor")
            desc = child(target, "descriptor")
            if case == "unknown-parent":
                child(target, "Buffer").text = "999999"
            elif case == "unknown-format":
                child(desc, "pixelFormat").text = "999"
            elif case == "short-row":
                child(target, "bytesPerRow").text = "1"
            else:
                child(target, "offset").text = "8192"
            xml = directory / f"{case}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{case}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            output = run(command, "replay", "--loops", "1", invalid, success=False)
            if "Failed to replay Metal chunk MTLBuffer::newTextureWithDescriptor" not in output:
                raise RuntimeError(f"unexpected rejection for {case}: {output}")
    print(f"frame-created typed view malformed captures rejected: {len(cases)}")


if __name__ == "__main__":
    main()
