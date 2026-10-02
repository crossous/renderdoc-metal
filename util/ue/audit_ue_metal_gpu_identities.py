#!/usr/bin/env python3
"""CPU-only coverage audit of explicitly selected UE IRDescriptorTableEntry buffers.

The table resource IDs must be established from UE source and capture bindings.
Matches are diagnostic candidates, not a replay relocation plan: freed descriptor
slots, heap aliases, frame GPU writes and shader usage are not resolved here.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile


def value(chunk, name):
    return next((n.text for n in chunk.iter() if n.get("name") == name), None)


def coverage(data, sampler, identities, lengths, sampler_descriptors):
    if len(data) % 24:
        raise ValueError("descriptor table size is not a multiple of 24")
    counts = Counter()
    missing, ambiguous, offsets = [], [], []
    for slot, (address, texture, metadata) in enumerate(struct.iter_unpack("<QQQ", data)):
        if not (address or texture or metadata):
            continue
        counts["nonzero_entries"] += 1
        for field, raw, kind in (("sampler" if sampler else "buffer", address, 2 if sampler else 0),
                                 ("texture", texture, 1)):
            if not raw:
                continue
            if kind == 0:
                matches = [(rid, raw - base) for base, resources in identities[0].items()
                           for rid in resources if base <= raw < base + lengths.get(rid, 0)]
            else:
                matches = [(rid, 0) for rid in identities[kind].get(raw, ())]
            counts[field + "_nonzero"] += 1
            equivalent = (kind == 2 and len(matches) > 1 and
                          all(rid in sampler_descriptors for rid, _ in matches) and
                          len({sampler_descriptors[rid] for rid, _ in matches}) == 1)
            status = ("equivalent_candidates" if equivalent else "unique_candidate" if len(matches) == 1
                      else "missing" if not matches else "ambiguous")
            counts[field + "_" + status] += 1
            detail = dict(slot=slot, field=field, value=raw, metadata=metadata)
            if not matches:
                missing.append(detail)
            elif len(matches) > 1 and not equivalent:
                ambiguous.append(dict(**detail, candidates=matches))
            elif len(matches) == 1 and matches[0][1]:
                offsets.append(dict(**detail, resource=matches[0][0], offset=matches[0][1]))
    return dict(counts=dict(counts), missing=missing, ambiguous=ambiguous, offsets=offsets)


def audit(xml, resource_table, sampler_table):
    chunks = ET.parse(xml).find("./chunks")
    identities = {k: defaultdict(set) for k in range(3)}
    lengths, initial, sampler_descriptors = {}, {}, {}
    query_count = Counter()
    identity_timing = Counter()
    pre_frame_identities, creations, new_frame_identities = {}, {}, []
    frame = False
    for c in chunks:
        name = c.get("name", "")
        # SystemChunk::CaptureScope is exported as Internal::Frame Metadata.
        if c.get("id") == "5":
            frame = True
        for api, field in (("::newBuffer", "Buffer"), ("::newTexture", "Texture"),
                           ("::newSampler", "SamplerState")):
            if api in name and value(c, field) is not None:
                creations[int(value(c, field))] = dict(chunk=int(c.get("chunkIndex")),
                    api=name, phase="frame" if frame else "pre_frame")
        if name == "MTLResource::CaptureGPUIdentity":
            kind = int(value(c, "kind"))
            rid, raw = int(value(c, "resource")), int(value(c, "value"))
            identities[kind][raw].add(rid)
            query_count[kind] += 1
            if not frame:
                pre_frame_identities[rid] = (kind, raw)
            else:
                identity_timing["frame_queries"] += 1
                if pre_frame_identities.get(rid) == (kind, raw):
                    identity_timing["identical_duplicates"] += 1
                elif rid in pre_frame_identities:
                    identity_timing["conflicting_queries"] += 1
                else:
                    creation = creations.get(rid)
                    identity_timing["new_frame_identities"] += 1
                    identity_timing["creation_" + (creation["phase"] if creation else "missing")] += 1
                    new_frame_identities.append(dict(resource=rid, kind=kind, value=raw,
                        query_chunk=int(c.get("chunkIndex")), creation=creation))
        if name in ("MTLHeap::newBuffer(offset)", "MTLHeap::newBufferWithLength",
                    "MTLDevice::newBufferWithLength", "MTLDevice::newBufferWithBytes"):
            lengths[int(value(c, "Buffer"))] = int(value(c, "length"))
        if name == "Internal::Initial Contents" and value(c, "type") == "1":
            initial[int(value(c, "id"))] = int(value(c, "Contents"))
        if name == "MTLDevice::newSamplerStateWithDescriptor":
            descriptor = c.find("struct[@name='descriptor']")
            if descriptor is not None:
                sampler_descriptors[int(value(c, "SamplerState"))] = tuple(
                    (n.get("name"), n.text) for n in descriptor)
    tables = []
    with zipfile.ZipFile(xml.with_suffix("")) as archive:
        for rid, sampler in ((resource_table, False), (sampler_table, True)):
            if rid not in initial:
                raise ValueError(f"table {rid} has no captured initial contents")
            data = bytearray(archive.read(f"{initial[rid]:06d}"))
            bindings = Counter((c.get("name"), value(c, "index")) for c in chunks
                               if value(c, "buffer") == str(rid) and "::set" in c.get("name", ""))
            snapshots = [dict(chunk="initial", **coverage(data, sampler, identities, lengths,
                                                           sampler_descriptors))]
            for c in chunks:
                if c.get("name") != "Internal_MTLBufferModifyCPUContents" or value(c, "Buffer") != str(rid):
                    continue
                start, size = int(value(c, "start")), int(value(c, "size"))
                update = archive.read(f'{int(value(c, "data")):06d}')
                if size != len(update) or start < 0 or start + size > len(data):
                    raise ValueError(f"invalid CPU table update at chunk {c.get('chunkIndex')}")
                data[start:start + size] = update
                snapshots.append(dict(chunk=int(c.get("chunkIndex")), start=start, size=size,
                                      **coverage(data, sampler, identities, lengths, sampler_descriptors)))
            tables.append(dict(resource=rid, schema="sampler" if sampler else "resource", bytes=len(data),
                               bindings=[dict(api=api, index=index, count=count)
                                         for (api, index), count in sorted(bindings.items())],
                               snapshots=snapshots))
    return dict(xml=str(xml), query_chunks=dict(query_count),
                identity_timing=dict(identity_timing), new_frame_identities=new_frame_identities,
                unique_identity_values={k: len(v) for k, v in identities.items()}, tables=tables,
                sampler_aliases=[dict(value=raw, resources=sorted(resources),
                                      descriptors_equal=all(rid in sampler_descriptors for rid in resources)
                                      and len({sampler_descriptors.get(rid) for rid in resources}) == 1)
                                 for raw, resources in sorted(identities[2].items()) if len(resources) > 1],
                limitations=["No GPU execution or capture rewriting.",
                             "Matching uses all captured identities, not resource lifetime intervals.",
                             "Missing entries may be stale slots or omitted live resources.",
                             "CPU updates are reconstructed; GPU writes/copies are not simulated.",
                             "A unique candidate does not prove shader usage or safe relocation."])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("xml", type=Path, help="renderdoccmd zip.xml export with its paired ZIP")
    parser.add_argument("--resource-table", type=int, required=True)
    parser.add_argument("--sampler-table", type=int, required=True)
    args = parser.parse_args()
    print(json.dumps(audit(args.xml, args.resource_table, args.sampler_table), indent=2))


if __name__ == "__main__":
    main()
