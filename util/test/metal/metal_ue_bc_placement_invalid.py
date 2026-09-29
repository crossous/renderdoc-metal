#!/usr/bin/env python3
"""Small malformed BC placement and initial-state replay checks."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run([str(x) for x in args], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=30)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected exit {result.returncode}: {args}\n{result.stdout}")
    return result.stdout


def child(node, name):
    return next(x for x in node if x.get("name") == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: invalid.py renderdoccmd capture.rdc")
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="metal-ue-bc-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "base.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        source = ET.parse(original)
        cases = ("missing-initial", "unknown-initial-id", "unknown-format",
                 "misaligned-offset", "short-copy-row")
        for case in cases:
            tree = copy.deepcopy(source)
            chunks = tree.findall("./chunks/chunk")
            texture = next(x for x in chunks if x.get("name") == "MTLHeap::newTexture(offset)")
            initial = next(x for x in chunks if x.get("name") == "Internal::Initial Contents"
                           and child(x, "type").text == "9")
            if case == "missing-initial":
                tree.find("chunks").remove(initial)
            elif case == "unknown-initial-id":
                child(initial, "id").text = "999999"
            elif case == "unknown-format":
                child(child(texture, "descriptor"), "pixelFormat").text = "999"
            elif case == "misaligned-offset":
                child(texture, "offset").text = "1"
            else:
                copy_chunk = next(x for x in chunks if x.get("name") ==
                                  "MTLBlitCommandEncoder::copyFromBuffer")
                child(copy_chunk, "sourceBytesPerRow").text = "1"
            xml = directory / f"{case}.zip.xml"
            tree.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{case}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            output = run(command, "replay", "--loops", "1", invalid, success=False)
            if "Failed to process Metal chunk" not in output and "Failed to replay Metal chunk" not in output and "Missing Metal BC placement texture initial contents" not in output:
                raise RuntimeError(f"unexpected rejection {case}: {output}")
        print(f"BC placement malformed captures safely rejected: {len(cases)}")


if __name__ == "__main__":
    main()
