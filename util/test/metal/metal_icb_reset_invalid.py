#!/usr/bin/env python3
"""Check malformed reset/re-encode ICB captures against replay validation."""

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
    with tempfile.TemporaryDirectory(prefix="metal-t24-invalid-") as path:
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
            ("reset-past-range", "1242", 0, "length", "3"),
            ("reset-past-index", "1242", 0, "location", "3"),
            ("reset-zero-length", "1242", 0, "length", "0"),
            ("reset-missing-icb", "1242", 0, "IndirectCommandBuffer", "999999"),
            ("reset-leaves-empty-command", "1242", 0, "length", "2"),
            ("replacement-missing-pipeline", "1237", 3, "pipeline", "999999"),
            ("replacement-missing-buffer", "1238", 3, "buffer", "999999"),
            ("replacement-past-offset", "1238", 3, "offset", "104"),
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
    print(f"T24 invalid reset/re-encode ICB captures rejected: {len(cases)} cases")


if __name__ == "__main__":
    main()
