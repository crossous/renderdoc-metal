#!/usr/bin/env python3
"""Require a valid interleaved command-buffer capture to replay successfully."""
import pathlib
import copy
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args):
    return subprocess.run([str(arg) for arg in args], stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, timeout=30)


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    result = run(command, "replay", "--loops", "1", capture)
    output = result.stdout
    if result.returncode != 0:
        raise RuntimeError(f"interleaved command-buffer gate changed: {result.returncode}\n{output}")
    with tempfile.TemporaryDirectory(prefix="metal-interleaved-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "source.zip.xml"
        converted = run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        if converted.returncode:
            raise RuntimeError(converted.stdout)
        tree = ET.parse(original)
        for tag, name, field in (
            ("unknown-encoder-owner", "MTLCommandBuffer::computeCommandEncoder", "CommandBuffer"),
            ("unknown-commit-owner", "MTLCommandBuffer::commit", "CommandBuffer"),
        ):
            variant = copy.deepcopy(tree)
            chunk = next(c for c in variant.findall("./chunks/chunk") if c.get("name") == name)
            next(c for c in chunk if c.get("name") == field).text = "999999"
            xml = directory / f"{tag}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{tag}.rdc"
            converted = run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            if converted.returncode:
                raise RuntimeError(converted.stdout)
            rejected = run(command, "replay", "--loops", "1", invalid)
            if rejected.returncode == 0 or "Failed to replay Metal chunk" not in rejected.stdout:
                raise RuntimeError(f"{tag}: {rejected.returncode}\n{rejected.stdout}")
    print("Interleaved command buffers: CLI replay passed; 2 owner-identity negatives rejected")


if __name__ == "__main__":
    main()
