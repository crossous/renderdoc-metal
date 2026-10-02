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
    heaps = []
    command_buffers = {}
    commits = []
    event_operations = []
    event_initial_values = {}
    completion_handlers = []
    encoders = {}
    unattributed_gpu_operations = 0
    unattributed_operations = []
    pending_commands = set()
    peak_pending_commands = 0
    frame_scope = False
    queue_creations = {}
    queue_commits = {}
    queue_enqueues = {}
    gpu_identity_queries = []

    def command_info(identity):
        return command_buffers.setdefault(identity, {
            "command_buffer": identity, "queue": None, "creation_chunk": None,
            "commit_chunks": [], "encoder_chunks": [], "gpu_operations": 0})

    for _, chunk in ET.iterparse(args.xml, events=("end",)):
        if chunk.tag != "chunk":
            continue
        index = int(chunk.get("chunkIndex", "-1"))
        name = chunk.get("name", "")
        chunk_count += 1
        chunk_names[name] += 1
        if chunk.get("id") == "5":
            frame_scope = True

        if name == "MTLResource::CaptureGPUIdentity":
            gpu_identity_queries.append({"chunk": index,
                                         "resource": named_value(chunk, "resource"),
                                         "kind": named_value(chunk, "kind"),
                                         "value": named_value(chunk, "value")})

        if name == "MTLSharedEvent::setInitialSignaledValue":
            event_initial_values[named_value(chunk, "event")] = int(named_value(chunk, "value") or 0)
        if name == "MTLDevice::newHeapWithDescriptor":
            heaps.append({"chunk": index, "heap": named_value(chunk, "Heap"),
                          "size_bytes": int(named_value(chunk, "size") or 0),
                          "storage_mode": named_value(chunk, "storageMode"),
                          "type": named_value(chunk, "type")})
        if name.startswith("MTLCommandQueue::commandBuffer"):
            identity = named_value(chunk, "CommandBuffer")
            info = command_info(identity)
            info.update(queue=named_value(chunk, "CommandQueue"), creation_chunk=index)
            if frame_scope:
                pending_commands.add(identity)
                peak_pending_commands = max(peak_pending_commands, len(pending_commands))
                queue_creations.setdefault(info["queue"], []).append(identity)
        if name.startswith("MTLCommandBuffer::"):
            identity = named_value(chunk, "CommandBuffer")
            if identity:
                info = command_info(identity)
                for node in chunk:
                    if node.tag == "ResourceId" and node.get("name", "").endswith("Encoder"):
                        encoders[node.text] = identity
                        info["encoder_chunks"].append(index)
                if name in ("MTLCommandBuffer::encodeSignalEvent",
                            "MTLCommandBuffer::encodeWaitForEvent"):
                    event_operations.append({"chunk": index, "api": name,
                                             "command_buffer": identity,
                                             "event": named_value(chunk, "event"),
                                             "value": named_value(chunk, "value")})
                if name == "MTLCommandBuffer::addCompletedHandler":
                    completion_handlers.append({"chunk": index, "command_buffer": identity})
        if name == "MTLParallelRenderCommandEncoder::renderCommandEncoder":
            parent = named_value(chunk, "ParallelRenderCommandEncoder")
            child = named_value(chunk, "RenderCommandEncoder")
            identity = encoders.get(parent)
            if identity and child:
                encoders[child] = identity
                command_info(identity)["encoder_chunks"].append(index)
        if frame_scope and name == "MTLCommandBuffer::enqueue":
            identity = named_value(chunk, "CommandBuffer")
            queue_enqueues.setdefault(command_info(identity)["queue"], []).append(identity)
        if (name.startswith("MTLRenderCommandEncoder::draw") or
                name.startswith("MTLComputeCommandEncoder::dispatch") or
                name.startswith("MTLBlitCommandEncoder::copy") or
                name == "MTLBlitCommandEncoder::fillBuffer"):
            identity = None
            for node in chunk:
                if node.tag == "ResourceId" and node.get("name", "").endswith("Encoder"):
                    identity = encoders.get(node.text)
                    if identity:
                        command_info(identity)["gpu_operations"] += 1
                    break
            if not identity:
                unattributed_gpu_operations += 1
                unattributed_operations.append({"chunk": index, "api": name})

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
            identity = named_value(chunk, "CommandBuffer")
            info = command_info(identity)
            info["commit_chunks"].append(index)
            commit = {"chunk": index, "command_buffer": identity, "queue": info["queue"]}
            commits.append(commit)
            recent_commits.append(commit)
            if frame_scope:
                pending_commands.discard(identity)
                queue_commits.setdefault(info["queue"], []).append(identity)
        if name == "MTLCommandBuffer::waitUntilCompleted":
            waits.append(index)
        if name in ("MTLBuffer::setPurgeableState", "MTLTexture::setPurgeableState"):
            purges.append({"chunk": index, "api": name, "resource":
                           named_value(chunk, "Buffer") or named_value(chunk, "Texture"),
                           "state": named_value(chunk, "State"),
                           "last_blit_read": last_blit_reads.get(named_value(chunk, "Buffer")),
                           "preceding_commits": list(recent_commits)})
        chunk.clear()

    encoded_values = dict(event_initial_values)
    submitted_values = dict(event_initial_values)
    signal_by_command = {}
    signal_issues = []
    signal_count = 0
    for operation in event_operations:
        if operation["api"] != "MTLCommandBuffer::encodeSignalEvent":
            continue
        signal_count += 1
        event, command, value = operation["event"], operation["command_buffer"], int(operation["value"] or 0)
        if value <= encoded_values.get(event, 0):
            signal_issues.append({"chunk": operation["chunk"], "reason": "signal encoding does not increase", "event": event})
        encoded_values[event] = value
        signal_by_command.setdefault(command, {}).setdefault(event, []).append(value)
    for commit in commits:
        for event, values in signal_by_command.get(commit["command_buffer"], {}).items():
            if values[0] <= submitted_values.get(event, 0):
                signal_issues.append({"chunk": commit["chunk"], "reason": "signal submission order does not increase", "event": event})
            submitted_values[event] = values[-1]
    signal_analysis = {"signal_count": signal_count,
                       "gpu_wait_count": len(event_operations) - signal_count,
                       "initial_host_values": event_initial_values,
                       "issues": signal_issues}

    print(json.dumps({"capture_xml": args.xml.name, "chunks": chunk_count,
                      "draws": draw_count, "render_passes": render_passes,
                      "mrt_passes": mrt_passes,
                      "top_debug_groups": markers.most_common(20),
                      "wait_until_completed_chunks": waits, "purgeable_changes": purges,
                      "heaps": heaps,
                      "declared_heap_bytes": sum(heap["size_bytes"] for heap in heaps),
                      "command_buffers": list(command_buffers.values()),
                      "commits": commits, "event_operations": event_operations,
                      "event_timeline_analysis": signal_analysis,
                      "unattributed_gpu_operations": unattributed_gpu_operations,
                      "unattributed_operations": unattributed_operations,
                      "peak_uncommitted_command_buffers": peak_pending_commands,
                      "uncommitted_command_buffers_at_end": sorted(pending_commands),
                      "queue_creation_order": queue_creations,
                      "queue_commit_order": queue_commits,
                      "queue_enqueue_order": queue_enqueues,
                      "completion_handler_registrations": completion_handlers,
                      "gpu_identity_queries": gpu_identity_queries,
                      "notes": ["Heap sizes are declared capacities, not measured footprint.",
                                "Chunks describe CPU API order, not GPU completion times.",
                                "Handler registration does not prove callback completion.",
                                "GPU operation counts include draw, dispatch, blit copy/fill only."]},
                     ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
