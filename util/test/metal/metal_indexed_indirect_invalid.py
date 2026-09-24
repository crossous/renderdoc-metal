#!/usr/bin/env python3
"""Check that malformed T21 indexed indirect captures fail during replay."""

import argparse
import copy
import pathlib
import re
import shutil
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile


def run(*argv, expect_success=True):
    result = subprocess.run(argv, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if (result.returncode == 0) != expect_success:
        raise RuntimeError(f"unexpected command result: {argv}\n{result.stdout}")
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("renderdoccmd", type=pathlib.Path)
    parser.add_argument("capture", type=pathlib.Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="metal-t21-invalid-") as directory:
        directory = pathlib.Path(directory)
        original = directory / "original.zip.xml"
        run(args.renderdoccmd, "convert", "-f", args.capture, "-o", original,
            "-c", "zip.xml")
        xml = ET.parse(original)
        chunks = xml.findall("./chunks/chunk")
        draw = next(chunk for chunk in chunks if chunk.get("id") == "1149")
        fields = {child.get("name"): child for child in draw}
        assert fields["indexBufferOffset"].text == "4"
        assert fields["indirectBufferOffset"].text == "16"
        assert fields["indexBuffer"].text not in (None, "0")
        assert fields["indirectBuffer"].text not in (None, "0")

        cases = [
            ("missing-index", "indexBuffer", "0"),
            ("missing-arguments", "indirectBuffer", "0"),
            ("misaligned-index", "indexBufferOffset", "3"),
            ("past-index", "indexBufferOffset", "16"),
            ("misaligned-arguments", "indirectBufferOffset", "17"),
            ("past-arguments", "indirectBufferOffset", "36"),
        ]
        for name, field, value in cases:
            variant = copy.deepcopy(xml)
            variant_draw = next(chunk for chunk in variant.findall("./chunks/chunk")
                                if chunk.get("id") == "1149")
            next(child for child in variant_draw if child.get("name") == field).text = value
            xml_path = directory / f"{name}.zip.xml"
            variant.write(xml_path, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml_path)[:-4])
            replay_rejected(args.renderdoccmd, xml_path, directory / f"{name}.rdc")

        # Both copies of the shared packet must change: initial contents and frame update.
        for name, word, value in (("past-index-start", 2, 7),
                                  ("past-index-count", 0, 8),
                                  ("zero-index-count", 0, 0)):
            zip_path = directory / f"{name}.zip"
            with zipfile.ZipFile(str(original)[:-4], "r") as source, \
                 zipfile.ZipFile(zip_path, "w") as target:
                for entry in source.infolist():
                    data = bytearray(source.read(entry.filename))
                    if entry.filename in ("000003", "000007"):
                        assert len(data) == 52
                        struct.pack_into("<I", data, 16 + 4 * word, value)
                    target.writestr(entry, data)
            xml_path = directory / f"{name}.zip.xml"
            xml_path.write_bytes(original.read_bytes())
            replay_rejected(args.renderdoccmd, xml_path, directory / f"{name}.rdc")

    print("T21 invalid indexed indirect captures rejected: 9 cases")


def replay_rejected(command, xml_path, capture_path):
    run(command, "convert", "-f", xml_path, "-o", capture_path, "-c", "rdc")
    output = run(command, "replay", "--loops", "1", capture_path,
                 expect_success=False)
    if not re.search(r"failed|invalid|Couldn't load", output, re.I):
        raise RuntimeError(f"replay failed without a useful diagnostic:\n{output}")


if __name__ == "__main__":
    main()
