#!/usr/bin/env python3
"""Reject malformed UE-style blit counter attachment captures in one short replay each."""
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
    if (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result {result.returncode}: {args}\n{result.stdout}")
    return result.stdout


def named(parent, name):
    return next(item for item in parent if item.get("name") == name)


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    cases = [
        ("sample-id-zero", "sampleBufferId", "0"),
        ("sample-id-unknown", "sampleBufferId", "999999"),
        ("sample-resource-unknown", "sampleBuffer", "999999"),
        ("start-index-oob", "startOfEncoderSampleIndex", "4"),
        ("end-index-oob", "endOfEncoderSampleIndex", "4"),
        ("flag-inconsistent", "hasSampleBuffers", "false"),
    ]
    with tempfile.TemporaryDirectory(prefix="metal-ue-blit-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "source.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        for tag, field, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall("./chunks/chunk")
                         if item.get("name") == "MTLCommandBuffer::blitCommandEncoderWithDescriptor")
            node = (named(chunk, field) if field == "hasSampleBuffers" else
                    named(named(chunk, "attachments")[0], field))
            node.text = value
            xml = directory / f"{tag}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{tag}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            output = run(command, "replay", "--loops", "1", invalid, success=False)
            assert "Failed to process Metal chunk" in output or "Failed to replay Metal chunk" in output, (tag, output)
    print(f"UE blit counter malformed captures rejected: {len(cases)}")


if __name__ == "__main__":
    main()
