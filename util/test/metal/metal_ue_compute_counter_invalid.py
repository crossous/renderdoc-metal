#!/usr/bin/env python3
"""Reject malformed UE-style compute counter descriptor captures in isolated replays."""
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
        ("sample-resource-unknown", "sampleBuffer", "999999"),
        ("start-index-oob", "startOfEncoderSampleIndex", "4"),
        ("end-index-oob", "endOfEncoderSampleIndex", "4"),
    ]
    with tempfile.TemporaryDirectory(prefix="metal-ue-compute-counter-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        original = directory / "source.zip.xml"
        run(command, "convert", "-f", capture, "-o", original, "-c", "zip.xml")
        tree = ET.parse(original)
        creation = next(item for item in tree.findall("./chunks/chunk")
                        if item.get("name") == "MTLDevice::newCounterSampleBufferWithDescriptor")
        count = int(named(creation, "sampleCount").text)
        cases = [(tag, field, str(count) if field.endswith("SampleIndex") else value)
                 for tag, field, value in cases]
        for tag, field, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall("./chunks/chunk")
                         if item.get("name") == "MTLCommandBuffer::computeCommandEncoderWithDescriptor")
            named(named(chunk, "attachments")[0], field).text = value
            xml = directory / f"{tag}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{tag}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            output = run(command, "replay", "--loops", "1", invalid, success=False)
            assert "Failed to process Metal chunk" in output or "Failed to replay Metal chunk" in output, (tag, output)
        for tag, value in (("sample-count-zero", "0"), ("sample-count-over-limit", "4097")):
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall("./chunks/chunk")
                         if item.get("name") == "MTLDevice::newCounterSampleBufferWithDescriptor")
            named(chunk, "sampleCount").text = value
            xml = directory / f"{tag}.zip.xml"
            variant.write(xml, encoding="unicode", xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f"{tag}.rdc"
            run(command, "convert", "-f", xml, "-o", invalid, "-c", "rdc")
            output = run(command, "replay", "--loops", "1", invalid, success=False)
            assert "Failed to process Metal chunk" in output or "Failed to replay Metal chunk" in output, (tag, output)
    print(f"UE compute counter malformed captures rejected: {len(cases) + 2}")


if __name__ == "__main__":
    main()
