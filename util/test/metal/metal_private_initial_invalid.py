#!/usr/bin/env python3
"""Check that a Private buffer Initial Contents record cannot target an unknown resource."""

import copy
import pathlib
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
    with tempfile.TemporaryDirectory(prefix="metal-private-initial-invalid-") as tmp:
        directory = pathlib.Path(tmp)
        source = directory / "source.zip.xml"
        converted = run(command, "convert", "-f", capture, "-o", source, "-c", "zip.xml")
        if converted.returncode:
            raise RuntimeError(converted.stdout)

        tree = ET.parse(source)
        chunks = tree.findall("./chunks/chunk")
        initial = next(c for c in chunks if c.get("name") == "Internal::Initial Contents")
        resource = next(e for e in initial if e.get("name") == "id")
        resource.text = "999999"
        invalid_xml = directory / "unknown-private-initial.zip.xml"
        tree.write(invalid_xml, encoding="unicode", xml_declaration=True)
        shutil.copyfile(source.with_suffix(""), invalid_xml.with_suffix(""))
        invalid = directory / "unknown-private-initial.rdc"
        converted = run(command, "convert", "-f", invalid_xml, "-o", invalid, "-c", "rdc")
        if converted.returncode:
            raise RuntimeError(converted.stdout)
        rejected = run(command, "replay", "--loops", "1", invalid)
        if rejected.returncode == 0 or "Failed to process Metal chunk" not in rejected.stdout:
            raise RuntimeError(f"unknown Private initial resource: {rejected.returncode}\n"
                               f"{rejected.stdout}")
    print("Private Initial Contents: unknown resource safely rejected")


if __name__ == "__main__":
    main()
