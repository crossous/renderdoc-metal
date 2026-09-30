#!/usr/bin/env python3
"""Summarize a RenderDoc Metal XML export without opening a GPU replay device."""

import argparse
from collections import Counter, deque
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def named_value(chunk, name):
    return next((node.text for node in chunk.iter() if node.get("name") == name), None)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("xml", type=Path, help="XML from renderdoccmd convert")
    args = parser.parse_args()

    chunk_count = 0
    chunk_names = Counter()
    markers = Counter()
    render_passes = 0
    mrt_passes = []
    draw_count = 0
    recent_commits = deque(maxlen=12)
    waits = []
    purges = []
    blit_encoders = {}
    last_blit_reads = {}
    for _, chunk in ET.iterparse(args.xml, events=("end",)):
        if chunk.tag != "chunk":
            continue
        index = int(chunk.get("chunkIndex", "-1"))
        name = chunk.get("name", "")
        chunk_count += 1
        chunk_names[name] += 1

        if name.endswith("::pushDebugGroup"):
            label = named_value(chunk, "string")
            if label:
                markers[label] += 1
        if name == "MTLCommandBuffer::renderCommandEncoderWithDescriptor":
            render_passes += 1
            descriptor = chunk.find("struct[@name='descriptor']")
            colors = descriptor.find("array[@name='colorAttachments']") if descriptor is not None else None
            attachments = [node.text for attachment in colors for node in attachment
                           if node.get("name") == "texture" and node.text != "0"] if colors is not None else []
            if len(attachments) > 1:
                mrt_passes.append({"chunk": index, "color_attachments": attachments})
        if name.startswith("MTLRenderCommandEncoder::draw"):
            draw_count += 1
        if name.startswith("MTLCommandBuffer::blitCommandEncoder"):
            blit_encoders[named_value(chunk, "BlitCommandEncoder")] = named_value(chunk, "CommandBuffer")
        if name == "MTLBlitCommandEncoder::copyFromBuffer":
            source = named_value(chunk, "sourceBuffer")
            encoder = named_value(chunk, "BlitCommandEncoder")
            last_blit_reads[source] = {"chunk": index, "encoder": encoder,
                                       "command_buffer": blit_encoders.get(encoder)}
        if name == "MTLCommandBuffer::commit":
            recent_commits.append({"chunk": index, "command_buffer": named_value(chunk, "CommandBuffer")})
        if name == "MTLCommandBuffer::waitUntilCompleted":
            waits.append(index)
        if name in ("MTLBuffer::setPurgeableState", "MTLTexture::setPurgeableState"):
            purges.append({"chunk": index, "api": name, "resource":
                           named_value(chunk, "Buffer") or named_value(chunk, "Texture"),
                           "state": named_value(chunk, "State"),
                           "last_blit_read": last_blit_reads.get(named_value(chunk, "Buffer")),
                           "preceding_commits": list(recent_commits)})
        chunk.clear()

    print(json.dumps({"capture_xml": args.xml.name, "chunks": chunk_count,
                      "draws": draw_count, "render_passes": render_passes,
                      "mrt_passes": mrt_passes,
                      "top_debug_groups": markers.most_common(20),
                      "wait_until_completed_chunks": waits, "purgeable_changes": purges},
                     ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
