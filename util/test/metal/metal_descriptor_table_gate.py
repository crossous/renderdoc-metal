#!/usr/bin/env python3
"""Typed descriptor replay and pre-submission negative checks (small fixtures only)."""
import copy
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    if len(sys.argv) != 7:
        raise SystemExit("usage: gate.py cli open_probe replay_probe native_log capture output_dir")
    cli, opener, replay, native, capture, folder = map(Path, sys.argv[1:])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER="1", RENDERDOC_METAL_TRACE_REPLAY_WAITS="1",
               RENDERDOC_METAL_TRACE_INITIAL_PRIVATE="1")

    def run(label, args, refuse=False):
        result = subprocess.run(list(map(str, args)), env=env, capture_output=True,
                                text=True, timeout=20)
        text = result.stdout + result.stderr
        (folder / f"{label}.log").write_text(text)
        if refuse:
            assert result.returncode in (1, 4), (label, result.returncode, text)
            # Schema4 is now a typed ray field, admitted only with a complete IR
            # declaration. A legacy untyped capture must still fail before GPU work.
            assert ((any(word in text.lower() for word in ("descriptor", "gpu identity")) or
                     "Metal ray resource layout requires an IR dispatch declaration" in text)
                    and "failed" in text.lower()), (label, text)
            assert "Metal replay wait begin" not in text, (label, text)
            assert "Private initial contents upload" not in text, (label, text)
        else:
            assert result.returncode == 0, (label, result.returncode, text)

    address = re.search(r"VA_A=(\d+)", native.read_text()).group(1)
    run("positive-bytes", [replay, capture, address])
    run("positive-cli", [cli, "replay", "--loops", "1", capture])
    source = folder / "source.zip.xml"
    run("export", [cli, "convert", "-f", capture, "-o", source, "-c", "zip.xml"])
    original = ET.parse(source)
    with zipfile.ZipFile(source.with_suffix("")) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}

    def first(chunks, name):
        return next(c for c in chunks if c.get("name") == name)

    def field(chunk, name):
        return next(n for n in chunk if n.get("name") == name)

    def mutate(name, tree, data):
        chunks = tree.find("./chunks")
        layout = first(chunks, "MTLBuffer::DeclareDescriptorTable")
        table = field(layout, "buffer").text
        if name == "missing-coverage":
            chunks.remove(first(chunks, "MTLDevice::DeclareDescriptorCoverage"))
        elif name == "unknown-schema":
            field(layout, "schema").text = "4"
        elif name == "overlap":
            chunks.insert(list(chunks).index(layout), copy.deepcopy(layout))
        elif name == "late-identity":
            identity = first(chunks, "MTLResource::CaptureGPUIdentity")
            chunks.remove(identity)
            chunks.insert(len(chunks) - 1, identity)
        elif name == "conflicting-late-identity":
            identity = copy.deepcopy(first(chunks, "MTLResource::CaptureGPUIdentity"))
            field(identity, "value").text = str(int(field(identity, "value").text) + 8)
            chunks.insert(len(chunks) - 1, identity)
        elif name in ("equivalent-sampler-alias", "conflicting-sampler-alias"):
            sampler = copy.deepcopy(first(chunks, "MTLDevice::newSamplerStateWithDescriptor"))
            field(sampler, "SamplerState").text = "999"
            if name == "conflicting-sampler-alias":
                minimum = field(field(sampler, "descriptor"), "minFilter")
                minimum.text = "1"
                minimum.set("string", "MTLSamplerMinMagFilterLinear")
            identity = copy.deepcopy(next(c for c in chunks
                if c.get("name") == "MTLResource::CaptureGPUIdentity" and field(c, "kind").text == "2"))
            field(identity, "resource").text = "999"
            at = list(chunks).index(first(chunks, "MTLDevice::newSamplerStateWithDescriptor"))
            chunks.insert(at, sampler)
            chunks.insert(at + 1, identity)
        elif name == "writable-table":
            for c in chunks:
                if c.get("name") == "MTLComputeCommandEncoder::setBuffer" and field(c, "index").text == "1":
                    field(c, "buffer").text = table
        elif name == "frame-allocation":
            allocation = copy.deepcopy(first(chunks, "MTLDevice::newBufferWithBytes"))
            field(allocation, "Buffer").text = "999"
            commit = first(chunks, "MTLCommandBuffer::commit")
            chunks.insert(list(chunks).index(commit), allocation)
        elif name == "late-invalid-pointer":
            update = next(c for c in chunks if c.get("name") == "Internal_MTLBufferModifyCPUContents"
                          and field(c, "Buffer").text == table)
            field(update, "size").text = "8"
            payload = field(update, "data")
            payload.set("byteLength", "8")
            data[f"{int(payload.text):06d}"] = struct.pack("<Q", 0xfffffffffffffff0)
        elif name == "unresolved-initial":
            initial = next(c for c in chunks if c.get("name") == "Internal::Initial Contents"
                           and field(c, "id").text == table)
            payload = field(initial, "Contents")
            key = f"{int(payload.text):06d}"
            value = bytearray(data[key]); struct.pack_into("<Q", value, 32, 0xfffffffffffffff0)
            data[key] = value
        else:
            raise AssertionError(name)
        for i, c in enumerate(chunks):
            c.set("chunkIndex", str(i))

    cases = ["equivalent-sampler-alias", "missing-coverage", "unknown-schema", "overlap", "late-identity",
             "conflicting-late-identity", "conflicting-sampler-alias",
             "writable-table", "frame-allocation", "late-invalid-pointer", "unresolved-initial"]
    for name in cases:
        tree = copy.deepcopy(original); data = dict(blobs)
        mutate(name, tree, data)
        xml = folder / f"{name}.zip.xml"
        tree.write(xml, encoding="unicode", xml_declaration=True)
        with zipfile.ZipFile(xml.with_suffix(""), "w", zipfile.ZIP_DEFLATED) as archive:
            for key, value in data.items():
                archive.writestr(key, value)
        rdc = folder / f"{name}.rdc"
        run(f"{name}-convert", [cli, "convert", "-f", xml, "-o", rdc, "-c", "rdc"])
        refusal = name != "equivalent-sampler-alias"
        run(f"{name}-api", [opener, rdc], refusal)
        run(f"{name}-cli", [cli, "replay", "--loops", "1", rdc], refusal)
        if not refusal:
            run(f"{name}-bytes", [replay, rdc, address])
    print(f"PASS typed GPU bytes/seek + CLI, equivalent sampler alias; {len(cases) - 1} negatives API+CLI before frame GPU submission")


if __name__ == "__main__":
    main()
