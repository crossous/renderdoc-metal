#!/usr/bin/env python3
"""Check malformed indexed ICB captures against replay validation."""

import argparse
import copy
import pathlib
import re
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result: {args}\n{result.stdout}")
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("renderdoccmd", type=pathlib.Path)
    parser.add_argument("capture", type=pathlib.Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="metal-t23-invalid-") as path:
        path = pathlib.Path(path)
        original = path / "original.zip.xml"
        run(args.renderdoccmd, "convert", "-f", args.capture,
            "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)

        def set_field(xml, chunk_id, occurrence, field, value):
            chunks = [item for item in xml.findall("./chunks/chunk")
                      if item.get("id") == chunk_id]
            chunk = chunks[occurrence]
            entry = next(item for item in chunk.iter() if item.get("name") == field)
            entry.text = value

        cases = [
            ("missing-index", "1241", 0, "indexBuffer", "999999"),
            ("misaligned-offset", "1241", 0, "indexBufferOffset", "5"),
            ("past-offset", "1241", 0, "indexBufferOffset", "14"),
            ("past-index-count", "1241", 0, "indexCount", "5"),
            ("zero-index-count", "1241", 0, "indexCount", "0"),
            ("invalid-index-type", "1241", 0, "indexType", "3"),
            ("zero-instance-count", "1241", 0, "instanceCount", "0"),
            ("base-vertex-overflow", "1241", 0, "baseVertex", "2147483648"),
            ("base-instance-overflow", "1241", 0, "baseInstance", "4294967296"),
            ("missing-pipeline", "1237", 0, "pipeline", "999999"),
            ("missing-position", "1238", 0, "buffer", "999999"),
            ("missing-instance", "1238", 1, "buffer", "999999"),
            ("past-position-offset", "1238", 0, "offset", "40"),
            ("past-instance-offset", "1238", 1, "offset", "96"),
            ("execute-past-index", "1240", 0, "location", "2"),
            ("execute-past-range", "1240", 0, "length", "2"),
            ("draw-past-index", "1184", 0, "location", "2"),
            ("inherit-buffers", "1031", 0, "inheritBuffers", "true"),
        ]
        for name, chunk_id, occurrence, field, value in cases:
            variant = copy.deepcopy(tree)
            set_field(variant, chunk_id, occurrence, field, value)
            xml_path = path / f"{name}.zip.xml"
            variant.write(xml_path, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml_path)[:-4])
            capture = path / f"{name}.rdc"
            run(args.renderdoccmd, "convert", "-f", xml_path, "-o", capture, "-c", "rdc")
            output = run(args.renderdoccmd, "replay", "--loops", "1", capture,
                         success=False)
            if not re.search(r"failed|invalid|Couldn't load", output, re.I):
                raise RuntimeError(f"missing diagnostic for {name}: {output}")
    print(f"T23 invalid indexed ICB captures rejected: {len(cases)} cases")


if __name__ == "__main__":
    main()
