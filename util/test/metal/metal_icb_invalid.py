#!/usr/bin/env python3
"""Check malformed single-command ICB captures against replay validation."""

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
    with tempfile.TemporaryDirectory(prefix="metal-t20-invalid-") as path:
        path = pathlib.Path(path)
        original = path / "original.zip.xml"
        run(args.renderdoccmd, "convert", "-f", args.capture,
            "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)

        def set_field(xml, chunk_id, field, value):
            chunk = next(item for item in xml.findall("./chunks/chunk")
                         if item.get("id") == chunk_id)
            entry = next(item for item in chunk.iter() if item.get("name") == field)
            entry.text = value

        cases = [
            ("past-range", "1184", "length", "3"),
            ("past-command", "1184", "location", "2"),
            ("missing-pipeline", "1237", "pipeline", "999999"),
            ("missing-buffer", "1238", "buffer", "999999"),
            ("past-vertex-offset", "1238", "offset", "152"),
            ("inherit-buffers", "1031", "inheritBuffers", "true"),
        ]
        for name, chunk_id, field, value in cases:
            variant = copy.deepcopy(tree)
            set_field(variant, chunk_id, field, value)
            xml_path = path / f"{name}.zip.xml"
            variant.write(xml_path, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml_path)[:-4])
            capture = path / f"{name}.rdc"
            run(args.renderdoccmd, "convert", "-f", xml_path, "-o", capture, "-c", "rdc")
            output = run(args.renderdoccmd, "replay", "--loops", "1", capture,
                         success=False)
            if not re.search(r"failed|invalid|Couldn't load", output, re.I):
                raise RuntimeError(f"missing diagnostic for {name}: {output}")
    print("T20 invalid ICB captures rejected: 6 cases")


if __name__ == "__main__":
    main()
