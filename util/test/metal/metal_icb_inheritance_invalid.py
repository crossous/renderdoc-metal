#!/usr/bin/env python3
"""Reject malformed pipeline- and buffer-inheriting render ICB captures."""

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


def mutate(tree, chunk_id, occurrence, field, value):
    chunks = [item for item in tree.findall("./chunks/chunk")
              if item.get("id") == chunk_id]
    entry = next(item for item in chunks[occurrence].iter()
                 if item.get("name") == field)
    entry.text = value


def check_cases(command, capture, tag, cases, path):
    original = path / f"{tag}.zip.xml"
    run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
    tree = ET.parse(original)
    for name, edits in cases:
        variant = copy.deepcopy(tree)
        for chunk_id, occurrence, field, value in edits:
            mutate(variant, chunk_id, occurrence, field, value)
        xml_path = path / f"{tag}-{name}.zip.xml"
        variant.write(xml_path, encoding="unicode", xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(xml_path)[:-4])
        bad_capture = path / f"{tag}-{name}.rdc"
        run(command, "convert", "-f", xml_path, "-o", bad_capture, "-c", "rdc")
        output = run(command, "replay", "--loops", "1", bad_capture, success=False)
        if not re.search(r"failed|invalid|unsupported|missing|Couldn't load", output, re.I):
            raise RuntimeError(f"missing diagnostic for {tag}-{name}: {output}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("renderdoccmd", type=pathlib.Path)
    parser.add_argument("pipeline_capture", type=pathlib.Path)
    parser.add_argument("buffers_capture", type=pathlib.Path)
    parser.add_argument("plain_icb_capture", type=pathlib.Path)
    args = parser.parse_args()
    t26 = [
        ("bad-bind-limit", [("1031", 0, "maxVertexBufferBindCount", "0")]),
        ("bad-command-type", [("1031", 0, "commandTypes", "4")]),
        ("missing-outer-pipeline", [("1090", 0, "pipelineState", "999999")]),
        ("missing-command-buffer", [("1238", 0, "buffer", "999999")]),
        ("past-command-offset", [("1238", 0, "offset", "24")]),
        ("past-range", [("1184", 1, "location", "1")]),
    ]
    t27 = [
        ("bad-bind-limit", [("1031", 0, "maxVertexBufferBindCount", "1")]),
        ("bad-command-type", [("1031", 0, "commandTypes", "4")]),
        ("missing-outer-buffer", [("1092", 0, "buffer", "999999")]),
        ("past-outer-offset", [("1092", 1, "offset", "104")]),
        ("missing-command-pipeline", [("1237", 0, "pipeline", "999999")]),
        ("past-range", [("1184", 1, "location", "1")]),
    ]
    plain = [
        ("pipeline-command-conflict", [("1031", 0, "inheritPipelineState", "true")]),
        ("buffer-command-conflict", [("1031", 0, "inheritBuffers", "true"),
                                      ("1031", 0, "maxVertexBufferBindCount", "0")]),
    ]
    with tempfile.TemporaryDirectory(prefix="metal-icb-inheritance-invalid-") as tmp:
        path = pathlib.Path(tmp)
        check_cases(args.renderdoccmd, args.pipeline_capture, "t26", t26, path)
        check_cases(args.renderdoccmd, args.buffers_capture, "t27", t27, path)
        check_cases(args.renderdoccmd, args.plain_icb_capture, "plain", plain, path)
    print(f"T26/T27 invalid inheritance captures rejected: {len(t26) + len(t27) + len(plain)} cases")


if __name__ == "__main__":
    main()
