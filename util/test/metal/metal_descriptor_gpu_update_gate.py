#!/usr/bin/env python3
"""Explicit CPU provenance prevents GPU descriptor output becoming a CPU snapshot."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    if len(sys.argv) != 5:
        raise SystemExit("usage: gate.py cli open_probe capture output_dir")
    cli, opener, capture, folder = map(Path, sys.argv[1:])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER="1", RENDERDOC_METAL_TRACE_REPLAY_WAITS="1",
               RENDERDOC_METAL_TRACE_INITIAL_PRIVATE="1")

    def run(name, arguments, refuse=False):
        result = subprocess.run(list(map(str, arguments)), env=env, capture_output=True,
                                text=True, timeout=20)
        output = result.stdout + result.stderr
        (folder / f"{name}.log").write_text(output)
        if refuse:
            assert result.returncode in (1, 4), (name, result.returncode, output)
            assert "descriptor" in output.lower() and "failed" in output.lower(), (name, output)
            assert "Metal replay wait begin" not in output, (name, output)
            assert "Private initial contents upload" not in output, (name, output)
        else:
            assert result.returncode == 0, (name, result.returncode, output)

    def field(chunk, name):
        return next(n for n in chunk if n.get("name") == name)

    xml = folder / "source.zip.xml"
    run("export", [cli, "convert", "-f", capture, "-o", xml, "-c", "zip.xml"])
    original = ET.parse(xml)
    chunks = original.find("./chunks")
    gpu = next(c for c in chunks if c.get("name") == "MTLBuffer::DeclareDescriptorGPUWrites")
    destination = field(gpu, "buffer").text
    cpu = [c for c in chunks if c.get("name") == "MTLBuffer::DescriptorCPUWrite"]
    assert len(cpu) == 1 and field(cpu[0], "buffer").text == destination
    assert field(cpu[0], "start").text == "24" and field(cpu[0], "data").get("byteLength") == "24"
    assert not any(c.get("name") == "Internal_MTLBufferModifyCPUContents" and
                   field(c, "Buffer").text == destination for c in chunks), \
        "GPU-written descriptors leaked into automatic CPU snapshots"
    run("positive-cli", [cli, "replay", "--loops", "1", capture])
    with zipfile.ZipFile(xml.with_suffix("")) as archive:
        blobs = {n: archive.read(n) for n in archive.namelist()}
    for name in ("old-coverage", "unaligned-write", "partial-entry", "snapshot-injection", "missing-gpu-declaration"):
        tree = copy.deepcopy(original); data = dict(blobs); chunks = tree.find("./chunks")
        cpu = next(c for c in chunks if c.get("name") == "MTLBuffer::DescriptorCPUWrite")
        if name == "old-coverage":
            coverage = next(c for c in chunks if c.get("name") == "MTLDevice::DeclareDescriptorCoverage")
            field(coverage, "version").text = "1"
        elif name == "unaligned-write":
            field(cpu, "start").text = "25"
        elif name == "partial-entry":
            payload = field(cpu, "data"); payload.set("byteLength", "8")
            key = f"{int(payload.text):06d}"; data[key] = data[key][:8]
        elif name == "missing-gpu-declaration":
            gpu = next(c for c in chunks if c.get("name") == "MTLBuffer::DeclareDescriptorGPUWrites")
            chunks.remove(gpu)
        elif name == "snapshot-injection":
            snapshot = next(c for c in chunks if c.get("name") == "Internal_MTLBufferModifyCPUContents")
            injected = copy.deepcopy(snapshot)
            field(injected, "Buffer").text = destination
            chunks.insert(list(chunks).index(cpu), injected)
        for i, c in enumerate(chunks): c.set("chunkIndex", str(i))
        path = folder / f"{name}.zip.xml"
        tree.write(path, encoding="unicode", xml_declaration=True)
        with zipfile.ZipFile(path.with_suffix(""), "w", zipfile.ZIP_DEFLATED) as archive:
            for key, contents in data.items(): archive.writestr(key, contents)
        rdc = folder / f"{name}.rdc"
        run(f"{name}-convert", [cli, "convert", "-f", path, "-o", rdc, "-c", "rdc"])
        run(f"{name}-api", [opener, rdc], True)
        run(f"{name}-cli", [cli, "replay", "--loops", "1", rdc], True)
    print("PASS GPU-written table: exactly one explicit CPU entry, no GPU-to-CPU snapshot; 5 negatives API+CLI")


if __name__ == "__main__":
    main()
