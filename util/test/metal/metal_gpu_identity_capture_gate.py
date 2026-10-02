#!/usr/bin/env python3
"""Verify diagnostic GPU identity metadata and refusal before any frame submissions."""
import copy
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile


def run(*args, success=True):
    env = os.environ.copy()
    env.update(MTL_DEBUG_LAYER="1", RENDERDOC_METAL_TRACE_REPLAY_WAITS="1",
               RENDERDOC_METAL_TRACE_INITIAL_PRIVATE="1")
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True,
                            env=env, timeout=10)
    output = result.stdout + result.stderr
    if success and result.returncode:
        raise RuntimeError(f"{args}: {result.returncode}\n{output}")
    if not success:
        if result.returncode not in (1, 4):
            raise RuntimeError(f"expected ordinary refusal: {result.returncode}\n{output}")
        if "descriptor relocation is unsupported" not in output:
            raise RuntimeError(f"wrong rejection: {output}")
        if "Metal replay wait begin" in output or "Private initial contents upload" in output:
            raise RuntimeError(f"GPU work preceded refusal: {output}")
    return output


def main():
    if len(sys.argv) != 6:
        raise SystemExit("usage: gate.py renderdoccmd api_probe native_log capture1 capture2")
    command, probe, log, *captures = map(Path, sys.argv[1:])
    match = re.search(r"bufferVA=(\d+) textureID=(\d+) samplerID=(\d+) captures=2",
                      log.read_text())
    assert match, "missing two-capture native result"
    expected = dict(enumerate(map(int, match.groups())))
    with tempfile.TemporaryDirectory(prefix="metal-gpu-identity-") as tmp:
        folder = Path(tmp)
        for i, capture in enumerate(captures):
            xml = folder / f"source{i}.zip.xml"
            run(command, "convert", "-f", capture, "-o", xml, "-c", "zip.xml")
            tree = ET.parse(xml)
            chunks = tree.find("./chunks")
            identities = [c for c in chunks if c.get("name") == "MTLResource::CaptureGPUIdentity"]
            assert len(identities) in (3, 5), len(identities)
            seen = set()
            resource_ids = {}
            creation_ids = {int(n.text) for c in chunks
                            if "::new" in c.get("name", "")
                            for n in c if n.tag == "ResourceId" and n.get("name") in
                            ("Buffer", "Texture", "SamplerState")}
            for c in identities:
                fields = {n.get("name"): int(n.text) for n in c}
                assert fields["value"] == expected[fields["kind"]]
                assert fields["resource"] != 0
                assert fields["resource"] in creation_ids, "identity resource creation omitted"
                seen.add(fields["kind"])
                resource_ids[fields["kind"]] = fields["resource"]
            assert seen == {0, 1, 2}
            with zipfile.ZipFile(xml.with_suffix("")) as archive:
                buffers = [n for n in tree.iter("buffer") if n.get("byteLength") in ("8", "24")]
                tables = (struct.pack("<Q", expected[0]),
                          struct.pack("<QQQ", *(expected[k] for k in range(3))))
                assert any(archive.read(f"{int(n.text):06d}") in tables for n in buffers), \
                    "GPU pointer table does not match captured identity"
                def field(c, name):
                    return next((n.text for n in c if n.get("name") == name), None)
                input_bytes = [n for c in chunks
                               if (c.get("name") == "Internal::Initial Contents" and
                                   field(c, "id") == str(resource_ids[0])) or
                                  (field(c, "Buffer") == str(resource_ids[0]) and
                                   "::newBuffer" in c.get("name", ""))
                               for n in c if n.tag == "buffer" and n.get("name") in
                               ("Contents", "initialData")]
                assert any(archive.read(f"{int(n.text):06d}") == struct.pack("<I", 41)
                           for n in input_bytes), "indirect input initial bytes omitted"
                texture_bytes = [n for c in chunks
                                 if c.get("name", "").startswith("MTLTexture::replaceRegion") and
                                    field(c, "Texture") == str(resource_ids[1])
                                 for n in c if n.tag == "buffer" and n.get("name") == "contents"]
                assert any(archive.read(f"{int(n.text):06d}") == bytes((64, 128, 192, 255))
                           for n in texture_bytes), "indirect texture upload omitted"
            run(probe, capture, success=False)
            run(command, "replay", "--loops", "1", capture, success=False)

            # A query at frame end must also be refused before the first commit.
            variant = copy.deepcopy(tree)
            moved = variant.find("./chunks")
            for c in list(moved):
                if c.get("name") == "MTLResource::CaptureGPUIdentity":
                    moved.remove(c)
                    moved.insert(len(moved) - 1, c)
            for index, c in enumerate(moved):
                c.set("chunkIndex", str(index))
            destination = folder / f"late{i}.zip.xml"
            variant.write(destination, encoding="unicode", xml_declaration=True)
            shutil.copyfile(xml.with_suffix(""), destination.with_suffix(""))
            late = folder / f"late{i}.rdc"
            run(command, "convert", "-f", destination, "-o", late, "-c", "rdc")
            run(probe, late, success=False)
            run(command, "replay", "--loops", "1", late, success=False)
    print("GPU identities: two captures match native values/table; early/late API+CLI refusal PASS")


if __name__ == "__main__":
    main()
