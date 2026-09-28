/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include <fstream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include "renderdoc/api/replay/renderdoc_replay.h"

template <>
rdcstr DoStringise(const uint32_t &value)
{
  char buffer[16] = {};
  snprintf(buffer, sizeof(buffer), "%u", value);
  return buffer;
}

#include "renderdoc/api/replay/pipestate.inl"
#include "renderdoc/driver/metal/official/metal-cpp.h"

REPLAY_PROGRAM_MARKER()

static bool HasUsage(IReplayController *renderer, ResourceId resource, ResourceUsage usage);
static bool Near(float actual, float expected);
static bool PixelMatches(IReplayController *renderer, ResourceId texture, uint32_t eventId,
                         uint32_t x, uint32_t y, float r, float g, float b);
static bool PixelMatchesRGBA(IReplayController *renderer, ResourceId texture, uint32_t eventId,
                             uint32_t x, uint32_t y, float r, float g, float b, float a);

static const ActionDescription *FindAction(const rdcarray<ActionDescription> &actions,
                                           ActionFlags flags)
{
  for(const ActionDescription &action : actions)
  {
    if(action.flags & flags)
      return &action;
    if(const ActionDescription *child = FindAction(action.children, flags))
      return child;
  }
  return NULL;
}

static void FindDrawActions(const rdcarray<ActionDescription> &actions,
                            rdcarray<const ActionDescription *> &draws)
{
  for(const ActionDescription &action : actions)
  {
    if(action.flags & ActionFlags::Drawcall)
      draws.push_back(&action);
    FindDrawActions(action.children, draws);
  }
}

static void FindActions(const rdcarray<ActionDescription> &actions,
                        rdcarray<const ActionDescription *> &result)
{
  for(const ActionDescription &action : actions)
  {
    result.push_back(&action);
    FindActions(action.children, result);
  }
}

static bool ValidateFakePassMarkers(IReplayController *renderer)
{
  renderer->AddFakeMarkers();
  for(const ActionDescription &action : renderer->GetRootActions())
  {
    if(!action.IsFakeMarker())
      continue;
    rdcarray<const ActionDescription *> children;
    FindActions(action.children, children);
    for(const ActionDescription *child : children)
      if(child->flags & ActionFlags::PassBoundary)
        return false;
  }
  return true;
}

static bool ValidateCounterStageFixture(IReplayController *renderer, ResourceId colorTarget)
{
  const SDChunk *creation = NULL, *pass = NULL, *resolve = NULL;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newCounterSampleBufferWithDescriptor") creation = chunk;
    if(chunk->name == "MTLCommandBuffer::renderCommandEncoderWithDescriptor") pass = chunk;
    if(chunk->name == "MTLBlitCommandEncoder::resolveCounters") resolve = chunk;
  }
  if(!creation && !resolve) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T101 counter stage validation failed: %s\n", message);
    return false;
  };
  if(!creation || !pass || !resolve) return fail("required chunks missing");
  const SDObject *sample = creation->FindChild("CounterSampleBuffer");
  const SDObject *count = creation->FindChild("sampleCount");
  const SDObject *set = creation->FindChild("counterSetName");
  const SDObject *mode = creation->FindChild("storageMode");
  const SDObject *descriptor = pass->FindChild("descriptor");
  const SDObject *attachments = descriptor ? descriptor->FindChild("sampleBufferAttachments") : NULL;
  const SDObject *attachment = attachments && attachments->NumChildren() == 1 ?
      attachments->GetChild(0) : NULL;
  const SDObject *bound = attachment ? attachment->FindChild("sampleBuffer") : NULL;
  const SDObject *boundId = attachment ? attachment->FindChild("sampleBufferId") : NULL;
  const SDObject *source = resolve->FindChild("sampleBuffer");
  const SDObject *destination = resolve->FindChild("destinationBuffer");
  const SDObject *range = resolve->FindChild("range");
  const SDObject *location = range ? range->FindChild("location") : NULL;
  const SDObject *length = range ? range->FindChild("length") : NULL;
  const SDObject *offset = resolve->FindChild("destinationOffset");
  const uint64_t sampleCount = count ? count->AsUInt64() : 0;
  const uint64_t firstSample = sampleCount == 8 ? 2 : 0;
  const uint64_t destinationOffset = sampleCount == 8 ? 16 : 0;
  if(!sample || sample->AsResourceId() == ResourceId() ||
     (sampleCount != 4 && sampleCount != 8) ||
     !set || set->AsString() != "timestamp" || !mode || mode->AsUInt64() != 0 ||
     !bound || bound->AsResourceId() != sample->AsResourceId() ||
     !boundId || boundId->AsResourceId() != sample->AsResourceId() ||
     !source || source->AsResourceId() != sample->AsResourceId() ||
     !destination || destination->AsResourceId() == ResourceId() ||
     !location || location->AsUInt64() != firstSample ||
     !length || length->AsUInt64() != 4 ||
     !offset || offset->AsUInt64() != destinationOffset)
    return fail("capture resource identity or descriptor fields");
  const char *names[] = {"startOfVertexSampleIndex", "endOfVertexSampleIndex",
                         "startOfFragmentSampleIndex", "endOfFragmentSampleIndex"};
  for(uint64_t i = 0; i < 4; i++)
  {
    const SDObject *index = attachment->FindChild(names[i]);
    if(!index || index->AsUInt64() != i + firstSample) return fail("stage sample index");
  }
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *draw = NULL, *copy = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Drawcall) draw = action;
    if(action->customName.contains("resolveCounters")) copy = action;
  }
  if(!draw || !copy || !(copy->flags & ActionFlags::Copy) ||
     copy->copySource != sample->AsResourceId() ||
     copy->copyDestination != destination->AsResourceId() ||
     !HasUsage(renderer,sample->AsResourceId(),ResourceUsage::CopySrc) ||
     !HasUsage(renderer,destination->AsResourceId(),ResourceUsage::CopyDst))
    return fail("resolve action or resource usages");
  for(uint32_t event : {copy->eventId, draw->eventId, copy->eventId})
  {
    renderer->SetFrameEvent(event,true);
    if(event == draw->eventId)
    {
      if(!PixelMatches(renderer,colorTarget,event,200,150,0.2f,0.7f,0.3f))
        return fail("draw pixel");
      continue;
    }
    const bytebuf data = renderer->GetBufferData(destination->AsResourceId(),destinationOffset,32);
    if(data.size() != 32) return fail("resolved data size");
    uint64_t values[4]; memcpy(values,data.data(),32);
    if(!values[0] || values[0] >= values[1] || values[1] >= values[2] ||
       values[2] >= values[3])
      return fail("resolved timestamps");
  }
  return true;
}

static bool ValidateDynamicLibraryFixture(IReplayController *renderer, ResourceId colorTarget)
{
  rdcarray<const SDChunk *> sources, dynamics;
  const SDChunk *executable = NULL;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newDynamicLibrary" ||
       chunk->name == "MTLDevice::newDynamicLibraryWithURL") dynamics.push_back(chunk);
    if(chunk->name == "MTLDevice::newLibraryWithSource" ||
       chunk->name == "MTLDevice::newLibraryWithSource(completionHandler)")
    {
      const SDObject *type = chunk->FindChild("libraryType");
      if(type && type->AsUInt64() == 1) sources.push_back(chunk);
      if(type && type->AsUInt64() == 0 && chunk->FindChild("dependencies") &&
         chunk->FindChild("dependencies")->NumChildren() > 0) executable = chunk;
    }
  }
  if(dynamics.empty()) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"Metal dynamic library validation failed: %s\n",message);
    return false;
  };
  size_t urlCount = 0;
  for(const SDChunk *dynamic : dynamics)
    urlCount += dynamic->name == "MTLDevice::newDynamicLibraryWithURL";
  if(!executable || dynamics.size() > 2 || sources.size() + urlCount != dynamics.size())
    return fail("source or executable chunk missing");
  const SDObject *deps = executable->FindChild("dependencies");
  const SDObject *executableSupported = executable->FindChild("supported");
  if(!deps || deps->NumChildren() != dynamics.size() ||
       !executableSupported || !executableSupported->AsBool())
    return fail("dynamic library resource graph or options");
  size_t sourceIndex = 0;
  for(size_t i = 0; i < dynamics.size(); i++)
  {
    const SDObject *dynamicId = dynamics[i]->FindChild("DynamicLibrary");
    if(!dynamicId || dynamicId->AsResourceId() == ResourceId() ||
       deps->GetChild(i)->AsResourceId() != dynamicId->AsResourceId())
      return fail("dynamic library identity or dependency order");
    if(dynamics[i]->name == "MTLDevice::newDynamicLibraryWithURL")
    {
      const SDObject *origin = dynamics[i]->FindChild("origin");
      const SDObject *installName = dynamics[i]->FindChild("installName");
      const SDObject *data = dynamics[i]->FindChild("data");
      if(!origin || origin->AsString().empty() ||
         !installName || installName->AsString().empty() ||
         !data || !data->IsBuffer() || data->type.byteSize < 4)
        return fail("URL dynamic library bytes or install name");
      continue;
    }
    if(sourceIndex >= sources.size()) return fail("source ordering");
    const SDObject *sourceId = sources[sourceIndex]->FindChild("Library");
    const SDObject *parent = dynamics[i]->FindChild("library");
    const SDObject *install = sources[sourceIndex]->FindChild("installName");
    const SDObject *sourceSupported = sources[sourceIndex]->FindChild("supported");
    const SDObject *dynamicSupported = dynamics[i]->FindChild("supported");
    if(!sourceId || sourceId->AsResourceId() == ResourceId() ||
       !dynamicId || dynamicId->AsResourceId() == ResourceId() ||
       !parent || parent->AsResourceId() != sourceId->AsResourceId() ||
       !install || install->AsString().empty() || !sourceSupported ||
       !sourceSupported->AsBool() || !dynamicSupported || !dynamicSupported->AsBool())
      return fail("dynamic library identity or dependency order");
    sourceIndex++;
  }
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name != "MTLDevice::newComputePipelineStateWithDescriptor" &&
       chunk->name != "MTLDevice::newComputePipelineStateWithDescriptor(completionHandler)")
      continue;
    const SDObject *descriptor = chunk->FindChild("descriptor");
    const SDObject *preloaded = descriptor ? descriptor->FindChild("preloadedLibraries") : NULL;
    if(!preloaded || preloaded->NumChildren() != 1 ||
       preloaded->GetChild(0)->AsResourceId() !=
           dynamics[0]->FindChild("DynamicLibrary")->AsResourceId())
      return fail("compute pipeline preload dependency");
  }
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name != "MTLDevice::newRenderPipelineStateWithDescriptor" &&
       chunk->name != "MTLDevice::newRenderPipelineStateWithDescriptor(completionHandler)" &&
       chunk->name != "MTLDevice::newRenderPipelineStateWithDescriptor(options, completionHandler)")
      continue;
    const SDObject *descriptor = chunk->FindChild("descriptor");
    const SDObject *vertex = descriptor ? descriptor->FindChild("vertexPreloadedLibraries") : NULL;
    const SDObject *fragment = descriptor ? descriptor->FindChild("fragmentPreloadedLibraries") : NULL;
    const size_t vertexCount = vertex ? vertex->NumChildren() : 0;
    const size_t fragmentCount = fragment ? fragment->NumChildren() : 0;
    if(vertexCount || fragmentCount)
    {
      const SDObject *preload = vertexCount ? vertex->GetChild(0) : fragment->GetChild(0);
      if(vertexCount + fragmentCount != 1 ||
         preload->AsResourceId() != dynamics[0]->FindChild("DynamicLibrary")->AsResourceId())
        return fail("render pipeline stage preload dependency");
    }
  }
  rdcarray<const ActionDescription *> actions, draws, dispatches;
  FindActions(renderer->GetRootActions(),actions);
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
  }
  if(dispatches.size() != 1 || draws.size() != 1 ||
     dispatches[0]->eventId >= draws[0]->eventId)
    return fail("expected dispatch followed by draw");
  ResourceId output;
  for(uint32_t event : {dispatches[0]->eventId,draws[0]->eventId,dispatches[0]->eventId})
  {
    renderer->SetFrameEvent(event,true);
    if(event == dispatches[0]->eventId)
    {
      const MetalPipe::State *state = renderer->GetPipelineState().GetMetalPipelineState();
      if(!state || state->computePipelineResourceId == ResourceId() ||
         state->computeBuffers.empty() || state->computeBuffers[0].resourceId == ResourceId())
        return fail("compute pipeline or output buffer missing");
      output = state->computeBuffers[0].resourceId;
    }
    const bytebuf bytes = renderer->GetBufferData(output,0,4);
    float result = 0.0f;
    if(bytes.size() != 4) return fail("compute result size");
    memcpy(&result,bytes.data(),4);
    if(result != (dynamics.size() == 2 ? 6.0f : 3.0f))
      return fail("linked compute result");
    if(event == draws[0]->eventId &&
       !PixelMatches(renderer,colorTarget,event,200,150,1.0f,0.0f,0.0f))
      return fail("linked output draw pixel");
  }
  return true;
}

static bool ValidateMetalEventSequence(IReplayController *renderer)
{
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const SDFile &file = renderer->GetStructuredFile();
  std::map<uint32_t, const APIEvent *> events;
  uint32_t last = 0;
  for(const ActionDescription *action : actions)
    for(const APIEvent &event : action->events)
    {
      if(event.eventId == 0 || events.count(event.eventId) || event.chunkIndex >= file.chunks.size())
        return false;
      events[event.eventId] = &event;
      if(event.eventId > last) last = event.eventId;
    }
  for(uint32_t eid = 1; eid <= last; ++eid)
    if(!events.count(eid))
      return false;
  return true;
}

static bool IsCommandCreationFixture(IReplayController *renderer)
{
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  size_t dispatches = 0;
  size_t draws = 0;
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch)
      dispatches++;
    if(action->flags & ActionFlags::Drawcall)
      draws++;
  }
  return dispatches == 2 && draws == 0;
}

static bool ValidateCommandCreationFixture(IReplayController *renderer)
{
  if(!IsCommandCreationFixture(renderer))
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "T35 command creation validation failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  rdcarray<const ActionDescription *> dispatches;
  for(const ActionDescription *action : actions)
    if(action->flags & ActionFlags::Dispatch)
      dispatches.push_back(action);
  if(dispatches.size() != 2 || dispatches[0]->dispatchDimension[0] != 1 ||
     dispatches[1]->dispatchDimension[0] != 1)
    return fail("dispatch actions incorrect");

  ResourceId output;
  for(size_t i : {0U, 1U, 0U, 1U})
  {
    renderer->SetFrameEvent(dispatches[i]->eventId, true);
    const MetalPipe::State *state = renderer->GetPipelineState().GetMetalPipelineState();
    if(!state || state->computePipelineResourceId == ResourceId() ||
       state->computeBuffers.empty() || state->computeBuffers[0].resourceId == ResourceId() ||
       state->computeBuffers[0].byteOffset != i * sizeof(uint32_t))
      return fail("compute pipeline or output binding incorrect");
    output = state->computeBuffers[0].resourceId;
    const bytebuf bytes = renderer->GetBufferData(output, 0, 8);
    if(bytes.size() != 8)
      return fail("output readback size incorrect");
    const uint32_t *values = (const uint32_t *)bytes.data();
    // Native clears this buffer before BeginCaptureFrame. Dispatch 0 has not written word 1:
    // expecting 17 there masked the old failure to restore Shared initial contents on replay.
    if(values[0] != 17 || values[1] != (i == 0 ? 0U : 17U))
      return fail("output values incorrect");
  }

  const SDFile &structured = renderer->GetStructuredFile();
  const char *required[] = {
      "MTLDevice::newCommandQueueWithMaxCommandBufferCount",
      "MTLCommandQueue::commandBufferWithUnretainedReferences",
      "MTLCommandBuffer::computeCommandEncoderWithDispatchType",
      "MTLCommandBuffer::waitUntilScheduled",
      "MTLCommandQueue::commandBufferWithDescriptor",
      "MTLCommandBuffer::computeCommandEncoderWithDescriptor",
  };
  for(const char *name : required)
  {
    bool found = false;
    for(const SDChunk *chunk : structured.chunks)
      found |= chunk->name == name;
    if(!found)
      return fail("required structured chunk missing");
  }
  if(!HasUsage(renderer, output, ResourceUsage::CS_RWResource))
    return fail("output usage incorrect");
  return true;
}

static bool ValidateRenderInlineBatchFixture(IReplayController *renderer, ResourceId colorTarget)
{
  const SDFile &structured = renderer->GetStructuredFile();
  bool isFixture = false;
  for(const SDChunk *chunk : structured.chunks)
    isFixture |= chunk->name == "MTLRenderCommandEncoder::setVertexBuffers";
  if(!isFixture)
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "T34 render inline/batch validation failed: %s\n", message);
    return false;
  };
  const char *required[] = {
      "MTLRenderCommandEncoder::setVertexBuffers",
      "MTLRenderCommandEncoder::setVertexBufferOffset",
      "MTLRenderCommandEncoder::setVertexBytes",
      "MTLRenderCommandEncoder::setFragmentBuffers",
      "MTLRenderCommandEncoder::setFragmentBytes",
  };
  for(const char *name : required)
  {
    bool found = false;
    for(const SDChunk *chunk : structured.chunks)
      found |= chunk->name == name;
    if(!found)
      return fail("required structured chunk missing");
  }

  const ActionDescription *draw = FindAction(renderer->GetRootActions(), ActionFlags::Drawcall);
  if(!draw)
    return fail("draw action missing");
  renderer->SetFrameEvent(draw->eventId, true);
  const MetalPipe::State *state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!state || state->vertexStorageBuffers.size() < 2 || state->fragmentBuffers.size() < 5 ||
     state->vertexStorageBuffers[0].byteOffset != 16 ||
     state->vertexStorageBuffers[1].byteOffset != 256 ||
     state->fragmentBuffers[3].byteOffset != 0 ||
     state->fragmentBuffers[4].byteOffset != 16)
    return fail("pipeline buffer offsets incorrect");
  if(!HasUsage(renderer, state->vertexStorageBuffers[0].resourceId,
               ResourceUsage::VS_Resource) ||
     !HasUsage(renderer, state->fragmentBuffers[3].resourceId, ResourceUsage::PS_Resource))
    return fail("buffer usage incorrect");
  if(!PixelMatches(renderer, colorTarget, draw->eventId, 200, 150, 0.5625f, 0.5f, 0.5f))
    return fail("final render output incorrect");
  return true;
}

static bool ValidateRenderDynamicStateFixture(IReplayController *renderer, ResourceId colorTarget)
{
  const SDFile &structured = renderer->GetStructuredFile();
  bool isFixture = false;
  for(const SDChunk *chunk : structured.chunks)
    isFixture |= chunk->name == "MTLRenderCommandEncoder::setBlendColor";
  if(!isFixture)
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "T36 render dynamic state validation failed: %s\n", message);
    return false;
  };
  const char *required[] = {
      "MTLCommandBuffer::pushDebugGroup",
      "MTLCommandBuffer::popDebugGroup",
      "MTLRenderCommandEncoder::pushDebugGroup",
      "MTLRenderCommandEncoder::insertDebugSignpost",
      "MTLRenderCommandEncoder::popDebugGroup",
      "MTLRenderCommandEncoder::setViewports",
      "MTLRenderCommandEncoder::setScissorRects",
      "MTLRenderCommandEncoder::setDepthClipMode",
      "MTLRenderCommandEncoder::setDepthBias",
      "MTLRenderCommandEncoder::setTriangleFillMode",
      "MTLRenderCommandEncoder::setBlendColor",
      "MTLRenderCommandEncoder::setVisibilityResultMode",
      "MTLRenderCommandEncoder::setColorStoreAction",
      "MTLRenderCommandEncoder::setDepthStoreAction",
      "MTLRenderCommandEncoder::setStencilStoreAction",
      "MTLRenderCommandEncoder::setColorStoreActionOptions",
      "MTLRenderCommandEncoder::setDepthStoreActionOptions",
      "MTLRenderCommandEncoder::setStencilStoreActionOptions",
      "MTLRenderCommandEncoder::textureBarrier",
  };
  for(const char *name : required)
  {
    bool found = false;
    for(const SDChunk *chunk : structured.chunks)
      found |= chunk->name == name;
    if(!found)
      return fail("required structured chunk missing");
  }

  const ActionDescription *draw = FindAction(renderer->GetRootActions(), ActionFlags::Drawcall);
  if(!draw)
    return fail("draw action missing");
  renderer->SetFrameEvent(draw->eventId, true);
  const MetalPipe::State *state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!state || !state->rasterizer.viewport.enabled ||
     state->rasterizer.viewport.width != 400.0f ||
     state->rasterizer.viewport.height != 300.0f ||
     !state->rasterizer.scissor.enabled || state->rasterizer.scissor.x != 64 ||
     state->rasterizer.scissor.y != 48 || state->rasterizer.scissor.width != 272 ||
     state->rasterizer.scissor.height != 204)
    return fail("viewport or scissor state incorrect");
  if(!PixelMatchesRGBA(renderer, colorTarget, draw->eventId, 200, 150,
                       0.2f, 0.4f, 0.6f, 0.8f) ||
     !PixelMatchesRGBA(renderer, colorTarget, draw->eventId, 8, 8,
                       0.0f, 0.0f, 0.0f, 1.0f))
    return fail("blend constant, depth clamp, fill, or scissor output incorrect");
  ResourceId visibility;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 8)
      visibility = buffer.resourceId;
  if(visibility == ResourceId())
    return fail("visibility buffer not captured");
  const bytebuf counter = renderer->GetBufferData(visibility, 0, 8);
  uint64_t visible = 0;
  if(counter.size() == 8)
    memcpy(&visible, counter.data(), 8);
  if(visible != 272 * 204)
    return fail("GPU visibility count incorrect");
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *begin = NULL, *end = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName.contains("Begin Metal Render Pass"))
      begin = action;
    if(action->customName.contains("End Metal Render Pass"))
      end = action;
  }
  if(!begin || !end || !end->customName.contains("C0=Store, D=Store, S=Store") ||
     draw->depthOut == ResourceId())
    return fail("dynamic attachment store state missing");
  renderer->SetFrameEvent(end->eventId, true);
  const PixelValue depth =
      renderer->PickPixel(draw->depthOut, 200, 150, {0, 0, 0}, CompType::Typeless);
  if(!Near(depth.floatValue[0], 0.75f))
  {
    fprintf(stderr, "T36 depth: %f (event %u)\n", depth.floatValue[0], end->eventId);
    return fail("depth attachment not preserved");
  }
  if(!Near(depth.floatValue[1], 23.0f / 255.0f))
    return fail("stencil attachment not preserved");
  const bytebuf depthBytes = renderer->GetTextureData(draw->depthOut, {0, 0, 0});
  if(depthBytes.size() != 400 * 300 * 8)
    return fail("D32S8 readback size incorrect");
  for(size_t i = 0; i < depthBytes.size(); i += 8)
  {
    float value = 0.0f;
    memcpy(&value, depthBytes.data() + i, sizeof(value));
    if(!Near(value, 0.75f) || depthBytes[i + 4] != 23 || depthBytes[i + 5] != 0 ||
       depthBytes[i + 6] != 0 || depthBytes[i + 7] != 0)
      return fail("D32S8 depth/stencil packing or row stride incorrect");
  }
  // This event stops before the deferred setters. End the partial pass safely, then rewind.
  renderer->SetFrameEvent(begin->eventId, true);
  renderer->SetFrameEvent(end->eventId, true);
  if(!PixelMatchesRGBA(renderer, colorTarget, end->eventId, 200, 150,
                       0.2f, 0.4f, 0.6f, 0.8f))
    return fail("deferred store action seek is unstable");
  return true;
}

static bool ValidateSamplerLODFixture(IReplayController *renderer, ResourceId colorTarget)
{
  bool fixture = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    fixture |= chunk->name == "MTLComputeCommandEncoder::setSamplerState_lodclamp";
  if(!fixture) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T38 sampler LOD validation failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> actions, draws, dispatches;
  FindActions(renderer->GetRootActions(), actions);
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
  }
  if(draws.size() != 3 || dispatches.size() != 3)
    return fail("three draw/dispatch actions required");
  ResourceId samplerObject;
  auto checkSampler = [&](ShaderStage stage, float minimum, float maximum) {
    const auto samplers = renderer->GetPipelineState().GetSamplers(stage, false);
    if(samplers.size() != 1 || samplers[0].sampler.object == ResourceId() ||
       !Near(samplers[0].sampler.minLOD, minimum) || !Near(samplers[0].sampler.maxLOD, maximum))
      return false;
    if(samplerObject != ResourceId() && samplerObject != samplers[0].sampler.object)
      return false;
    samplerObject = samplers[0].sampler.object;
    return true;
  };
  const float expected[] = {0,1,0,1, 0,0,1,1, 1,0,0,1};
  for(uint32_t i : {0U, 1U, 2U, 0U, 2U})
  {
    renderer->SetFrameEvent(dispatches[i]->eventId, true);
    if(!checkSampler(ShaderStage::Compute, i == 2 ? 0.0f : float(i + 1),
                      i == 0 ? 1.0f : 2.0f))
      return fail("CS clamp, nil slot, plain-binding reset or rewind incorrect");
    const auto *state = renderer->GetPipelineState().GetMetalPipelineState();
    if(!state || state->computeBuffers.empty()) return fail("CS buffer missing");
    const bytebuf bytes = renderer->GetBufferData(state->computeBuffers[0].resourceId, 0, 0);
    float values[12] = {};
    if(bytes.size() != sizeof(values)) return fail("CS readback size incorrect");
    memcpy(values, bytes.data(), sizeof(values));
    for(uint32_t j = 0; j < 12; j++)
      if(!Near(values[j], j < (i + 1) * 4 ? expected[j] : 0.0f))
        return fail("CS GPU mip selection or rewind incorrect");
  }
  for(uint32_t i : {0U, 1U, 2U, 0U, 2U})
  {
    renderer->SetFrameEvent(draws[i]->eventId, true);
    const float vertexMin[] = {1, 2, 0}, vertexMax[] = {1, 2, 2};
    const float fragmentMin[] = {2, 1, 0}, fragmentMax[] = {2, 1, 2};
    if(!checkSampler(ShaderStage::Vertex, vertexMin[i], vertexMax[i]) ||
       !checkSampler(ShaderStage::Fragment, fragmentMin[i], fragmentMax[i]))
      return fail("VS/FS LOD descriptors or regular sampler reset incorrect");
    if(!PixelMatches(renderer, colorTarget, draws[i]->eventId, 64 + 128 * i, 150,
                       i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, i == 2 ? 1.0f : 0.0f))
      return fail("VS/FS native mip selection incorrect");
  }
  return true;
}

static bool ValidateComputeInlineFixture(IReplayController *renderer, ResourceId colorTarget)
{
  bool fixture = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    fixture |= chunk->name == "MTLComputeCommandEncoder::setThreadgroupMemoryLength";
  if(!fixture) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T40 compute inline validation failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> actions, dispatches;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draw = action;
  }
  if(dispatches.size() != 5 || !draw) return fail("five dispatches and one draw required");
  const uint32_t expected[] = {92, 200, 108, 232, 124};
  ResourceId output, params;
  for(uint32_t i : {0U, 1U, 2U, 3U, 4U, 1U, 0U, 4U})
  {
    renderer->SetFrameEvent(dispatches[i]->eventId, true);
    const auto *state = renderer->GetPipelineState().GetMetalPipelineState();
    if(!state || state->computeBuffers.size() <= 2) return fail("compute bindings missing");
    const auto &out = state->computeBuffers[0];
    const auto &input = state->computeBuffers[2];
    if(out.resourceId == ResourceId() || (output != ResourceId() && output != out.resourceId) ||
       out.byteOffset != 16 * i || out.byteSize != 80 - 16 * i)
      return fail("output identity, offset or remaining size incorrect");
    output = out.resourceId;
    if(i == 2 || i == 3)
    {
      if(input.resourceId == ResourceId() || (params != ResourceId() && params != input.resourceId) ||
         input.byteOffset != (i == 2 ? 0 : 16) || input.byteSize != (i == 2 ? 256 : 240))
        return fail("real-buffer rebind or absolute offset incorrect");
      params = input.resourceId;
    }
    else if(input.resourceId != ResourceId() || input.byteOffset != 0 ||
            input.byteSize != 16)
      return fail("inline bytes retain stale buffer identity/offset/size");
    const bytebuf bytes = renderer->GetBufferData(output, 0, 0);
    uint32_t values[20] = {};
    if(bytes.size() != sizeof(values)) return fail("output readback size incorrect");
    memcpy(values, bytes.data(), sizeof(values));
    for(uint32_t j = 0; j < 20; j++)
      if(values[j] != (j % 4 == 0 && j / 4 <= i ? expected[j / 4] : 0))
        return fail("reduction result, untouched padding or event rewind incorrect");
    const bool uniform = i == 0 || i == 2 || i == 4;
    if(dispatches[i]->dispatchDimension[0] != (uniform ? 1 : 8) ||
       dispatches[i]->dispatchThreadsDimension[0] != 8)
      return fail("dispatch metadata incorrect");
  }
  return PixelMatches(renderer, colorTarget, draw->eventId, 200, 150,
                      92.0f / 255.0f, 200.0f / 255.0f, 108.0f / 255.0f);
}

static bool ValidatePrivateBufferFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId gpu;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 516 && HasUsage(renderer, buffer.resourceId, ResourceUsage::CopySrc))
      gpu = buffer.resourceId;
  if(gpu == ResourceId()) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T39 private buffer validation failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *fill = NULL, *dispatch = NULL, *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName.contains("fillBuffer")) fill = action;
    if(action->flags & ActionFlags::Dispatch) dispatch = action;
    if(action->flags & ActionFlags::Drawcall) draw = action;
  }
  if(!fill || !dispatch || !draw) return fail("required actions missing");
  for(bool written : {false, true, false, true})
  {
    renderer->SetFrameEvent(written ? dispatch->eventId : fill->eventId, true);
    const uint64_t offsets[] = {0, 3, 255, 515, 512};
    const uint64_t lengths[] = {0, 9, 17, 0, ~0ULL};
    for(size_t test = 0; test < sizeof(offsets) / sizeof(offsets[0]); test++)
    {
      const uint64_t offset = offsets[test];
      const uint64_t available = 516 - offset;
      const size_t length = size_t(lengths[test] == 0 || lengths[test] > available ?
                                   available : lengths[test]);
      const bytebuf bytes = renderer->GetBufferData(gpu, offset, lengths[test]);
      if(bytes.size() != length) return fail("private or unaligned range readback failed");
      for(size_t i = 0; i < length; i++)
        if(bytes[i] != (written ? byte((i + offset) * 7 + 3) : byte(0xa5)))
          return fail("private buffer contents/seek incorrect");
    }
  }
  if(!renderer->GetBufferData(gpu, 516, 0).empty() ||
     !renderer->GetBufferData(gpu, ~0ULL, 0).empty() ||
     !renderer->GetBufferData(ResourceId(), 0, 0).empty() ||
     !renderer->GetBufferData(colorTarget, 0, 0).empty())
    return fail("invalid resource/range was not rejected");
  const SDChunk *computePipeline = NULL, *computeHandle = NULL, *computeTable = NULL;
  const SDChunk *computeUpdate = NULL, *computeBind = NULL, *computeBindRange = NULL;
  const SDChunk *argumentCreate = NULL, *argumentSelect = NULL, *argumentTable = NULL;
  const SDChunk *computeArgumentBind = NULL;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newComputePipelineStateWithDescriptor") computePipeline = chunk;
    if(chunk->name == "MTLComputePipelineState::functionHandleWithFunction") computeHandle = chunk;
    if(chunk->name == "MTLComputePipelineState::newVisibleFunctionTableWithDescriptor") computeTable = chunk;
    if(chunk->name == "MTLComputeCommandEncoder::setVisibleFunctionTable") computeBind = chunk;
    if(chunk->name == "MTLComputeCommandEncoder::setVisibleFunctionTables") computeBindRange = chunk;
    if(chunk->name == "MTLVisibleFunctionTable::setFunction") computeUpdate = chunk;
    if(chunk->name == "MTLFunction::newArgumentEncoderWithBufferIndex") argumentCreate = chunk;
    if(chunk->name == "MTLArgumentEncoder::setArgumentBuffer") argumentSelect = chunk;
    if(chunk->name == "MTLArgumentEncoder::setVisibleFunctionTable") argumentTable = chunk;
    if(chunk->name == "MTLComputeCommandEncoder::setBuffer" &&
       chunk->FindChild("index") && chunk->FindChild("index")->AsUInt64() == 1)
      computeArgumentBind = chunk;
  }
  if(computeBind || computeBindRange)
  {
    const SDObject *desc = computePipeline ? computePipeline->FindChild("descriptor") : NULL;
    const SDObject *links = desc ? desc->FindChild("linkedFunctions") : NULL;
    const SDObject *functions = links ? links->FindChild("functions") : NULL;
    const SDObject *pipelineId = computePipeline ?
        computePipeline->FindChild("ComputePipelineState") : NULL;
    const SDObject *functionId = computeHandle ? computeHandle->FindChild("function") : NULL;
    const SDObject *handleId = computeHandle ? computeHandle->FindChild("Handle") : NULL;
    const SDObject *tableId = computeTable ? computeTable->FindChild("Table") : NULL;
    const SDObject *setTable = computeUpdate ? computeUpdate->FindChild("Table") : NULL;
    const SDObject *setHandle = computeUpdate ? computeUpdate->FindChild("function") : NULL;
    const SDObject *boundTable = computeBind ? computeBind->FindChild("table") :
        computeBindRange->FindChild("tables");
    if(computeBindRange)
      boundTable = boundTable && boundTable->NumChildren() == 1 ? boundTable->GetChild(0) : NULL;
    const SDObject *boundIndex = computeBind ? computeBind->FindChild("index") :
        computeBindRange->FindChild("range");
    if(computeBindRange) boundIndex = boundIndex ? boundIndex->FindChild("location") : NULL;
    if(!functions || functions->NumChildren() != 1 || !pipelineId || !functionId ||
       !handleId || !tableId || !setTable || !setHandle || !boundTable || !boundIndex ||
       functions->GetChild(0)->AsResourceId() != functionId->AsResourceId() ||
       !computeHandle->FindChild("Pipeline") ||
       computeHandle->FindChild("Pipeline")->AsResourceId() != pipelineId->AsResourceId() ||
       !computeTable->FindChild("Pipeline") ||
       computeTable->FindChild("Pipeline")->AsResourceId() != pipelineId->AsResourceId() ||
       handleId->AsResourceId() != setHandle->AsResourceId() ||
       tableId->AsResourceId() != setTable->AsResourceId() ||
       tableId->AsResourceId() != boundTable->AsResourceId() || boundIndex->AsUInt64() != 1)
      return fail("compute visible-function-table graph or binding incorrect");
  }
  if(argumentTable)
  {
    const SDObject *desc = computePipeline ? computePipeline->FindChild("descriptor") : NULL;
    const SDObject *kernel = desc ? desc->FindChild("computeFunction") : NULL;
    const SDObject *createdFunction = argumentCreate ? argumentCreate->FindChild("Function") : NULL;
    const SDObject *createdEncoder = argumentCreate ? argumentCreate->FindChild("ArgumentEncoder") : NULL;
    const SDObject *selectedEncoder = argumentSelect ? argumentSelect->FindChild("ArgumentEncoder") : NULL;
    const SDObject *tableEncoder = argumentTable->FindChild("ArgumentEncoder");
    const SDObject *tableResource = argumentTable->FindChild("table");
    const SDObject *tableIndex = argumentTable->FindChild("index");
    const SDObject *tableId = computeTable ? computeTable->FindChild("Table") : NULL;
    const SDObject *selectedBuffer = argumentSelect ? argumentSelect->FindChild("argumentBuffer") : NULL;
    const SDObject *boundBuffer = computeArgumentBind ? computeArgumentBind->FindChild("buffer") : NULL;
    if(!kernel || !createdFunction || !createdEncoder || !selectedEncoder || !tableEncoder ||
       !tableResource || !tableIndex || !tableId || !selectedBuffer || !boundBuffer ||
       kernel->AsResourceId() != createdFunction->AsResourceId() ||
       createdEncoder->AsResourceId() != selectedEncoder->AsResourceId() ||
       createdEncoder->AsResourceId() != tableEncoder->AsResourceId() ||
       tableResource->AsResourceId() != tableId->AsResourceId() ||
       selectedBuffer->AsResourceId() != boundBuffer->AsResourceId() ||
       tableIndex->AsUInt64() != 0)
      return fail("argument visible-function-table graph or binding incorrect");
  }
  const SDChunk *handle = NULL, *table = NULL, *update = NULL, *bind = NULL;
  const SDChunk *bindRange = NULL;
  const SDChunk *vertexBind = NULL;
  const SDChunk *vertexBindRange = NULL;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLRenderPipelineState::functionHandleWithFunction") handle = chunk;
    if(chunk->name == "MTLRenderPipelineState::newVisibleFunctionTableWithDescriptor") table = chunk;
    if(chunk->name == "MTLVisibleFunctionTable::setFunction") update = chunk;
    if(chunk->name == "MTLRenderCommandEncoder::setFragmentVisibleFunctionTable") bind = chunk;
    if(chunk->name == "MTLRenderCommandEncoder::setFragmentVisibleFunctionTables") bindRange = chunk;
    if(chunk->name == "MTLRenderCommandEncoder::setVertexVisibleFunctionTable") vertexBind = chunk;
    if(chunk->name == "MTLRenderCommandEncoder::setVertexVisibleFunctionTables") vertexBindRange = chunk;
  }
  if(bind || bindRange || vertexBind || vertexBindRange)
  {
    const SDObject *bound = (bind || vertexBind) ?
        (bind ? bind : vertexBind)->FindChild("table") :
        (bindRange ? bindRange : vertexBindRange)->FindChild("tables");
    if(bindRange || vertexBindRange)
      bound = bound && bound->NumChildren() == 1 ? bound->GetChild(0) : NULL;
    if(!handle || !table || !update ||
       !handle->FindChild("Handle") || !table->FindChild("Table") ||
       !update->FindChild("function") || !update->FindChild("Table") ||
       !bound ||
       handle->FindChild("Handle")->AsResourceId() !=
           update->FindChild("function")->AsResourceId() ||
       table->FindChild("Table")->AsResourceId() !=
           update->FindChild("Table")->AsResourceId() ||
       table->FindChild("Table")->AsResourceId() !=
           bound->AsResourceId())
      return fail("visible function handle/table/binding identity incorrect");
    return PixelMatches(renderer, colorTarget, draw->eventId, 200, 150,
                        (vertexBind || vertexBindRange) ? 0.0f : 1.0f,
                        (vertexBind || vertexBindRange) ? 1.0f : 0.0f, 0.0f);
  }
  return PixelMatches(renderer, colorTarget, draw->eventId, 200, 150,
                       3.0f / 255.0f, 3.0f / 255.0f, 24.0f / 255.0f);
}

static bool ValidateICBOperationsFixture(IReplayController *renderer, ResourceId colorTarget)
{
  const SDFile &file = renderer->GetStructuredFile();
  uint32_t counts[4] = {};
  for(const SDChunk *chunk : file.chunks)
  {
    if(chunk->metadata.chunkID >= 1224 && chunk->metadata.chunkID <= 1226)
      counts[chunk->metadata.chunkID - 1224]++;
    if(chunk->metadata.chunkID == 1270) counts[3]++;
  }
  if(!counts[3]) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T53 ICB operations validation failed: %s\n", message); return false;
  };
  if(counts[0] != 2 || counts[1] != 2 || counts[2] != 3 || counts[3] != 2)
    return fail("reset/copy/optimize/command reset counts");
  ResourceId params[5], tint, indices;
  for(const auto &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 16) tint = buffer.resourceId;
    if(buffer.length == 18) indices = buffer.resourceId;
    if(buffer.length != 80) continue;
    bytebuf bytes = renderer->GetBufferData(buffer.resourceId, 0, 0);
    uint32_t prefix; memcpy(&prefix, bytes.data(), 4);
    if((prefix & 0xffffff00) != 0x53000000 || (prefix & 15) || (prefix & 255) / 16 >= 5)
      return fail("packet identity");
    unsigned i = (prefix & 255) / 16;
    params[i] = buffer.resourceId;
    for(unsigned j = 0; j < 8; ++j)
    {
      uint32_t value; memcpy(&value, bytes.data() + 48 + j * 4, 4);
      if(value != 0x53ff0000 + i * 16 + j) return fail("packet padding");
    }
  }
  for(auto id : params) if(id == ResourceId()) return fail("all encoded buffers retained");
  if(tint == ResourceId() || indices == ResourceId()) return fail("index/tint buffer");
  rdcarray<const ActionDescription *> actions, groups, draws, empty;
  rdcarray<uint32_t> gpuEvents;
  FindActions(renderer->GetRootActions(), actions);
  for(const auto *action : actions)
  {
    if(action->flags & ActionFlags::MultiAction) groups.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
    if(action->customName.contains("empty command")) empty.push_back(action);
    for(const auto &event : action->events)
      if(file.chunks[event.chunkIndex]->metadata.chunkID >= 1224 &&
         file.chunks[event.chunkIndex]->metadata.chunkID <= 1226) gpuEvents.push_back(event.eventId);
  }
  if(groups.size() != 4 || draws.size() != 9 || empty.size() != 5 || gpuEvents.size() != 7)
    return fail("group/draw/empty/API counts");
  const unsigned lengths[] = {1,6,3,4}, expected[] = {0,1,2,3,1,3,1,1,3};
  for(unsigned i = 0; i < 4; ++i)
    if(groups[i]->children.size() != lengths[i]) return fail("one child per ICB slot");
  ResourceId pipelines[2];
  for(unsigned i = 0; i < 9; ++i)
  {
    const auto *draw = draws[i];
    renderer->SetFrameEvent(draw->eventId, true);
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    if(!state || !renderer->GetFatalErrorStatus().OK() || draw->numIndices != 6 ||
       draw->numInstances != 1 || !(draw->flags & ActionFlags::Indirect) ||
       bool(draw->flags & ActionFlags::Indexed) != (expected[i] == 2) ||
       state->vertexBuffers.size() < 2 || state->vertexBuffers[0].resourceId != params[expected[i]] ||
       state->vertexBuffers[0].byteOffset != 16 || state->vertexBuffers[1].resourceId != tint ||
       state->vertexBuffers[1].byteOffset != 0)
    {
      fprintf(stderr, "T53 draw %u event %u count %u instances %u indexed %u vertex slots %zu storage slots %zu\n",
              i, draw->eventId, draw->numIndices, draw->numInstances,
              unsigned(bool(draw->flags & ActionFlags::Indexed)), state ? state->vertexBuffers.size() : 0,
              state ? state->vertexStorageBuffers.size() : 0);
      if(state) for(size_t slot = 0; slot < state->vertexBuffers.size(); ++slot)
        fprintf(stderr, " slot %zu offset %llu param match %u tint match %u\n", slot,
                (unsigned long long)state->vertexBuffers[slot].byteOffset,
                unsigned(state->vertexBuffers[slot].resourceId == params[expected[i]]),
                unsigned(state->vertexBuffers[slot].resourceId == tint));
      return fail("copied draw/binding/indexed state");
    }
    unsigned pipeline = expected[i] == 2 ? 1 : 0;
    if(pipelines[pipeline] != ResourceId() && pipelines[pipeline] != state->pipelineResourceId)
      return fail("copied pipeline identity");
    pipelines[pipeline] = state->pipelineResourceId;
    if(pipeline && (pipe.GetIBuffer().resourceId != indices || pipe.GetIBuffer().byteOffset != 4 ||
                     pipe.GetIBuffer().byteSize != 12 || pipe.GetIBuffer().byteStride != 2))
      return fail("copied index range");
  }
  if(pipelines[0] == ResourceId() || pipelines[0] == pipelines[1]) return fail("two pipelines");
  for(const auto *action : empty)
    if(action->flags & (ActionFlags::Drawcall | ActionFlags::Dispatch)) return fail("empty became draw");
  // Later GPU resets/copies must not change the initial magenta command on rewind.
  for(unsigned stage : {3U,0U,2U,1U,0U,3U,1U})
    for(unsigned band = 0; band < 3; ++band)
      if(!PixelMatches(renderer, colorTarget, groups[stage]->eventId, (2 * band + 1) * 400 / 6, 150,
                        stage == 0 || band == 0 ? 1 : 0,
                        stage == 1 && band == 1 ? 1 : 0,
                        stage == 0 || band == 2 ? 1 : 0)) return fail("GPU result or baseline rewind");
  for(uint32_t event : gpuEvents)
  {
    renderer->SetFrameEvent(event, true);
    if(!renderer->GetFatalErrorStatus().OK()) return fail("GPU operation seek");
  }
  if(HasUsage(renderer, params[4], ResourceUsage::VS_Constants)) return fail("reset yellow still used");
  return HasUsage(renderer, indices, ResourceUsage::IndexBuffer) &&
         PixelMatches(renderer, colorTarget, draws.back()->eventId, 200, 150, 0, 0, 0);
}

static bool ValidateAsyncCreationFixture(IReplayController *renderer, ResourceId colorTarget)
{
  uint32_t counts[6] = {};
  for(const auto *chunk : renderer->GetStructuredFile().chunks)
    if(chunk->metadata.chunkID >= 1264 && chunk->metadata.chunkID <= 1269)
      counts[chunk->metadata.chunkID - 1264]++;
  bool isT51 = false;
  if(counts[0])
    for(const auto &buffer : renderer->GetBuffers())
      if(buffer.length == 428) isT51 = true;
  if(!isT51) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T51 async creation validation failed: %s\n", message); return false;
  };
  for(uint32_t count : counts) if(count != 1) return fail("six async creation chunks");
  ResourceId output;
  for(const auto &buffer : renderer->GetBuffers()) if(buffer.length == 428) output = buffer.resourceId;
  uint32_t pipelines = 0, shaders = 0, libraries = 0;
  for(const auto &resource : renderer->GetResources())
  {
    if(resource.type == ResourceType::PipelineState) pipelines++;
    if(resource.type == ResourceType::Shader) shaders++;
    if(resource.type == ResourceType::Pool) libraries++;
  }
  rdcarray<const ActionDescription *> actions, dispatches, draws;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *fill = NULL;
  for(const auto *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
    if(action->customName.contains("fillBuffer")) fill = action;
  }
  if(pipelines != 5 || shaders != 3 || libraries != 1 || output == ResourceId() || !fill ||
     dispatches.size() != 3 || draws.size() != 2) return fail("resource/action counts");
  const uint32_t biases[] = {31, 83, 127};
  ResourceId computePipelines[3], renderPipelines[2];
  for(uint32_t event : {dispatches[2]->eventId, dispatches[0]->eventId, fill->eventId,
                        dispatches[1]->eventId, draws[1]->eventId, dispatches[2]->eventId})
  {
    renderer->SetFrameEvent(event, true);
    if(!renderer->GetFatalErrorStatus().OK()) return fail("seek");
    bytebuf bytes = renderer->GetBufferData(output, 0, 0);
    if(bytes.size() != 428) return fail("buffer length");
    for(uint32_t i = 0; i < 107; i++)
    {
      uint32_t value; memcpy(&value, bytes.data() + 4 * i, 4);
      if(value != (i < 96 && event >= dispatches[i / 32]->eventId ? biases[i / 32] + i % 32 : 0))
        return fail("result, padding or rewind");
    }
  }
  for(uint32_t i = 0; i < 3; i++)
  {
    renderer->SetFrameEvent(dispatches[i]->eventId, true);
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    const auto *reflection = pipe.GetShaderReflection(ShaderStage::Compute);
    if(!state || !reflection || reflection->entryPoint != "cs_async" ||
       reflection->constantBlocks.size() != 1 || reflection->constantBlocks[0].byteSize != 4 ||
       state->computeBuffers.size() < 2 || state->computeBuffers[0].resourceId != output ||
       state->computeBuffers[0].byteOffset != i * 128) return fail("compute reflection/binding");
    computePipelines[i] = state->computePipelineResourceId;
  }
  for(uint32_t i = 0; i < 2; i++)
  {
    renderer->SetFrameEvent(draws[i]->eventId, true);
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    const auto *reflection = pipe.GetShaderReflection(ShaderStage::Fragment);
    if(!state || !reflection || reflection->entryPoint != "fs_async") return fail("render reflection");
    renderPipelines[i] = state->pipelineResourceId;
  }
  if(computePipelines[0] == ResourceId() || computePipelines[0] == computePipelines[1] ||
     computePipelines[0] == computePipelines[2] || computePipelines[1] == computePipelines[2] ||
     renderPipelines[0] == ResourceId() || renderPipelines[0] == renderPipelines[1])
    return fail("distinct pipeline identities");
  return HasUsage(renderer, output, ResourceUsage::CS_RWResource) &&
         HasUsage(renderer, output, ResourceUsage::PS_Resource) &&
         PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 150, 31.0f / 255, 83.0f / 255, 127.0f / 255) &&
         PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 150, 0, 0, 0) &&
         PixelMatches(renderer, colorTarget, draws[1]->eventId, 300, 150, 31.0f / 255, 83.0f / 255, 127.0f / 255);
}

static bool ValidateFunctionVariantsFixture(IReplayController *renderer, ResourceId colorTarget)
{
  uint32_t counts[4] = {};
  for(const auto *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->metadata.chunkID == 1040) counts[0]++;
    if(chunk->metadata.chunkID == 1041) counts[1]++;
    if(chunk->metadata.chunkID == 1272) counts[2]++;
    if(chunk->metadata.chunkID == 1273) counts[3]++;
  }
  if(!counts[2] && !counts[3]) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T55 function variants validation failed: %s\n",message); return false;
  };
  for(uint32_t count : counts) if(count != 1) return fail("four creation paths");
  ResourceId output;
  for(const auto &buffer : renderer->GetBuffers()) if(buffer.length == 548) output = buffer.resourceId;
  rdcarray<const ActionDescription *> actions, dispatches, draws;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *fill = NULL;
  for(const auto *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
    if(action->customName.contains("fillBuffer")) fill = action;
  }
  if(!fill || output == ResourceId() || dispatches.size() != 4 || draws.size() != 1)
    return fail("resource/action counts");
  const uint32_t starts[] = {17,43,79,127};
  ResourceId pipelines[4], shaders[4];
  for(uint32_t slot : {3U,0U,2U,1U,0U,3U})
  {
    renderer->SetFrameEvent(dispatches[slot]->eventId,true);
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    const auto *reflection = pipe.GetShaderReflection(ShaderStage::Compute);
    if(!renderer->GetFatalErrorStatus().OK() || !state || !reflection ||
       reflection->readWriteResources.size() != 1 || state->computeBuffers.empty() ||
       state->computeBuffers[0].resourceId != output || state->computeBuffers[0].byteOffset != slot*128 ||
       (reflection->entryPoint != "cs_function_variant" && reflection->entryPoint != "cs_function_named"))
      return fail("specialized shader, reflection, binding or seek");
    pipelines[slot] = state->computePipelineResourceId;
    shaders[slot] = reflection->resourceId;
    const bytebuf data = renderer->GetBufferData(output,0,0);
    if(data.size() != 548) return fail("output length");
    for(uint32_t i = 0; i < 137; i++)
    {
      uint32_t actual; memcpy(&actual,data.data()+4*i,4);
      if(actual != (i < 128 && i/32 <= slot ? starts[i/32]+3*(i%32) : 0))
        return fail("specialization values, padding or rewind");
    }
  }
  for(uint32_t i = 0; i < 4; i++)
    for(uint32_t j = i+1; j < 4; j++)
      if(pipelines[i] == ResourceId() || shaders[i] == ResourceId() ||
         pipelines[i] == pipelines[j] || shaders[i] == shaders[j])
        return fail("distinct specialized identities");
  renderer->SetFrameEvent(fill->eventId,true);
  const bytebuf cleared = renderer->GetBufferData(output,0,0);
  for(byte value : cleared) if(value) return fail("clear rewind");
  return HasUsage(renderer,output,ResourceUsage::CS_RWResource) &&
         HasUsage(renderer,output,ResourceUsage::PS_Resource) &&
         PixelMatches(renderer,colorTarget,draws[0]->eventId,200,150,17.0f/255,43.0f/255,79.0f/255);
}

static bool ValidateEventSyncFixture(IReplayController *renderer, ResourceId colorTarget)
{
  bool fixture = false;
  const auto &file = renderer->GetStructuredFile();
  for(const auto *chunk : file.chunks) fixture |= chunk->metadata.chunkID == 1032;
  if(!fixture) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T52 event sync validation failed: %s\n", message); return false;
  };
  ResourceId input, output;
  for(const auto &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 436) input = buffer.resourceId;
    if(buffer.length == 444) output = buffer.resourceId;
  }
  uint32_t syncCount = 0;
  for(const auto &resource : renderer->GetResources()) if(resource.type == ResourceType::Sync) syncCount++;
  rdcarray<const ActionDescription *> actions, dispatches, draws, fills;
  rdcarray<uint32_t> events;
  FindActions(renderer->GetRootActions(), actions);
  for(const auto *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
    if(action->customName.contains("fillBuffer")) fills.push_back(action);
    for(const auto &event : action->events)
      if(file.chunks[event.chunkIndex]->metadata.chunkID == 1062 ||
         file.chunks[event.chunkIndex]->metadata.chunkID == 1063) events.push_back(event.eventId);
  }
  if(syncCount != 2 || events.size() != 6 || input == ResourceId() || output == ResourceId() ||
     fills.size() != 2 || dispatches.size() != 1 || draws.size() != 1)
    return fail("resource/actions/event counts");
  for(uint32_t event : {events.back(), fills[1]->eventId, dispatches[0]->eventId,
                        events[1], draws[0]->eventId, fills[1]->eventId, events.back()})
  {
    renderer->SetFrameEvent(event, true);
    if(!renderer->GetFatalErrorStatus().OK()) return fail("signal/wait seek");
    bytebuf bytes = renderer->GetBufferData(input, 0, 0);
    if(bytes.size() != 436) return fail("input size");
    for(byte value : bytes) if(value != 3) return fail("producer GPU data");
    // Before the consumer clear, output contents are unspecified; only inspect after that clear.
    if(event >= fills[1]->eventId)
    {
      bytes = renderer->GetBufferData(output, 0, 0);
      if(bytes.size() != 444) return fail("output size");
      for(uint32_t i = 0; i < 111; i++)
      {
        uint32_t value; memcpy(&value, bytes.data() + 4 * i, 4);
        if(value != (i < 96 && event >= dispatches[0]->eventId ? 51 + i : 0))
          return fail("consumer GPU data, padding or rewind");
      }
    }
  }
  return HasUsage(renderer, input, ResourceUsage::CS_Resource) &&
         HasUsage(renderer, output, ResourceUsage::CS_RWResource) &&
         HasUsage(renderer, output, ResourceUsage::PS_Resource) &&
         PixelMatches(renderer, colorTarget, draws[0]->eventId, 200, 150, 51.0f / 255, 83.0f / 255, 115.0f / 255);
}

static bool ValidateSharedEventFixture(IReplayController *renderer, ResourceId colorTarget)
{
  const auto &file = renderer->GetStructuredFile();
  unsigned creations = 0, imports = 0, hostInitializations = 0, signals = 0, waits = 0;
  for(const SDChunk *chunk : file.chunks)
  {
    creations += chunk->metadata.chunkID == 1033;
    imports += chunk->metadata.chunkID == 1034;
    hostInitializations += chunk->metadata.chunkID == 1283;
    signals += chunk->metadata.chunkID == 1063;
    waits += chunk->metadata.chunkID == 1062;
  }
  if(!creations) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T65 shared event validation failed: %s\n", message); return false;
  };
  if(creations != 1 || imports > 1 || hostInitializations != 1 || signals != 2 || waits != 3)
    return fail("shared event creation, host initial value or five GPU synchronization commands");
  ResourceId colour;
  for(const auto &buffer : renderer->GetBuffers())
    if(buffer.length == 256) colour = buffer.resourceId;
  unsigned syncCount = 0;
  for(const auto &resource : renderer->GetResources())
    syncCount += resource.type == ResourceType::Sync;
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(colour == ResourceId() || syncCount != 1 + imports || draws.size() != 2 ||
     !HasUsage(renderer, colour, ResourceUsage::PS_Resource))
    return fail("shared event, sampled buffer or two draws");
  for(unsigned phase : {1U, 0U, 1U, 0U, 1U})
  {
    const uint8_t expected = phase ? 80 : 10;
    renderer->SetFrameEvent(draws[phase]->eventId, true);
    if(!renderer->GetFatalErrorStatus().OK()) return fail("seek across shared-event submissions");
    const auto *state = renderer->GetPipelineState().GetMetalPipelineState();
    if(!state || state->fragmentBuffers.empty() ||
       state->fragmentBuffers[0].resourceId != colour)
      return fail("fragment buffer identity after seek");
    bytebuf bytes = renderer->GetBufferData(colour, 0, 0);
    if(bytes.size() != 256) return fail("colour buffer size");
    for(byte value : bytes) if(value != expected) return fail("GPU fill after seek");
    if(!PixelMatches(renderer, colorTarget, draws[phase]->eventId, 200, 150,
                     expected / 255.0f, expected / 255.0f, expected / 255.0f))
      return fail("shared-event dependent draw pixel after seek");
  }
  return true;
}

static bool ValidateTessellationFixture(IReplayController *renderer, ResourceId colorTarget)
{
  unsigned factorBuffers = 0, scales = 0, patchDraws = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->metadata.chunkID >= 1156 && chunk->metadata.chunkID <= 1158)
      return true; // T67 has its own three-variant validator below.
    factorBuffers += chunk->name == "MTLRenderCommandEncoder::setTessellationFactorBuffer";
    scales += chunk->name == "MTLRenderCommandEncoder::setTessellationFactorScale";
    patchDraws += chunk->name == "MTLRenderCommandEncoder::drawPatches";
  }
  if(!factorBuffers && !scales && !patchDraws) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T66 tessellation validation failed: %s\n", message); return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(factorBuffers != 1 || scales != 1 || patchDraws != 1 || draws.size() != 1 ||
     !clear ||
     draws[0]->customName.find("drawPatches") == -1 ||
     draws[0]->numIndices != 3 || draws[0]->numInstances != 1)
    return fail("factor state, patch chunk, or action metadata");
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 200, 150, 0, 0, 0) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 200, 150, .25f, .5f, .75f) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 200, 150, 0, 0, 0) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 200, 150, .25f, .5f, .75f))
    return fail("clear/draw/rewind pixels");
  return true;
}

static bool ValidateTessellationVariantsFixture(IReplayController *renderer,
                                                ResourceId colorTarget)
{
  unsigned variants[3] = {};
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    if(chunk->metadata.chunkID >= 1156 && chunk->metadata.chunkID <= 1158)
      variants[chunk->metadata.chunkID - 1156]++;
  if(!variants[0] && !variants[1] && !variants[2]) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T67 tessellation variants validation failed: %s\n", message); return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(!clear || variants[0] != 1 || variants[1] != 1 || variants[2] != 1 || draws.size() != 3 ||
     !(draws[0]->flags & ActionFlags::Indirect) ||
     !(draws[1]->flags & ActionFlags::Indexed) ||
     (draws[1]->flags & ActionFlags::Indirect) ||
     !(draws[2]->flags & ActionFlags::Indexed) ||
     !(draws[2]->flags & ActionFlags::Indirect) ||
     draws[0]->numIndices != 0 || draws[0]->numInstances != 0 ||
     draws[1]->numIndices != 3 || draws[1]->numInstances != 1 ||
     draws[2]->numIndices != 0 || draws[2]->numInstances != 0)
    return fail("three draw variants or GPU-unknown indirect metadata");
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 67, 150, 0, 0, 0) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 67, 150, 1, 0, 0) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 200, 150, 0, 1, 0) ||
     !PixelMatches(renderer, colorTarget, draws[2]->eventId, 333, 150, 0, 0, 1) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 200, 150, 0, 0, 0) ||
     !PixelMatches(renderer, colorTarget, draws[2]->eventId, 333, 150, 0, 0, 1))
    return fail("clear, three colors, or backward seek");
  return true;
}

static bool ValidateTextureReadbackFixture(IReplayController *renderer, ResourceId colorTarget)
{
  const auto &file = renderer->GetStructuredFile();
  bool fixture = false;
  for(const SDChunk *chunk : file.chunks)
    fixture |= chunk->metadata.chunkID == 1072 || chunk->metadata.chunkID == 1073;
  if(!fixture) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T50 texture CPU readback validation failed: %s\n", message);
    return false;
  };
  ResourceId plain, array, readOnly, syncOnly, volume, parameters;
  for(const auto &texture : renderer->GetTextures())
  {
    if(texture.width == 11 && texture.height == 7 && texture.mips == 2) plain = texture.resourceId;
    if(texture.width == 18 && texture.height == 10 && texture.arraysize == 2 && texture.mips == 2)
      array = texture.resourceId;
    if(texture.width == 7 && texture.height == 5) readOnly = texture.resourceId;
    if(texture.width == 19 && texture.height == 3) syncOnly = texture.resourceId;
    if(texture.width == 8 && texture.height == 4 && texture.depth == 2) volume = texture.resourceId;
  }
  for(const auto &buffer : renderer->GetBuffers())
    if(buffer.length == 68) parameters = buffer.resourceId;
  rdcarray<const ActionDescription *> actions, draws;
  rdcarray<uint32_t> reads, syncs;
  FindActions(renderer->GetRootActions(), actions);
  for(const auto *action : actions)
  {
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
    for(const APIEvent &event : action->events)
    {
      uint32_t id = file.chunks[event.chunkIndex]->metadata.chunkID;
      if(id == 1072 || id == 1073) reads.push_back(event.eventId);
      if(id == 1205) syncs.push_back(event.eventId);
    }
  }
  if(plain == ResourceId() || array == ResourceId() || readOnly == ResourceId() ||
     volume == ResourceId() || syncOnly == ResourceId() || parameters == ResourceId() || draws.size() != 1 ||
     reads.size() != 4 || syncs.size() != 3 || syncs.back() >= reads.front() ||
     reads.back() >= draws[0]->eventId)
    return fail("resources, event counts or CPU read ordering");
  auto textureMatches = [&](ResourceId texture, Subresource sub, size_t pixels, byte value) {
    bytebuf bytes = renderer->GetTextureData(texture, sub);
    if(bytes.size() != pixels * 4) return false;
    for(size_t i = 0; i < bytes.size(); i++)
      if(bytes[i] != (i % 4 == 3 && value ? 255 : value)) return false;
    return true;
  };
  const uint32_t expected[] = {43, 79, 113, 151, 152};
  for(uint32_t event : {draws[0]->eventId, reads.front(), reads.back(), draws[0]->eventId,
                        syncs.back(), draws[0]->eventId})
  {
    renderer->SetFrameEvent(event, true);
    if(!renderer->GetFatalErrorStatus().OK()) return fail("event seek");
    bytebuf bytes = renderer->GetBufferData(parameters, 0, 0);
    if(bytes.size() != 68) return fail("parameter buffer size");
    for(uint32_t i = 0; i < 17; i++)
    {
      uint32_t value;
      memcpy(&value, bytes.data() + i * 4, 4);
      if(value != (i < 5 && event == draws[0]->eventId ? expected[i] : 0))
        return fail("CPU-derived GPU input, padding or rewind");
    }
    if(!textureMatches(plain, {1, 0, 0}, 5 * 3, 43) ||
       !textureMatches(array, {1, 1, 0}, 9 * 5, 79) ||
       !textureMatches(readOnly, {0, 0, 0}, 7 * 5, 113) ||
       !textureMatches(syncOnly, {0, 0, 0}, 19 * 3, 197) ||
       !textureMatches(plain, {0, 0, 0}, 11 * 7, 0) ||
       !textureMatches(array, {1, 0, 0}, 9 * 5, 0))
      return fail("mip/slice GPU contents or untouched subresource");
  }
  if(!HasUsage(renderer, parameters, ResourceUsage::PS_Resource)) return fail("GPU parameter usage");
  return PixelMatches(renderer, colorTarget, draws[0]->eventId, 200, 150,
                       43.0f / 255, 79.0f / 255, 113.0f / 255);
}

static bool ValidateCommandHandlersFixture(IReplayController *renderer, ResourceId colorTarget)
{
  const auto &file = renderer->GetStructuredFile();
  uint32_t scheduled = 0, completed = 0, updates = 0;
  for(const SDChunk *chunk : file.chunks)
  {
    if(chunk->metadata.chunkID == 1049) scheduled++;
    if(chunk->metadata.chunkID == 1054) completed++;
    if(chunk->metadata.chunkID == 1198) updates++;
  }
  if(!scheduled && !completed) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T49 command handler validation failed: %s\n", message);
    return false;
  };
  ResourceId producer, parameters, output;
  for(const auto &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 300) producer = buffer.resourceId;
    if(buffer.length == 12) parameters = buffer.resourceId;
    if(buffer.length == 412) output = buffer.resourceId;
  }
  rdcarray<const ActionDescription *> actions, dispatches, draws, fills;
  FindActions(renderer->GetRootActions(), actions);
  uint32_t handlerEvents = 0;
  for(const auto *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
    if(action->customName.contains("fillBuffer")) fills.push_back(action);
    for(const APIEvent &event : action->events)
      if(file.chunks[event.chunkIndex]->metadata.chunkID == 1049 ||
         file.chunks[event.chunkIndex]->metadata.chunkID == 1054)
        handlerEvents++;
  }
  if(scheduled != 4 || completed != 4 || updates < 1 || handlerEvents != 8 ||
     producer == ResourceId() || parameters == ResourceId() || output == ResourceId() ||
     dispatches.size() != 2 || draws.size() != 1 || fills.size() != 3)
    return fail("registration events, CPU update or action/resource counts");
  const uint32_t biases[] = {41, 67, 101};
  const uint32_t initialParameters[] = {7, 11, 13};
  for(uint32_t event : {dispatches[1]->eventId, fills[2]->eventId, draws[0]->eventId,
                        dispatches[0]->eventId, fills[1]->eventId,
                        dispatches[1]->eventId, draws[0]->eventId})
  {
    renderer->SetFrameEvent(event, true);
    if(!renderer->GetFatalErrorStatus().OK()) return fail("event seek failed");
    bytebuf bytes = renderer->GetBufferData(producer, 0, 0);
    if(bytes.size() != 300) return fail("producer size");
    for(byte value : bytes) if(value != 29) return fail("producer fill contents");
    // The CPU update is replayed before the consumer command buffer is encoded; no block runs.
    bytes = renderer->GetBufferData(parameters, 0, 0);
    if(bytes.size() != 12) return fail("callback parameter size");
    for(uint32_t i = 0; i < 3; i++)
    {
      uint32_t value;
      memcpy(&value, bytes.data() + 4 * i, 4);
      if(value != (event >= fills[2]->eventId ? biases[i] : initialParameters[i]))
        return fail("callback CPU update or rewind");
    }
    bytes = renderer->GetBufferData(output, 0, 0);
    if(bytes.size() != 412) return fail("output size");
    for(uint32_t i = 0; i < 103; i++)
    {
      uint32_t value;
      memcpy(&value, bytes.data() + 4 * i, 4);
      uint32_t expected = 0;
      if(i < 96 && event >= dispatches[1]->eventId) expected = biases[i / 32] + i % 32;
      else if(i < 96 && event >= dispatches[0]->eventId && event < fills[2]->eventId)
        expected = initialParameters[i / 32] + i % 32;
      if(value != expected)
        return fail("consumer data, padding or rewind");
    }
  }
  renderer->SetFrameEvent(dispatches[0]->eventId, true);
  const auto *reflection = renderer->GetPipelineState().GetShaderReflection(ShaderStage::Compute);
  const auto *state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!reflection || reflection->entryPoint != "cs_handlers" ||
     reflection->constantBlocks.size() != 1 || reflection->constantBlocks[0].byteSize != 12 ||
     !state || state->computeBuffers.size() < 2 || state->computeBuffers[1].resourceId != parameters ||
     state->computeBuffers[1].byteSize != 12)
    return fail("callback parameter reflection");
  if(!HasUsage(renderer, output, ResourceUsage::CS_RWResource) ||
     !HasUsage(renderer, output, ResourceUsage::PS_Resource))
    return fail("resource usage");
  if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, 200, 150,
                   41.0f / 255, 67.0f / 255, 101.0f / 255))
    return fail("final pixel");
  return true;
}

static bool ValidateBinaryLibraryFixture(IReplayController *renderer, ResourceId colorTarget)
{
  uint32_t binaryLoads = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    if(chunk->metadata.chunkID >= 1014 && chunk->metadata.chunkID <= 1018) binaryLoads++;
  if(!binaryLoads) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T48 binary library validation failed: %s\n", message);
    return false;
  };
  ResourceId output;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 676) output = buffer.resourceId;
  uint32_t libraries = 0, shaders = 0;
  for(const ResourceDescription &resource : renderer->GetResources())
  {
    if(resource.type == ResourceType::Pool) libraries++;
    if(resource.type == ResourceType::Shader)
    {
      shaders++;
      if(resource.parentResources.size() != 1) return fail("shader library dependency missing");
    }
  }
  rdcarray<const ActionDescription *> actions, dispatches;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *draw = NULL, *fill = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draw = action;
    if(action->customName.contains("fillBuffer")) fill = action;
  }
  if(binaryLoads != 5 || libraries != 5 || shaders != 7 || output == ResourceId() ||
     dispatches.size() != 5 || !draw || !fill) return fail("library, shader or action counts");
  const uint32_t biases[] = {29, 61, 97, 137, 173};
  auto checkData = [&](uint32_t event) {
    renderer->SetFrameEvent(event, true);
    if(!renderer->GetFatalErrorStatus().OK()) return fail("event seek failed");
    bytebuf bytes = renderer->GetBufferData(output, 0, 0);
    if(bytes.size() != 676) return fail("output size");
    for(uint32_t i = 0; i < 169; i++)
    {
      uint32_t value;
      memcpy(&value, bytes.data() + i * 4, 4);
      uint32_t expected = i < 160 && event >= dispatches[i / 32]->eventId ?
                              biases[i / 32] + i % 32 : 0;
      if(value != expected) return fail("binary shader output, padding or event rewind");
    }
    return true;
  };
  ResourceId shaderIDs[5];
  for(uint32_t i : {0U, 1U, 2U, 3U, 4U, 0U, 3U, 1U, 4U})
  {
    if(!checkData(dispatches[i]->eventId)) return false;
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    const auto *reflection = pipe.GetShaderReflection(ShaderStage::Compute);
    if(!reflection || reflection->entryPoint != "cs_binary" || !reflection->rawBytes.empty() ||
       !reflection->debugInfo.files.empty() || reflection->readWriteResources.size() != 1 ||
       reflection->constantBlocks.size() != 1 || reflection->constantBlocks[0].byteSize != 4 ||
       !state || state->computeBuffers.size() < 2 || state->computeBuffers[0].resourceId != output ||
       state->computeBuffers[0].byteOffset != i * 128 || state->computeBuffers[1].byteSize != 4)
      return fail("binary compute reflection, source availability or bindings");
    shaderIDs[i] = reflection->resourceId;
  }
  for(uint32_t i = 0; i < 5; i++)
    for(uint32_t j = i + 1; j < 5; j++)
      if(shaderIDs[i] == shaderIDs[j]) return fail("library function identities aliased");
  if(!checkData(fill->eventId) || !checkData(draw->eventId)) return false;
  const auto &pipe = renderer->GetPipelineState();
  const auto *vs = pipe.GetShaderReflection(ShaderStage::Vertex);
  const auto *fs = pipe.GetShaderReflection(ShaderStage::Fragment);
  if(!vs || !fs || vs->entryPoint != "vs_binary" || fs->entryPoint != "fs_binary" ||
     fs->readOnlyResources.size() != 1 || !vs->rawBytes.empty() || !fs->rawBytes.empty())
    return fail("binary render reflection");
  return HasUsage(renderer, output, ResourceUsage::CS_RWResource) &&
         HasUsage(renderer, output, ResourceUsage::PS_Resource) &&
         PixelMatches(renderer, colorTarget, draw->eventId, 200, 150,
                      29.0f / 255, 61.0f / 255, 97.0f / 255);
}

static bool ValidateBinaryArchiveFixture(IReplayController *renderer, ResourceId colorTarget)
{
  const SDChunk *archive = NULL;
  uint32_t archiveCount = 0, computeCount = 0, renderCount = 0;
  ResourceId archiveId;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newBinaryArchiveWithDescriptor")
    {
      archive = chunk;
      archiveCount++;
    }
  }
  if(!archiveCount) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T301 binary archive validation failed: %s\n", message);
    return false;
  };
  if(archiveCount != 1 || !archive->FindChild("Archive") || !archive->FindChild("data"))
    return fail("archive chunk");
  archiveId = archive->FindChild("Archive")->AsResourceId();
  if(archiveId == ResourceId()) return fail("archive identity");
  const SDChunk *meshAdd = NULL, *meshPipeline = NULL, *writerPipeline = NULL;
  uint32_t meshAdds = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLBinaryArchive::addMeshRenderPipelineFunctionsWithDescriptor")
    {
      meshAdd = chunk;
      meshAdds++;
    }
    if(chunk->name == "MTLDevice::newRenderPipelineStateWithMeshDescriptor")
      meshPipeline = chunk;
    if(chunk->name == "MTLDevice::newComputePipelineStateWithDescriptor")
      writerPipeline = chunk;
  }
  if(meshAdds)
  {
    if(meshAdds != 1 || !meshPipeline || !writerPipeline ||
       !meshAdd->FindChild("BinaryArchive") ||
       meshAdd->FindChild("BinaryArchive")->AsResourceId() != archiveId ||
       !meshAdd->FindChild("mesh") || !meshAdd->FindChild("fragment") ||
       !meshPipeline->FindChild("meshFunction") ||
       !meshPipeline->FindChild("fragmentFunction") ||
       meshAdd->FindChild("mesh")->AsResourceId() !=
           meshPipeline->FindChild("meshFunction")->AsResourceId() ||
       meshAdd->FindChild("fragment")->AsResourceId() !=
           meshPipeline->FindChild("fragmentFunction")->AsResourceId())
      return fail("archive mesh mutation or source functions");
    const SDObject *descriptor = writerPipeline->FindChild("descriptor");
    const SDObject *archives = descriptor ? descriptor->FindChild("binaryArchives") : NULL;
    if(!archives || archives->NumChildren() != 1 ||
       archives->GetChild(0)->AsResourceId() != archiveId ||
       !writerPipeline->FindChild("optionsValue") ||
       writerPipeline->FindChild("optionsValue")->AsUInt64() != 4)
      return fail("archive mesh fixture compute dependency");
    const SDObject *meshArchives = meshPipeline->FindChild("binaryArchives");
    if(meshArchives && meshArchives->NumChildren())
    {
      if(meshArchives->NumChildren() != 1 ||
         meshArchives->GetChild(0)->AsResourceId() != archiveId ||
         !meshPipeline->FindChild("options") ||
         meshPipeline->FindChild("options")->AsUInt64() != 4)
        return fail("archive mesh pipeline dependency or miss policy");
      bool pipelineParent = false;
      const SDObject *pipelineId = meshPipeline->FindChild("PipelineState");
      for(const ResourceDescription &resource : renderer->GetResources())
        if(pipelineId && resource.resourceId == pipelineId->AsResourceId())
          for(ResourceId parent : resource.parentResources)
            pipelineParent |= parent == archiveId;
      if(!pipelineParent) return fail("archive mesh pipeline resource parent");
    }
    bool meshParent = false, fragmentParent = false;
    for(const ResourceDescription &resource : renderer->GetResources())
      if(resource.resourceId == archiveId)
        for(ResourceId parent : resource.parentResources)
        {
          meshParent |= parent == meshAdd->FindChild("mesh")->AsResourceId();
          fragmentParent |= parent == meshAdd->FindChild("fragment")->AsResourceId();
        }
    return (meshParent && fragmentParent) || fail("archive mesh source parents");
  }
  const SDChunk *tileAdd = NULL, *tilePipeline = NULL, *drawPipeline = NULL;
  uint32_t tileAdds = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLBinaryArchive::addTileRenderPipelineFunctionsWithDescriptor")
    {
      tileAdd = chunk;
      tileAdds++;
    }
    if(chunk->name == "MTLDevice::newRenderPipelineStateWithTileDescriptor")
      tilePipeline = chunk;
    if(chunk->name == "MTLDevice::newRenderPipelineStateWithDescriptor")
      drawPipeline = chunk;
  }
  if(tileAdds)
  {
    if(tileAdds != 1 || !tilePipeline || !drawPipeline ||
       !tileAdd->FindChild("BinaryArchive") ||
       tileAdd->FindChild("BinaryArchive")->AsResourceId() != archiveId ||
       !tileAdd->FindChild("function") || !tilePipeline->FindChild("tileFunction") ||
       tileAdd->FindChild("function")->AsResourceId() !=
           tilePipeline->FindChild("tileFunction")->AsResourceId())
      return fail("archive tile mutation or source function");
    const SDObject *descriptor = drawPipeline->FindChild("descriptor");
    const SDObject *archives = descriptor ? descriptor->FindChild("binaryArchives") : NULL;
    if(!archives || archives->NumChildren() != 1 ||
       archives->GetChild(0)->AsResourceId() != archiveId ||
       !drawPipeline->FindChild("optionsValue") ||
       drawPipeline->FindChild("optionsValue")->AsUInt64() != 4)
      return fail("archive tile fixture draw dependency");
    const SDObject *tileArchives = tilePipeline->FindChild("binaryArchives");
    if(tileArchives && tileArchives->NumChildren())
    {
      if(tileArchives->NumChildren() != 1 ||
         tileArchives->GetChild(0)->AsResourceId() != archiveId ||
         !tilePipeline->FindChild("options") ||
         tilePipeline->FindChild("options")->AsUInt64() != 4)
        return fail("archive tile pipeline dependency or miss policy");
      bool pipelineParent = false;
      const SDObject *pipelineId = tilePipeline->FindChild("PipelineState");
      for(const ResourceDescription &resource : renderer->GetResources())
        if(pipelineId && resource.resourceId == pipelineId->AsResourceId())
          for(ResourceId parent : resource.parentResources)
            pipelineParent |= parent == archiveId;
      if(!pipelineParent) return fail("archive tile pipeline resource parent");
    }
    bool sourceParent = false;
    for(const ResourceDescription &resource : renderer->GetResources())
      if(resource.resourceId == archiveId)
        for(ResourceId parent : resource.parentResources)
          sourceParent |= parent == tileAdd->FindChild("function")->AsResourceId();
    return sourceParent || fail("archive tile source parent");
  }
  const SDObject *emptyArchive = archive->FindChild("emptyArchive");
  if(emptyArchive && emptyArchive->AsBool())
  {
    uint32_t computeAdds = 0, renderAdds = 0;
    uint32_t functionAdds = 0;
    uint32_t libraryAdds = 0;
    ResourceId sourceLibrary;
    ResourceId stitchedSourceFunction;
    ResourceId addedFunctions[3];
    bool sawComputePipeline = false, sawRenderPipeline = false;
    for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    {
      bool addCompute = chunk->name ==
          "MTLBinaryArchive::addComputePipelineFunctionsWithDescriptor";
      bool addRender = chunk->name ==
          "MTLBinaryArchive::addRenderPipelineFunctionsWithDescriptor";
      bool addFunction = chunk->name ==
          "MTLBinaryArchive::addFunctionWithDescriptor:library:";
      bool addLibrary = chunk->name ==
          "MTLBinaryArchive::addLibraryWithDescriptor";
      if(addLibrary)
      {
        const SDObject *target = chunk->FindChild("BinaryArchive");
        const SDObject *function = chunk->FindChild("function");
        const SDObject *graph = chunk->FindChild("graphName");
        const SDObject *name = chunk->FindChild("functionName");
        if(!target || target->AsResourceId() != archiveId || !function ||
           !graph || graph->AsString() != "archive_stitched" ||
           !name || name->AsString() != "archive_visible" ||
           sawComputePipeline || sawRenderPipeline)
          return fail("archive stitched library identity or ordering");
        stitchedSourceFunction = function->AsResourceId();
        libraryAdds++;
      }
      if(addFunction)
      {
        const SDObject *target = chunk->FindChild("BinaryArchive");
        const SDObject *library = chunk->FindChild("library");
        const SDObject *name = chunk->FindChild("functionName");
        if(!target || target->AsResourceId() != archiveId || !library || !name ||
           name->AsString() != "archive_visible" || sawComputePipeline || sawRenderPipeline)
          return fail("archive visible function identity or ordering");
        sourceLibrary = library->AsResourceId();
        functionAdds++;
      }
      if(addCompute || addRender)
      {
        const SDObject *target = chunk->FindChild("BinaryArchive");
        const SDObject *descriptor = chunk->FindChild("descriptor");
        if(!target || target->AsResourceId() != archiveId || !descriptor ||
           (addCompute && sawComputePipeline) || (addRender && sawRenderPipeline))
          return fail("archive mutation identity or ordering");
        if(addCompute)
        {
          const SDObject *function = descriptor->FindChild("computeFunction");
          if(!function) return fail("archive compute function");
          addedFunctions[0] = function->AsResourceId();
          computeAdds++;
        }
        else
        {
          const SDObject *vertex = descriptor->FindChild("vertexFunction");
          const SDObject *fragment = descriptor->FindChild("fragmentFunction");
          if(!vertex || !fragment) return fail("archive render functions");
          addedFunctions[1] = vertex->AsResourceId();
          addedFunctions[2] = fragment->AsResourceId();
          renderAdds++;
        }
      }
      if(chunk->name == "MTLDevice::newComputePipelineStateWithDescriptor" ||
         chunk->name == "MTLDevice::newComputePipelineStateWithDescriptor(completionHandler)")
        sawComputePipeline = true;
      if(chunk->name == "MTLDevice::newRenderPipelineStateWithDescriptor" ||
         chunk->name == "MTLDevice::newRenderPipelineStateWithDescriptor(options, completionHandler)")
        sawRenderPipeline = true;
    }
    if(computeAdds != 1 || renderAdds != 1) return fail("archive mutation counts");
    if(functionAdds > 1 || (functionAdds && sourceLibrary == ResourceId()))
      return fail("archive visible function count");
    if(libraryAdds > 1 || (libraryAdds && stitchedSourceFunction == ResourceId()))
      return fail("archive stitched library count");
    bool parents[3] = {};
    bool libraryParent = false;
    bool stitchedParent = false;
    for(const ResourceDescription &resource : renderer->GetResources())
      if(resource.resourceId == archiveId)
        for(ResourceId parent : resource.parentResources)
        {
          libraryParent |= functionAdds && parent == sourceLibrary;
          stitchedParent |= libraryAdds && parent == stitchedSourceFunction;
          for(uint32_t i = 0; i < 3; i++) parents[i] |= parent == addedFunctions[i];
        }
    if(!parents[0] || !parents[1] || !parents[2])
      return fail("archive mutation resource parents");
    if(functionAdds && !libraryParent) return fail("archive visible function library parent");
    if(libraryAdds && !stitchedParent) return fail("archive stitched library function parent");
  }
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    bool compute = chunk->name == "MTLDevice::newComputePipelineStateWithDescriptor" ||
                   chunk->name == "MTLDevice::newComputePipelineStateWithDescriptor(completionHandler)";
    bool render = chunk->name == "MTLDevice::newRenderPipelineStateWithDescriptor" ||
                  chunk->name == "MTLDevice::newRenderPipelineStateWithDescriptor(options, completionHandler)";
    if(!compute && !render) continue;
    const SDObject *descriptor = chunk->FindChild("descriptor");
    const SDObject *archives = descriptor ? descriptor->FindChild("binaryArchives") : NULL;
    if(!archives || archives->NumChildren() != 1 ||
       archives->GetChild(0)->AsResourceId() != archiveId ||
       !chunk->FindChild("optionsValue") ||
       chunk->FindChild("optionsValue")->AsUInt64() != 4 ||
       !chunk->FindChild("supported") || !chunk->FindChild("supported")->AsBool())
      return fail("pipeline archive dependency or miss policy");
    const SDObject *pipelineId = chunk->FindChild(compute ? "ComputePipelineState" :
                                                           "RenderPipelineState");
    bool dependency = false;
    for(const ResourceDescription &resource : renderer->GetResources())
      if(pipelineId && resource.resourceId == pipelineId->AsResourceId())
        for(ResourceId parent : resource.parentResources)
          dependency |= parent == archiveId;
    if(!dependency) return fail("resource graph archive parent");
    if(compute) computeCount++;
    else renderCount++;
  }
  if(computeCount != 1 || renderCount != 1) return fail("pipeline counts");
  ResourceId output;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 128) output = buffer.resourceId;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *dispatch = NULL, *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch) dispatch = action;
    if(action->flags & ActionFlags::Drawcall) draw = action;
  }
  if(output == ResourceId() || !dispatch || !draw) return fail("buffer or actions");
  renderer->SetFrameEvent(dispatch->eventId, true);
  if(!renderer->GetFatalErrorStatus().OK()) return fail("dispatch seek");
  bytebuf bytes = renderer->GetBufferData(output, 0, 0);
  if(bytes.size() != 128) return fail("output buffer size");
  for(uint32_t i = 0; i < 32; i++)
  {
    uint32_t value;
    memcpy(&value, bytes.data() + i * 4, 4);
    if(value != i + 17) return fail("compute output");
  }
  return PixelMatches(renderer, colorTarget, draw->eventId, 200, 150, 0.2f, 0.7f, 0.3f);
}

static bool ValidateStitchedLibraryFixture(IReplayController *renderer, ResourceId colorTarget)
{
  const SDChunk *stitched = NULL, *visible = NULL, *compute = NULL;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newLibraryWithStitchedDescriptor" ||
       chunk->name == "MTLDevice::newLibraryWithStitchedDescriptor(completionHandler)")
      stitched = chunk;
    if(chunk->name == "MTLLibrary::newFunctionWithName" &&
       chunk->FindChild("FunctionName") &&
       chunk->FindChild("FunctionName")->AsString() == "stitched_scale") visible = chunk;
    if(chunk->name == "MTLDevice::newComputePipelineStateWithDescriptor") compute = chunk;
  }
  if(!stitched) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T303 stitched library validation failed: %s\n", message);
    return false;
  };
  const SDObject *library = stitched->FindChild("Library");
  const SDObject *base = stitched->FindChild("function");
  const SDObject *graphName = stitched->FindChild("graphName");
  const SDObject *functionName = stitched->FindChild("functionName");
  const SDObject *inputIndex = stitched->FindChild("inputIndex");
  if(!library || !base || !graphName || !functionName || !inputIndex ||
     graphName->AsString() != "stitched_scale" ||
     functionName->AsString() != "stitch_scale" || inputIndex->AsUInt64() != 0 ||
     !visible || !compute || !visible->FindChild("Library") ||
     visible->FindChild("Library")->AsResourceId() != library->AsResourceId())
    return fail("graph schema or function creation");
  ResourceId visibleId = visible->FindChild("Function")->AsResourceId();
  const SDObject *descriptor = compute->FindChild("descriptor");
  const SDObject *links = descriptor ? descriptor->FindChild("linkedFunctions") : NULL;
  const SDObject *functions = links ? links->FindChild("functions") : NULL;
  if(!functions || functions->NumChildren() != 1 ||
     functions->GetChild(0)->AsResourceId() != visibleId)
    return fail("compute link to stitched function");
  bool libraryParent = false, functionParent = false;
  for(const ResourceDescription &resource : renderer->GetResources())
  {
    if(resource.resourceId == library->AsResourceId())
      for(ResourceId parent : resource.parentResources) libraryParent |= parent == base->AsResourceId();
    if(resource.resourceId == visibleId)
      for(ResourceId parent : resource.parentResources) functionParent |= parent == library->AsResourceId();
  }
  if(!libraryParent || !functionParent) return fail("resource dependency graph");
  ResourceId output;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 128) output = buffer.resourceId;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *dispatch = NULL, *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch) dispatch = action;
    if(action->flags & ActionFlags::Drawcall) draw = action;
  }
  if(output == ResourceId() || !dispatch || !draw) return fail("output or actions");
  renderer->SetFrameEvent(dispatch->eventId, true);
  if(!renderer->GetFatalErrorStatus().OK()) return fail("dispatch seek");
  bytebuf bytes = renderer->GetBufferData(output, 0, 0);
  if(bytes.size() != 128) return fail("output length");
  for(uint32_t i = 0; i < 32; i++)
  {
    float value;
    memcpy(&value, bytes.data() + i * 4, 4);
    if(value != float((i + 1) * 2)) return fail("stitched GPU output");
  }
  return PixelMatches(renderer, colorTarget, draw->eventId, 200, 150, 0.2f, 0.7f, 0.3f);
}

static bool ValidatePipelineVariantsFixture(IReplayController *renderer, ResourceId colorTarget)
{
  uint32_t computeDescriptors = 0, computeOptions = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newComputePipelineStateWithDescriptor") computeDescriptors++;
    if(chunk->metadata.chunkID == 1024) computeOptions++;
  }
  // T55 now also creates two descriptor pipelines for linked intersection functions.
  if(computeDescriptors == 0 || computeOptions == 0) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T47 pipeline variants validation failed: %s\n", message);
    return false;
  };
  ResourceId output;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 532) output = buffer.resourceId;
  rdcarray<const ActionDescription *> actions, dispatches, draws;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *fill = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
    if(action->customName.contains("fillBuffer")) fill = action;
  }
  if(computeDescriptors != 2 || output == ResourceId() || dispatches.size() != 4 ||
     draws.size() != 2 || !fill) return fail("fixture resources/actions missing");
  ResourceId pipelines[4];
  const uint32_t biases[] = {17, 41, 73, 109};
  auto checkData = [&](uint32_t event) {
    renderer->SetFrameEvent(event, true);
    if(!renderer->GetFatalErrorStatus().OK()) return fail("seek failed");
    bytebuf bytes = renderer->GetBufferData(output, 0, 0);
    if(bytes.size() != 532) return fail("buffer size incorrect");
    for(uint32_t i = 0; i < 133; i++)
    {
      uint32_t value;
      memcpy(&value, bytes.data() + i * 4, 4);
      const uint32_t expected = i < 128 && event >= dispatches[i / 32]->eventId ?
                                    biases[i / 32] + i % 32 : 0;
      if(value != expected) return fail("dispatch output, padding or rewind mismatch");
    }
    return true;
  };
  for(uint32_t i : {0U, 1U, 2U, 3U, 1U, 0U, 3U})
  {
    if(!checkData(dispatches[i]->eventId)) return false;
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    const auto *reflection = pipe.GetShaderReflection(ShaderStage::Compute);
    if(!state || !reflection || reflection->entryPoint != "cs_main" ||
       reflection->readWriteResources.size() != 1 || reflection->constantBlocks.size() != 1 ||
       reflection->constantBlocks[0].byteSize != 4 ||
       state->computePipelineResourceId == ResourceId() || state->computeBuffers.size() < 2 ||
       state->computeBuffers[0].resourceId != output || state->computeBuffers[0].byteOffset != i * 128 ||
       state->computeBuffers[1].byteSize != 4)
      return fail("compute pipeline identity, reflection or bindings incorrect");
    pipelines[i] = state->computePipelineResourceId;
  }
  for(uint32_t i = 0; i < 4; i++)
    for(uint32_t j = i + 1; j < 4; j++)
      if(pipelines[i] == pipelines[j]) return fail("distinct pipeline creations were aliased");
  if(!checkData(fill->eventId) || !checkData(draws[1]->eventId)) return false;
  for(uint32_t i : {0U, 1U, 0U, 1U})
  {
    renderer->SetFrameEvent(draws[i]->eventId, true);
    const auto &pipe = renderer->GetPipelineState();
    const auto *vs = pipe.GetShaderReflection(ShaderStage::Vertex);
    const auto *fs = pipe.GetShaderReflection(ShaderStage::Fragment);
    if(!vs || !fs || vs->entryPoint != "vs_main" || fs->entryPoint != "fs_main" ||
       fs->readOnlyResources.size() != 1) return fail("render reflection missing");
    if(!PixelMatches(renderer, colorTarget, draws[i]->eventId, 100, 150,
                     17.0f / 255, 41.0f / 255, 73.0f / 255) ||
       !PixelMatches(renderer, colorTarget, draws[i]->eventId, 300, 150,
                     i ? 17.0f / 255 : 0, i ? 41.0f / 255 : 0, i ? 73.0f / 255 : 0))
      return fail("render pipeline variants or partial draw replay incorrect");
  }
  return HasUsage(renderer, output, ResourceUsage::CS_RWResource) &&
         HasUsage(renderer, output, ResourceUsage::PS_Resource);
}

static bool ValidateFencePresentFixture(IReplayController *renderer, ResourceId colorTarget)
{
  bool fixture = false, markers = false, timed = false, minimum = false;
  const SDFile &file = renderer->GetStructuredFile();
  for(const SDChunk *chunk : file.chunks)
  {
    fixture |= chunk->name == "MTLDevice::newFence";
    markers |= chunk->name == "MTLBuffer::addDebugMarker";
    if(chunk->name == "MTLCommandBuffer::presentDrawable" && chunk->FindChild("time"))
    {
      timed = true;
      minimum = chunk->metadata.chunkID == 1052;
    }
  }
  if(!fixture) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T44-46 fence/present validation failed: %s\n", message);
    return false;
  };
  ResourceId data, readback, markerBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 304) data = buffer.resourceId;
    if(buffer.length == 308) readback = buffer.resourceId;
    if(buffer.length == 172) markerBuffer = buffer.resourceId;
  }
  uint32_t fenceCount = 0;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.type == ResourceType::Sync) fenceCount++;
  if(data == ResourceId() || readback == ResourceId() || fenceCount != 4 ||
     (markers && markerBuffer == ResourceId()))
    return fail("buffer, annotation-only resource or four fences missing");
  rdcarray<const ActionDescription *> actions, draws, dispatches;
  rdcarray<uint32_t> fenceEvents;
  const ActionDescription *copy = NULL, *present = NULL, *fill = NULL;
  FindActions(renderer->GetRootActions(), actions);
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->customName.contains("copyFromBuffer")) copy = action;
    if(action->customName.contains("fillBuffer")) fill = action;
    if((action->flags & ActionFlags::Present) && action->customName.contains("presentDrawable"))
      present = action;
    for(const APIEvent &event : action->events)
      if(rdcstr(file.chunks[event.chunkIndex]->name).contains("Fence") &&
         !rdcstr(file.chunks[event.chunkIndex]->name).contains("newFence"))
        fenceEvents.push_back(event.eventId);
  }
  if(draws.size() != 2 || dispatches.size() != 2 || !copy || !present || !fill ||
     fenceEvents.size() != 10 || present->copyDestination != colorTarget)
    return fail("fence events, dependent actions or present output incorrect");
  if(timed && !present->customName.contains(minimum ? "afterMinimumDuration" : "atTime"))
  {
    fprintf(stderr, "Present action '%s', minimum=%d\n", present->customName.c_str(), minimum);
    return fail("timed present action variant incorrect");
  }
  auto check = [&](uint32_t event) {
    renderer->SetFrameEvent(event, true);
    if(!renderer->GetFatalErrorStatus().OK()) return fail("event seek failed");
    for(ResourceId resource : {data, readback})
    {
      const bytebuf bytes = renderer->GetBufferData(resource, 0, 0);
      if(bytes.size() != (resource == data ? 304 : 308)) return fail("buffer size incorrect");
      for(uint32_t i = 0; i < bytes.size() / 4; i++)
      {
        uint32_t value = 0, expected = 0;
        memcpy(&value, bytes.data() + i * 4, 4);
        if(i < 76 && resource == data && event >= dispatches[0]->eventId)
          expected = i + 3 + (event >= draws[0]->eventId && i < 3 ? 7 : 0);
        if(i < 76 && resource == readback && event >= copy->eventId)
          expected = i + 3 + (i < 3 ? 7 : 0) + (event >= dispatches[1]->eventId ? 5 : 0);
        if(value != expected) return fail("fence dependency, padding or rewind data mismatch");
      }
    }
    if(markers)
    {
      const bytebuf bytes = renderer->GetBufferData(markerBuffer, 0, 0);
      if(bytes.size() != 172) return fail("annotation-only buffer size incorrect");
      for(byte value : bytes)
        if(value != 0x72) return fail("annotation-only initial contents missing");
    }
    return true;
  };
  for(uint32_t event : {dispatches[0]->eventId, draws[0]->eventId, copy->eventId,
                       dispatches[1]->eventId, present->eventId, fill->eventId,
                       present->eventId, dispatches[0]->eventId, dispatches[1]->eventId})
    if(!check(event)) return false;
  // Exercise partial replays stopping exactly at state-only update/wait API events as well.
  for(uint32_t event : fenceEvents)
    if(!check(event)) return false;
  if(!HasUsage(renderer, data, ResourceUsage::VS_RWResource) ||
     !HasUsage(renderer, readback, ResourceUsage::CS_RWResource) ||
     !HasUsage(renderer, data, ResourceUsage::CopySrc) ||
     !HasUsage(renderer, readback, ResourceUsage::CopyDst))
    return fail("shader/copy resource usage incorrect");
  return PixelMatches(renderer, colorTarget, present->eventId, 200, 150,
                       15.0f / 255, 16.0f / 255, 17.0f / 255);
}

static bool ValidateResourceBarrierFixture(IReplayController *renderer, ResourceId colorTarget)
{
  bool compute = false, render = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    compute |= chunk->name == "MTLComputeCommandEncoder::memoryBarrierWithScope";
    render |= chunk->name == "MTLRenderCommandEncoder::memoryBarrierWithScope";
  }
  if(!compute && !render) return true;
  auto fail = [&](const char *message) {
    fprintf(stderr, "T%u resource barrier validation failed: %s\n", compute ? 42 : 43, message);
    return false;
  };
  ResourceId data, declaredBuffer, declaredTexture;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == (compute ? 272 : 112)) data = buffer.resourceId;
    if(buffer.length == (compute ? 44 : 52)) declaredBuffer = buffer.resourceId;
  }
  for(const TextureDescription &texture : renderer->GetTextures())
    if(texture.width == (compute ? 11 : 12) && texture.height == 9)
      declaredTexture = texture.resourceId;
  if(data == ResourceId() || declaredBuffer == ResourceId() || declaredTexture == ResourceId())
    return fail("declared-only or GPU data resource missing");
  rdcarray<const ActionDescription *> actions, draws, dispatches;
  FindActions(renderer->GetRootActions(), actions);
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Drawcall) draws.push_back(action);
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
  }
  if(draws.size() != (compute ? 1 : 3) || dispatches.size() != (compute ? 3 : 0))
    return fail("unexpected draw/dispatch count");
  const auto &steps = compute ? dispatches : draws;
  for(uint32_t step : {0U, 1U, 2U, 0U, 2U, 1U})
  {
    renderer->SetFrameEvent(steps[step]->eventId, true);
    const bytebuf bytes = renderer->GetBufferData(data, 0, 0);
    if(bytes.size() != (compute ? 272 : 112)) return fail("data size incorrect");
    for(uint32_t i = 0; i < bytes.size() / 4; i++)
    {
      uint32_t value = 0;
      memcpy(&value, bytes.data() + i * 4, 4);
      uint32_t expected = compute ? (step == 0 ? i + 1 : 3 * i + (step == 1 ? 10 : 21))
                                  : (i < 3 ? 20 * (i + 1) + (step ? 7 : 0) : 0);
      if(value != expected) return fail("dependent GPU write, padding or event rewind incorrect");
    }
    const auto &pipe = renderer->GetPipelineState();
    const auto rw = pipe.GetReadWriteResources(compute ? ShaderStage::Compute : ShaderStage::Vertex, true);
    if(compute || step < 2)
    {
      if(rw.size() != 1 || rw[0].descriptor.type != DescriptorType::ReadWriteBuffer ||
         rw[0].descriptor.resource != data || rw[0].descriptor.byteOffset != 0 ||
         rw[0].descriptor.byteSize != bytes.size())
        return fail("writable shader buffer descriptor missing or incorrect");
    }
    else if(!rw.empty()) return fail("stale vertex write descriptor after pipeline change");
    const bytebuf sentinel = renderer->GetBufferData(declaredBuffer, 0, 0);
    if(sentinel.size() != (compute ? 44 : 52)) return fail("declaration-only buffer size");
    for(byte value : sentinel)
      if(value != 0x6d) return fail("declaration-only buffer initial contents");
    const bytebuf pixels = renderer->GetTextureData(declaredTexture, {0, 0, 0});
    if(pixels.size() != (compute ? 11 : 12) * 9 * 4) return fail("declaration-only texture size");
    for(size_t i = 0; i < pixels.size(); i += 4)
      if(pixels[i] != 34 || pixels[i+1] != 68 || pixels[i+2] != 102 || pixels[i+3] != 255)
        return fail("declaration-only texture initial contents");
  }
  if(!HasUsage(renderer, data, compute ? ResourceUsage::CS_RWResource : ResourceUsage::VS_RWResource))
    return fail("shader write usage missing");
  return PixelMatches(renderer, colorTarget, draws.back()->eventId, 200, 150,
                       (compute ? 21 : 27) / 255.0f, (compute ? 117 : 47) / 255.0f,
                       (compute ? 222 : 67) / 255.0f);
}

static bool ValidateBlitOptimizationFixture(IReplayController *renderer)
{
  bool fixture = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    fixture |= chunk->name == "MTLCommandBuffer::blitCommandEncoderWithDescriptor";
  if(!fixture) return true;
  ResourceId hintOnly;
  for(const TextureDescription &texture : renderer->GetTextures())
    if(texture.width == 13 && texture.height == 7)
      hintOnly = texture.resourceId;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *copy = NULL, *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Copy) copy = action;
    if(action->flags & ActionFlags::Drawcall) draw = action;
  }
  if(hintOnly == ResourceId() || !copy || !draw)
  {
    fprintf(stderr, "T41 hint-only texture or blit actions missing\n");
    return false;
  }
  for(uint32_t event : {draw->eventId, copy->eventId, draw->eventId})
  {
    renderer->SetFrameEvent(event, true);
    const bytebuf bytes = renderer->GetTextureData(hintOnly, {0, 0, 0});
    if(bytes.size() != 13 * 7 * 4) return false;
    for(size_t i = 0; i < bytes.size(); i += 4)
      if(bytes[i] != 17 || bytes[i + 1] != 34 || bytes[i + 2] != 51 || bytes[i + 3] != 255)
      {
        fprintf(stderr, "T41 hint-only texture contents not captured or restored\n");
        return false;
      }
  }
  return true;
}

static bool ValidateBlitTransferFixture(IReplayController *renderer, ResourceId colorTarget)
{
  rdcarray<const ActionDescription *> actions, copies;
  FindActions(renderer->GetRootActions(), actions);
  bool fixture = false;
  for(const ActionDescription *action : actions)
    fixture |= action->customName == "copyFromBuffer(to texture)";
  if(!fixture)
    return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T37 blit transfer validation failed: %s\n", message);
    return false;
  };
  const ActionDescription *fill = NULL, *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::Copy) copies.push_back(action);
    if(action->customName.contains("fillBuffer")) fill = action;
    if(action->flags & ActionFlags::Drawcall) draw = action;
  }
  if(copies.size() != 6 || !fill || !draw ||
     copies[2]->customName != "copyFromTexture(whole texture)" ||
     copies[3]->copySourceSubresource != Subresource(1, 1) ||
     copies[3]->copyDestinationSubresource != Subresource(1, 0))
    return fail("copy action metadata incorrect");
  const ResourceId readback = copies[4]->copyDestination;
  if(copies[5]->copyDestination != readback ||
     !HasUsage(renderer, copies[0]->copySource, ResourceUsage::CopySrc) ||
     !HasUsage(renderer, readback, ResourceUsage::CopyDst))
    return fail("copy resource usage incorrect");
  auto checkReadback = [&](uint32_t eventId, uint32_t completed) {
    renderer->SetFrameEvent(eventId, true);
    const bytebuf bytes = renderer->GetBufferData(readback, 0, 0);
    if(bytes.size() != 4096) return false;
    for(size_t i = 0; i < bytes.size(); i++)
    {
      byte expected = 0xa5;
      for(uint32_t transfer = 0; transfer < completed; transfer++)
        for(size_t y = 0; y < 2; y++)
        {
          const size_t base = 512 + transfer * 1024 + y * 256;
          if(i >= base && i < base + 12)
          {
            const size_t x = (i - base) / 4;
            const byte pixel[] = {byte(64 + x), byte(128 + y), 192, 255};
            expected = pixel[(i - base) % 4];
          }
        }
      if(bytes[i] != expected) return false;
    }
    return true;
  };
  if(!checkReadback(fill->eventId, 0) || !checkReadback(copies[4]->eventId, 1) ||
     !checkReadback(copies[5]->eventId, 2) || !checkReadback(fill->eventId, 0) ||
     !checkReadback(copies[5]->eventId, 2))
    return fail("pitched readback, padding or event rewind incorrect");
  const bytebuf copied = renderer->GetTextureData(copies[3]->copyDestination, {1, 0, 0});
  if(copied.size() != 8 * 4 * 4)
    return fail("mip readback size incorrect");
  for(size_t y = 0; y < 4; y++)
    for(size_t x = 0; x < 8; x++)
    {
      byte pixel[4] = {};
      if(x >= 1 && x < 4 && y >= 1 && y < 3)
      {
        pixel[0] = byte(63 + x); pixel[1] = byte(127 + y);
        pixel[2] = 192; pixel[3] = 255;
      }
      if(memcmp(copied.data() + (y * 8 + x) * 4, pixel, 4) != 0)
        return fail("array slice/mip copy or untouched texels incorrect");
    }
  return PixelMatches(renderer, colorTarget, draw->eventId, 200, 150,
                       64.0f / 255, 128.0f / 255, 192.0f / 255);
}

static bool ValidateComputeIndirectDispatchFixture(IReplayController *renderer,
                                                   const char *savePath)
{
  ResourceId arguments;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 44 && (buffer.creationFlags & BufferCategory::Indirect))
      arguments = buffer.resourceId;
  if(arguments == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "T32 indirect compute validation failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *dispatch = NULL;
  const ActionDescription *begin = NULL;
  const ActionDescription *firstBegin = NULL;
  const ActionDescription *writer = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Begin Metal Compute Pass")
    {
      if(!firstBegin) firstBegin = action;
      begin = action;
    }
    if((action->flags & ActionFlags::Dispatch) && (action->flags & ActionFlags::Indirect))
      dispatch = action;
    else if(action->flags & ActionFlags::Dispatch)
      writer = action;
  }
  if(begin == NULL || dispatch == NULL || begin->eventId >= dispatch->eventId ||
     !dispatch->customName.contains("indirect, <2, 2, 1>") ||
     dispatch->dispatchDimension[0] != 2 || dispatch->dispatchDimension[1] != 2 ||
     dispatch->dispatchDimension[2] != 1 ||
     dispatch->dispatchThreadsDimension[0] != 4 ||
     dispatch->dispatchThreadsDimension[1] != 4 ||
     dispatch->dispatchThreadsDimension[2] != 1)
    return fail("compute action sequence or metadata incorrect");

  renderer->SetFrameEvent(dispatch->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(!state || state->indirectBuffer.resourceId != arguments ||
     state->indirectBuffer.byteOffset != 16 || state->indirectBuffer.byteSize != 12 ||
     state->computeTextures.size() < 2 || state->computeTextures[1] == ResourceId() ||
     state->computeTextures[0] == ResourceId() ||
     state->computeBuffers.size() < 5 ||
     state->computeBuffers[2].resourceId == ResourceId() ||
     state->computeBuffers[4].resourceId == ResourceId() ||
     state->computeBuffers[2].byteOffset != 32 ||
     state->computeBuffers[4].byteOffset != 64 ||
     !HasUsage(renderer, arguments, ResourceUsage::Indirect))
    return fail("indirect or compute pipeline state incorrect");

  const ResourceId source = state->computeTextures[0];
  const ResourceId input = state->computeBuffers[2].resourceId;
  const ResourceId destination = state->computeTextures[1];
  const ResourceId output = state->computeBuffers[4].resourceId;
  if(pipe.GetShaderEntryPoint(ShaderStage::Compute) != "filter_main" ||
     !HasUsage(renderer, source, ResourceUsage::CS_Resource) ||
     !HasUsage(renderer, destination, ResourceUsage::CS_RWResource) ||
     !HasUsage(renderer, input, ResourceUsage::CS_Resource) ||
     !HasUsage(renderer, output, ResourceUsage::CS_RWResource))
    return fail("compute shader or resource usage incorrect");
  const auto ro = pipe.GetReadOnlyResources(ShaderStage::Compute, true);
  const auto rw = pipe.GetReadWriteResources(ShaderStage::Compute, true);
  bool readTexture = false, readBuffer = false, writeTexture = false, writeBuffer = false;
  for(const UsedDescriptor &binding : ro)
  {
    readTexture |= binding.descriptor.resource == source;
    readBuffer |= binding.descriptor.resource == input && binding.descriptor.byteOffset == 32;
  }
  for(const UsedDescriptor &binding : rw)
  {
    writeTexture |= binding.descriptor.resource == destination;
    writeBuffer |= binding.descriptor.resource == output && binding.descriptor.byteOffset == 64;
  }
  if(!readTexture || !readBuffer || !writeTexture || !writeBuffer)
    return fail("compute descriptors incorrect");

  const bytebuf argumentBytes = renderer->GetBufferData(arguments, 0, 0);
  static const uint32_t expectedGroups[3] = {2, 2, 1};
  if(argumentBytes.size() != 44 ||
     memcmp(argumentBytes.data() + 16, expectedGroups, sizeof(expectedGroups)) != 0)
    return fail("indirect argument record incorrect");
  for(size_t i = 0; i < argumentBytes.size(); ++i)
    if((i < 16 || i >= 28) && argumentBytes[i] != 0x6d)
      return fail("indirect argument sentinel changed");

  if(writer)
  {
    if(!firstBegin || firstBegin->eventId >= writer->eventId ||
       writer->eventId >= begin->eventId ||
       !HasUsage(renderer, arguments, ResourceUsage::CS_RWResource))
      return fail("GPU argument writer action or usage incorrect");
    renderer->SetFrameEvent(firstBegin->eventId, true);
    const bytebuf initial = renderer->GetBufferData(arguments, 16, 12);
    if(initial.size() != 12)
      return fail("GPU argument initial readback unavailable");
    for(byte value : initial)
      if(value != 0)
        return fail("GPU argument record was not zero before writer");
    renderer->SetFrameEvent(writer->eventId, true);
    const MetalPipe::State *writerState = renderer->GetPipelineState().GetMetalPipelineState();
    if(!writerState || writerState->computeBuffers.empty() ||
       writerState->computeBuffers[0].resourceId != arguments ||
       writerState->computeBuffers[0].byteOffset != 16)
      return fail("GPU argument writer binding incorrect");
    bool writerDescriptor = false;
    for(const UsedDescriptor &binding :
        renderer->GetPipelineState().GetReadWriteResources(ShaderStage::Compute, true))
      writerDescriptor |= binding.descriptor.resource == arguments &&
                          binding.descriptor.byteOffset == 16;
    if(!writerDescriptor)
      return fail("GPU argument writer descriptor incorrect");
    const bytebuf produced = renderer->GetBufferData(arguments, 16, 12);
    if(produced.size() != sizeof(expectedGroups) ||
       memcmp(produced.data(), expectedGroups, sizeof(expectedGroups)) != 0)
      return fail("GPU argument writer did not produce 2,2,1");
    renderer->SetFrameEvent(dispatch->eventId, true);
  }

  bytebuf pixels = renderer->GetTextureData(destination, {0, 0, 0});
  bytebuf outputBytes = renderer->GetBufferData(output, 0, 0);
  if(pixels.size() != 256 || outputBytes.size() != 336)
    return fail("output sizes incorrect");
  for(uint32_t y = 0; y < 8; ++y)
    for(uint32_t x = 0; x < 8; ++x)
    {
      const uint32_t i = y * 8 + x;
      const uint32_t expected = 19 + i * 2;
      if(pixels[i * 4 + 0] != expected || pixels[i * 4 + 1] != 32 + 24 * x ||
         pixels[i * 4 + 2] != 24 + 24 * y || pixels[i * 4 + 3] != 255 ||
         memcmp(outputBytes.data() + 64 + i * 4, &expected, 4) != 0)
      {
        return fail("compute output differs from CPU reference");
      }
    }
  for(size_t i = 0; i < outputBytes.size(); ++i)
    if((i < 64 || i >= 320) && outputBytes[i] != 0xa5)
      return fail("output buffer sentinel changed");

  renderer->SetFrameEvent(begin->eventId, true);
  pixels = renderer->GetTextureData(destination, {0, 0, 0});
  for(byte value : pixels)
    if(value != 0)
      return fail("pre-dispatch texture not zero");
  if(writer)
  {
    const bytebuf produced = renderer->GetBufferData(arguments, 16, 12);
    if(produced.size() != sizeof(expectedGroups) ||
       memcmp(produced.data(), expectedGroups, sizeof(expectedGroups)) != 0)
      return fail("GPU arguments not visible to the next compute encoder");
  }
  renderer->SetFrameEvent(dispatch->eventId, true);
  pixels = renderer->GetTextureData(destination, {0, 0, 0});
  if(pixels.size() != 256 || pixels[0] != 19 || pixels[1] != 32 || pixels[2] != 24)
    return fail("seek forward did not restore compute output");
  if(savePath)
  {
    TextureSave save;
    save.resourceId = destination;
    save.destType = FileType::DDS;
    save.mip = 0;
    save.slice.sliceIndex = 0;
    if(!renderer->SaveTexture(save, savePath).OK())
      return fail("computed texture DDS export failed");
    std::ifstream dds(savePath, std::ios::binary | std::ios::ate);
    if(!dds || dds.tellg() != std::streampos(384))
      return fail("computed texture DDS size incorrect");
    const rdcstr rawPath = rdcstr(savePath) + ".bin";
    std::ofstream raw(rawPath.c_str(), std::ios::binary);
    raw.write((const char *)argumentBytes.data(), argumentBytes.size());
    if(!raw)
      return fail("indirect argument raw export failed");
  }
  fprintf(stderr, "%s indirect compute EIDs writer=%u begin=%u dispatch=%u, offset=16 groups=2,2,1\n",
          writer ? "T33" : "T32", writer ? writer->eventId : 0,
          begin->eventId, dispatch->eventId);
  return true;
}

static bool ValidateHeadlessThumbnail(IReplayController *renderer, ResourceId texture,
                                      uint32_t eventId, bool expectContent,
                                      const Subresource &sub = {0, 0, 0}, bool wholeImage = false)
{
  IReplayOutput *output = renderer->CreateOutput(CreateHeadlessWindowingData(64, 64),
                                                 ReplayOutputType::Texture);
  if(!output)
    return false;
  renderer->SetFrameEvent(eventId, true);
  const bytebuf pixels = output->DrawThumbnail(64, 64, texture, sub, CompType::Typeless);
  output->Shutdown();
  if(pixels.size() != 64 * 64 * 3)
    return false;
  const size_t center = (32 * 64 + 32) * 3;
  bool hasContent = pixels[center] || pixels[center + 1] || pixels[center + 2];
  if(wholeImage)
    for(byte value : pixels)
      hasContent |= value != 0;
  if(hasContent != expectContent)
    fprintf(stderr, "Headless thumbnail content mismatch at EID %u\n", eventId);
  return hasContent == expectContent;
}

static bool WriteOutput(IReplayController *renderer, IReplayOutput *output, uint32_t eventId,
                        const char *path)
{
  renderer->SetFrameEvent(eventId, true);
  output->Display();
  bytebuf pixels = output->ReadbackOutputTexture();

  std::ofstream ppm(path, std::ios::binary);
  ppm << "P6\n640 480\n255\n";
  ppm.write((const char *)pixels.data(), pixels.size());
  ppm.close();
  return pixels.size() == 640 * 480 * 3;
}

static bool Near(float actual, float expected)
{
  return fabsf(actual - expected) <= 1.5f / 255.0f;
}

static bool ValidateShaders(IReplayController *renderer)
{
  bool vertexFound = false;
  bool fragmentFound = false;
  uint32_t shaderCount = 0;

  for(const ResourceDescription &resource : renderer->GetResources())
  {
    if(resource.type != ResourceType::Shader)
      continue;

    shaderCount++;
    rdcarray<ShaderEntryPoint> entries = renderer->GetShaderEntryPoints(resource.resourceId);
    if(entries.size() != 1)
      return false;

    const ShaderReflection *reflection =
        renderer->GetShader(ResourceId(), resource.resourceId, entries[0]);
    if(reflection == NULL || reflection->encoding != ShaderEncoding::MSL ||
       reflection->debugInfo.encoding != ShaderEncoding::MSL ||
       reflection->debugInfo.files.size() != 1 ||
       !reflection->debugInfo.files[0].contents.contains("#include <metal_stdlib>") ||
       !reflection->debugInfo.files[0].contents.contains("vertex VSOut vs_main") ||
       !reflection->debugInfo.files[0].contents.contains("fragment float4 fs_main"))
      return false;

    if(entries[0] == ShaderEntryPoint("vs_main", ShaderStage::Vertex))
      vertexFound = true;
    else if(entries[0] == ShaderEntryPoint("fs_main", ShaderStage::Fragment))
      fragmentFound = true;
    else
      return false;
  }

  return shaderCount == 2 && vertexFound && fragmentFound;
}

static ResourceType GetResourceType(IReplayController *renderer, ResourceId id)
{
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.resourceId == id)
      return resource.type;
  return ResourceType::Unknown;
}

static bool PipelineFailure(const char *message)
{
  fprintf(stderr, "Metal pipeline state validation failed: %s\n", message);
  return false;
}

static bool ValidatePipelineState(IReplayController *renderer, ResourceId colorTarget,
                                  uint32_t clearEvent, uint32_t drawEvent)
{
  renderer->SetFrameEvent(clearEvent, true);
  const PipeState &clear = renderer->GetPipelineState();
  if(!clear.IsCaptureMetal())
    return PipelineFailure("clear event is not reported as Metal");

  rdcarray<Descriptor> clearTargets = clear.GetOutputTargets();
  if(clearTargets.empty())
    return PipelineFailure("clear event has no color target");
  if(clearTargets[0].resource != colorTarget)
    return PipelineFailure("clear event color target does not match the swapbuffer");

  renderer->SetFrameEvent(drawEvent, true);
  const PipeState &draw = renderer->GetPipelineState();
  const ResourceId pipeline = draw.GetGraphicsPipelineObject();
  const ResourceId vertexShader = draw.GetShader(ShaderStage::Vertex);
  const ResourceId fragmentShader = draw.GetShader(ShaderStage::Fragment);
  const rdcarray<BoundVBuffer> vertexBuffers = draw.GetVBuffers();
  const rdcarray<Descriptor> colorTargets = draw.GetOutputTargets();

  if(!draw.IsCaptureMetal())
    return PipelineFailure("draw event is not reported as Metal");
  if(pipeline == ResourceId() ||
     GetResourceType(renderer, pipeline) != ResourceType::PipelineState)
    return PipelineFailure("draw event has no valid render pipeline");
  if(vertexShader == ResourceId() || fragmentShader == ResourceId())
    return PipelineFailure("draw event is missing a shader");
  if(draw.GetShaderEntryPoint(ShaderStage::Vertex) != "vs_main" ||
     draw.GetShaderEntryPoint(ShaderStage::Fragment) != "fs_main")
    return PipelineFailure("draw event shader entry points do not match");
  if(draw.GetShaderReflection(ShaderStage::Vertex) == NULL ||
     draw.GetShaderReflection(ShaderStage::Fragment) == NULL)
    return PipelineFailure("draw event is missing shader reflection");
  if(draw.GetPrimitiveTopology() != Topology::TriangleList)
    return PipelineFailure("draw event topology is not TriangleList");
  if(vertexBuffers.size() != 1)
    return PipelineFailure("draw event does not have exactly one vertex buffer");
  if(GetResourceType(renderer, vertexBuffers[0].resourceId) != ResourceType::Buffer)
    return PipelineFailure("draw event vertex buffer is not a buffer resource");
  if(vertexBuffers[0].byteOffset != 0 || vertexBuffers[0].byteSize != 96)
    return PipelineFailure("draw event vertex buffer range does not match");
  if(colorTargets.empty() || colorTargets[0].resource != colorTarget)
    return PipelineFailure("draw event color target does not match the swapbuffer");

  return true;
}

static bool ValidateTextureData(IReplayController *renderer, ResourceId texture,
                                const TextureDescription &desc, uint32_t eventId,
                                bool expectBackgroundAtCentre)
{
  renderer->SetFrameEvent(eventId, true);

  const Subresource sub = {0, 0, 0};
  bytebuf data = renderer->GetTextureData(texture, sub);
  if(desc.width != 400 || desc.height != 300 || data.size() != size_t(400 * 300 * 4))
    return false;

  const size_t centre = (size_t(desc.height / 2) * desc.width + desc.width / 2) * 4;
  const bool centreIsBackground = data[centre + 0] == 0x1a && data[centre + 1] == 0x14 &&
                                  data[centre + 2] == 0x14 && data[centre + 3] == 0xff;
  if(centreIsBackground != expectBackgroundAtCentre)
    return false;

  PixelValue background = renderer->PickPixel(texture, 10, 10, sub, CompType::Typeless);
  if(!Near(background.floatValue[0], 0.08f) || !Near(background.floatValue[1], 0.08f) ||
     !Near(background.floatValue[2], 0.10f) || !Near(background.floatValue[3], 1.0f))
    return false;

  PixelValue picked =
      renderer->PickPixel(texture, desc.width / 2, desc.height / 2, sub, CompType::Typeless);
  const bool pickedIsBackground = Near(picked.floatValue[0], 0.08f) &&
                                  Near(picked.floatValue[1], 0.08f) &&
                                  Near(picked.floatValue[2], 0.10f) &&
                                  Near(picked.floatValue[3], 1.0f);
  return pickedIsBackground == expectBackgroundAtCentre;
}

static bool ValidateTextureSave(IReplayController *renderer, ResourceId texture,
                                uint32_t eventId, const char *path)
{
  renderer->SetFrameEvent(eventId, true);

  TextureSave save;
  save.resourceId = texture;
  save.destType = FileType::DDS;
  save.mip = 0;
  save.slice.sliceIndex = 0;
  ResultDetails result = renderer->SaveTexture(save, path);
  if(!result.OK())
    return false;

  std::ifstream file(path, std::ios::binary | std::ios::ate);
  return file && file.tellg() > 128;
}

static bool ValidateTexturedFixture(IReplayController *renderer)
{
  TextureDescription sampledTexture;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(!(texture.creationFlags & TextureCategory::SwapBuffer) && texture.width == 4 &&
       texture.height == 4 && texture.depth == 1 && texture.mips == 1 && texture.arraysize == 1)
    {
      sampledTexture = texture;
      break;
    }
  }

  // Other fixtures do not contain T03's 4x4 texture.
  if(sampledTexture.resourceId == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal textured fixture validation failed: %s\n", message);
    return false;
  };

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1 || (draws[0]->flags & ActionFlags::Indexed) || draws[0]->numIndices != 4 ||
     draws[0]->numInstances != 1)
    return fail("draw action does not match the four-vertex triangle strip");

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  if(pipe.GetPrimitiveTopology() != Topology::TriangleStrip)
    return fail("pipeline topology is not TriangleStrip");

  const MetalPipe::State *metal = pipe.GetMetalPipelineState();
  if(metal != NULL && !metal->fragmentArgumentBuffers.empty())
    return true;
  if(metal == NULL || metal->fragmentTextures.size() != 2 ||
     metal->fragmentTextures[0] != sampledTexture.resourceId ||
     metal->fragmentTextures[1] != sampledTexture.resourceId ||
     metal->fragmentSamplers.size() != 2 || metal->fragmentSamplers[0] == ResourceId() ||
     metal->fragmentSamplers[1] != metal->fragmentSamplers[0])
    return fail("Metal fragment texture/sampler slots do not match");

  const rdcarray<UsedDescriptor> resources =
      pipe.GetReadOnlyResources(ShaderStage::Fragment, false);
  const rdcarray<UsedDescriptor> samplers = pipe.GetSamplers(ShaderStage::Fragment, false);
  const rdcarray<UsedDescriptor> usedResources =
      pipe.GetReadOnlyResources(ShaderStage::Fragment, true);
  const rdcarray<UsedDescriptor> usedSamplers = pipe.GetSamplers(ShaderStage::Fragment, true);
  if(resources.size() != 2 || resources[0].access.index != 0 ||
     resources[0].access.type != DescriptorType::Image ||
     resources[0].access.staticallyUnused ||
     resources[0].descriptor.resource != sampledTexture.resourceId ||
     resources[0].descriptor.type != DescriptorType::Image ||
     resources[0].descriptor.textureType != TextureType::Texture2D ||
     resources[1].access.index != DescriptorAccess::NoShaderBinding ||
     !resources[1].access.staticallyUnused ||
     resources[1].descriptor.resource != sampledTexture.resourceId || usedResources.size() != 1)
    return fail("generic fragment texture used/unused descriptors do not match slots 0/1");
  if(samplers.size() != 2 || samplers[0].access.index != 0 ||
     samplers[0].access.type != DescriptorType::Sampler ||
     samplers[0].access.staticallyUnused ||
     samplers[0].sampler.object != metal->fragmentSamplers[0] ||
     samplers[0].sampler.type != DescriptorType::Sampler ||
     samplers[0].sampler.filter.minify != FilterMode::Point ||
     samplers[0].sampler.filter.magnify != FilterMode::Point ||
     samplers[0].sampler.filter.mip != FilterMode::NoFilter ||
     samplers[0].sampler.addressU != AddressMode::ClampEdge ||
     samplers[0].sampler.addressV != AddressMode::ClampEdge ||
     samplers[0].sampler.addressW != AddressMode::ClampEdge ||
     samplers[1].access.index != DescriptorAccess::NoShaderBinding ||
     !samplers[1].access.staticallyUnused ||
     samplers[1].sampler.object != metal->fragmentSamplers[1] || usedSamplers.size() != 1)
    return fail("generic fragment sampler used/unused descriptors do not match slots 0/1");

  const ShaderReflection *fragmentReflection = pipe.GetShaderReflection(ShaderStage::Fragment);
  if(fragmentReflection == NULL || fragmentReflection->readOnlyResources.size() != 1 ||
     fragmentReflection->samplers.size() != 1 ||
     fragmentReflection->readOnlyResources[0].name != "colourTexture" ||
     fragmentReflection->readOnlyResources[0].fixedBindNumber != 0 ||
     fragmentReflection->readOnlyResources[0].descriptorType != DescriptorType::Image ||
     fragmentReflection->readOnlyResources[0].textureType != TextureType::Texture2D ||
     !fragmentReflection->readOnlyResources[0].isTexture ||
     !fragmentReflection->readOnlyResources[0].isReadOnly ||
     fragmentReflection->samplers[0].name != "colourSampler" ||
     fragmentReflection->samplers[0].fixedBindNumber != 0)
    return fail("fragment shader resource binding reflection does not match MSL arguments");

  const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
  const rdcarray<BoundVBuffer> vertexBuffers = pipe.GetVBuffers();
  if(inputs.size() != 2 || vertexBuffers.size() != 1 || inputs[0].byteOffset != 0 ||
     inputs[0].format.compType != CompType::Float || inputs[0].format.compByteWidth != 4 ||
     inputs[0].format.compCount != 2 || inputs[1].byteOffset != 8 ||
     inputs[1].format.compType != CompType::Float || inputs[1].format.compByteWidth != 4 ||
     inputs[1].format.compCount != 2 || vertexBuffers[0].byteOffset != 0 ||
     vertexBuffers[0].byteSize != 64 || vertexBuffers[0].byteStride != 16)
    return fail("Float2 position/UV vertex input does not match");

  static const byte expected[] = {
      255, 32, 16, 255, 255, 32, 16, 255, 16, 224, 48, 255, 16, 224, 48, 255,
      255, 32, 16, 255, 255, 32, 16, 255, 16, 224, 48, 255, 16, 224, 48, 255,
      24, 64, 255, 255, 24, 64, 255, 255, 240, 208, 32, 255, 240, 208, 32, 255,
      24, 64, 255, 255, 24, 64, 255, 255, 240, 208, 32, 255, 240, 208, 32, 255,
  };
  const Subresource sub = {0, 0, 0};
  const bytebuf data = renderer->GetTextureData(sampledTexture.resourceId, sub);
  if(data.size() != sizeof(expected) || memcmp(data.data(), expected, sizeof(expected)) != 0)
    return fail("captured RGBA8 texels do not match the upload");
  if(!ValidateHeadlessThumbnail(renderer, sampledTexture.resourceId, draws[0]->eventId, true))
    return fail("fragment input thumbnail is blank");

  const PixelValue red =
      renderer->PickPixel(sampledTexture.resourceId, 0, 0, sub, CompType::Typeless);
  const PixelValue yellow =
      renderer->PickPixel(sampledTexture.resourceId, 3, 3, sub, CompType::Typeless);
  if(!Near(red.floatValue[0], 1.0f) || !Near(red.floatValue[1], 32.0f / 255.0f) ||
     !Near(red.floatValue[2], 16.0f / 255.0f) || !Near(red.floatValue[3], 1.0f) ||
     !Near(yellow.floatValue[0], 240.0f / 255.0f) ||
     !Near(yellow.floatValue[1], 208.0f / 255.0f) ||
     !Near(yellow.floatValue[2], 32.0f / 255.0f) || !Near(yellow.floatValue[3], 1.0f))
    return fail("PickPixel did not preserve RGBA channel order");

  size_t samplerCount = 0;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.type == ResourceType::Sampler)
      samplerCount++;
  if(samplerCount != 1)
    return fail("capture does not expose exactly one sampler resource");

  return true;
}

static bool PixelMatches(IReplayController *renderer, ResourceId texture, uint32_t eventId,
                         uint32_t x, uint32_t y, float r, float g, float b);

static bool ValidateTextureSubresourceFixture(IReplayController *renderer, IReplayOutput *output,
                                              ResourceId colorTarget, const char *savePath)
{
  TextureDescription mipTexture;
  TextureDescription arrayTexture;
  TextureDescription cubeTexture;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(texture.width != 4 || texture.height != 4 || texture.depth != 1 ||
       texture.format.compByteWidth != 1 || texture.format.compCount != 4)
      continue;

    if(texture.type == TextureType::Texture2D && texture.mips == 3 && texture.arraysize == 1)
      mipTexture = texture;
    else if(texture.type == TextureType::Texture2DArray && texture.mips == 1 &&
            texture.arraysize == 3)
      arrayTexture = texture;
    else if(texture.type == TextureType::TextureCube && texture.cubemap && texture.mips == 1 &&
            texture.arraysize == 6)
      cubeTexture = texture;
  }

  // Other fixtures do not contain T09's three distinct 4x4 texture shapes.
  if(cubeTexture.resourceId == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal texture subresource fixture validation failed: %s\n", message);
    return false;
  };

  if(mipTexture.resourceId == ResourceId() || arrayTexture.resourceId == ResourceId() ||
     cubeTexture.resourceId == ResourceId() || mipTexture.byteSize != 84 ||
     arrayTexture.byteSize != 192 || cubeTexture.byteSize != 384)
    return fail("mip/array/cube texture descriptions or byte sizes do not match");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1 || (draws[0]->flags & ActionFlags::Indexed) || draws[0]->numIndices != 4)
    return fail("draw action does not match the four-vertex triangle strip");

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *metal = pipe.GetMetalPipelineState();
  if(metal == NULL || pipe.GetPrimitiveTopology() != Topology::TriangleStrip ||
     metal->fragmentTextures.size() != 3 ||
     metal->fragmentTextures[0] != mipTexture.resourceId ||
     metal->fragmentTextures[1] != arrayTexture.resourceId ||
     metal->fragmentTextures[2] != cubeTexture.resourceId ||
     metal->fragmentSamplers.size() != 1 || metal->fragmentSamplers[0] == ResourceId())
    return fail("Metal fragment texture/sampler slots do not match");

  const rdcarray<UsedDescriptor> resources =
      pipe.GetReadOnlyResources(ShaderStage::Fragment, true);
  if(resources.size() != 3 || resources[0].access.index != 0 ||
     resources[0].descriptor.resource != mipTexture.resourceId ||
     resources[0].descriptor.textureType != TextureType::Texture2D ||
     resources[0].descriptor.numMips != 3 || resources[0].descriptor.numSlices != 1 ||
     resources[1].access.index != 1 ||
     resources[1].descriptor.resource != arrayTexture.resourceId ||
     resources[1].descriptor.textureType != TextureType::Texture2DArray ||
     resources[1].descriptor.numMips != 1 || resources[1].descriptor.numSlices != 3 ||
     resources[2].access.index != 2 ||
     resources[2].descriptor.resource != cubeTexture.resourceId ||
     resources[2].descriptor.textureType != TextureType::TextureCube ||
     resources[2].descriptor.numMips != 1 || resources[2].descriptor.numSlices != 6)
    return fail("generic texture descriptors do not expose mip/slice/face counts");

  const ShaderReflection *reflection = pipe.GetShaderReflection(ShaderStage::Fragment);
  if(reflection == NULL || reflection->readOnlyResources.size() != 3 ||
     reflection->readOnlyResources[0].name != "mipTexture" ||
     reflection->readOnlyResources[0].textureType != TextureType::Texture2D ||
     reflection->readOnlyResources[1].name != "arrayTexture" ||
     reflection->readOnlyResources[1].textureType != TextureType::Texture2DArray ||
     reflection->readOnlyResources[2].name != "cubeTexture" ||
     reflection->readOnlyResources[2].textureType != TextureType::TextureCube)
    return fail("fragment shader reflection does not preserve texture binding types");

  static const byte colours[12][4] = {
      {255, 32, 16, 255},  {16, 224, 48, 255},  {24, 64, 255, 255},
      {240, 208, 32, 255}, {224, 48, 192, 255}, {32, 208, 224, 255},
      {255, 128, 32, 255}, {128, 32, 255, 255},  {32, 255, 128, 255},
      {255, 64, 128, 255}, {128, 255, 32, 255},  {32, 128, 255, 255},
  };

  auto displayMatches = [output](ResourceId resource, const Subresource &sub,
                                 const byte colour[4]) {
    TextureDisplay display;
    display.resourceId = resource;
    display.subresource = sub;
    display.scale = -1.0f;
    display.rangeMin = 0.0f;
    display.rangeMax = 1.0f;
    display.red = display.green = display.blue = display.alpha = true;
    display.rawOutput = true;
    output->SetTextureDisplay(display);
    output->Display();
    const bytebuf pixels = output->ReadbackOutputTexture();
    if(pixels.size() != 640 * 480 * 3)
      return false;
    const size_t centre = (size_t(240) * 640 + 320) * 3;
    return pixels[centre + 0] == colour[0] && pixels[centre + 1] == colour[1] &&
           pixels[centre + 2] == colour[2];
  };

  for(uint32_t mip = 0; mip < 3; mip++)
  {
    const uint32_t size = 4U >> mip;
    const bytebuf data = renderer->GetTextureData(mipTexture.resourceId, {mip, 0, 0});
    if(data.size() != size_t(size * size * 4))
      return fail("mip readback size does not match");
    for(size_t i = 0; i < data.size(); i += 4)
      if(memcmp(data.data() + i, colours[mip], 4) != 0)
        return fail("mip readback texels do not match");
    if(!displayMatches(mipTexture.resourceId, {mip, 0, 0}, colours[mip]))
      return fail("mip display did not render the selected level");
  }
  for(uint32_t slice = 0; slice < 3; slice++)
  {
    const bytebuf data = renderer->GetTextureData(arrayTexture.resourceId, {0, slice, 0});
    if(data.size() != 64)
      return fail("array slice readback size does not match");
    for(size_t i = 0; i < data.size(); i += 4)
      if(memcmp(data.data() + i, colours[3 + slice], 4) != 0)
        return fail("array slice readback texels do not match");
    if(!displayMatches(arrayTexture.resourceId, {0, slice, 0}, colours[3 + slice]))
      return fail("array display did not render the selected slice");
  }
  for(uint32_t face = 0; face < 6; face++)
  {
    const Subresource sub = {0, face, 0};
    const bytebuf data = renderer->GetTextureData(cubeTexture.resourceId, sub);
    const PixelValue pixel =
        renderer->PickPixel(cubeTexture.resourceId, 2, 2, sub, CompType::Typeless);
    if(data.size() != 64)
      return fail("cube face readback size does not match");
    for(size_t i = 0; i < data.size(); i += 4)
      if(memcmp(data.data() + i, colours[6 + face], 4) != 0)
        return fail("cube face readback texels do not match");
    if(!Near(pixel.floatValue[0], colours[6 + face][0] / 255.0f) ||
       !Near(pixel.floatValue[1], colours[6 + face][1] / 255.0f) ||
       !Near(pixel.floatValue[2], colours[6 + face][2] / 255.0f) ||
       !Near(pixel.floatValue[3], 1.0f))
      return fail("cube face PickPixel value does not match");
    if(!displayMatches(cubeTexture.resourceId, sub, colours[6 + face]))
      return fail("cube display did not render the selected face");
  }

  if(!ValidateHeadlessThumbnail(renderer, mipTexture.resourceId, draws[0]->eventId, true,
                                {2, 0, 0}) ||
     !ValidateHeadlessThumbnail(renderer, arrayTexture.resourceId, draws[0]->eventId, true,
                                {0, 2, 0}) ||
     !ValidateHeadlessThumbnail(renderer, cubeTexture.resourceId, draws[0]->eventId, true,
                                {0, 5, 0}))
    return fail("mip/array/cube thumbnails are blank");

  if(!renderer->GetTextureData(mipTexture.resourceId, {3, 0, 0}).empty() ||
     !renderer->GetTextureData(arrayTexture.resourceId, {0, 3, 0}).empty() ||
     !renderer->GetTextureData(cubeTexture.resourceId, {0, 6, 0}).empty())
    return fail("out-of-range mip/slice/face readback was not rejected");

  for(uint32_t band = 0; band < 12; band++)
  {
    const uint32_t x = (band * 400 + 200) / 12;
    if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, x, 150,
                     colours[band][0] / 255.0f, colours[band][1] / 255.0f,
                     colours[band][2] / 255.0f))
      return fail("sampled output band does not match its subresource colour");
  }

  if(savePath != NULL)
  {
    TextureSave save;
    save.resourceId = cubeTexture.resourceId;
    save.destType = FileType::DDS;
    save.mip = -1;
    save.slice.sliceIndex = -1;
    ResultDetails result = renderer->SaveTexture(save, savePath);
    if(!result.OK())
      return fail("cube DDS save failed");
    std::ifstream file(savePath, std::ios::binary | std::ios::ate);
    if(!file || file.tellg() != std::streampos(128 + 6 * 4 * 4 * 4))
      return fail("cube DDS save did not contain all six faces");
  }

  return true;
}

static bool PixelMatches(IReplayController *renderer, ResourceId texture, uint32_t eventId,
                         uint32_t x, uint32_t y, float r, float g, float b)
{
  renderer->SetFrameEvent(eventId, true);
  const PixelValue pixel =
      renderer->PickPixel(texture, x, y, {0, 0, 0}, CompType::Typeless);
  return Near(pixel.floatValue[0], r) && Near(pixel.floatValue[1], g) &&
         Near(pixel.floatValue[2], b) && Near(pixel.floatValue[3], 1.0f);
}

static bool PixelMatchesRGBA(IReplayController *renderer, ResourceId texture, uint32_t eventId,
                             uint32_t x, uint32_t y, float r, float g, float b, float a)
{
  renderer->SetFrameEvent(eventId, true);
  const PixelValue pixel =
      renderer->PickPixel(texture, x, y, {0, 0, 0}, CompType::Typeless);
  return Near(pixel.floatValue[0], r) && Near(pixel.floatValue[1], g) &&
         Near(pixel.floatValue[2], b) && Near(pixel.floatValue[3], a);
}

static bool HasUsage(IReplayController *renderer, ResourceId resource, ResourceUsage usage)
{
  for(const EventUsage &event : renderer->GetUsage(resource))
    if(event.usage == usage)
      return true;
  return false;
}

static bool HasUsageAt(IReplayController *renderer, ResourceId resource, ResourceUsage usage,
                       uint32_t eventId)
{
  for(const EventUsage &event : renderer->GetUsage(resource))
    if(event.usage == usage && event.eventId == eventId)
      return true;
  return false;
}

static const ActionDescription *FindNamedAction(const rdcarray<ActionDescription> &actions,
                                                const char *prefix)
{
  for(const ActionDescription &action : actions)
  {
    if(action.customName.find(prefix) == 0)
      return &action;
    if(const ActionDescription *child = FindNamedAction(action.children, prefix))
      return child;
  }
  return NULL;
}

static bool ValidateComputeFixture(IReplayController *renderer, ResourceId colorTarget,
                                   const char *savePath)
{
  rdcarray<const ActionDescription *> computeActions;
  FindActions(renderer->GetRootActions(), computeActions);
  for(const ActionDescription *action : computeActions)
    if(action->flags & ActionFlags::Dispatch)
    {
      renderer->SetFrameEvent(action->eventId, true);
      const MetalPipe::State *state = renderer->GetPipelineState().GetMetalPipelineState();
      if(state && !state->computeSamplers.empty())
        return true;
      break;
    }
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 304 &&
       HasUsage(renderer, buffer.resourceId, ResourceUsage::CS_Resource))
      return true;
  ResourceId source;
  ResourceId destination;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(texture.width != 8 || texture.height != 8 || texture.mips != 1 ||
       texture.format.compType != CompType::UNorm || texture.format.compCount != 4 ||
       texture.format.compByteWidth != 1)
      continue;
    if(HasUsage(renderer, texture.resourceId, ResourceUsage::CS_Resource))
      source = texture.resourceId;
    if(HasUsage(renderer, texture.resourceId, ResourceUsage::CS_RWResource))
      destination = texture.resourceId;
  }
  if(source == ResourceId() && destination == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal compute fixture validation failed: %s\n", message);
    return false;
  };
  if(source == ResourceId() || destination == ResourceId())
    return fail("compute source/destination usage is missing");

  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *begin = NULL;
  const ActionDescription *dispatch = NULL;
  const ActionDescription *end = NULL;
  const ActionDescription *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Begin Metal Compute Pass")
      begin = action;
    if(action->flags & ActionFlags::Dispatch)
      dispatch = action;
    if(action->customName == "End Metal Compute Pass")
      end = action;
    if(action->flags & ActionFlags::Drawcall)
      draw = action;
  }
  if(!begin || !dispatch || !end || !draw || begin->eventId >= dispatch->eventId ||
     dispatch->eventId >= end->eventId || end->eventId >= draw->eventId ||
     dispatch->dispatchDimension[0] != 2 || dispatch->dispatchDimension[1] != 2 ||
     dispatch->dispatchDimension[2] != 1)
    return fail("compute event order or dispatch dimensions are wrong");

  renderer->SetFrameEvent(begin->eventId, true);
  bytebuf pixels = renderer->GetTextureData(destination, {0, 0, 0});
  if(pixels.size() != 256)
    return fail("pre-dispatch destination readback is unavailable");
  for(byte value : pixels)
    if(value != 0)
    {
      fprintf(stderr, "pre-dispatch bytes: %u %u %u %u\n", pixels[0], pixels[1], pixels[2], pixels[3]);
      return fail("pre-dispatch destination is not zero");
    }

  renderer->SetFrameEvent(dispatch->eventId, true);
  const PipeState &computeState = renderer->GetPipelineState();
  if(computeState.GetComputePipelineObject() == ResourceId() ||
     computeState.GetShader(ShaderStage::Compute) == ResourceId() ||
     computeState.GetShaderEntryPoint(ShaderStage::Compute) != "filter_main")
    return fail("compute pipeline or shader state is missing");
  const MetalPipe::State *metal = computeState.GetMetalPipelineState();
  if(!metal || metal->computeTextures.size() != 2 || metal->computeTextures[0] != source ||
     metal->computeTextures[1] != destination)
    return fail("compute texture bindings are wrong");
  const rdcarray<UsedDescriptor> readOnly =
      computeState.GetReadOnlyResources(ShaderStage::Compute, true);
  const rdcarray<UsedDescriptor> readWrite =
      computeState.GetReadWriteResources(ShaderStage::Compute, true);
  if(readOnly.size() != 1 || readWrite.size() != 1 ||
     readOnly[0].descriptor.resource != source ||
     readWrite[0].descriptor.resource != destination ||
     readOnly[0].access.stage != ShaderStage::Compute ||
     readWrite[0].access.stage != ShaderStage::Compute)
    return fail("compute descriptors do not expose the read/write textures");

  pixels = renderer->GetTextureData(destination, {0, 0, 0});
  if(pixels.size() != 256)
    return fail("post-dispatch destination readback is unavailable");
  for(uint32_t y = 0; y < 8; y++)
    for(uint32_t x = 0; x < 8; x++)
    {
      const byte expected[4] = {byte(16 + 8 * (x + y)), byte(32 + 24 * x),
                                byte(24 + 24 * y), 255};
      if(memcmp(pixels.data() + (y * 8 + x) * 4, expected, 4) != 0)
        return fail("post-dispatch texels do not match the CPU swizzle reference");
    }

  renderer->SetFrameEvent(begin->eventId, true);
  pixels = renderer->GetTextureData(destination, {0, 0, 0});
  for(byte value : pixels)
    if(value != 0)
      return fail("rewinding before dispatch did not restore zero texels");

  if(!ValidateHeadlessThumbnail(renderer, destination, begin->eventId, false) ||
     !ValidateHeadlessThumbnail(renderer, source, dispatch->eventId, true) ||
     !ValidateHeadlessThumbnail(renderer, destination, dispatch->eventId, true))
    return fail("compute Input/Output thumbnails are blank or stale");

  renderer->SetFrameEvent(draw->eventId, true);
  metal = renderer->GetPipelineState().GetMetalPipelineState();
  if(!metal || metal->fragmentTextures.size() != 1 || metal->fragmentTextures[0] != destination)
    return fail("final render draw is not sampling the compute destination");
  if(!PixelMatches(renderer, colorTarget, draw->eventId, 25, 20, 16.0f / 255.0f,
                   32.0f / 255.0f, 24.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 375, 280, 128.0f / 255.0f,
                   200.0f / 255.0f, 192.0f / 255.0f))
    return fail("final render output did not sample the boundary texels");

  if(savePath)
  {
    TextureSave save;
    save.resourceId = destination;
    ResultDetails result = renderer->SaveTexture(save, savePath);
    if(!result.OK())
      return fail("compute destination DDS save failed");
    std::ifstream file(savePath, std::ios::binary | std::ios::ate);
    if(!file || file.tellg() != std::streampos(128 + 256))
      return fail("compute destination DDS byte size is wrong");
  }
  return true;
}

static bool ValidateComputeSamplerFixture(IReplayController *renderer, ResourceId colorTarget,
                                          const char *savePath)
{
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  rdcarray<const ActionDescription *> dispatches;
  const ActionDescription *begin = NULL, *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Begin Metal Compute Pass") begin = action;
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draw = action;
  }
  if(dispatches.size() != 2)
    return true;
  renderer->SetFrameEvent(dispatches[0]->eventId, true);
  const MetalPipe::State *state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!state || state->computeSamplers.size() != 1)
    return true;
  auto fail = [](const char *message) { return PipelineFailure(message); };
  if(!begin || !draw || !(begin->eventId < dispatches[0]->eventId &&
                         dispatches[0]->eventId < dispatches[1]->eventId &&
                         dispatches[1]->eventId < draw->eventId))
    return fail("T30 action sequence incorrect");
  const SDFile &structured = renderer->GetStructuredFile();
  uint32_t samplerEIDs[2] = {};
  for(size_t i = 0; i < 2; i++)
    for(const APIEvent &event : dispatches[i]->events)
      if(event.chunkIndex < structured.chunks.size() &&
         structured.chunks[event.chunkIndex]->name ==
             "MTLComputeCommandEncoder::setSamplerState")
        samplerEIDs[i] = event.eventId;
  if(!(begin->eventId < samplerEIDs[0] && samplerEIDs[0] < dispatches[0]->eventId &&
       dispatches[0]->eventId < samplerEIDs[1] &&
       samplerEIDs[1] < dispatches[1]->eventId))
    return fail("T30 sampler API EIDs incorrect");
  ResourceId source = state->computeTextures[0];
  ResourceId point = state->computeTextures[1];
  ResourceId pointSampler = state->computeSamplers[0];
  auto samplers = renderer->GetPipelineState().GetSamplers(ShaderStage::Compute, true);
  if(source == ResourceId() || point == ResourceId() || pointSampler == ResourceId() ||
     samplers.size() != 1 || samplers[0].sampler.object != pointSampler ||
     samplers[0].sampler.filter.minify != FilterMode::Point ||
     samplers[0].sampler.filter.magnify != FilterMode::Point ||
     samplers[0].sampler.addressU != AddressMode::ClampEdge ||
     samplers[0].access.byteOffset != 0x1000)
    return fail("T30 point sampler descriptor incorrect");
  renderer->SetFrameEvent(samplerEIDs[0], true);
  state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!state || state->computeSamplers[0] != pointSampler)
    return fail("T30 point sampler event state incorrect");
  renderer->SetFrameEvent(begin->eventId, true);
  bytebuf pixels = renderer->GetTextureData(point, {0, 0, 0});
  for(byte value : pixels) if(value != 0) return fail("T30 point sentinel incorrect");
  renderer->SetFrameEvent(dispatches[0]->eventId, true);
  pixels = renderer->GetTextureData(point, {0, 0, 0});
  if(pixels.size() != 256) return fail("T30 point texture size incorrect");
  for(uint32_t y = 0; y < 8; y++)
    for(uint32_t x = 0; x < 8; x++)
    {
      const byte expected[4] = {byte(16 + 8 * (x + y)), byte(32 + 24 * x),
                                byte(24 + 24 * y), 255};
      if(memcmp(pixels.data() + (y * 8 + x) * 4, expected, 4))
        return fail("T30 point texels incorrect");
    }
  renderer->SetFrameEvent(dispatches[1]->eventId, true);
  state = renderer->GetPipelineState().GetMetalPipelineState();
  ResourceId linear = state->computeTextures[1];
  ResourceId linearSampler = state->computeSamplers[0];
  samplers = renderer->GetPipelineState().GetSamplers(ShaderStage::Compute, true);
  if(linear == point || linearSampler == pointSampler || samplers.size() != 1 ||
     samplers[0].sampler.object != linearSampler ||
     samplers[0].sampler.filter.minify != FilterMode::Linear ||
     samplers[0].sampler.filter.magnify != FilterMode::Linear ||
     samplers[0].sampler.addressU != AddressMode::ClampEdge ||
     samplers[0].access.byteOffset != 0x1000)
    return fail("T30 linear sampler descriptor incorrect");
  renderer->SetFrameEvent(samplerEIDs[1], true);
  state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!state || state->computeSamplers[0] != linearSampler)
    return fail("T30 linear sampler event state incorrect");
  renderer->SetFrameEvent(dispatches[1]->eventId, true);
  pixels = renderer->GetTextureData(linear, {0, 0, 0});
  if(pixels.size() != 256) return fail("T30 linear texture size incorrect");
  for(uint32_t y = 0; y < 8; y++)
    for(uint32_t x = 0; x < 8; x++)
    {
      const uint32_t dx = x < 7 ? 1 : 0, dy = y < 7 ? 1 : 0;
      const byte expected[4] = {byte(16 + 8 * (x + y) + 2 * (dx + dy)),
                                byte(32 + 24 * x + 6 * dx),
                                byte(24 + 24 * y + 6 * dy), 255};
      if(memcmp(pixels.data() + (y * 8 + x) * 4, expected, 4))
        return fail("T30 linear texels incorrect");
    }
  renderer->SetFrameEvent(begin->eventId, true);
  pixels = renderer->GetTextureData(linear, {0, 0, 0});
  for(byte value : pixels) if(value != 0) return fail("T30 seek rewind incorrect");
  if(const ActionDescription *end =
         FindNamedAction(renderer->GetRootActions(), "End Metal Compute Pass"))
    fprintf(stderr, "T30 compute scope begin=%u end=%u\n", begin->eventId, end->eventId);
  if(!ValidateHeadlessThumbnail(renderer, point, begin->eventId, false) ||
     !ValidateHeadlessThumbnail(renderer, source, dispatches[0]->eventId, true) ||
     !ValidateHeadlessThumbnail(renderer, point, dispatches[0]->eventId, true) ||
     !ValidateHeadlessThumbnail(renderer, linear, dispatches[1]->eventId, true))
    return fail("T30 Input/Output thumbnails incorrect");
  if(!HasUsage(renderer, source, ResourceUsage::CS_Resource) ||
     !HasUsage(renderer, point, ResourceUsage::CS_RWResource) ||
     !HasUsage(renderer, linear, ResourceUsage::CS_RWResource))
    return fail("T30 texture usage incorrect");
  if(!PixelMatches(renderer, colorTarget, draw->eventId, 25, 20,
                   20.0f / 255, 38.0f / 255, 30.0f / 255) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 375, 280,
                   128.0f / 255, 200.0f / 255, 192.0f / 255))
    return fail("T30 final render output incorrect");
  if(savePath)
  {
    renderer->SetFrameEvent(dispatches[1]->eventId, true);
    TextureSave save;
    save.resourceId = linear;
    if(!renderer->SaveTexture(save, savePath).OK())
      return fail("T30 DDS export failed");
  }
  fprintf(stderr, "T30 EIDs begin=%u sampler=%u point=%u sampler=%u linear=%u draw=%u\n",
          begin->eventId, samplerEIDs[0], dispatches[0]->eventId,
          samplerEIDs[1], dispatches[1]->eventId, draw->eventId);
  return true;
}

static bool ValidateComputeBatchFixture(IReplayController *renderer, ResourceId colorTarget,
                                        const char *savePath)
{
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  rdcarray<const ActionDescription *> dispatches;
  const ActionDescription *begin = NULL, *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Begin Metal Compute Pass") begin = action;
    if(action->flags & ActionFlags::Dispatch) dispatches.push_back(action);
    if(action->flags & ActionFlags::Drawcall) draw = action;
  }
  if(dispatches.size() != 2)
    return true;
  renderer->SetFrameEvent(dispatches[0]->eventId, true);
  const MetalPipe::State *state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!state || state->computeSamplers.size() < 4)
    return true;
  auto fail = [](const char *message) { return PipelineFailure(message); };
  if(!begin || !draw || !(begin->eventId < dispatches[0]->eventId &&
                         dispatches[0]->eventId < dispatches[1]->eventId &&
                         dispatches[1]->eventId < draw->eventId))
    return fail("T31 action sequence incorrect");
  if(begin->depthOut != ResourceId())
    return fail("T31 compute begin unexpectedly has a depth output");
  for(ResourceId output : begin->outputs)
    if(output != ResourceId())
      return fail("T31 compute begin unexpectedly has a color output");
  if(state->computeTextures.size() < 6 || state->computeTextures[0] != ResourceId() ||
     state->computeTextures[1] == ResourceId() || state->computeTextures[2] != ResourceId() ||
     state->computeTextures[3] == ResourceId() || state->computeSamplers[2] == ResourceId() ||
     state->computeSamplers[3] != ResourceId() || state->computeBuffers.size() < 7 ||
     state->computeBuffers[4].resourceId == ResourceId() ||
     state->computeBuffers[5].resourceId != ResourceId() ||
     state->computeBuffers[6].resourceId == ResourceId() ||
     state->computeBuffers[4].byteOffset != 32 || state->computeBuffers[6].byteOffset != 64)
    return fail("T31 batch slots or null clears incorrect");
  const ResourceId source = state->computeTextures[1];
  const ResourceId point = state->computeTextures[3];
  const ResourceId input = state->computeBuffers[4].resourceId;
  const ResourceId output = state->computeBuffers[6].resourceId;
  const ResourceId pointSampler = state->computeSamplers[2];
  auto ro = renderer->GetPipelineState().GetReadOnlyResources(ShaderStage::Compute, true);
  auto rw = renderer->GetPipelineState().GetReadWriteResources(ShaderStage::Compute, true);
  auto samplers = renderer->GetPipelineState().GetSamplers(ShaderStage::Compute, true);
  if(ro.size() != 2 || rw.size() != 2 || samplers.size() != 1 ||
     samplers[0].sampler.object != pointSampler ||
     samplers[0].sampler.filter.minify != FilterMode::Point ||
     samplers[0].sampler.addressU != AddressMode::ClampEdge ||
     samplers[0].access.byteOffset != 0x1002)
    return fail("T31 reflected descriptors incorrect");
  bool readTexture = false, readBuffer = false, writeTexture = false, writeBuffer = false;
  for(const UsedDescriptor &binding : ro)
  {
    if(binding.access.type == DescriptorType::Image &&
       binding.descriptor.resource == source && binding.access.byteOffset == 0x301)
      readTexture = true;
    if(binding.access.type == DescriptorType::Buffer &&
       binding.descriptor.resource == input && binding.descriptor.byteOffset == 32 &&
       binding.access.byteOffset == 0xB04)
      readBuffer = true;
  }
  for(const UsedDescriptor &binding : rw)
  {
    if(binding.access.type == DescriptorType::ReadWriteImage &&
       binding.descriptor.resource == point && binding.access.byteOffset == 0x403)
      writeTexture = true;
    if(binding.access.type == DescriptorType::ReadWriteBuffer &&
       binding.descriptor.resource == output && binding.descriptor.byteOffset == 64 &&
       binding.access.byteOffset == 0xC06)
      writeBuffer = true;
  }
  if(!readTexture || !readBuffer || !writeTexture || !writeBuffer)
    return fail("T31 descriptor resources, slots or buffer offsets incorrect");
  const auto allRO = renderer->GetPipelineState().GetReadOnlyResources(ShaderStage::Compute, false);
  const auto allSamplers = renderer->GetPipelineState().GetSamplers(ShaderStage::Compute, false);
  bool unusedTexture = false, unusedBuffer = false, unusedSampler = false;
  for(const UsedDescriptor &binding : allRO)
  {
    if(binding.access.staticallyUnused &&
       binding.access.type == DescriptorType::Image &&
       binding.descriptor.resource == state->computeTextures[5]) unusedTexture = true;
    if(binding.access.staticallyUnused &&
       binding.access.type == DescriptorType::Buffer &&
       binding.descriptor.resource == state->computeBuffers[8].resourceId) unusedBuffer = true;
  }
  for(const UsedDescriptor &binding : allSamplers)
    if(binding.access.staticallyUnused &&
       binding.sampler.object == state->computeSamplers[5]) unusedSampler = true;
  if(!unusedTexture || !unusedBuffer || !unusedSampler)
    return fail("T31 declared unused bindings not exposed");
  const SDFile &structured = renderer->GetStructuredFile();
  const char *names[] = {"MTLComputeCommandEncoder::setTextures",
                         "MTLComputeCommandEncoder::setSamplerStates",
                         "MTLComputeCommandEncoder::setBuffers"};
  uint32_t bindingEIDs[3] = {};
  for(const APIEvent &event : dispatches[0]->events)
    if(event.chunkIndex < structured.chunks.size())
      for(size_t i = 0; i < 3; i++)
        if(structured.chunks[event.chunkIndex]->name == names[i])
          bindingEIDs[i] = event.eventId;
  if(!(begin->eventId < bindingEIDs[0] && bindingEIDs[0] < bindingEIDs[1] &&
       bindingEIDs[1] < bindingEIDs[2] && bindingEIDs[2] < dispatches[0]->eventId))
    return fail("T31 batch API EIDs incorrect");
  renderer->SetFrameEvent(bindingEIDs[0], true);
  state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!state || state->computeTextures[2] != ResourceId() ||
     state->computeTextures[3] != point)
    return fail("T31 texture batch event state incorrect");
  renderer->SetFrameEvent(bindingEIDs[1], true);
  state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!state || state->computeSamplers[3] != ResourceId() ||
     state->computeSamplers[2] != pointSampler)
    return fail("T31 sampler batch event state incorrect");
  renderer->SetFrameEvent(bindingEIDs[2], true);
  state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!state || state->computeBuffers[5].resourceId != ResourceId() ||
     state->computeBuffers[6].resourceId != output)
    return fail("T31 buffer batch event state incorrect");
  renderer->SetFrameEvent(begin->eventId, true);
  bytebuf pixels = renderer->GetTextureData(point, {0, 0, 0});
  for(byte value : pixels) if(value != 0) return fail("T31 pre-dispatch texture sentinel incorrect");
  bytebuf bytes = renderer->GetBufferData(output, 0, 0);
  if(bytes.size() != 336) return fail("T31 output buffer size incorrect");
  for(byte value : bytes) if(value != 0xa5) return fail("T31 pre-dispatch buffer sentinel incorrect");
  for(size_t dispatchIndex = 0; dispatchIndex < 2; dispatchIndex++)
  {
    renderer->SetFrameEvent(dispatches[dispatchIndex]->eventId, true);
    state = renderer->GetPipelineState().GetMetalPipelineState();
    const ResourceId texture = state->computeTextures[3];
    if(state->computeBuffers[6].byteOffset != (dispatchIndex ? 80 : 64))
      return fail("T31 direct buffer override incorrect");
    samplers = renderer->GetPipelineState().GetSamplers(ShaderStage::Compute, true);
    if(samplers.size() != 1 ||
       samplers[0].sampler.filter.minify != (dispatchIndex ? FilterMode::Linear : FilterMode::Point))
      return fail("T31 direct sampler override incorrect");
    pixels = renderer->GetTextureData(texture, {0, 0, 0});
    if(pixels.size() != 256) return fail("T31 texture readback size incorrect");
    for(uint32_t y = 0; y < 8; y++)
      for(uint32_t x = 0; x < 8; x++)
      {
        const uint32_t dx = dispatchIndex && x < 7 ? 1 : 0;
        const uint32_t dy = dispatchIndex && y < 7 ? 1 : 0;
        const byte expected[4] = {byte(19 + 2 * (y * 8 + x)),
                                  byte(32 + 24 * x + 6 * dx),
                                  byte(24 + 24 * y + 6 * dy), 255};
        if(memcmp(pixels.data() + (y * 8 + x) * 4, expected, 4))
          return fail("T31 texture pixels incorrect");
      }
  }
  bytes = renderer->GetBufferData(output, 0, 0);
  if(bytes.size() != 336) return fail("T31 output buffer readback incorrect");
  for(size_t i = 0; i < bytes.size(); i++)
  {
    byte expected = 0xa5;
    if(i >= 64 && i < 80)
    {
      uint32_t value = 19 + (uint32_t(i - 64) / 4) * 2;
      expected = reinterpret_cast<byte *>(&value)[(i - 64) % 4];
    }
    else if(i >= 80 && i < 336)
    {
      uint32_t value = 19 + (uint32_t(i - 80) / 4) * 2;
      expected = reinterpret_cast<byte *>(&value)[(i - 80) % 4];
    }
    if(bytes[i] != expected) return fail("T31 output raw bytes or untouched sentinel incorrect");
  }
  renderer->SetFrameEvent(begin->eventId, true);
  bytes = renderer->GetBufferData(output, 0, 0);
  for(byte value : bytes) if(value != 0xa5) return fail("T31 seek rewind buffer incorrect");
  if(!ValidateHeadlessThumbnail(renderer, point, begin->eventId, false) ||
     !ValidateHeadlessThumbnail(renderer, source, dispatches[0]->eventId, true) ||
     !ValidateHeadlessThumbnail(renderer, point, dispatches[0]->eventId, true))
    return fail("T31 Input/Output thumbnails incorrect");
  if(!HasUsage(renderer, source, ResourceUsage::CS_Resource) ||
     !HasUsage(renderer, input, ResourceUsage::CS_Resource) ||
     !HasUsage(renderer, output, ResourceUsage::CS_RWResource))
    return fail("T31 usage incorrect");
  if(!PixelMatches(renderer, colorTarget, draw->eventId, 25, 20,
                   19.0f / 255, 38.0f / 255, 30.0f / 255) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 375, 280,
                   145.0f / 255, 200.0f / 255, 192.0f / 255))
    return fail("T31 final render output incorrect");
  if(savePath)
  {
    renderer->SetFrameEvent(dispatches[1]->eventId, true);
    TextureSave save;
    save.resourceId = renderer->GetPipelineState().GetMetalPipelineState()->computeTextures[3];
    if(!renderer->SaveTexture(save, savePath).OK())
      return fail("T31 DDS save failed");
    const bytebuf rawBytes = renderer->GetBufferData(output, 0, 0);
    std::string rawPath = std::string(savePath) + ".bin";
    std::ofstream raw(rawPath, std::ios::binary);
    raw.write((const char *)rawBytes.data(), rawBytes.size());
    if(!raw || rawBytes.size() != 336)
      return fail("T31 raw buffer save failed");
  }
  fprintf(stderr, "T31 EIDs begin=%u batches=%u,%u,%u point=%u linear=%u draw=%u\n",
          begin->eventId, bindingEIDs[0], bindingEIDs[1], bindingEIDs[2],
          dispatches[0]->eventId, dispatches[1]->eventId, draw->eventId);
  const uint32_t beginEID = begin->eventId;
  renderer->AddFakeMarkers();
  bool beginAtRoot = false, endAtRoot = false;
  uint32_t endEID = 0;
  uint32_t copyBeginEID = 0, copyEndEID = 0;
  for(const ActionDescription &action : renderer->GetRootActions())
  {
    beginAtRoot |= action.eventId == beginEID && action.customName == "Begin Metal Compute Pass";
    endAtRoot |= action.customName == "End Metal Compute Pass";
    if(action.customName == "End Metal Compute Pass")
      endEID = action.eventId;
    if(action.IsFakeMarker() && action.customName.find("Copy/Clear Pass") == 0 &&
       !action.children.empty())
    {
      copyBeginEID = action.children.front().events.front().eventId;
      copyEndEID = action.children.back().eventId;
    }
  }
  if(!beginAtRoot || !endAtRoot || copyEndEID >= beginEID)
    return fail("T31 fake pass marker swallowed a compute pass boundary");
  fprintf(stderr, "T31 copy group=%u-%u, compute scope begin=%u end=%u at root\n",
          copyBeginEID, copyEndEID, beginEID, endEID);
  return true;
}

static bool ValidateDispatchThreadsFixture(IReplayController *renderer, ResourceId colorTarget,
                                           const char *savePath)
{
  ResourceId source, destination;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(texture.width != 10 || texture.height != 7)
      continue;
    if(HasUsage(renderer, texture.resourceId, ResourceUsage::CS_Resource))
      source = texture.resourceId;
    if(HasUsage(renderer, texture.resourceId, ResourceUsage::CS_RWResource))
      destination = texture.resourceId;
  }
  if(source == ResourceId() && destination == ResourceId())
    return true;
  if(source == ResourceId() || destination == ResourceId())
    return PipelineFailure("T28 texture usage missing");

  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *begin = NULL, *dispatch = NULL, *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Begin Metal Compute Pass")
      begin = action;
    if(action->flags & ActionFlags::Dispatch)
      dispatch = action;
    if(action->flags & ActionFlags::Drawcall)
      draw = action;
  }
  if(!begin || !dispatch || !draw || begin->eventId >= dispatch->eventId ||
     dispatch->eventId >= draw->eventId ||
     !dispatch->customName.contains("dispatchThreads") ||
     dispatch->dispatchDimension[0] != 7 || dispatch->dispatchDimension[1] != 5 ||
     dispatch->dispatchThreadsDimension[0] != 4 || dispatch->dispatchThreadsDimension[1] != 3)
    return PipelineFailure("T28 action/grid incorrect");

  renderer->SetFrameEvent(begin->eventId, true);
  bytebuf bytes = renderer->GetTextureData(destination, {0, 0, 0});
  if(bytes.size() != 280)
    return PipelineFailure("T28 pre-dispatch size incorrect");
  for(byte b : bytes)
    if(b != 0)
      return PipelineFailure("T28 pre-dispatch sentinel changed");

  renderer->SetFrameEvent(dispatch->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *metal = pipe.GetMetalPipelineState();
  if(pipe.GetComputePipelineObject() == ResourceId() ||
     pipe.GetShaderEntryPoint(ShaderStage::Compute) != "filter_main" || !metal ||
     metal->computeTextures.size() != 2 || metal->computeTextures[0] != source ||
     metal->computeTextures[1] != destination)
    return PipelineFailure("T28 compute pipeline/binding incorrect");
  auto ro = pipe.GetReadOnlyResources(ShaderStage::Compute, true);
  auto rw = pipe.GetReadWriteResources(ShaderStage::Compute, true);
  if(ro.size() != 1 || rw.size() != 1 || ro[0].descriptor.resource != source ||
     rw[0].descriptor.resource != destination)
    return PipelineFailure("T28 descriptors incorrect");
  bytes = renderer->GetTextureData(destination, {0, 0, 0});
  if(bytes.size() != 280)
    return PipelineFailure("T28 post-dispatch size incorrect");
  for(uint32_t y = 0; y < 7; y++)
    for(uint32_t x = 0; x < 10; x++)
    {
      const byte expected[4] = {byte(x < 7 && y < 5 ? 16 + 8 * (x + y) : 0),
                                byte(x < 7 && y < 5 ? 32 + 24 * x : 0),
                                byte(x < 7 && y < 5 ? 24 + 24 * y : 0),
                                byte(x < 7 && y < 5 ? 255 : 0)};
      if(memcmp(bytes.data() + (y * 10 + x) * 4, expected, 4) != 0)
        return PipelineFailure("T28 thread coverage or untouched sentinel incorrect");
    }
  renderer->SetFrameEvent(begin->eventId, true);
  bytes = renderer->GetTextureData(destination, {0, 0, 0});
  for(byte b : bytes)
    if(b != 0)
      return PipelineFailure("T28 seek rewind incorrect");
  if(!ValidateHeadlessThumbnail(renderer, destination, begin->eventId, false) ||
     !ValidateHeadlessThumbnail(renderer, source, dispatch->eventId, true) ||
     !ValidateHeadlessThumbnail(renderer, destination, dispatch->eventId, true))
    return PipelineFailure("T28 Input/Output headless thumbnails are blank or stale");
  renderer->SetFrameEvent(draw->eventId, true);
  metal = renderer->GetPipelineState().GetMetalPipelineState();
  if(!metal || metal->fragmentTextures.empty() || metal->fragmentTextures[0] != destination)
    return PipelineFailure("T28 render binding incorrect");
  if(!PixelMatches(renderer, colorTarget, draw->eventId, 25, 20,
                   16.0f / 255.0f, 32.0f / 255.0f, 24.0f / 255.0f) ||
     !PixelMatchesRGBA(renderer, colorTarget, draw->eventId, 550, 350,
                       0.0f, 0.0f, 0.0f, 0.0f))
    return PipelineFailure("T28 render output incorrect");
  if(savePath)
  {
    TextureSave save;
    save.resourceId = destination;
    if(!renderer->SaveTexture(save, savePath).OK())
      return PipelineFailure("T28 DDS save failed");
    std::ifstream file(savePath, std::ios::binary | std::ios::ate);
    if(!file || file.tellg() != std::streampos(128 + 280))
      return PipelineFailure("T28 DDS byte size incorrect");
  }
  fprintf(stderr, "T28 EIDs begin=%u dispatch=%u draw=%u\n",
          begin->eventId, dispatch->eventId, draw->eventId);
  for(const ActionDescription *action : actions)
    fprintf(stderr, "T28 action EID %u: %s\n", action->eventId, action->customName.c_str());
  return true;
}

static bool ValidateComputeBufferFixture(IReplayController *renderer, ResourceId colorTarget,
                                         const char *savePath)
{
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 44 && (buffer.creationFlags & BufferCategory::Indirect))
      return true;
  ResourceId input, output;
  ResourceId destinationTexture;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 304 && HasUsage(renderer, buffer.resourceId, ResourceUsage::CS_Resource))
      input = buffer.resourceId;
    if(buffer.length == 336 && HasUsage(renderer, buffer.resourceId, ResourceUsage::CS_RWResource))
      output = buffer.resourceId;
  }
  if(input == ResourceId() && output == ResourceId())
    return true;
  if(input == ResourceId() || output == ResourceId())
    return PipelineFailure("T29 buffer usage missing");
  for(const TextureDescription &texture : renderer->GetTextures())
    if(texture.width == 8 && texture.height == 8 &&
       HasUsage(renderer, texture.resourceId, ResourceUsage::CS_RWResource))
      destinationTexture = texture.resourceId;
  if(destinationTexture == ResourceId())
    return PipelineFailure("T29 computed texture missing");

  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *begin = NULL, *dispatch = NULL, *draw = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Begin Metal Compute Pass") begin = action;
    if(action->flags & ActionFlags::Dispatch) dispatch = action;
    if(action->flags & ActionFlags::Drawcall) draw = action;
  }
  if(!begin || !dispatch || !draw || begin->eventId >= dispatch->eventId ||
     dispatch->eventId >= draw->eventId || dispatch->dispatchDimension[0] != 2 ||
     dispatch->dispatchDimension[1] != 2 || dispatch->dispatchThreadsDimension[0] != 4 ||
     dispatch->dispatchThreadsDimension[1] != 4)
    return PipelineFailure("T29 action/grid incorrect");

  rdcarray<const ActionDescription *> computeDispatches;
  for(const ActionDescription *action : actions)
    if(action->flags & ActionFlags::Dispatch)
      computeDispatches.push_back(action);
  if(computeDispatches.size() != 1)
    return true;

  const SDFile &structured = renderer->GetStructuredFile();
  rdcarray<uint32_t> bufferBindingEvents;
  for(const APIEvent &event : dispatch->events)
    if(event.chunkIndex < structured.chunks.size() &&
       structured.chunks[event.chunkIndex]->name == "MTLComputeCommandEncoder::setBuffer")
      bufferBindingEvents.push_back(event.eventId);
  if(bufferBindingEvents.size() != 2 ||
     !(begin->eventId < bufferBindingEvents[0] &&
       bufferBindingEvents[0] < bufferBindingEvents[1] &&
       bufferBindingEvents[1] < dispatch->eventId))
    return PipelineFailure("T29 setBuffer calls lack ordered Event Browser EIDs");
  fprintf(stderr, "T29 setBuffer EIDs input=%u output=%u\n",
          bufferBindingEvents[0], bufferBindingEvents[1]);

  renderer->SetFrameEvent(bufferBindingEvents[0], true);
  const MetalPipe::State *firstBinding = renderer->GetPipelineState().GetMetalPipelineState();
  if(!firstBinding || firstBinding->computeBuffers.size() < 3 ||
     firstBinding->computeBuffers[2].resourceId != input ||
     firstBinding->computeBuffers[2].byteOffset != 32)
    return PipelineFailure("T29 first setBuffer EID state incorrect");
  renderer->SetFrameEvent(bufferBindingEvents[1], true);
  const MetalPipe::State *secondBinding = renderer->GetPipelineState().GetMetalPipelineState();
  if(!secondBinding || secondBinding->computeBuffers.size() < 5 ||
     secondBinding->computeBuffers[4].resourceId != output ||
     secondBinding->computeBuffers[4].byteOffset != 64)
    return PipelineFailure("T29 second setBuffer EID state incorrect");

  renderer->SetFrameEvent(begin->eventId, true);
  bytebuf bytes = renderer->GetBufferData(output, 0, 0);
  if(bytes.size() != 336)
    return PipelineFailure("T29 pre-dispatch output size incorrect");
  for(byte b : bytes)
    if(b != 0xa5)
      return PipelineFailure("T29 pre-dispatch sentinel incorrect");

  renderer->SetFrameEvent(dispatch->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(pipe.GetComputePipelineObject() == ResourceId() ||
     pipe.GetShaderEntryPoint(ShaderStage::Compute) != "filter_main" || !state ||
     state->computeBuffers.size() != 5 || state->computeBuffers[2].resourceId != input ||
     state->computeBuffers[2].byteOffset != 32 || state->computeBuffers[2].byteSize != 272 ||
     state->computeBuffers[4].resourceId != output ||
     state->computeBuffers[4].byteOffset != 64 || state->computeBuffers[4].byteSize != 272)
    return PipelineFailure("T29 compute bindings incorrect");
  auto ro = pipe.GetReadOnlyResources(ShaderStage::Compute, true);
  auto rw = pipe.GetReadWriteResources(ShaderStage::Compute, true);
  bool roFound = false, rwFound = false;
  for(const UsedDescriptor &d : ro)
    roFound |= d.access.type == DescriptorType::Buffer && d.descriptor.resource == input &&
               d.descriptor.byteOffset == 32 && d.descriptor.byteSize == 272;
  for(const UsedDescriptor &d : rw)
    rwFound |= d.access.type == DescriptorType::ReadWriteBuffer &&
               d.descriptor.resource == output && d.descriptor.byteOffset == 64 &&
               d.descriptor.byteSize == 272;
  if(!roFound || !rwFound)
    return PipelineFailure("T29 descriptor offset/range incorrect");

  const bytebuf inputBytes = renderer->GetBufferData(input, 0, 0);
  bytes = renderer->GetBufferData(output, 0, 0);
  if(inputBytes.size() != 304 || bytes.size() != 336)
    return PipelineFailure("T29 buffer readback size incorrect");
  for(uint32_t i = 0; i < 64; i++)
  {
    uint32_t actualInput = 0, actualOutput = 0;
    memcpy(&actualInput, inputBytes.data() + 32 + i * 4, 4);
    memcpy(&actualOutput, bytes.data() + 64 + i * 4, 4);
    if(actualInput != 16 + i * 2 || actualOutput != 19 + i * 2)
      return PipelineFailure("T29 computed buffer data incorrect");
  }
  for(uint32_t i = 0; i < bytes.size(); i++)
    if((i < 64 || i >= 320) && bytes[i] != 0xa5)
      return PipelineFailure("T29 untouched output sentinel incorrect");
  const bytebuf texels = renderer->GetTextureData(destinationTexture, {0, 0, 0});
  if(texels.size() != 256)
    return PipelineFailure("T29 computed texture readback size incorrect");
  for(uint32_t y = 0; y < 8; y++)
    for(uint32_t x = 0; x < 8; x++)
    {
      const byte expected[4] = {byte(19 + 2 * (y * 8 + x)), byte(32 + 24 * x),
                                byte(24 + 24 * y), 255};
      if(memcmp(texels.data() + (y * 8 + x) * 4, expected, 4) != 0)
        return PipelineFailure("T29 computed texture pixels incorrect");
    }

  renderer->SetFrameEvent(begin->eventId, true);
  bytes = renderer->GetBufferData(output, 0, 0);
  for(byte b : bytes)
    if(b != 0xa5)
      return PipelineFailure("T29 seek rewind incorrect");
  ResourceId sourceTexture;
  for(const TextureDescription &texture : renderer->GetTextures())
    if(texture.width == 8 && texture.height == 8 && texture.resourceId != destinationTexture &&
       HasUsage(renderer, texture.resourceId, ResourceUsage::CS_Resource))
      sourceTexture = texture.resourceId;
  if(sourceTexture == ResourceId() ||
     !ValidateHeadlessThumbnail(renderer, destinationTexture, begin->eventId, false) ||
     !ValidateHeadlessThumbnail(renderer, sourceTexture, dispatch->eventId, true) ||
     !ValidateHeadlessThumbnail(renderer, destinationTexture, dispatch->eventId, true))
    return PipelineFailure("T29 Input/Output headless thumbnails are blank or stale");
  renderer->SetFrameEvent(draw->eventId, true);
  if(!PixelMatches(renderer, colorTarget, draw->eventId, 25, 20,
                   19.0f / 255.0f, 32.0f / 255.0f,
                   24.0f / 255.0f))
    return PipelineFailure("T29 render output incorrect");
  if(savePath)
  {
    bytes = renderer->GetBufferData(output, 0, 0);
    std::ofstream raw(savePath, std::ios::binary);
    raw.write((const char *)bytes.data(), bytes.size());
    if(!raw)
      return PipelineFailure("T29 raw export failed");
  }
  fprintf(stderr, "T29 EIDs begin=%u dispatch=%u draw=%u\n",
          begin->eventId, dispatch->eventId, draw->eventId);
  for(const ActionDescription *action : actions)
    fprintf(stderr, "T29 action EID %u: %s\n", action->eventId, action->customName.c_str());
  return true;
}

static bool ValidateBlitFixture(IReplayController *renderer, ResourceId colorTarget,
                                const char *savePath)
{
  ResourceId sourceBuffer;
  ResourceId destinationBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length != 64)
      continue;
    if(HasUsage(renderer, buffer.resourceId, ResourceUsage::CopySrc))
      sourceBuffer = buffer.resourceId;
    if(HasUsage(renderer, buffer.resourceId, ResourceUsage::CopyDst) &&
       HasUsage(renderer, buffer.resourceId, ResourceUsage::Clear))
      destinationBuffer = buffer.resourceId;
  }

  TextureDescription sourceTexture;
  TextureDescription copiedTexture;
  TextureDescription mipTexture;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(texture.width != 8 || texture.height != 8 || texture.depth != 1 ||
       texture.type != TextureType::Texture2D || texture.arraysize != 1 ||
       texture.format.compType != CompType::UNorm || texture.format.compByteWidth != 1 ||
       texture.format.compCount != 4)
      continue;

    if(HasUsage(renderer, texture.resourceId, ResourceUsage::CopySrc))
      sourceTexture = texture;
    if(HasUsage(renderer, texture.resourceId, ResourceUsage::CopyDst))
      copiedTexture = texture;
    if(HasUsage(renderer, texture.resourceId, ResourceUsage::GenMips))
      mipTexture = texture;
  }

  // Other fixtures do not contain T10's blit usage set and 8x8 mip chain.
  if(sourceBuffer == ResourceId() && destinationBuffer == ResourceId() &&
     sourceTexture.resourceId == ResourceId() && copiedTexture.resourceId == ResourceId() &&
     mipTexture.resourceId == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal blit fixture validation failed: %s\n", message);
    return false;
  };

  if(sourceBuffer == ResourceId() || destinationBuffer == ResourceId() ||
     sourceTexture.resourceId == ResourceId() || copiedTexture.resourceId == ResourceId() ||
     mipTexture.resourceId == ResourceId() || sourceTexture.mips != 1 || copiedTexture.mips != 1 ||
     mipTexture.mips != 4 || sourceTexture.byteSize != 256 || copiedTexture.byteSize != 256 ||
     mipTexture.byteSize != 340)
    return fail("blit source/destination buffer or texture descriptions do not match");

  const rdcarray<EventUsage> sourceBufferUses = renderer->GetUsage(sourceBuffer);
  const rdcarray<EventUsage> destinationBufferUses = renderer->GetUsage(destinationBuffer);
  if(sourceBufferUses.size() != 1 || sourceBufferUses[0].usage != ResourceUsage::CopySrc ||
     destinationBufferUses.size() != 3 ||
     destinationBufferUses[0].usage != ResourceUsage::CopyDst ||
     destinationBufferUses[1].usage != ResourceUsage::Clear ||
     destinationBufferUses[2].usage != ResourceUsage::PS_Resource ||
     !HasUsage(renderer, sourceTexture.resourceId, ResourceUsage::CopySrc) ||
     !HasUsage(renderer, copiedTexture.resourceId, ResourceUsage::CopyDst) ||
     !HasUsage(renderer, mipTexture.resourceId, ResourceUsage::GenMips))
    return fail("resource usage does not expose CopySrc/CopyDst/Clear/GenMips");

  const ActionDescription *bufferCopy = NULL;
  const ActionDescription *bufferFill = NULL;
  const ActionDescription *textureCopy = NULL;
  const ActionDescription *generateMips = NULL;
  bool beginBlit = false;
  bool endBlit = false;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  for(const ActionDescription *action : actions)
  {
    beginBlit |= action->customName == "Begin Metal Blit Pass" &&
                 (action->flags & ActionFlags::BeginPass);
    endBlit |= action->customName == "End Metal Blit Pass" &&
               (action->flags & ActionFlags::EndPass);
    if((action->flags & ActionFlags::Copy) && action->copySource == sourceBuffer &&
       action->copyDestination == destinationBuffer)
      bufferCopy = action;
    if((action->flags & ActionFlags::Clear) && action->copyDestination == destinationBuffer &&
       action->customName.contains("fillBuffer"))
      bufferFill = action;
    if((action->flags & ActionFlags::Copy) && action->copySource == sourceTexture.resourceId &&
       action->copyDestination == copiedTexture.resourceId)
      textureCopy = action;
    if((action->flags & ActionFlags::GenMips) && action->copySource == mipTexture.resourceId &&
       action->copyDestination == mipTexture.resourceId)
      generateMips = action;
  }

  if(!beginBlit || !endBlit || bufferCopy == NULL || bufferFill == NULL || textureCopy == NULL ||
     generateMips == NULL || bufferCopy->eventId >= bufferFill->eventId ||
     bufferFill->eventId >= textureCopy->eventId || textureCopy->eventId >= generateMips->eventId ||
     textureCopy->copySourceSubresource != Subresource(0, 0) ||
     textureCopy->copyDestinationSubresource != Subresource(0, 0))
    return fail("blit event ordering, flags, or resource links do not match");

  static const byte copiedColour[4] = {255, 128, 32, 255};
  renderer->SetFrameEvent(bufferCopy->eventId, true);
  bytebuf destination = renderer->GetBufferData(destinationBuffer, 0, 0);
  if(destination.size() != 64)
    return fail("destination buffer readback size does not match");
  for(size_t i = 0; i < 16; i += 4)
    if(memcmp(destination.data() + i, copiedColour, 4) != 0)
      return fail("buffer copy event did not write the fixed orange bytes");
  for(size_t i = 16; i < 32; i++)
    if(destination[i] != 0)
      return fail("buffer copy event did not overwrite the future fill range with zero");

  renderer->SetFrameEvent(bufferFill->eventId, true);
  destination = renderer->GetBufferData(destinationBuffer, 0, 0);
  for(size_t i = 16; i < 32; i++)
    if(destination[i] != 0x60)
      return fail("buffer fill event did not write 0x60");

  renderer->SetFrameEvent(bufferCopy->eventId, true);
  destination = renderer->GetBufferData(destinationBuffer, 0, 0);
  for(size_t i = 16; i < 32; i++)
    if(destination[i] != 0)
      return fail("rewinding to buffer copy did not restore its event result");

  static const byte colours[4][4] = {
      {255, 32, 16, 255}, {16, 224, 48, 255}, {24, 64, 255, 255}, {240, 208, 32, 255},
  };
  renderer->SetFrameEvent(textureCopy->eventId, true);
  const bytebuf copied = renderer->GetTextureData(copiedTexture.resourceId, {0, 0, 0});
  if(copied.size() != 256)
    return fail("copied texture readback size does not match");
  for(uint32_t y = 0; y < 8; y++)
    for(uint32_t x = 0; x < 8; x++)
    {
      const uint32_t quadrant = (x >= 4 ? 1U : 0U) + (y >= 4 ? 2U : 0U);
      if(memcmp(copied.data() + (y * 8 + x) * 4, colours[quadrant], 4) != 0)
        return fail("texture copy event did not preserve the quadrant pattern");
    }

  renderer->SetFrameEvent(generateMips->eventId, true);
  const bytebuf generatedMip1 = renderer->GetTextureData(mipTexture.resourceId, {1, 0, 0});
  const bytebuf generatedMip3 = renderer->GetTextureData(mipTexture.resourceId, {3, 0, 0});
  if(generatedMip1.size() != 64 || generatedMip3.size() != 4 ||
     memcmp(generatedMip1.data(), colours[0], 4) != 0 ||
     abs(int(generatedMip3[0]) - 134) > 1 || abs(int(generatedMip3[1]) - 132) > 1 ||
     abs(int(generatedMip3[2]) - 88) > 1 || generatedMip3[3] != 255)
    return fail("generated mip readback does not match the deterministic averages");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1)
    return fail("expected one final sampling draw");
  renderer->SetFrameEvent(draws[0]->eventId, true);
  const MetalPipe::State *state = renderer->GetPipelineState().GetMetalPipelineState();
  if(state == NULL || state->fragmentBuffers.size() != 1 ||
     state->fragmentBuffers[0].resourceId != destinationBuffer ||
     state->fragmentTextures.size() != 2 ||
     state->fragmentTextures[0] != copiedTexture.resourceId ||
     state->fragmentTextures[1] != mipTexture.resourceId)
    return fail("final draw does not bind all independent blit results");

  if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, 50, 150, 1.0f, 128.0f / 255.0f,
                   32.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 150, 150, 96.0f / 255.0f,
                   96.0f / 255.0f, 96.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 250, 150, 1.0f, 32.0f / 255.0f,
                   16.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 350, 150, 134.0f / 255.0f,
                   132.0f / 255.0f, 88.0f / 255.0f))
    return fail("final draw bands do not sample the four blit results");

  if(savePath != NULL)
  {
    TextureSave save;
    save.resourceId = mipTexture.resourceId;
    save.destType = FileType::DDS;
    save.mip = -1;
    ResultDetails result = renderer->SaveTexture(save, savePath);
    if(!result.OK())
      return fail("mip texture DDS save failed");
    std::ifstream file(savePath, std::ios::binary | std::ios::ate);
    if(!file || file.tellg() != std::streampos(128 + 340))
      return fail("mip texture DDS save did not contain all four levels");
  }

  return true;
}

static bool ValidateTextureViewsFixture(IReplayController *renderer, ResourceId colorTarget)
{
  unsigned viewChunks[3] = {};
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    if(chunk->metadata.chunkID >= 1076 && chunk->metadata.chunkID <= 1078)
      viewChunks[chunk->metadata.chunkID - 1076]++;
  if(!viewChunks[0] && !viewChunks[1] && !viewChunks[2]) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T58 texture views failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(viewChunks[0] != 1 || viewChunks[1] != 1 || viewChunks[2] != 1 || draws.size() != 9)
    return fail("three view creations or nine draw states missing");
  ResourceId source, array, subset, swizzled;
  std::map<ResourceId, TextureDescription> textures;
  for(const TextureDescription &texture : renderer->GetTextures())
    textures[texture.resourceId] = texture;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.type == ResourceType::Texture && resource.parentResources.size() == 1)
    {
      const TextureDescription desc = textures[resource.resourceId];
      if(desc.width == 2 && desc.height == 2 && desc.mips == 1)
      {
        if(subset == ResourceId())
          subset = resource.resourceId;
        else if(swizzled == ResourceId())
          swizzled = resource.resourceId;
        array = resource.parentResources[0];
      }
    }
  if(array == ResourceId() || subset == ResourceId() || swizzled == ResourceId() ||
     textures[array].arraysize != 2)
    return fail("subset/swizzled parent or dimensions missing");
  renderer->SetFrameEvent(draws[0]->eventId, true);
  const auto *firstState = renderer->GetPipelineState().GetMetalPipelineState();
  if(!firstState || firstState->fragmentTextures.empty())
    return fail("direct source texture binding missing");
  source = firstState->fragmentTextures[0];
  for(unsigned draw : {8U, 0U, 4U, 1U, 7U, 2U, 5U, 3U, 6U, 8U})
  {
    renderer->SetFrameEvent(draws[draw]->eventId, true);
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    const auto *shader = pipe.GetShaderReflection(ShaderStage::Fragment);
    if(!state || !shader || shader->entryPoint != "fs_view" ||
       state->fragmentTextures.size() < 1 ||
       pipe.GetReadOnlyResources(ShaderStage::Fragment, true).size() != 1)
      return fail("fragment reflection, texture binding or generic descriptor missing");
    const unsigned phase = draw / 3, band = draw % 3;
    const ResourceId bound = state->fragmentTextures[0];
    if(bound != (band == 0 ? source : band == 1 ? subset : swizzled))
      return fail("mip/slice view identities changed across draw seeks");
    if(band == 0 && (source == array || textures[source].mips != 3))
      return fail("direct source texture identity");
    const auto descriptors = pipe.GetReadOnlyResources(ShaderStage::Fragment, true);
    if(descriptors[0].descriptor.resource != bound ||
       !HasUsage(renderer, bound, ResourceUsage::PS_Resource))
      return fail("view resource descriptor or fragment usage");
    const byte red = byte(band == 0 ? (phase ? 34 + phase * 10 : 12) : 21 + phase * 10);
    const byte green = byte(band == 0 ? (phase ? 56 + phase * 10 : 34) : 43 + phase * 10);
    const byte blue = byte(band == 0 ? (phase ? 78 + phase * 10 : 56) : 65 + phase * 10);
    if(!PixelMatches(renderer, colorTarget, draws[draw]->eventId,
                     (2 * band + 1) * 400 / 6, 150,
                     (band == 2 ? blue : red) / 255.0f, green / 255.0f,
                     (band == 2 ? red : blue) / 255.0f))
    {
      const PixelValue actual = renderer->PickPixel(colorTarget, (2 * band + 1) * 400 / 6,
                                                   150, {0, 0, 0}, CompType::Typeless);
      fprintf(stderr, "T58 draw %u phase %u band %u actual %.3f/%.3f/%.3f expected %u/%u/%u\n",
              draw, phase, band, actual.floatValue[0], actual.floatValue[1], actual.floatValue[2],
              band == 2 ? blue : red, green, band == 2 ? red : blue);
      return fail("view/source/shared GPU write or swizzled draw pixel");
    }
    const bytebuf sourceData = renderer->GetTextureData(source, {0, 0, 0});
    const bytebuf arrayData = renderer->GetTextureData(array, {1, 1, 0});
    if(sourceData.size() != 64 || arrayData.size() != 16 ||
       sourceData[0] != (phase ? 34 + phase * 10 : 12) ||
       sourceData[1] != (phase ? 56 + phase * 10 : 34) ||
       sourceData[2] != (phase ? 78 + phase * 10 : 56) ||
       arrayData[0] != 21 + phase * 10 || arrayData[1] != 43 + phase * 10 ||
       arrayData[2] != 65 + phase * 10)
      return fail("parent texture bytes did not restore on backward/forward seeks");
  }
  return true;
}

static bool ValidateBufferTextureFixture(IReplayController *renderer, ResourceId colorTarget)
{
  unsigned creations = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    creations += chunk->metadata.chunkID == 1193;
  if(!creations) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T59 buffer texture failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(creations != 1 || draws.size() != 3)
    return fail("creation or three draw phases missing");
  ResourceId texture, buffer;
  uint64_t length = 0;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.type == ResourceType::Texture && resource.parentResources.size() == 1)
    {
      for(const TextureDescription &desc : renderer->GetTextures())
        if(desc.resourceId == resource.resourceId && desc.width == 3 && desc.height == 2 &&
           desc.mips == 1 && desc.arraysize == 1)
        {
          texture = resource.resourceId;
          buffer = resource.parentResources[0];
        }
    }
  for(const BufferDescription &desc : renderer->GetBuffers())
    if(desc.resourceId == buffer) length = desc.length;
  if(texture == ResourceId() || buffer == ResourceId() || length < 4*16+89 ||
     (length - 89) % 4)
    return fail("buffer/texture derived resource relationship or length");
  const uint64_t rowPitch = (length - 89) / 4, offset = rowPitch * 2;
  for(unsigned phase : {2U, 0U, 1U, 0U, 2U})
  {
    renderer->SetFrameEvent(draws[phase]->eventId, true);
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    const auto *shader = pipe.GetShaderReflection(ShaderStage::Fragment);
    const auto descriptors = pipe.GetReadOnlyResources(ShaderStage::Fragment, true);
    if(!state || !shader || shader->entryPoint != "fs_buffer_texture" ||
       state->fragmentTextures.size() != 1 || state->fragmentTextures[0] != texture ||
       descriptors.size() != 1 || descriptors[0].descriptor.resource != texture ||
       !HasUsage(renderer, texture, ResourceUsage::PS_Resource))
      return fail("shader, texture descriptor or usage");
    const byte red = phase == 0 ? 10 : phase == 1 ? 40 : 70;
    const byte green = phase == 0 ? 20 : phase == 1 ? 50 : 80;
    const byte blue = phase == 0 ? 30 : phase == 1 ? 60 : 90;
    if(!PixelMatches(renderer, colorTarget, draws[phase]->eventId, 200, 150,
                     red / 255.0f, green / 255.0f, blue / 255.0f))
    {
      const PixelValue actual = renderer->PickPixel(colorTarget, 200, 150, {0, 0, 0},
                                                   CompType::Typeless);
      fprintf(stderr, "T59 phase %u pixel %.3f/%.3f/%.3f expected %u/%u/%u\n", phase,
              actual.floatValue[0], actual.floatValue[1], actual.floatValue[2],
              red, green, blue);
      return fail("texture draw did not follow CPU/GPU buffer alias writes");
    }
    const bytebuf bytes = renderer->GetBufferData(buffer, 0, 0);
    const bytebuf texels = renderer->GetTextureData(texture, {0, 0, 0});
    if(bytes.size() != length || texels.size() != 24)
      return fail("buffer/texture readback sizes");
    for(uint64_t i = 0; i < length; i++)
    {
      byte expected = 0xa5;
      for(uint64_t y = 0; y < 2; y++)
        for(uint64_t x = 0; x < 3; x++)
        {
          const uint64_t start = offset + y * rowPitch + x * 4;
          if(i >= start && i < start + 4)
          {
            const bool changed = phase == 2 || (phase == 1 && x == 0 && y == 0);
            const byte values[] = {byte(changed ? red : 10), byte(changed ? green : 20),
                                   byte(changed ? blue : 30), 255};
            expected = values[i - start];
          }
        }
      if(bytes[i] != expected)
      {
        fprintf(stderr, "T59 phase %u buffer byte %llu actual %u expected %u\n", phase,
                (unsigned long long)i, bytes[i], expected);
        return fail("buffer alias contents or row/prefix/suffix padding across seeks");
      }
    }
    if(texels[0] != red || texels[1] != green || texels[2] != blue || texels[3] != 255)
      return fail("texture readback did not follow buffer contents");
  }
  return true;
}

static bool ValidateDeviceArgumentEncoderFixture(IReplayController *renderer, ResourceId colorTarget)
{
  unsigned creations = 0, textureWrites = 0, samplerWrites = 0;
  bool bindingEncoder = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    creations += chunk->metadata.chunkID == 1028 ||
                 chunk->name == "MTLDevice::newArgumentEncoderWithBufferBinding";
    if(chunk->name == "MTLDevice::newArgumentEncoderWithBufferBinding")
    {
      bindingEncoder = true;
      const SDObject *descriptors = chunk->FindChild("descriptors");
      const SDObject *length = chunk->FindChild("encodedLength");
      const SDObject *alignment = chunk->FindChild("alignment");
      const SDObject *supported = chunk->FindChild("supported");
      if(!descriptors || descriptors->NumChildren() != 12 ||
         !length || length->AsUInt64() != 16 ||
         !alignment || alignment->AsUInt64() != 8 ||
         !supported || !supported->AsBool())
        return false;
    }
    textureWrites += chunk->metadata.chunkID == 1234;
    samplerWrites += chunk->metadata.chunkID == 1235;
  }
  if(!creations) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T60/T102 device argument encoder failed: %s\n",message); return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(),draws);
  if(creations != 1 || textureWrites != 2 || samplerWrites != 2 || draws.size() != 2)
    return fail("creation or two draws missing");
  ResourceId outer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 325) outer = buffer.resourceId;
  if(outer == ResourceId()) return fail("argument storage buffer missing");
  const bytebuf payload = renderer->GetBufferData(outer,0,0);
  if(payload.size() != 325) return fail("argument storage size");
  for(size_t i = 0; i < payload.size(); i++)
    if((i < 256 || i >= 288) && payload[i] != 0xa5)
      return fail("argument storage prefix/suffix padding changed");
  ResourceId textures[2], sampler;
  for(unsigned draw : {1U,0U,1U})
  {
    renderer->SetFrameEvent(draws[draw]->eventId,true);
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    const auto *reflection = pipe.GetShaderReflection(ShaderStage::Fragment);
    if(!state || !reflection || reflection->entryPoint != "fs_device_arg" ||
       state->fragmentArgumentBuffers.size() != 1 ||
       state->fragmentBuffers[0].resourceId != outer)
      return fail("shader or argument buffer selection");
    const auto &argument = state->fragmentArgumentBuffers[0];
    if(argument.buffer.resourceId != outer || argument.textures.size() != 1 ||
       argument.samplers.size() != 2)
      return fail("encoded member state");
    if(textures[draw] == ResourceId()) textures[draw] = argument.textures[0];
    if(sampler == ResourceId()) sampler = argument.samplers[1];
    if(textures[draw] == ResourceId() || argument.textures[0] != textures[draw] ||
       argument.samplers[1] != sampler ||
       !HasUsage(renderer,outer,ResourceUsage::PS_Constants) ||
       !HasUsage(renderer,textures[draw],ResourceUsage::PS_Resource))
      return fail("member identity or usage across seek");
    const auto resources = pipe.GetReadOnlyResources(ShaderStage::Fragment,true);
    const auto filters = pipe.GetSamplers(ShaderStage::Fragment,true);
    if(resources.size() != 1 || filters.size() != 1 ||
       resources[0].descriptor.resource != textures[draw] ||
       filters[0].sampler.object != sampler)
      return fail("generic texture/sampler descriptors");
    const byte red = 20+50*draw, green = 40+40*draw, blue = 60+30*draw;
    if(!PixelMatches(renderer,colorTarget,draws[draw]->eventId,draw ? 300 : 100,150,
                     red/255.0f,green/255.0f,blue/255.0f))
      return fail("argument-encoded texture pixel or rewind");
  }
  if(bindingEncoder)
    fprintf(stderr,"T102 reflection binding argument encoder layout and replay validated\n");
  return true;
}

static bool ValidateNoCopyBufferFixture(IReplayController *renderer, ResourceId colorTarget)
{
  unsigned creations = 0, updates = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    creations += chunk->metadata.chunkID == 1006;
    updates += chunk->metadata.chunkID == 1198;
  }
  if(!creations) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T61 no-copy buffer failed: %s\n",message); return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(),draws);
  if(creations != 1 || updates != 2 || draws.size() != 3)
    return fail("creation, CPU writes or three draws missing");
  ResourceId buffer;
  for(const BufferDescription &desc : renderer->GetBuffers())
    if(desc.length == 4096) buffer = desc.resourceId;
  if(buffer == ResourceId()) return fail("page-aligned buffer resource missing");
  for(unsigned phase : {2U,0U,1U,0U,2U})
  {
    renderer->SetFrameEvent(draws[phase]->eventId,true);
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    const auto *shader = pipe.GetShaderReflection(ShaderStage::Fragment);
    if(!state || !shader || shader->entryPoint != "fs_no_copy" ||
       state->fragmentBuffers[0].resourceId != buffer ||
       state->fragmentBuffers[0].byteOffset != 256 ||
       !HasUsage(renderer,buffer,ResourceUsage::PS_Resource))
    {
      fprintf(stderr,"T61 phase %u shader %s bufferMatch %d offset %llu usage %d\n",
              phase,shader ? shader->entryPoint.c_str() : "none",
              state && state->fragmentBuffers[0].resourceId == buffer,
              state ? (unsigned long long)state->fragmentBuffers[0].byteOffset : 0ULL,
              HasUsage(renderer,buffer,ResourceUsage::PS_Resource));
      return fail("fragment buffer, offset, shader or usage");
    }
    const byte red = 10+30*phase, green = 20+30*phase, blue = 30+30*phase;
    if(!PixelMatches(renderer,colorTarget,draws[phase]->eventId,200,150,
                     red/255.0f,green/255.0f,blue/255.0f))
      return fail("no-copy CPU write draw or backward seek");
    const bytebuf bytes = renderer->GetBufferData(buffer,0,0);
    if(bytes.size() != 4096) return fail("buffer data size");
    const uint32_t expected[] = {red,green,blue,0};
    if(memcmp(bytes.data()+256,expected,16)) return fail("no-copy CPU payload at selected event");
    for(size_t i = 0; i < bytes.size(); i++)
      if((i < 256 || i >= 272) && bytes[i] != 0xa5)
        return fail("page padding changed across seek");
  }
  return true;
}

static bool ValidatePurgeableStateFixture(IReplayController *renderer, ResourceId colorTarget)
{
  unsigned buffers = 0, textures = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    buffers += chunk->metadata.chunkID == 1189;
    textures += chunk->metadata.chunkID == 1070;
  }
  if(!buffers && !textures) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T62 purgeable state failed: %s\n",message); return false;
  };
  if(buffers < 2 || textures < 2 || buffers != textures)
    return fail("buffer and texture state chunks");
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(),draws);
  if(draws.size() != 1) return fail("draw count");
  renderer->SetFrameEvent(draws[0]->eventId,true);
  const auto &pipe = renderer->GetPipelineState();
  const auto *state = pipe.GetMetalPipelineState();
  const auto *shader = pipe.GetShaderReflection(ShaderStage::Fragment);
  if(!state || !shader || shader->entryPoint != "fs_purgeable" ||
     state->fragmentBuffers[0].resourceId == ResourceId() ||
     state->fragmentTextures.empty() || state->fragmentTextures[0] == ResourceId())
    return fail("fragment bindings or shader");
  if(!PixelMatches(renderer,colorTarget,draws[0]->eventId,200,150,
                   15/255.0f,26/255.0f,37/255.0f))
    return fail("buffer and texture pixel");
  return true;
}

static bool ValidateDynamicVertexStrideFixture(IReplayController *renderer, ResourceId colorTarget)
{
  unsigned singles = 0, batches = 0, offsets = 0, bytes = 0, amplification = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    singles += chunk->metadata.chunkID == 1278;
    batches += chunk->metadata.chunkID == 1279;
    offsets += chunk->metadata.chunkID == 1280;
    bytes += chunk->metadata.chunkID == 1281;
    amplification += chunk->metadata.chunkID == 1109;
  }
  if(!singles && !batches && !offsets && !bytes) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T63 dynamic vertex stride failed: %s\n",message); return false;
  };
  if(singles != 2 || batches != 1 || offsets != 1 || bytes != 1 || amplification != 1)
    return fail("four overload and single-view amplification chunk counts");
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(),draws);
  if(draws.size() != 4) return fail("four draw phases");
  renderer->SetFrameEvent(draws[0]->eventId,true);
  const auto *firstState = renderer->GetPipelineState().GetMetalPipelineState();
  if(!firstState || firstState->vertexBuffers.size() < 2)
    return fail("first draw vertex bindings missing");
  const ResourceId positionBuffer = firstState->vertexBuffers[0].resourceId;
  const ResourceId colorBuffer = firstState->vertexBuffers[1].resourceId;
  bool positionLength = false, colorLength = false;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.resourceId == positionBuffer) positionLength = buffer.length == 192;
    if(buffer.resourceId == colorBuffer) colorLength = buffer.length == 192;
  }
  if(positionBuffer == ResourceId() || colorBuffer == ResourceId() ||
     positionBuffer == colorBuffer || !positionLength || !colorLength)
    return fail("position or color buffer missing");
  const uint64_t positionOffsets[] = {0,48,144,0};
  const uint64_t colorOffsets[] = {0,48,96,144};
  const byte rgb[4][3] = {{10,20,30},{40,50,60},{70,80,90},{100,110,120}};
  for(unsigned phase : {3U,0U,2U,1U,3U})
  {
    renderer->SetFrameEvent(draws[phase]->eventId,true);
    const auto *state = renderer->GetPipelineState().GetMetalPipelineState();
    const auto *shader = renderer->GetPipelineState().GetShaderReflection(ShaderStage::Vertex);
    if(!state || !shader || shader->entryPoint != "vs_dynamic_stride" ||
       state->vertexBuffers.size() < 2 || state->vertexBuffers[0].byteStride != 16 ||
       state->vertexBuffers[1].byteStride != 16 ||
       state->vertexBuffers[0].resourceId != (phase == 3 ? ResourceId() : positionBuffer) ||
       state->vertexBuffers[0].byteOffset != positionOffsets[phase] ||
       state->vertexBuffers[1].resourceId != colorBuffer ||
       state->vertexBuffers[1].byteOffset != colorOffsets[phase])
      return fail("vertex source, effective stride or offset across seek");
    if(!PixelMatches(renderer,colorTarget,draws[phase]->eventId,200,150,
                     rgb[phase][0]/255.0f,rgb[phase][1]/255.0f,rgb[phase][2]/255.0f))
      return fail("dynamic vertex draw pixel across seek");
  }
  return true;
}

static bool ValidateSharedTextureFixture(IReplayController *renderer, ResourceId colorTarget)
{
  unsigned creations = 0, aliases = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newSharedTextureWithHandle")
      return true; // Same-process handle alias has its own T68 validator.
    creations += chunk->metadata.chunkID == 1011 || chunk->name == "MTLHeap::newTexture";
    aliases += chunk->name == "MTLTexture::makeAliasable";
  }
  if(!creations) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T64/T72 private texture failed: %s\n",message); return false;
  };
  if(creations != 1 + aliases || aliases > 1)
    return fail("descriptor creation chunk count");
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(),draws);
  if(draws.size() != 3) return fail("three sampled draw phases");
  renderer->SetFrameEvent(draws[0]->eventId,true);
  const auto *first = renderer->GetPipelineState().GetMetalPipelineState();
  ResourceId shared = first && !first->fragmentTextures.empty() ?
      first->fragmentTextures[0] : ResourceId();
  bool oneByOne = false;
  for(const TextureDescription &desc : renderer->GetTextures())
    if(desc.resourceId == shared) oneByOne = desc.width == 1 && desc.height == 1;
  if(shared == ResourceId() || !oneByOne)
    return fail("1x1 shared texture resource missing");
  const byte rgb[3][3] = {{10,20,30},{40,50,60},{70,80,90}};
  for(unsigned phase : {2U,0U,1U,0U,2U})
  {
    renderer->SetFrameEvent(draws[phase]->eventId,true);
    const auto *state = renderer->GetPipelineState().GetMetalPipelineState();
    const auto *shader = renderer->GetPipelineState().GetShaderReflection(ShaderStage::Fragment);
    if(!state || !shader || shader->entryPoint != "fs_shared" ||
       state->fragmentTextures.empty() || state->fragmentTextures[0] != shared ||
       !HasUsage(renderer,shared,ResourceUsage::PS_Resource))
      return fail("shared texture binding, shader or usage across seek");
    if(!PixelMatches(renderer,colorTarget,draws[phase]->eventId,200,150,
                     rgb[phase][0]/255.0f,rgb[phase][1]/255.0f,rgb[phase][2]/255.0f))
      return fail("GPU clear and sample pixel across seek");
  }
  return true;
}

static bool ValidateHeapAliasFixture(IReplayController *renderer)
{
  const SDFile &file = renderer->GetStructuredFile();
  ResourceId aliased, heap;
  bool texture = false;
  size_t aliasIndex = 0;
  unsigned aliases = 0;
  for(size_t i = 0; i < file.chunks.size(); i++)
  {
    const SDChunk *chunk = file.chunks[i];
    if(chunk->name == "MTLBuffer::makeAliasable" ||
       chunk->name == "MTLTexture::makeAliasable")
    {
      aliases++;
      texture = chunk->name == "MTLTexture::makeAliasable";
      const SDObject *id = chunk->FindChild(texture ? "Texture" : "Buffer");
      if(!id) return false;
      aliased = id->AsResourceId();
      aliasIndex = i;
    }
  }
  if(!aliases) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T131/132 alias fixture failed: %s\n",message); return false;
  };
  if(aliases != 1 || aliased == ResourceId()) return fail("alias call identity");
  unsigned creationCount = 0;
  uint64_t aliasOffset = UINT64_MAX;
  for(size_t i = 0; i < aliasIndex; i++)
  {
    const SDChunk *chunk = file.chunks[i];
    if(chunk->name == (texture ? "MTLHeap::newTexture" : "MTLHeap::newBuffer") ||
       chunk->name == (texture ? "MTLHeap::newTexture(offset)" : "MTLHeap::newBuffer(offset)"))
    {
      const SDObject *child = chunk->FindChild(texture ? "Texture" : "Buffer");
      const SDObject *parent = chunk->FindChild("Heap");
      if(child && parent && child->AsResourceId() == aliased)
      {
        creationCount++;
        heap = parent->AsResourceId();
        const SDObject *offset = chunk->FindChild("offset");
        if(offset) aliasOffset = offset->AsUInt64();
      }
    }
  }
  if(creationCount != 1 || heap == ResourceId()) return fail("heap child creation");
  bool childFound = false, heapFound = false;
  for(const ResourceDescription &resource : renderer->GetResources())
  {
    if(resource.resourceId == heap)
      heapFound = resource.type == ResourceType::Pool;
    if(resource.resourceId == aliased)
    {
      childFound = resource.type == (texture ? ResourceType::Texture : ResourceType::Buffer);
      bool parentFound = false;
      for(ResourceId parent : resource.parentResources)
        parentFound |= parent == heap;
      childFound &= parentFound;
    }
  }
  if(!childFound || !heapFound) return fail("heap-to-alias resource relationship");
  if(aliasOffset != UINT64_MAX)
  {
    unsigned reuseCount = 0;
    for(size_t i = aliasIndex + 1; i < file.chunks.size(); i++)
    {
      const SDChunk *chunk = file.chunks[i];
      if(chunk->name == (texture ? "MTLHeap::newTexture(offset)" :
                                 "MTLHeap::newBuffer(offset)"))
      {
        const SDObject *parent = chunk->FindChild("Heap");
        const SDObject *offset = chunk->FindChild("offset");
        const SDObject *child = chunk->FindChild(texture ? "Texture" : "Buffer");
        if(parent && offset && child && parent->AsResourceId() == heap &&
           offset->AsUInt64() == aliasOffset && child->AsResourceId() != aliased)
          reuseCount++;
      }
    }
    if(reuseCount != 1) return fail("placement alias reuse must follow the alias call");
  }
  else
    for(size_t i = aliasIndex + 1; i < file.chunks.size(); i++)
      if(file.chunks[i]->name != "Internal::End of Capture")
        return fail("alias must be after GPU work in this fixture");
  return true;
}

static bool ValidateCompactedAccelerationStructureFixture(IReplayController *renderer)
{
  ResourceId source, destination, sizeBuffer, output, firstCommandBuffer, secondCommandBuffer;
  unsigned created = 0, begun = 0, built = 0, compacted = 0, written = 0, ended = 0;
  uint64_t expected = 0;
  rdcstr buildActionName;
  bool waitedForFirst = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newAccelerationStructureWithSize" ||
       chunk->name == "MTLDevice::newAccelerationStructureWithDescriptor")
    {
      const SDObject *id = chunk->FindChild("Structure");
      const SDObject *size = chunk->FindChild("size");
      if(!id || !size) return false;
      if(++created == 1) { source = id->AsResourceId(); if(size->AsUInt64() != 1536) return false; }
      else if(created == 2) { destination = id->AsResourceId(); if(size->AsUInt64() != 1280) return false; }
      else return false;
    }
    if(chunk->name == "MTLCommandBuffer::accelerationStructureCommandEncoder")
    {
      const SDObject *cb = chunk->FindChild("CommandBuffer");
      if(!cb) return false;
      if(++begun == 1) firstCommandBuffer = cb->AsResourceId();
      else if(begun == 2) secondCommandBuffer = cb->AsResourceId();
      else return false;
    }
    if(chunk->name == "MTLCommandBuffer::waitUntilCompleted" && written == 1)
    {
      const SDObject *cb = chunk->FindChild("CommandBuffer");
      waitedForFirst |= cb && cb->AsResourceId() == firstCommandBuffer;
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildAccelerationStructure" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBox" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxExtended" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxStrided")
    {
      built++;
      const bool boxed = chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBox" ||
                         chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxExtended" ||
                         chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxStrided";
      const bool indexed = chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangle";
      const SDObject *as = chunk->FindChild("structure");
      const SDObject *geometry = chunk->FindChild(boxed ? "boxes" : "vertices");
      const SDObject *scratch = chunk->FindChild("scratch");
      const SDObject *count = chunk->FindChild(boxed ? "boxCount" : "triangleCount");
      const SDObject *indices = indexed ? chunk->FindChild("indices") : NULL;
      if(!as || !geometry || !scratch || !count || as->AsResourceId() != source ||
         geometry->AsResourceId() == ResourceId() || scratch->AsResourceId() == ResourceId() ||
         (count->AsUInt64() != 1 &&
          !(chunk->name == "MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle" &&
            count->AsUInt64() == 2)) ||
         (indexed && (!indices || indices->AsResourceId() == ResourceId())))
        return false;
      buildActionName = boxed ? "Build Metal Bounding Box Acceleration Structure" :
          indexed ? "Build Metal Indexed Triangle Acceleration Structure" :
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle" ?
              "Build Metal Non-Opaque Triangle Acceleration Structure" :
              "Build Metal Triangle Acceleration Structure";
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::copyAndCompactAccelerationStructure")
    {
      compacted++;
      const SDObject *src = chunk->FindChild("source");
      const SDObject *dst = chunk->FindChild("destination");
      const SDObject *buf = chunk->FindChild("sizeBuffer");
      const SDObject *offset = chunk->FindChild("sizeOffset");
      const SDObject *type = chunk->FindChild("sizeDataType");
      const SDObject *value = chunk->FindChild("expectedSize");
      if(!src || !dst || !buf || !offset || !type || !value ||
         src->AsResourceId() != source || dst->AsResourceId() != destination ||
         buf->AsResourceId() != sizeBuffer || !waitedForFirst ||
         firstCommandBuffer == secondCommandBuffer ||
         offset->AsUInt64() != 0 || type->AsUInt64() != 85) return false;
      sizeBuffer = buf->AsResourceId();
      expected = value->AsUInt64();
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::writeCompactedAccelerationStructureSize")
    {
      const SDObject *as = chunk->FindChild("structure");
      const SDObject *buf = chunk->FindChild("buffer");
      if(!as || !buf) return false;
      if(++written == 1)
      {
        if(as->AsResourceId() != source) return false;
        sizeBuffer = buf->AsResourceId();
      }
      else if(written == 2)
      {
        if(as->AsResourceId() != destination) return false;
        output = buf->AsResourceId();
      }
      else return false;
    }
    ended += chunk->name == "MTLAccelerationStructureCommandEncoder::endEncoding";
  }
  if(created != 2 || begun != 2 || built != 1 || compacted != 1 ||
     written != 2 || ended != 2 || !waitedForFirst ||
     firstCommandBuffer == ResourceId() || secondCommandBuffer == ResourceId() ||
     source == ResourceId() ||
     destination == ResourceId() || sizeBuffer == ResourceId() ||
     output == ResourceId() || expected != 1280) return false;
  unsigned asResources = 0;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.resourceId == source || resource.resourceId == destination)
      asResources += resource.type == ResourceType::AccelerationStructure;
  if(asResources != 2) return false;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *build = NULL, *compact = NULL, *firstWrite = NULL, *lastWrite = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == buildActionName) build = action;
    if(action->customName == "Copy and Compact Metal Acceleration Structure") compact = action;
    if(action->customName == "Write Metal Compacted Acceleration Structure Size")
    { if(!firstWrite) firstWrite = action; else lastWrite = action; }
  }
  if(!build || !compact || !firstWrite || !lastWrite ||
     !(build->eventId < firstWrite->eventId && firstWrite->eventId < compact->eventId &&
       compact->eventId < lastWrite->eventId)) return false;
  for(uint32_t event : {lastWrite->eventId, compact->eventId, lastWrite->eventId})
  {
    renderer->SetFrameEvent(event, true);
    const bytebuf sourceSize = renderer->GetBufferData(sizeBuffer, 0, 8);
    const bytebuf result = renderer->GetBufferData(output, 0, 8);
    uint64_t sourceValue = 0, resultValue = 0;
    if(sourceSize.size() != 8 || result.size() != 8) return false;
    memcpy(&sourceValue, sourceSize.data(), 8);
    memcpy(&resultValue, result.data(), 8);
    if(sourceValue != 1280 || resultValue != (event == lastWrite->eventId ? 1280ULL : 0ULL))
      return false;
  }
  return true;
}

static bool ValidateRayRefitFixture(IReplayController *renderer)
{
  unsigned builds = 0, refits = 0, bindings = 0, dispatches = 0, blits = 0;
  unsigned compactWrites = 0, compactions = 0;
  unsigned descriptorAllocations = 0;
  unsigned extendedRefits = 0;
  bool noDuplicateBuild = false, noDuplicateRefit = false;
  bool formattedRefit = false;
  uint64_t formattedFormat = 0;
  unsigned triangleCount = 0;
  ResourceId structure, output, vertices, refitVertices, compacted, compactBuffer, descriptorStructure,
             refitDestination;
  size_t buildAt = 0, firstRayAt = 0, blitAt = 0, refitAt = 0, lastRayAt = 0;
  size_t compactWriteAt = 0, compactAt = 0;
  bool alternateVertices = false;
  const auto &chunks = renderer->GetStructuredFile().chunks;
  for(size_t position = 0; position < chunks.size(); position++)
  {
    const SDChunk *chunk = chunks[position];
    if(chunk->name == "MTLDevice::newAccelerationStructureWithDescriptor")
    {
      const SDObject *id = chunk->FindChild("Structure");
      const SDObject *size = chunk->FindChild("size");
      if(!id || !size || id->AsResourceId() == ResourceId() ||
         size->AsUInt64() != 2048) return false;
      descriptorAllocations++;
      descriptorStructure = id->AsResourceId();
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildRefittableTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildRefittableTriangleNoDuplicate" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildRefittableFormattedTriangle")
    {
      builds++;
      formattedRefit = chunk->name ==
          "MTLAccelerationStructureCommandEncoder::buildRefittableFormattedTriangle";
      if(formattedRefit)
      {
        const SDObject *stride = chunk->FindChild("vertexStride");
        const SDObject *format = chunk->FindChild("vertexFormat");
        const SDObject *duplicate = chunk->FindChild("allowDuplicate");
        if(!stride || stride->AsUInt64() != 16 || !format ||
           (format->AsUInt64() != MTL::AttributeFormatFloat3 &&
            format->AsUInt64() != MTL::AttributeFormatFloat4) || !duplicate)
          return false;
        formattedFormat = format->AsUInt64();
        noDuplicateBuild = !duplicate->AsBool();
      }
      else
        noDuplicateBuild = chunk->name ==
            "MTLAccelerationStructureCommandEncoder::buildRefittableTriangleNoDuplicate";
      const SDObject *id = chunk->FindChild("structure");
      const SDObject *geometry = chunk->FindChild("vertices");
      const SDObject *count = chunk->FindChild("triangleCount");
      if(!id || !geometry || !count ||
         (count->AsUInt64() != 1 && count->AsUInt64() != 2)) return false;
      structure = id->AsResourceId();
      vertices = geometry->AsResourceId();
      triangleCount = (unsigned)count->AsUInt64();
      buildAt = position;
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::refitTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::refitTriangleExtended" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::refitTriangleNoDuplicate" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::refitFormattedTriangle")
    {
      const bool formattedChunk = chunk->name ==
          "MTLAccelerationStructureCommandEncoder::refitFormattedTriangle";
      if(formattedChunk)
      {
        const SDObject *stride = chunk->FindChild("vertexStride");
        const SDObject *format = chunk->FindChild("vertexFormat");
        const SDObject *duplicate = chunk->FindChild("allowDuplicate");
        if(!formattedRefit || !stride || stride->AsUInt64() != 16 || !format ||
           format->AsUInt64() != formattedFormat || !duplicate)
          return false;
        noDuplicateRefit = !duplicate->AsBool();
      }
      else
        noDuplicateRefit = chunk->name ==
            "MTLAccelerationStructureCommandEncoder::refitTriangleNoDuplicate";
      const bool extendedChunk = formattedChunk || noDuplicateRefit ||
          chunk->name == "MTLAccelerationStructureCommandEncoder::refitTriangleExtended";
      const SDObject *id = chunk->FindChild(extendedChunk ? "source" : "structure");
      const SDObject *destination = extendedChunk ? chunk->FindChild("destination") : NULL;
      const SDObject *scratchOffset = extendedChunk ? chunk->FindChild("scratchOffset") : NULL;
      const bool extended = extendedChunk && destination && id && scratchOffset &&
          (destination->AsResourceId() != id->AsResourceId() ||
           scratchOffset->AsUInt64() != 0);
      if(extended) extendedRefits++;
      refits++;
      const SDObject *geometry = chunk->FindChild("vertices");
      const SDObject *count = chunk->FindChild("triangleCount");
      if(!id || !geometry || id->AsResourceId() != structure || !count ||
         count->AsUInt64() != triangleCount) return false;
      refitVertices = geometry->AsResourceId();
      alternateVertices = refitVertices != vertices;
      if(extendedChunk && (!destination || !scratchOffset ||
                      destination->AsResourceId() == ResourceId() ||
                      (scratchOffset->AsUInt64() != 0 &&
                       scratchOffset->AsUInt64() != 256) ||
                      (!noDuplicateRefit && !formattedChunk &&
                       destination->AsResourceId() == structure &&
                       scratchOffset->AsUInt64() == 0))) return false;
      refitDestination = extendedChunk ? destination->AsResourceId() : structure;
      refitAt = position;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setAccelerationStructure")
    {
      bindings++;
      const SDObject *id = chunk->FindChild("structure");
      const SDObject *index = chunk->FindChild("index");
      if(!id || !index || id->AsResourceId() !=
          (bindings == 2 ? (compactions ? compacted : refitDestination) : structure) ||
         index->AsUInt64() != 0)
        return false;
    }
    if(chunk->name ==
       "MTLAccelerationStructureCommandEncoder::writeCompactedAccelerationStructureSize")
    {
      const SDObject *id = chunk->FindChild("structure");
      const SDObject *buffer = chunk->FindChild("buffer");
      const SDObject *type = chunk->FindChild("sizeDataType");
      if(!id || !buffer || !type || id->AsResourceId() != refitDestination ||
         buffer->AsResourceId() == ResourceId() || type->AsUInt64() != MTL::DataTypeULong)
        return false;
      compactWrites++;
      compactBuffer = buffer->AsResourceId();
      compactWriteAt = position;
    }
    if(chunk->name ==
       "MTLAccelerationStructureCommandEncoder::copyAndCompactAccelerationStructure")
    {
      const SDObject *source = chunk->FindChild("source");
      const SDObject *destination = chunk->FindChild("destination");
      const SDObject *buffer = chunk->FindChild("sizeBuffer");
      const SDObject *expected = chunk->FindChild("expectedSize");
      if(!source || !destination || !buffer || !expected ||
         source->AsResourceId() != refitDestination ||
         destination->AsResourceId() == ResourceId() ||
         buffer->AsResourceId() != compactBuffer || expected->AsUInt64() != 1792)
        return false;
      compactions++;
      compacted = destination->AsResourceId();
      compactAt = position;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setBuffer")
    {
      const SDObject *id = chunk->FindChild("buffer");
      const SDObject *index = chunk->FindChild("index");
      if(id && index && index->AsUInt64() == 1) output = id->AsResourceId();
    }
    if(chunk->name == "MTLBlitCommandEncoder::copyFromBuffer")
    {
      blits++;
      const SDObject *source = chunk->FindChild("sourceBuffer");
      const SDObject *destination = chunk->FindChild("destinationBuffer");
      const SDObject *bytes = chunk->FindChild("size");
      if(!source || !destination || !bytes ||
         source->AsResourceId() == vertices || destination->AsResourceId() != vertices ||
         bytes->AsUInt64() != triangleCount * (formattedRefit ? 48U : 36U)) return false;
      blitAt = position;
    }
    if(chunk->name == "MTLComputeCommandEncoder::dispatchThreads")
    {
      if(++dispatches == 1) firstRayAt = position;
      else lastRayAt = position;
    }
  }
  if(builds != 1 || refits != 1 || noDuplicateBuild != noDuplicateRefit ||
     bindings != 2 || dispatches != 2 || blits != (alternateVertices ? 0 : 1) ||
     descriptorAllocations > 1 || extendedRefits > 1 ||
     (descriptorAllocations && descriptorStructure != structure) ||
     compactions > 1 || compactWrites != compactions ||
     (compactions && !(refitAt < compactWriteAt && compactWriteAt < compactAt &&
                       compactAt < lastRayAt)) ||
     !(buildAt < firstRayAt && firstRayAt < refitAt && refitAt < lastRayAt) ||
     (!alternateVertices && !(firstRayAt < blitAt && blitAt < refitAt)) ||
     structure == ResourceId() || vertices == ResourceId() ||
     refitVertices == ResourceId() || output == ResourceId())
  {
    fprintf(stderr, "T141 AS refit chunk chain incomplete: %u/%u/%u/%u/%u\n",
            builds, refits, bindings, dispatches, blits);
    return false;
  }
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *build = NULL, *refit = NULL, *writeCompact = NULL,
                          *compact = NULL;
  rdcarray<const ActionDescription *> rays;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Build Metal Refittable Triangle Acceleration Structure" ||
       action->customName == "Build Metal Refittable Formatted Triangle Acceleration Structure")
      build = action;
    if(action->customName == "Refit Metal Triangle Acceleration Structure" ||
       action->customName == "Refit Metal Formatted Triangle Acceleration Structure") refit = action;
    if(action->customName == "Write Metal Compacted Acceleration Structure Size")
      writeCompact = action;
    if(action->customName == "Copy and Compact Metal Acceleration Structure")
      compact = action;
    if(action->customName.contains("dispatchThreads(")) rays.push_back(action);
  }
  if(!build || !refit || rays.size() != 2 ||
     (compactions && (!writeCompact || !compact ||
                      !(refit->eventId < writeCompact->eventId &&
                        writeCompact->eventId < compact->eventId &&
                        compact->eventId < rays[1]->eventId))) ||
     !(build->eventId < rays[0]->eventId && rays[0]->eventId < refit->eventId &&
       refit->eventId < rays[1]->eventId))
  {
    fprintf(stderr, "T141 AS refit actions missing or out of order\n");
    return false;
  }
  for(uint32_t event : {rays[1]->eventId, rays[0]->eventId, rays[1]->eventId})
  {
    renderer->SetFrameEvent(event, true);
    const bytebuf bytes = renderer->GetBufferData(output, 0, 8);
    uint32_t bits[2] = {};
    if(bytes.size() != sizeof(bits)) return false;
    memcpy(bits, bytes.data(), sizeof(bits));
    fprintf(stderr, "T141 ray event %u: %u,%u\n", event, bits[0], bits[1]);
    if(bits[0] != 1 || bits[1] != (event == rays[1]->eventId ? 0U : 9U))
      return false;
  }
  return true;
}

static bool ValidateIndexedRefitFixture(IReplayController *renderer)
{
  const auto &chunks = renderer->GetStructuredFile().chunks;
  ResourceId structure, destination, compacted, vertices, refitVertices, indices,
             refitIndices, output;
  size_t buildAt = 0, firstRayAt = 0, blitAt = 0, refitAt = 0, secondRayAt = 0;
  size_t compactWriteAt = 0, compactAt = 0;
  unsigned builds = 0, refits = 0, blits = 0, bindings = 0, dispatches = 0;
  unsigned compactWrites = 0, compactions = 0;
  uint64_t indexType = 0, vertexFormat = 0, vertexStride = 0, triangleCount = 0;
  uint64_t vertexOffset = 0, indexOffset = 0;
  uint64_t tableOffset = 0;
  bool opaqueGeometry = false;
  bool allowDuplicate = true;
  bool alternateIndices = false;
  bool alternateVertices = false;
  for(size_t position = 0; position < chunks.size(); position++)
  {
    const SDChunk *chunk = chunks[position];
    const bool build = chunk->name ==
        "MTLAccelerationStructureCommandEncoder::buildRefittableIndexedTriangle";
    const bool refit = chunk->name ==
        "MTLAccelerationStructureCommandEncoder::refitIndexedTriangle";
    if(build || refit)
    {
      const SDObject *id = chunk->FindChild(build ? "structure" : "source");
      const SDObject *target = refit ? chunk->FindChild("destination") : NULL;
      const SDObject *v = chunk->FindChild("vertices");
      const SDObject *i = chunk->FindChild("indices");
      const SDObject *vOffset = chunk->FindChild("vertexOffset");
      const SDObject *vStride = chunk->FindChild("vertexStride");
      const SDObject *vFormat = chunk->FindChild("vertexFormat");
      const SDObject *iType = chunk->FindChild("indexType");
      const SDObject *iOffset = chunk->FindChild("indexOffset");
      const SDObject *count = chunk->FindChild("triangleCount");
      const SDObject *table = chunk->FindChild("tableOffset");
      const SDObject *scratchOffset = chunk->FindChild("scratchOffset");
      const SDObject *opaque = chunk->FindChild("opaque");
      const SDObject *duplicate = chunk->FindChild("allowDuplicate");
      if(!id || !v || !i || !vOffset || !vStride || !vFormat || !iType || !iOffset ||
         !count || !table || !scratchOffset || !opaque || !duplicate ||
         (vOffset->AsUInt64() != 0 && vOffset->AsUInt64() != 16 &&
          vOffset->AsUInt64() != 36) ||
         (iOffset->AsUInt64() != 0 && iOffset->AsUInt64() != 2 &&
          iOffset->AsUInt64() != 8) ||
         (table->AsUInt64() != 0 && table->AsUInt64() != 1) ||
         (scratchOffset->AsUInt64() != 0 && scratchOffset->AsUInt64() != 256) ||
         (build && scratchOffset->AsUInt64()) ||
         (count->AsUInt64() != 1 && count->AsUInt64() != 2) ||
         (iType->AsUInt64() != MTL::IndexTypeUInt16 &&
          iType->AsUInt64() != MTL::IndexTypeUInt32) ||
         (vFormat->AsUInt64() != MTL::AttributeFormatFloat3 &&
          vFormat->AsUInt64() != MTL::AttributeFormatFloat4) ||
         vStride->AsUInt64() != (vFormat->AsUInt64() == MTL::AttributeFormatFloat4 ? 16 : 12))
        return false;
      if(build)
      {
        structure = id->AsResourceId();
        vertices = v->AsResourceId();
        indices = i->AsResourceId();
        indexType = iType->AsUInt64();
        vertexFormat = vFormat->AsUInt64();
        vertexStride = vStride->AsUInt64();
        vertexOffset = vOffset->AsUInt64();
        indexOffset = iOffset->AsUInt64();
        tableOffset = table->AsUInt64();
        opaqueGeometry = opaque->AsBool();
        triangleCount = count->AsUInt64();
        allowDuplicate = duplicate->AsBool();
        buildAt = position;
        builds++;
      }
      else
      {
        if(!target || id->AsResourceId() != structure ||
           vStride->AsUInt64() != vertexStride ||
           vOffset->AsUInt64() != vertexOffset ||
           iOffset->AsUInt64() != indexOffset ||
           table->AsUInt64() != tableOffset || opaque->AsBool() != opaqueGeometry ||
           vFormat->AsUInt64() != vertexFormat || iType->AsUInt64() != indexType ||
           count->AsUInt64() != triangleCount ||
           duplicate->AsBool() != allowDuplicate) return false;
        refitIndices = i->AsResourceId();
        alternateIndices = refitIndices != indices;
        refitVertices = v->AsResourceId();
        alternateVertices = refitVertices != vertices;
        destination = target->AsResourceId();
        refitAt = position;
        refits++;
      }
    }
    if(chunk->name == "MTLBlitCommandEncoder::copyFromBuffer")
    {
      const SDObject *target = chunk->FindChild("destinationBuffer");
      if(target && target->AsResourceId() == vertices)
      {
        blitAt = position;
        blits++;
      }
    }
    if(chunk->name ==
       "MTLAccelerationStructureCommandEncoder::writeCompactedAccelerationStructureSize")
    {
      const SDObject *id = chunk->FindChild("structure");
      if(!id || id->AsResourceId() != destination) return false;
      compactWriteAt = position;
      compactWrites++;
    }
    if(chunk->name ==
       "MTLAccelerationStructureCommandEncoder::copyAndCompactAccelerationStructure")
    {
      const SDObject *source = chunk->FindChild("source");
      const SDObject *target = chunk->FindChild("destination");
      if(!source || !target || source->AsResourceId() != destination ||
         target->AsResourceId() == ResourceId()) return false;
      compacted = target->AsResourceId();
      compactAt = position;
      compactions++;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setAccelerationStructure")
    {
      const SDObject *id = chunk->FindChild("structure");
      if(!id || id->AsResourceId() !=
          (bindings ? (compactions ? compacted : destination) : structure)) return false;
      bindings++;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setBuffer")
    {
      const SDObject *buffer = chunk->FindChild("buffer");
      const SDObject *index = chunk->FindChild("index");
      if(buffer && index && index->AsUInt64() == 1) output = buffer->AsResourceId();
    }
    if(chunk->name == "MTLComputeCommandEncoder::dispatchThreads")
    {
      if(++dispatches == 1) firstRayAt = position;
      else secondRayAt = position;
    }
  }
  if(builds != 1 || refits != 1 ||
     blits != (alternateIndices || alternateVertices ? 0 : 1) ||
     compactWrites != compactions || compactions > 1 ||
     bindings != 2 || dispatches != 2 ||
     structure == ResourceId() || destination == ResourceId() || vertices == ResourceId() ||
     indices == ResourceId() || refitIndices == ResourceId() ||
     refitVertices == ResourceId() || output == ResourceId() ||
     !(buildAt < firstRayAt && firstRayAt < refitAt && refitAt < secondRayAt) ||
     (!(alternateIndices || alternateVertices) &&
      !(firstRayAt < blitAt && blitAt < refitAt)) ||
     (compactions && !(refitAt < compactWriteAt && compactWriteAt < compactAt &&
                       compactAt < secondRayAt))) return false;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *build = NULL, *refit = NULL, *writeCompact = NULL,
                          *compact = NULL;
  rdcarray<const ActionDescription *> rays;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Build Metal Refittable Indexed Triangle Acceleration Structure")
      build = action;
    if(action->customName == "Refit Metal Indexed Triangle Acceleration Structure")
      refit = action;
    if(action->customName == "Write Metal Compacted Acceleration Structure Size")
      writeCompact = action;
    if(action->customName == "Copy and Compact Metal Acceleration Structure")
      compact = action;
    if(action->customName.contains("dispatchThreads(")) rays.push_back(action);
  }
  if(!build || !refit || rays.size() != 2 ||
     (compactions && (!writeCompact || !compact ||
                      !(refit->eventId < writeCompact->eventId &&
                        writeCompact->eventId < compact->eventId &&
                        compact->eventId < rays[1]->eventId))) ||
     !(build->eventId < rays[0]->eventId && rays[0]->eventId < refit->eventId &&
       refit->eventId < rays[1]->eventId)) return false;
  for(uint32_t index : {1U, 0U, 1U})
  {
    renderer->SetFrameEvent(rays[index]->eventId, true);
    const bytebuf bytes = renderer->GetBufferData(output, 0, 2 * sizeof(uint32_t));
    const uint32_t expected[2][2] = {{1, 9}, {1, 0}};
    if(bytes.size() != sizeof(expected[0]) ||
       memcmp(bytes.data(), expected[index], bytes.size())) return false;
  }
  return true;
}

static bool ValidateBoxRefitFixture(IReplayController *renderer)
{
  const auto &chunks = renderer->GetStructuredFile().chunks;
  ResourceId structure, refitTarget, copiedTarget, boxes, refitBoxes, output;
  size_t buildAt = 0, firstRayAt = 0, blitAt = 0, refitAt = 0, secondRayAt = 0;
  size_t copyAt = 0, thirdRayAt = 0, secondRefitAt = 0, fourthRayAt = 0;
  unsigned builds = 0, refits = 0, copies = 0, bindings = 0, dispatches = 0, blits = 0;
  bool combined = false;
  bool alternateBoxes = false;
  for(size_t position = 0; position < chunks.size(); position++)
  {
    const SDChunk *chunk = chunks[position];
    const bool build = chunk->name ==
        "MTLAccelerationStructureCommandEncoder::buildRefittableBoundingBox";
    const bool refit = chunk->name ==
        "MTLAccelerationStructureCommandEncoder::refitBoundingBox";
    if(build || refit)
    {
      const SDObject *id = chunk->FindChild(build ? "structure" : "source");
      const SDObject *destination = refit ? chunk->FindChild("destination") : NULL;
      const SDObject *buffer = chunk->FindChild("boxes");
      const SDObject *offset = chunk->FindChild("boxOffset");
      const SDObject *stride = chunk->FindChild("boxStride");
      const SDObject *count = chunk->FindChild("boxCount");
      const SDObject *tableOffset = chunk->FindChild("tableOffset");
      const SDObject *scratchOffset = chunk->FindChild("scratchOffset");
      const SDObject *opaque = chunk->FindChild("opaque");
      const SDObject *duplicate = chunk->FindChild("allowDuplicate");
      if(!id || !buffer || !offset || !stride || !count || !tableOffset ||
         !scratchOffset || !opaque || !duplicate ||
         stride->AsUInt64() != 32 || count->AsUInt64() != 2 || opaque->AsBool())
        return false;
      if(build)
      {
        combined = offset->AsUInt64() == 48;
        if(offset->AsUInt64() != (combined ? 48 : 0) ||
           tableOffset->AsUInt64() != (combined ? 1 : 0) ||
           scratchOffset->AsUInt64() != (combined ? 256 : 0) ||
           duplicate->AsBool() == combined) return false;
        structure = id->AsResourceId();
        boxes = buffer->AsResourceId();
        buildAt = position;
        builds++;
      }
      else
      {
        if(!destination || id->AsResourceId() != (refits ? copiedTarget : structure) ||
           offset->AsUInt64() != (combined ? 48 : 0) ||
           tableOffset->AsUInt64() != (combined ? 1 : 0) ||
           scratchOffset->AsUInt64() != (combined ? 256 : 0) ||
           duplicate->AsBool() == combined)
          return false;
        if(refits)
        {
          if(destination->AsResourceId() != copiedTarget) return false;
          secondRefitAt = position;
        }
        else
        {
          refitTarget = destination->AsResourceId();
          refitBoxes = buffer->AsResourceId();
          alternateBoxes = refitBoxes != boxes;
          refitAt = position;
        }
        if(refits && buffer->AsResourceId() != refitBoxes) return false;
        refits++;
      }
    }
    if(chunk->name == "MTLBlitCommandEncoder::copyFromBuffer")
    {
      const SDObject *destination = chunk->FindChild("destinationBuffer");
      const SDObject *size = chunk->FindChild("size");
      if(destination && destination->AsResourceId() == boxes)
      {
        if(!size || size->AsUInt64() != (combined ? 112 : 64)) return false;
        if(blits == 0) blitAt = position;
        blits++;
      }
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::copyAccelerationStructure")
    {
      const SDObject *source = chunk->FindChild("source");
      const SDObject *destination = chunk->FindChild("destination");
      if(!source || !destination || source->AsResourceId() != refitTarget) return false;
      copiedTarget = destination->AsResourceId();
      copyAt = position;
      copies++;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setAccelerationStructure")
    {
      const SDObject *id = chunk->FindChild("structure");
      const ResourceId expected = bindings == 0 ? structure :
          bindings == 1 ? refitTarget : copiedTarget;
      if(!id || id->AsResourceId() != expected) return false;
      bindings++;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setBuffer")
    {
      const SDObject *buffer = chunk->FindChild("buffer");
      const SDObject *index = chunk->FindChild("index");
      if(buffer && index && index->AsUInt64() == 1) output = buffer->AsResourceId();
    }
    if(chunk->name == "MTLComputeCommandEncoder::dispatchThreads")
    {
      if(++dispatches == 1) firstRayAt = position;
      else if(dispatches == 2) secondRayAt = position;
      else if(dispatches == 3) thirdRayAt = position;
      else fourthRayAt = position;
    }
  }
  const bool copiedRefit = refits == 2;
  if(builds != 1 || refits < 1 || refits > 2 || blits != refits - alternateBoxes ||
     bindings != 2 + copies + copiedRefit ||
     dispatches != 2 + copies + copiedRefit || copies > 1 ||
     structure == ResourceId() || refitTarget == ResourceId() || boxes == ResourceId() ||
     refitBoxes == ResourceId() ||
     output == ResourceId() ||
     !(buildAt < firstRayAt && firstRayAt < refitAt && refitAt < secondRayAt) ||
     (!alternateBoxes && !(firstRayAt < blitAt && blitAt < refitAt)) ||
     (copies && !(secondRayAt < copyAt && copyAt < thirdRayAt)) ||
     (copiedRefit && !(thirdRayAt < secondRefitAt && secondRefitAt < fourthRayAt)))
    return false;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *build = NULL, *refit = NULL, *secondRefit = NULL, *copy = NULL;
  rdcarray<const ActionDescription *> rays;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Build Metal Refittable Bounding Box Acceleration Structure")
      build = action;
    if(action->customName == "Refit Metal Bounding Box Acceleration Structure")
    {
      if(!refit) refit = action;
      else secondRefit = action;
    }
    if(action->customName == "Copy Metal Acceleration Structure") copy = action;
    if(action->customName.contains("dispatchThreads(")) rays.push_back(action);
  }
  if(!build || !refit || rays.size() != 2 + copies + copiedRefit ||
     (bool(copy) != bool(copies)) || (bool(secondRefit) != copiedRefit) ||
     !(build->eventId < rays[0]->eventId && rays[0]->eventId < refit->eventId &&
       refit->eventId < rays[1]->eventId) ||
     (copies && !(rays[1]->eventId < copy->eventId &&
                  copy->eventId < rays[2]->eventId)) ||
     (copiedRefit && !(rays[2]->eventId < secondRefit->eventId &&
                       secondRefit->eventId < rays[3]->eventId))) return false;
  const uint32_t expected[4][12] = {{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                    {1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
                                    {1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0},
                                    {1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0}};
  for(uint32_t index : {1U, 0U, 2U, 3U, 1U, 2U, 3U})
  {
    if(index == 2 && !copies) continue;
    if(index == 3 && !copiedRefit) continue;
    renderer->SetFrameEvent(rays[index]->eventId, true);
    const bytebuf bytes = renderer->GetBufferData(output, 0,
        (copiedRefit ? 12 : copies ? 9 : 6) * sizeof(uint32_t));
    if(bytes.size() != (copiedRefit ? 12 : copies ? 9 : 6) * sizeof(uint32_t) ||
       memcmp(bytes.data(), expected[index], bytes.size())) return false;
  }
  return true;
}

static bool ValidateBoxRayFixture(IReplayController *renderer)
{
  ResourceId pipeline, table, handle, structure, output, calls;
  unsigned created = 0, handled = 0, updated = 0, built = 0, bound = 0, dispatched = 0;
  unsigned asBound = 0;
  bool rangeBinding = false, offsetBuild = false, opaqueBuild = false, noDuplicateBuild = false;
  bool tableOffsetBuild = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLComputePipelineState::newIntersectionFunctionTableWithDescriptor")
    {
      const SDObject *p = chunk->FindChild("Pipeline");
      const SDObject *t = chunk->FindChild("Table");
      const SDObject *count = chunk->FindChild("count");
      if(!p || !t || !count ||
         (count->AsUInt64() != 1 && count->AsUInt64() != 2)) return false;
      pipeline = p->AsResourceId(); table = t->AsResourceId(); created++;
    }
    if(chunk->name == "MTLComputePipelineState::functionHandleWithFunction")
    {
      const SDObject *p = chunk->FindChild("Pipeline");
      const SDObject *h = chunk->FindChild("Handle");
      if(!p || !h || p->AsResourceId() != pipeline) return false;
      handle = h->AsResourceId(); handled++;
    }
    if(chunk->name == "MTLIntersectionFunctionTable::setFunction")
    {
      const SDObject *t = chunk->FindChild("Table");
      const SDObject *h = chunk->FindChild("function");
      const SDObject *index = chunk->FindChild("index");
      if(!t || !h || !index || t->AsResourceId() != table ||
         h->AsResourceId() != handle || index->AsUInt64() > 1) return false;
      tableOffsetBuild = index->AsUInt64() == 1;
      updated++;
    }
    if(chunk->name == "MTLIntersectionFunctionTable::setBuffer")
    {
      const SDObject *t = chunk->FindChild("Table");
      const SDObject *buffer = chunk->FindChild("buffer");
      const SDObject *index = chunk->FindChild("index");
      if(t && buffer && index && t->AsResourceId() == table && index->AsUInt64() == 0)
        calls = buffer->AsResourceId();
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxStrided" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxTableOffset" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxOpaque" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxNoDuplicate")
    {
      const SDObject *s = chunk->FindChild("structure");
      const SDObject *count = chunk->FindChild("boxCount");
      const SDObject *stride = chunk->FindChild("boxStride");
      const SDObject *boxOffset = chunk->FindChild("boxOffset");
      const SDObject *scratchOffset = chunk->FindChild("scratchOffset");
      const bool hasTableOffset =
          chunk->name != "MTLAccelerationStructureCommandEncoder::buildBoundingBoxStrided";
      const SDObject *tableOffset = hasTableOffset ? chunk->FindChild("tableOffset") : NULL;
      if(!s || !count || !stride || !boxOffset || !scratchOffset ||
         count->AsUInt64() != 2 || stride->AsUInt64() != 32 ||
         (boxOffset->AsUInt64() != 0 && boxOffset->AsUInt64() != 48) ||
         (scratchOffset->AsUInt64() != 0 && scratchOffset->AsUInt64() != 256) ||
         ((boxOffset->AsUInt64() == 0) != (scratchOffset->AsUInt64() == 0)) ||
         (hasTableOffset && (!tableOffset || tableOffset->AsUInt64() > 1))) return false;
      if((hasTableOffset && tableOffset->AsUInt64() == 1) != tableOffsetBuild) return false;
      opaqueBuild =
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxOpaque";
      noDuplicateBuild =
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxNoDuplicate";
      if(noDuplicateBuild)
      {
        const SDObject *opaque = chunk->FindChild("opaque");
        if(!opaque) return false;
        opaqueBuild = opaque->AsBool();
      }
      offsetBuild = boxOffset->AsUInt64() != 0;
      structure = s->AsResourceId(); built++;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setAccelerationStructure")
    {
      const SDObject *s = chunk->FindChild("structure");
      const SDObject *index = chunk->FindChild("index");
      if(!s || !index || s->AsResourceId() != structure || index->AsUInt64() != 0) return false;
      asBound++;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setBuffer")
    {
      const SDObject *buffer = chunk->FindChild("buffer");
      const SDObject *index = chunk->FindChild("index");
      if(buffer && index && index->AsUInt64() == 1) output = buffer->AsResourceId();
    }
    if(chunk->name == "MTLComputeCommandEncoder::setIntersectionFunctionTable")
    {
      const SDObject *t = chunk->FindChild("table");
      const SDObject *index = chunk->FindChild("index");
      if(!t || !index || t->AsResourceId() != table || index->AsUInt64() != 2) return false;
      bound++;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setIntersectionFunctionTables")
    {
      const SDObject *tables = chunk->FindChild("tables");
      const SDObject *range = chunk->FindChild("range");
      const SDObject *location = range ? range->FindChild("location") : NULL;
      const SDObject *length = range ? range->FindChild("length") : NULL;
      if(!tables || !range || tables->NumChildren() != 1 ||
         !location || !length || location->AsUInt64() != 2 || length->AsUInt64() != 1 ||
         tables->GetChild(0)->AsResourceId() != table) return false;
      rangeBinding = true; bound++;
    }
    dispatched += chunk->name == "MTLComputeCommandEncoder::dispatchThreads";
  }
  if(created != 1 || handled != 1 || updated != 1 || built != 1 || asBound != 1 ||
     bound != 1 || dispatched != 1 || output == ResourceId() ||
     table == ResourceId() || structure == ResourceId()) return false;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *build = NULL, *ray = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Build Metal Bounding Box Acceleration Structure") build = action;
    if(action->customName.contains("dispatchThreads(")) ray = action;
  }
  if(!build || !ray || !(build->eventId < ray->eventId)) return false;
  renderer->SetFrameEvent(ray->eventId, true);
  const bytebuf data = renderer->GetBufferData(output, 0, 3 * sizeof(uint32_t));
  uint32_t hits[3] = {};
  if(data.size() != sizeof(hits)) return false;
  memcpy(hits, data.data(), sizeof(hits));
  uint32_t callCount = 0;
  if(noDuplicateBuild)
  {
    if(calls == ResourceId()) return false;
    const bytebuf counter = renderer->GetBufferData(calls, 0, sizeof(callCount));
    if(counter.size() != sizeof(callCount)) return false;
    memcpy(&callCount, counter.data(), sizeof(callCount));
  }
  fprintf(stderr, "Box ray replay: %u/%u/%u offset=%u range=%u tableOffset=%u opaque=%u noDuplicate=%u calls=%u\n",
          hits[0], hits[1], hits[2], (unsigned)offsetBuild, (unsigned)rangeBinding,
          (unsigned)tableOffsetBuild, (unsigned)opaqueBuild, (unsigned)noDuplicateBuild,
          callCount);
  return hits[0] == (opaqueBuild ? 0U : 1U) &&
         hits[1] == (opaqueBuild ? 0U : 1U) && hits[2] == 0 &&
         (!noDuplicateBuild || callCount == 2);
}

static bool ValidateRayInstanceFixture(IReplayController *renderer)
{
  unsigned primitiveBuilds = 0, instanceBuilds = 0, bindings = 0, dispatches = 0;
  unsigned asDescriptorBegins = 0, asBareBegins = 0;
  unsigned descriptorAllocations = 0;
  unsigned instanceDescriptorAllocations = 0;
  unsigned primitiveCopies = 0;
  unsigned topCopies = 0;
  unsigned primitiveCompactions = 0, compactSizeWrites = 0;
  unsigned topCompactions = 0, topCompactSizeWrites = 0;
  ResourceId descriptorAllocationId;
  ResourceId instanceDescriptorAllocationId;
  ResourceId copiedPrimitive;
  ResourceId copiedTop;
  ResourceId compactedPrimitive, compactSizeBuffer;
  ResourceId compactedTop, topCompactSizeBuffer;
  bool multiple = false, distinct = false, multiDistinct = false, repeatedDistinct = false;
  unsigned instanceCount = 1;
  bool intersectionTable = false;
  bool intersectionRange = false;
  bool opaqueIntersectionRange = false;
  unsigned intersectionStage = 0;
  unsigned nonOpaqueBuilds = 0, opaqueBuilds = 0, indexedOpaqueBuilds = 0,
           indexedNonOpaqueBuilds = 0,
           intersectionHandles = 0, intersectionTables = 0,
           intersectionUpdates = 0, opaqueIntersectionUpdates = 0,
           intersectionBufferUpdates = 0, nestedVisibleTables = 0,
           nestedVisibleHandles = 0, nestedVisibleUpdates = 0,
           nestedVisibleFunctionUpdates = 0, nestedVisibleResidency = 0;
  bool intersectionBufferRange = false;
  bool indexedOpaque32 = false;
  bool indexedOffsetBuild = false;
  unsigned indexedExtendedCase = 0;
  bool indexedNonOpaque32 = false;
  bool opaqueVertexOffset = false;
  bool opaqueScratchOffset = false;
  bool nestedVisibleRange = false;
  ResourceId intersectionBuffer;
  ResourceId nestedVisibleTableId, nestedVisibleHandle, nestedPipeline;
  ResourceId intersectionHandle, intersectionTableId, intersectionPipeline;
  size_t intersectionHandleAt = 0, intersectionTableAt = 0,
         intersectionUpdateAt = 0, intersectionBindAt = 0;
  unsigned renderStage = 0;
  unsigned computeClears = 0, renderClears = 0;
  size_t computeClearAt = 0, renderClearAt = 0, lastRenderWorkAt = 0;
  ResourceId primitive, firstPrimitive, top, instances, output, fragmentOutput;
  rdcarray<ResourceId> primitiveIds;
  size_t primitiveAt = 0, instanceAt = 0, bindAt = 0, firstAt = 0, lastAt = 0,
         fragmentAt = 0, primitiveCopyAt = 0, topCopyAt = 0, primitiveCompactAt = 0,
         compactSizeWriteAt = 0, topCompactAt = 0, topCompactSizeWriteAt = 0;
  const auto &chunks = renderer->GetStructuredFile().chunks;
  bool triangleTableOffsetFixture = false;
  bool triangleNoDuplicateFixture = false;
  bool formattedTriangleFixture = false;
  bool indexedFormattedTriangleFixture = false;
  for(const SDChunk *chunk : chunks)
  {
    const bool noDuplicate =
        chunk->name == "MTLAccelerationStructureCommandEncoder::buildTriangleNoDuplicate";
    triangleNoDuplicateFixture |= noDuplicate;
    triangleTableOffsetFixture |= noDuplicate ||
        chunk->name == "MTLAccelerationStructureCommandEncoder::buildTriangleTableOffset";
  }
  for(size_t position = 0; position < chunks.size(); position++)
  {
    const SDChunk *chunk = chunks[position];
    if(chunk->name == "MTLCommandBuffer::accelerationStructureCommandEncoder" ||
       chunk->name == "MTLCommandBuffer::accelerationStructureCommandEncoderWithDescriptor")
    {
      const bool descriptor =
          chunk->name == "MTLCommandBuffer::accelerationStructureCommandEncoderWithDescriptor";
      const SDObject *commandBuffer = chunk->FindChild("CommandBuffer");
      const SDObject *encoder = chunk->FindChild("Encoder");
      const SDObject *samples = descriptor ? chunk->FindChild("hasSampleBuffers") : NULL;
      if(!commandBuffer || !encoder || commandBuffer->AsResourceId() == ResourceId() ||
         encoder->AsResourceId() == ResourceId() ||
         (descriptor && (!samples || samples->AsUInt64() != 0)))
      {
        fprintf(stderr, "AS pass descriptor fields invalid: descriptor=%u samples=%llu\n",
                descriptor ? 1U : 0U, samples ? (unsigned long long)samples->AsUInt64() : 999ULL);
        return false;
      }
      if(descriptor) asDescriptorBegins++;
      else asBareBegins++;
    }
    if(chunk->name == "MTLDevice::newAccelerationStructureWithDescriptor")
    {
      const SDObject *structure = chunk->FindChild("Structure");
      const SDObject *size = chunk->FindChild("size");
      if(!structure || !size || structure->AsResourceId() == ResourceId()) return false;
      if(size->AsUInt64() == 1536)
      {
        descriptorAllocations++;
        descriptorAllocationId = structure->AsResourceId();
      }
      else if(size->AsUInt64() == 1792 || size->AsUInt64() == 2048)
      {
        instanceDescriptorAllocations++;
        instanceDescriptorAllocationId = structure->AsResourceId();
      }
      else
        return false;
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildAccelerationStructure" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildOpaqueTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedOpaqueTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangleOffset" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangleExtended" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildTriangleTableOffset" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildTriangleNoDuplicate" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildFormattedTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedFormattedTriangle")
    {
      if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedFormattedTriangle")
      {
        indexedFormattedTriangleFixture = true;
        const SDObject *offset = chunk->FindChild("vertexOffset");
        const SDObject *stride = chunk->FindChild("vertexStride");
        const SDObject *format = chunk->FindChild("vertexFormat");
        const SDObject *indices = chunk->FindChild("indices");
        const SDObject *type = chunk->FindChild("indexType");
        const SDObject *indexOffset = chunk->FindChild("indexOffset");
        const SDObject *scratch = chunk->FindChild("scratchOffset");
        const SDObject *table = chunk->FindChild("tableOffset");
        const SDObject *opaque = chunk->FindChild("opaque");
        const SDObject *duplicate = chunk->FindChild("allowDuplicate");
        if(!offset || !stride || !format || !indices || !type || !indexOffset ||
           !scratch || !table || !opaque || !duplicate ||
           offset->AsUInt64() != (descriptorAllocations ? 16U : 0U) ||
           stride->AsUInt64() != 16 ||
           (format->AsUInt64() != MTL::AttributeFormatFloat3 &&
            format->AsUInt64() != MTL::AttributeFormatFloat4) ||
           indices->AsResourceId() == ResourceId() ||
           type->AsUInt64() != (descriptorAllocations ? MTL::IndexTypeUInt32 :
                                                     MTL::IndexTypeUInt16) ||
           indexOffset->AsUInt64() != (descriptorAllocations ? 8U : 0U) ||
           scratch->AsUInt64() != (descriptorAllocations ? 256U : 0U) ||
           table->AsUInt64() || opaque->AsUInt64() ||
           duplicate->AsUInt64() !=
               (format->AsUInt64() == MTL::AttributeFormatFloat4 ? 1U : 0U))
          return false;
      }
      if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildFormattedTriangle")
      {
        formattedTriangleFixture = true;
        const SDObject *offset = chunk->FindChild("vertexOffset");
        const SDObject *stride = chunk->FindChild("vertexStride");
        const SDObject *format = chunk->FindChild("vertexFormat");
        const SDObject *scratch = chunk->FindChild("scratchOffset");
        const SDObject *table = chunk->FindChild("tableOffset");
        const SDObject *opaque = chunk->FindChild("opaque");
        const SDObject *duplicate = chunk->FindChild("allowDuplicate");
        if(!offset || !stride || !format || !scratch || !table || !opaque || !duplicate ||
           (offset->AsUInt64() != 0 && offset->AsUInt64() != 16) ||
           stride->AsUInt64() != 16 ||
           (format->AsUInt64() != MTL::AttributeFormatFloat3 &&
            format->AsUInt64() != MTL::AttributeFormatFloat4) ||
           scratch->AsUInt64() != (offset->AsUInt64() ? 256U : 0U) ||
           table->AsUInt64() || duplicate->AsUInt64() > 1)
          return false;
      }
      if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle")
        nonOpaqueBuilds++;
      if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildTriangleTableOffset" ||
         chunk->name == "MTLAccelerationStructureCommandEncoder::buildTriangleNoDuplicate")
      {
        const SDObject *vertexOffset = chunk->FindChild("vertexOffset");
        const SDObject *indices = chunk->FindChild("indices");
        const SDObject *indexType = chunk->FindChild("indexType");
        const SDObject *indexOffset = chunk->FindChild("indexOffset");
        const SDObject *scratchOffset = chunk->FindChild("scratchOffset");
        const SDObject *tableOffset = chunk->FindChild("tableOffset");
        const SDObject *opaque = chunk->FindChild("opaque");
        if(!vertexOffset || !indices || !indexType || !indexOffset || !scratchOffset ||
           !tableOffset || tableOffset->AsUInt64() != 1 || !opaque || opaque->AsUInt64())
          return false;
        const bool combined = vertexOffset->AsUInt64() == 48 &&
            indexOffset->AsUInt64() == 8 && scratchOffset->AsUInt64() == 256 &&
            indexType->AsUInt64() == MTL::IndexTypeUInt32;
        const bool plain = vertexOffset->AsUInt64() == 0 &&
            indexOffset->AsUInt64() == 0 && scratchOffset->AsUInt64() == 0 &&
            indexType->AsUInt64() == MTL::IndexTypeUInt16;
        if(!plain && !combined) return false;
        indexedNonOpaque32 = combined;
        if(indices->AsResourceId() == ResourceId()) nonOpaqueBuilds++;
        else indexedNonOpaqueBuilds++;
      }
      if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildOpaqueTriangle")
      {
        opaqueBuilds++;
        const SDObject *offset = chunk->FindChild("vertexOffset");
        if(!offset || (offset->AsUInt64() != 0 && offset->AsUInt64() != 16)) return false;
        opaqueVertexOffset = offset->AsUInt64() == 16;
        const SDObject *scratchOffset = chunk->FindChild("scratchOffset");
        if(!scratchOffset ||
           (scratchOffset->AsUInt64() != 0 && scratchOffset->AsUInt64() != 256)) return false;
        opaqueScratchOffset = scratchOffset->AsUInt64() == 256;
      }
      if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangle")
      {
        indexedNonOpaqueBuilds++;
        const SDObject *indices = chunk->FindChild("indices");
        const SDObject *indexType = chunk->FindChild("indexType");
        if(!indices || indices->AsResourceId() == ResourceId() || !indexType ||
           (indexType->AsUInt64() != MTL::IndexTypeUInt16 &&
            indexType->AsUInt64() != MTL::IndexTypeUInt32)) return false;
      indexedNonOpaque32 = indexType->AsUInt64() == MTL::IndexTypeUInt32;
      }
      if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedOpaqueTriangle" ||
         chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangleOffset" ||
         chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangleExtended")
      {
        indexedOpaqueBuilds++;
        const SDObject *indices = chunk->FindChild("indices");
        const SDObject *indexType = chunk->FindChild("indexType");
        if(!indices || indices->AsResourceId() == ResourceId() || !indexType ||
           (indexType->AsUInt64() != MTL::IndexTypeUInt16 &&
            indexType->AsUInt64() != MTL::IndexTypeUInt32)) return false;
        indexedOpaque32 = indexType->AsUInt64() == MTL::IndexTypeUInt32;
        if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangleOffset")
        {
          const SDObject *offset = chunk->FindChild("indexOffset");
          const SDObject *opaque = chunk->FindChild("opaque");
          if(!offset || offset->AsUInt64() != 8 || !opaque || opaque->AsUInt64() != 1)
            return false;
          indexedOffsetBuild = true;
        }
        if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangleExtended")
        {
          const SDObject *vertexOffset = chunk->FindChild("vertexOffset");
          const SDObject *indexOffset = chunk->FindChild("indexOffset");
          const SDObject *scratchOffset = chunk->FindChild("scratchOffset");
          const SDObject *opaque = chunk->FindChild("opaque");
          if(!vertexOffset || !indexOffset || !scratchOffset || !opaque ||
             opaque->AsUInt64() != 1) return false;
          const uint64_t vertex = vertexOffset->AsUInt64();
          const uint64_t index = indexOffset->AsUInt64();
          const uint64_t scratch = scratchOffset->AsUInt64();
          if(vertex == 48 && index == 8 && scratch == 256)
            indexedExtendedCase = indexedOpaque32 ? 228U : 227U;
          else if(vertex == 48 && index == 0 && scratch == 0 && !indexedOpaque32)
            indexedExtendedCase = 229U;
          else if(vertex == 0 && index == 0 && scratch == 256 && !indexedOpaque32)
            indexedExtendedCase = 230U;
          else
            return false;
        }
      }
      primitiveBuilds++;
      const SDObject *id = chunk->FindChild("structure");
      const SDObject *count = chunk->FindChild("triangleCount");
      if(!id || (count && (count->AsUInt64() == 0 || count->AsUInt64() > 2))) return false;
      if(primitiveBuilds == 1) firstPrimitive = id->AsResourceId();
      primitive = id->AsResourceId();
      primitiveIds.push_back(primitive);
      primitiveAt = position;
    }
    if(chunk->name == "MTLRenderPipelineState::functionHandleWithFunction")
    {
      const SDObject *handle = chunk->FindChild("Handle");
      const SDObject *pipeline = chunk->FindChild("Pipeline");
      const SDObject *stage = chunk->FindChild("stageValue");
      if(!handle || !pipeline || !stage ||
         (stage->AsUInt64() != MTL::RenderStageFragment &&
          stage->AsUInt64() != MTL::RenderStageVertex &&
          stage->AsUInt64() != MTL::RenderStageTile))
        return false;
      if(nestedVisibleTables && intersectionTables == 0)
      {
        if(pipeline->AsResourceId() != nestedPipeline ||
           stage->AsUInt64() != MTL::RenderStageFragment) return false;
        nestedVisibleHandles++;
        nestedVisibleHandle = handle->AsResourceId();
        continue;
      }
      if(intersectionStage && intersectionStage != stage->AsUInt64()) return false;
      intersectionStage = (unsigned)stage->AsUInt64();
      intersectionHandles++;
      intersectionHandle = handle->AsResourceId();
      if(intersectionPipeline != ResourceId() &&
         intersectionPipeline != pipeline->AsResourceId()) return false;
      intersectionPipeline = pipeline->AsResourceId();
      intersectionHandleAt = position;
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::copyAccelerationStructure")
    {
      const SDObject *src = chunk->FindChild("source");
      const SDObject *dst = chunk->FindChild("destination");
      if(!src || !dst || dst->AsResourceId() == ResourceId() ||
         dst->AsResourceId() == src->AsResourceId())
        return false;
      if(src->AsResourceId() == primitive)
      {
        primitiveCopies++;
        copiedPrimitive = dst->AsResourceId();
        primitiveCopyAt = position;
      }
      else if(src->AsResourceId() == top)
      {
        topCopies++;
        copiedTop = dst->AsResourceId();
        topCopyAt = position;
      }
      else
        return false;
    }
    if(chunk->name ==
       "MTLAccelerationStructureCommandEncoder::writeCompactedAccelerationStructureSize")
    {
      const SDObject *as = chunk->FindChild("structure");
      const SDObject *buffer = chunk->FindChild("buffer");
      const SDObject *type = chunk->FindChild("sizeDataType");
      if(!as || !buffer || !type || buffer->AsResourceId() == ResourceId() ||
         type->AsUInt64() != MTL::DataTypeULong)
        return false;
      if(as->AsResourceId() == primitive)
      {
        compactSizeWrites++;
        compactSizeBuffer = buffer->AsResourceId();
        compactSizeWriteAt = position;
      }
      else if(as->AsResourceId() == top)
      {
        topCompactSizeWrites++;
        topCompactSizeBuffer = buffer->AsResourceId();
        topCompactSizeWriteAt = position;
      }
      else
        return false;
    }
    if(chunk->name ==
       "MTLAccelerationStructureCommandEncoder::copyAndCompactAccelerationStructure")
    {
      const SDObject *src = chunk->FindChild("source");
      const SDObject *dst = chunk->FindChild("destination");
      const SDObject *buffer = chunk->FindChild("sizeBuffer");
      const SDObject *expected = chunk->FindChild("expectedSize");
      if(!src || !dst || !buffer || !expected || dst->AsResourceId() == ResourceId())
        return false;
      if(src->AsResourceId() == primitive &&
         buffer->AsResourceId() == compactSizeBuffer && expected->AsUInt64() == 1280)
      {
        primitiveCompactions++;
        compactedPrimitive = dst->AsResourceId();
        primitiveCompactAt = position;
      }
      else if(src->AsResourceId() == top &&
              buffer->AsResourceId() == topCompactSizeBuffer &&
              expected->AsUInt64() == (instanceCount >= 3 ? 1792U : 1536U))
      {
        topCompactions++;
        compactedTop = dst->AsResourceId();
        topCompactAt = position;
      }
      else
        return false;
    }
    if(chunk->name == "MTLRenderPipelineState::newVisibleFunctionTableWithDescriptor")
    {
      const SDObject *table = chunk->FindChild("Table");
      const SDObject *pipeline = chunk->FindChild("Pipeline");
      const SDObject *count = chunk->FindChild("count");
      const SDObject *stage = chunk->FindChild("stageValue");
      if(!table || !pipeline || !count || !stage || count->AsUInt64() != 1 ||
         stage->AsUInt64() != MTL::RenderStageFragment) return false;
      nestedVisibleTables++;
      nestedVisibleTableId = table->AsResourceId();
      nestedPipeline = pipeline->AsResourceId();
    }
    if(chunk->name == "MTLVisibleFunctionTable::setFunction" && nestedVisibleTables)
    {
      const SDObject *table = chunk->FindChild("Table");
      const SDObject *function = chunk->FindChild("function");
      const SDObject *index = chunk->FindChild("index");
      if(!table || !function || !index ||
         table->AsResourceId() != nestedVisibleTableId ||
         function->AsResourceId() != nestedVisibleHandle || index->AsUInt64() != 0)
        return false;
      nestedVisibleFunctionUpdates++;
    }
    if(chunk->name == "MTLRenderPipelineState::newIntersectionFunctionTableWithDescriptor")
    {
      const SDObject *table = chunk->FindChild("Table");
      const SDObject *pipeline = chunk->FindChild("Pipeline");
      const SDObject *count = chunk->FindChild("count");
      const SDObject *stage = chunk->FindChild("stageValue");
      if(!table || !pipeline || !count || !stage ||
         (intersectionPipeline != ResourceId() &&
          pipeline->AsResourceId() != intersectionPipeline) ||
         count->AsUInt64() != (triangleTableOffsetFixture ? 2U : 1U) ||
         (stage->AsUInt64() != MTL::RenderStageFragment &&
          stage->AsUInt64() != MTL::RenderStageVertex &&
          stage->AsUInt64() != MTL::RenderStageTile))
        return false;
      if(intersectionStage && intersectionStage != stage->AsUInt64()) return false;
      intersectionStage = (unsigned)stage->AsUInt64();
      intersectionTables++;
      intersectionPipeline = pipeline->AsResourceId();
      intersectionTableId = table->AsResourceId();
      intersectionTableAt = position;
    }
    if(chunk->name == "MTLIntersectionFunctionTable::setFunction")
    {
      const SDObject *table = chunk->FindChild("Table");
      const SDObject *function = chunk->FindChild("function");
      const SDObject *index = chunk->FindChild("index");
      if(!table || !function || !index || table->AsResourceId() != intersectionTableId ||
         function->AsResourceId() != intersectionHandle ||
         index->AsUInt64() != (triangleTableOffsetFixture ? 1U : 0U))
        return false;
      intersectionUpdates++;
      intersectionUpdateAt = position;
    }
    if(chunk->name == "MTLIntersectionFunctionTable::setBuffer" ||
       chunk->name == "MTLIntersectionFunctionTable::setBuffers")
    {
      const bool rangeUpdate = chunk->name == "MTLIntersectionFunctionTable::setBuffers";
      const SDObject *table = chunk->FindChild("Table");
      const SDObject *buffers = rangeUpdate ? chunk->FindChild("buffers") : NULL;
      const SDObject *buffer = rangeUpdate ?
          (buffers && buffers->NumChildren() == 1 ? buffers->GetChild(0) : NULL) :
          chunk->FindChild("buffer");
      const SDObject *offsets = rangeUpdate ? chunk->FindChild("offsets") : NULL;
      const SDObject *offset = rangeUpdate ?
          (offsets && offsets->NumChildren() == 1 ? offsets->GetChild(0) : NULL) :
          chunk->FindChild("offset");
      const SDObject *range = rangeUpdate ? chunk->FindChild("range") : NULL;
      const SDObject *index = rangeUpdate ? (range ? range->FindChild("location") : NULL) :
                                         chunk->FindChild("index");
      const SDObject *length = rangeUpdate && range ? range->FindChild("length") : NULL;
      if(!table || !buffer || !offset || !index ||
         table->AsResourceId() != intersectionTableId ||
         buffer->AsResourceId() == ResourceId() || offset->AsUInt64() != 0 ||
         index->AsUInt64() != 0 || (rangeUpdate && (!length || length->AsUInt64() != 1)))
        return false;
      intersectionBufferUpdates++;
      intersectionBufferRange = rangeUpdate;
      intersectionBuffer = buffer->AsResourceId();
      if(!(intersectionUpdateAt < position)) return false;
      intersectionUpdateAt = position;
    }
    if(chunk->name == "MTLIntersectionFunctionTable::setVisibleFunctionTable" ||
       chunk->name == "MTLIntersectionFunctionTable::setVisibleFunctionTables")
    {
      const bool rangeUpdate =
          chunk->name == "MTLIntersectionFunctionTable::setVisibleFunctionTables";
      const SDObject *table = chunk->FindChild("Table");
      const SDObject *tables = rangeUpdate ? chunk->FindChild("tables") : NULL;
      const SDObject *visible = rangeUpdate ?
          (tables && tables->NumChildren() == 1 ? tables->GetChild(0) : NULL) :
          chunk->FindChild("visibleTable");
      const SDObject *range = rangeUpdate ? chunk->FindChild("range") : NULL;
      const SDObject *index = rangeUpdate ? (range ? range->FindChild("location") : NULL) :
                                         chunk->FindChild("index");
      const SDObject *length = rangeUpdate && range ? range->FindChild("length") : NULL;
      if(!table || !visible || !index ||
         table->AsResourceId() != intersectionTableId ||
         visible->AsResourceId() != nestedVisibleTableId || index->AsUInt64() != 1 ||
         (rangeUpdate && (!length || length->AsUInt64() != 1)) ||
         !(intersectionUpdateAt < position)) return false;
      nestedVisibleUpdates++;
      nestedVisibleRange = rangeUpdate;
      intersectionUpdateAt = position;
    }
    if(chunk->name == "MTLRenderCommandEncoder::useResource" && nestedVisibleTables)
    {
      const SDObject *resource = chunk->FindChild("resource");
      if(resource && resource->AsResourceId() == nestedVisibleTableId)
      {
        const SDObject *usage = chunk->FindChild("usageValue");
        const SDObject *stages = chunk->FindChild("stagesValue");
        if(!usage || !stages || usage->AsUInt64() != MTL::ResourceUsageRead ||
           stages->AsUInt64() != MTL::RenderStageFragment) return false;
        nestedVisibleResidency++;
      }
    }
    if(chunk->name == "MTLIntersectionFunctionTable::setOpaqueTriangleFunction" ||
       chunk->name == "MTLIntersectionFunctionTable::setOpaqueTriangleFunctions")
    {
      const bool rangeUpdate =
          chunk->name == "MTLIntersectionFunctionTable::setOpaqueTriangleFunctions";
      const SDObject *table = chunk->FindChild("Table");
      const SDObject *signature = chunk->FindChild("signatureValue");
      const SDObject *range = rangeUpdate ? chunk->FindChild("range") : NULL;
      const SDObject *index = rangeUpdate ? (range ? range->FindChild("location") : NULL) :
                                           chunk->FindChild("index");
      const SDObject *length = rangeUpdate && range ? range->FindChild("length") : NULL;
      if(!table || !signature || !index || table->AsResourceId() != intersectionTableId ||
         signature->AsUInt64() != (MTL::IntersectionFunctionSignatureInstancing |
                                   MTL::IntersectionFunctionSignatureTriangleData) ||
         index->AsUInt64() != 0 || (rangeUpdate && (!length || length->AsUInt64() != 1)))
        return false;
      opaqueIntersectionUpdates++;
      opaqueIntersectionRange = rangeUpdate;
      intersectionUpdateAt = position;
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildInstance" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildInstances" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildDistinctInstances" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildMultipleDistinctInstances" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildRepeatedDistinctInstances")
    {
      multiDistinct =
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildMultipleDistinctInstances";
      repeatedDistinct =
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildRepeatedDistinctInstances";
      distinct = multiDistinct || repeatedDistinct ||
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildDistinctInstances";
      multiple = distinct || chunk->name == "MTLAccelerationStructureCommandEncoder::buildInstances";
      instanceBuilds++;
      const SDObject *id = chunk->FindChild("structure");
      const SDObject *children = multiDistinct || repeatedDistinct ?
          chunk->FindChild("children") : NULL;
      const SDObject *child = children ? NULL : chunk->FindChild(distinct ? "child0" : "child");
      const SDObject *child1 = distinct && !children ? chunk->FindChild("child1") : NULL;
      const SDObject *buffer = chunk->FindChild("instances");
      const SDObject *bytes = chunk->FindChild("descriptorBytes");
      const SDObject *count = chunk->FindChild("count");
      instanceCount = repeatedDistinct ? count ? (unsigned)count->AsUInt64() : 0U :
                      multiDistinct ? children ? (unsigned)children->NumChildren() : 0U :
                      distinct ? 2U : multiple && count ? (unsigned)count->AsUInt64() : 1U;
      if(!id || (!children && !child) || !buffer || !bytes ||
         (!children && child->AsResourceId() != (distinct ? firstPrimitive :
                                  primitiveCompactions ? compactedPrimitive :
                                  primitiveCopies ? copiedPrimitive : primitive)) ||
         (distinct && !children &&
          (!child1 || child1->AsResourceId() != primitive ||
           child1->AsResourceId() == child->AsResourceId())) ||
         instanceCount < 1 || instanceCount > 8 ||
         bytes->NumChildren() != instanceCount * 64U ||
         (multiple && !distinct && (!count || instanceCount < 2))) return false;
      if(multiDistinct)
      {
        if(instanceCount < 3 || primitiveIds.size() != instanceCount) return false;
        for(unsigned i = 0; i < instanceCount; i++)
          if(children->GetChild(i)->AsResourceId() != primitiveIds[i]) return false;
      }
      if(repeatedDistinct)
      {
        if(instanceCount <= 2 || !children || children->NumChildren() != 2 ||
           primitiveIds.size() != 2 || !count) return false;
        for(unsigned i = 0; i < 2; i++)
          if(children->GetChild(i)->AsResourceId() != primitiveIds[i]) return false;
      }
      top = id->AsResourceId();
      instances = buffer->AsResourceId();
      instanceAt = position;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setAccelerationStructure")
    {
      const SDObject *id = chunk->FindChild("structure");
      const SDObject *index = chunk->FindChild("index");
      if(!id || !index || index->AsUInt64() != 0) return false;
      if(id->AsResourceId() == ResourceId())
      {
        computeClears++;
        computeClearAt = position;
        continue;
      }
      bindings++;
      if(id->AsResourceId() != (topCompactions ? compactedTop :
                               topCopies ? copiedTop : top)) return false;
      bindAt = position;
    }
    if(chunk->name == "MTLComputeCommandEncoder::setBuffer")
    {
      const SDObject *id = chunk->FindChild("buffer");
      const SDObject *index = chunk->FindChild("index");
      if(id && index && index->AsUInt64() == 1) output = id->AsResourceId();
    }
    unsigned bindingStage =
        chunk->name == "MTLRenderCommandEncoder::setFragmentAccelerationStructure" ? 1 :
        chunk->name == "MTLRenderCommandEncoder::setVertexAccelerationStructure" ? 2 :
        chunk->name == "MTLRenderCommandEncoder::setTileAccelerationStructure" ? 3 : 0;
    if(bindingStage)
    {
      const SDObject *id = chunk->FindChild("structure");
      const SDObject *index = chunk->FindChild("index");
      if(!id || !index || index->AsUInt64() != 0) return false;
      if(id->AsResourceId() == ResourceId())
      {
        if(renderStage != bindingStage) return false;
        renderClears++;
        renderClearAt = position;
        continue;
      }
      if(renderStage || id->AsResourceId() != (topCompactions ? compactedTop :
                                             topCopies ? copiedTop : top)) return false;
      renderStage = bindingStage;
      fragmentAt = position;
    }
    const bool fragmentIntersectionBind =
        chunk->name == "MTLRenderCommandEncoder::setFragmentIntersectionFunctionTable" ||
        chunk->name == "MTLRenderCommandEncoder::setFragmentIntersectionFunctionTables";
    const bool vertexIntersectionBind =
        chunk->name == "MTLRenderCommandEncoder::setVertexIntersectionFunctionTable" ||
        chunk->name == "MTLRenderCommandEncoder::setVertexIntersectionFunctionTables";
    const bool tileIntersectionBind =
        chunk->name == "MTLRenderCommandEncoder::setTileIntersectionFunctionTable" ||
        chunk->name == "MTLRenderCommandEncoder::setTileIntersectionFunctionTables";
    if(fragmentIntersectionBind || vertexIntersectionBind || tileIntersectionBind)
    {
      const unsigned expectedStage = tileIntersectionBind ? 3 : vertexIntersectionBind ? 2 : 1;
      const bool rangeBind =
          chunk->name == "MTLRenderCommandEncoder::setFragmentIntersectionFunctionTables" ||
          chunk->name == "MTLRenderCommandEncoder::setVertexIntersectionFunctionTables" ||
          chunk->name == "MTLRenderCommandEncoder::setTileIntersectionFunctionTables";
      const SDObject *tables = rangeBind ? chunk->FindChild("tables") : NULL;
      const SDObject *table = rangeBind ?
          (tables && tables->NumChildren() == 1 ? tables->GetChild(0) : NULL) :
          chunk->FindChild("table");
      const SDObject *range = rangeBind ? chunk->FindChild("range") : NULL;
      const SDObject *index = rangeBind ? (range ? range->FindChild("location") : NULL) :
                                          chunk->FindChild("index");
      const SDObject *length = rangeBind && range ? range->FindChild("length") : NULL;
      if(intersectionTable || !table || !index ||
         table->AsResourceId() != intersectionTableId || index->AsUInt64() != 2 ||
         (rangeBind && (!length || length->AsUInt64() != 1)) ||
         renderStage != expectedStage)
        return false;
      intersectionTable = true;
      intersectionRange = rangeBind;
      intersectionBindAt = position;
    }
    if(chunk->name == "MTLRenderCommandEncoder::setFragmentBuffer" ||
       chunk->name == "MTLRenderCommandEncoder::setVertexBuffer" ||
       chunk->name == "MTLRenderCommandEncoder::setTileBuffer")
    {
      const SDObject *id = chunk->FindChild("buffer");
      const SDObject *index = chunk->FindChild("index");
      if(id && index && index->AsUInt64() == 1 &&
         ((renderStage == 1 && chunk->name == "MTLRenderCommandEncoder::setFragmentBuffer") ||
          (renderStage == 2 && chunk->name == "MTLRenderCommandEncoder::setVertexBuffer") ||
          (renderStage == 3 && chunk->name == "MTLRenderCommandEncoder::setTileBuffer")))
        fragmentOutput = id->AsResourceId();
    }
    if(chunk->name == "MTLComputeCommandEncoder::dispatchThreads")
    {
      if(++dispatches == 1) firstAt = position;
      lastAt = position;
    }
    if(chunk->name == "MTLRenderCommandEncoder::drawPrimitives" ||
       chunk->name == "MTLRenderCommandEncoder::dispatchThreadsPerTile")
      lastRenderWorkAt = position;
  }
  if(computeClears != renderClears || computeClears > 1 ||
     (computeClears && !(lastAt < computeClearAt && lastRenderWorkAt < renderClearAt &&
                         fragmentAt < renderClearAt)))
  {
    fprintf(stderr, "AS clear sequence invalid: %u/%u at %zu/%zu, work %zu/%zu, bind %zu\n",
            computeClears, renderClears, computeClearAt, renderClearAt, lastAt,
            lastRenderWorkAt, fragmentAt);
    return false;
  }
  const unsigned rayCount = instanceCount >= 3 ? instanceCount : multiple ? 3U : 2U;
  if(primitiveBuilds != (multiDistinct ? instanceCount : distinct ? 2U : 1U) ||
     instanceBuilds != 1 || bindings != 1 ||
     primitiveCopies > 1 ||
     (primitiveCopies && !(primitiveAt < primitiveCopyAt && primitiveCopyAt < instanceAt)) ||
     topCopies > 1 ||
     (topCopies && !(instanceAt < topCopyAt && topCopyAt < bindAt)) ||
     primitiveCompactions > 1 || compactSizeWrites != primitiveCompactions ||
     topCompactions > 1 || topCompactSizeWrites != topCompactions ||
     (topCompactions && !(instanceAt < topCompactSizeWriteAt &&
                          topCompactSizeWriteAt < topCompactAt && topCompactAt < bindAt)) ||
     (primitiveCompactions &&
      !(primitiveAt < compactSizeWriteAt && compactSizeWriteAt < primitiveCompactAt &&
        primitiveCompactAt < instanceAt)) ||
     dispatches != rayCount ||
     !((asDescriptorBegins == 2 && asBareBegins == primitiveCompactions + topCompactions) ||
       (asDescriptorBegins == 0 &&
        asBareBegins == 2 + primitiveCompactions + topCompactions)) ||
     !(primitiveAt < instanceAt && instanceAt < bindAt && bindAt < firstAt &&
       firstAt < lastAt) || primitive == ResourceId() || top == ResourceId() ||
     instances == ResourceId() || output == ResourceId())
  {
    fprintf(stderr, "T142 instance AS chunk chain invalid\n");
    return false;
  }
  if(renderStage && (!distinct || fragmentOutput == ResourceId() ||
                     !(lastAt < fragmentAt))) return false;
  if(intersectionTable &&
     (!((nonOpaqueBuilds == 2 && opaqueBuilds == 0 && indexedOpaqueBuilds == 0 &&
         indexedNonOpaqueBuilds == 0) ||
        (nonOpaqueBuilds == 1 && indexedNonOpaqueBuilds == 1 &&
         opaqueBuilds == 0 && indexedOpaqueBuilds == 0) ||
        (nonOpaqueBuilds == 1 && opaqueBuilds + indexedOpaqueBuilds == 1 &&
         indexedNonOpaqueBuilds == 0 && primitiveBuilds == 2)) ||
      intersectionTables != 1 ||
      intersectionTableId == ResourceId() || intersectionPipeline == ResourceId() ||
      intersectionStage != (renderStage == 3 ? MTL::RenderStageTile :
                            renderStage == 2 ? MTL::RenderStageVertex :
                                               MTL::RenderStageFragment) ||
      !((intersectionHandles == 1 && intersectionUpdates == 1 &&
         opaqueIntersectionUpdates == 0 && intersectionHandle != ResourceId() &&
         intersectionHandleAt < intersectionUpdateAt) ||
        (intersectionHandles == 0 && intersectionUpdates == 0 &&
         opaqueIntersectionUpdates == 1 && renderStage == 1 && !intersectionRange)) ||
      !(intersectionTableAt < intersectionUpdateAt && intersectionUpdateAt < intersectionBindAt)))
    return false;
  if(intersectionBufferUpdates &&
     (intersectionBufferUpdates != 1 || intersectionBuffer == ResourceId() ||
      intersectionHandles != 1 || intersectionUpdates != 1 || renderStage != 1 ||
      intersectionRange || opaqueIntersectionUpdates))
    return false;
  if(descriptorAllocations &&
     (descriptorAllocations != 1 ||
      opaqueBuilds + indexedOpaqueBuilds +
          (indexedFormattedTriangleFixture ? 1U : 0U) +
          (triangleTableOffsetFixture && indexedNonOpaqueBuilds ? 1U : 0U) != 1 ||
      descriptorAllocationId != primitive)) return false;
  if(instanceDescriptorAllocations &&
     (instanceDescriptorAllocations != 1 || instanceDescriptorAllocationId != top)) return false;
  if(nestedVisibleUpdates &&
     (nestedVisibleTables != 1 || nestedVisibleHandles != 1 ||
      nestedVisibleFunctionUpdates != 1 || nestedVisibleUpdates != 1 ||
      nestedVisibleTableId == ResourceId() || nestedVisibleHandle == ResourceId() ||
      nestedPipeline != intersectionPipeline || intersectionBufferUpdates != 1 ||
      renderStage != 1 || nestedVisibleResidency > 1))
    return false;
  if(nestedVisibleTables && !nestedVisibleUpdates) return false;
  if(!intersectionTable &&
     ((nonOpaqueBuilds && !formattedTriangleFixture && !indexedFormattedTriangleFixture &&
       instanceCount < 3 &&
       !asDescriptorBegins && !computeClears &&
       !instanceDescriptorAllocations && !primitiveCopies && !primitiveCompactions &&
       !topCopies && !topCompactions) ||
      opaqueBuilds || indexedOpaqueBuilds ||
      (indexedNonOpaqueBuilds && !primitiveCompactions) ||
      intersectionHandles || intersectionTables || intersectionUpdates ||
      opaqueIntersectionUpdates || intersectionBufferUpdates || nestedVisibleTables))
    return false;
  unsigned asResources = 0;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.resourceId == primitive || resource.resourceId == top)
      asResources += resource.type == ResourceType::AccelerationStructure;
  if(asResources != 2) return false;
  if(distinct)
  {
    for(unsigned i = 0; i < (multiDistinct ? instanceCount : 1U); i++)
    {
      bool found = false;
      for(const ResourceDescription &resource : renderer->GetResources())
        if(resource.resourceId == primitiveIds[i] &&
           resource.type == ResourceType::AccelerationStructure)
          found = true;
      if(!found) return false;
    }
  }
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *buildPrimitive = NULL, *buildTop = NULL, *copyPrimitive = NULL;
  const ActionDescription *writeCompactSize = NULL, *compactPrimitive = NULL;
  char instanceActionName[96] = {};
  snprintf(instanceActionName, sizeof(instanceActionName),
           "Build Metal %u-Instance Acceleration Structure", instanceCount);
  rdcarray<const ActionDescription *> rays;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == "Build Metal Triangle Acceleration Structure" ||
       action->customName == "Build Metal Formatted Triangle Acceleration Structure" ||
       action->customName == "Build Metal Indexed Formatted Triangle Acceleration Structure" ||
       action->customName == "Build Metal Non-Opaque Triangle Acceleration Structure" ||
       action->customName == "Build Metal Indexed Triangle Acceleration Structure" ||
       action->customName == "Build Metal Opaque Triangle Acceleration Structure")
      buildPrimitive = action;
    char repeatedActionName[112] = {};
    snprintf(repeatedActionName, sizeof(repeatedActionName),
             "Build Metal %u-Instance %u-Child Acceleration Structure",
             instanceCount, 2U);
    if(action->customName == (repeatedDistinct ? repeatedActionName :
                             multiDistinct ?
                                 "Build Metal Multiple Distinct-Child Acceleration Structure" :
                             distinct ? "Build Metal Distinct-Child Acceleration Structure" :
                             multiple ? instanceActionName :
                                        "Build Metal Single-Instance Acceleration Structure"))
      buildTop = action;
    if(action->customName == "Copy Metal Acceleration Structure") copyPrimitive = action;
    if(action->customName == "Write Metal Compacted Acceleration Structure Size")
      writeCompactSize = action;
    if(action->customName == "Copy and Compact Metal Acceleration Structure")
      compactPrimitive = action;
    if(action->customName.contains("dispatchThreads(")) rays.push_back(action);
  }
  if(!buildPrimitive || !buildTop || rays.size() != rayCount ||
     (primitiveCopies && (!copyPrimitive ||
                          !(buildPrimitive->eventId < copyPrimitive->eventId &&
                            copyPrimitive->eventId < buildTop->eventId))) ||
     (topCopies && (!copyPrimitive ||
                    !(buildTop->eventId < copyPrimitive->eventId &&
                      copyPrimitive->eventId < rays[0]->eventId))) ||
     (primitiveCompactions &&
      (!writeCompactSize || !compactPrimitive ||
       !(buildPrimitive->eventId < writeCompactSize->eventId &&
         writeCompactSize->eventId < compactPrimitive->eventId &&
         compactPrimitive->eventId < buildTop->eventId))) ||
     (topCompactions &&
      (!writeCompactSize || !compactPrimitive ||
       !(buildTop->eventId < writeCompactSize->eventId &&
         writeCompactSize->eventId < compactPrimitive->eventId &&
         compactPrimitive->eventId < rays[0]->eventId))) ||
     !(buildPrimitive->eventId < buildTop->eventId &&
       buildTop->eventId < rays[0]->eventId && rays[0]->eventId < rays[1]->eventId) ||
     (multiple && rays[1]->eventId >= rays[2]->eventId))
    return false;
  rdcarray<uint32_t> events;
  events.push_back(rays.back()->eventId);
  events.push_back(rays[0]->eventId);
  for(unsigned i = 1; i + 1 < rayCount; i++) events.push_back(rays[i]->eventId);
  events.push_back(rays.back()->eventId);
  for(uint32_t event : events)
  {
    renderer->SetFrameEvent(event, true);
    const bytebuf bytes = renderer->GetBufferData(output, 0, rayCount * sizeof(uint32_t));
    uint32_t bits[8] = {};
    if(bytes.size() != rayCount * sizeof(uint32_t)) return false;
    memcpy(bits, bytes.data(), bytes.size());
    if(instanceCount >= 3)
    {
      for(unsigned i = 0; i < rayCount; i++)
      {
        const uint32_t initial = 7U + 2U * i;
        if(bits[i] != (event >= rays[i]->eventId ? 1U : initial)) return false;
      }
    }
    else if(multiple)
    {
      fprintf(stderr, "T%u ray event %u: %u,%u,%u\n", distinct ? 144U : 143U,
              event, bits[0], bits[1], bits[2]);
      if(bits[0] != 1 || bits[1] != (event == rays[0]->eventId ? 9U : 0U) ||
         bits[2] != (event == rays.back()->eventId ? 1U : 11U)) return false;
    }
    else
    {
      fprintf(stderr, "T%u ray event %u: %u,%u\n",
              asDescriptorBegins ? 164U : 142U, event, bits[0], bits[1]);
      if(bits[0] != 0 || bits[1] != (event == rays[1]->eventId ? 1U : 9U)) return false;
    }
  }
  if(renderStage)
  {
    const ActionDescription *renderAction = renderStage == 3 ? NULL :
        FindAction(renderer->GetRootActions(), ActionFlags::Drawcall);
    if(renderStage == 3)
      for(const ActionDescription *action : actions)
        if((action->flags & ActionFlags::Dispatch) &&
           action->customName.contains("dispatchThreadsPerTile"))
          renderAction = action;
    ResourceId swap;
    for(const TextureDescription &texture : renderer->GetTextures())
      if(texture.creationFlags & TextureCategory::SwapBuffer) swap = texture.resourceId;
    if(!renderAction || swap == ResourceId() || renderAction->eventId <= rays.back()->eventId)
      return false;
    uint32_t intersectionBufferValue = 0;
    if(intersectionBufferUpdates)
    {
      renderer->SetFrameEvent(renderAction->eventId, true);
      const bytebuf bytes = renderer->GetBufferData(intersectionBuffer, 0, 4);
      if(bytes.size() != 4) return false;
      memcpy(&intersectionBufferValue, bytes.data(), 4);
      if(intersectionBufferValue > 1) return false;
    }
    for(uint32_t event : {renderAction->eventId, rays.back()->eventId, renderAction->eventId})
    {
      renderer->SetFrameEvent(event, true);
      const bytebuf bytes = renderer->GetBufferData(fragmentOutput, 0, 4);
      uint32_t value = 0;
      if(bytes.size() != 4) return false;
      memcpy(&value, bytes.data(), 4);
      fprintf(stderr, "T%u render ray event %u: %u\n",
              triangleNoDuplicateFixture ? (indexedNonOpaque32 ? 239U :
                                            indexedNonOpaqueBuilds ? 238U : 237U) :
              triangleTableOffsetFixture ? (indexedNonOpaque32 ? 233U :
                                            indexedNonOpaqueBuilds ? 232U : 231U) :
              indexedNonOpaqueBuilds ? (indexedNonOpaque32 ? 172U : 170U) :
              indexedOpaqueBuilds ? (indexedExtendedCase ? indexedExtendedCase :
                                     indexedOffsetBuild ? (indexedOpaque32 ? 226U : 225U) :
                                     descriptorAllocations ? (indexedOpaque32 ? 171U : 169U) :
                                     indexedOpaque32 ? 168U : 167U) :
              opaqueBuilds ? (opaqueScratchOffset ?
                              (opaqueVertexOffset ? (descriptorAllocations ? 177U : 176U) : 175U) :
                              opaqueVertexOffset ? (descriptorAllocations ? 174U : 173U) :
                              descriptorAllocations ? 166U : 165U) :
              nestedVisibleUpdates ? (nestedVisibleResidency ? 163U :
                                      nestedVisibleRange ? 162U : 161U) :
              intersectionBufferUpdates ? (intersectionBufferRange ? 160U : 159U) :
              opaqueIntersectionUpdates ? (opaqueIntersectionRange ? 157U : 156U) :
              intersectionTable ? ((intersectionRange ? 151U : 148U) + renderStage - 1U) :
                                  144U + renderStage,
              event, value);
      if(value != (event == renderAction->eventId ?
                   (intersectionBufferUpdates ? intersectionBufferValue :
                    (opaqueBuilds || indexedOpaqueBuilds) ? 1U :
                    intersectionTable && !opaqueIntersectionUpdates ? 0U : 1U) : 7U))
        return false;
    }
    if(renderStage != 3 &&
       !PixelMatches(renderer, swap, renderAction->eventId, 200, 150,
                     intersectionBufferUpdates ? !intersectionBufferValue :
                     (opaqueBuilds || indexedOpaqueBuilds) ? 0 :
                     intersectionTable && !opaqueIntersectionUpdates ? 1 : 0,
                     intersectionBufferUpdates ? !!intersectionBufferValue :
                     (opaqueBuilds || indexedOpaqueBuilds) ? 1 :
                     intersectionTable && !opaqueIntersectionUpdates ? 0 : 1, 0))
      return false;
  }
  return true;
}

static bool ValidateAccelerationStructureFixture(IReplayController *renderer)
{
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::copyAndCompactAccelerationStructure")
      return ValidateCompactedAccelerationStructureFixture(renderer);
  ResourceId structure, copyDestination, encoder, vertices, indices, scratch, output;
  bool indexed = false, boxed = false, nonOpaque = false, multiBox = false, multiTriangle = false,
       multiIndexed = false,
       descriptorCreated = false;
  unsigned created = 0, begun = 0, built = 0, copied = 0, written = 0, ended = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newAccelerationStructureWithSize" ||
       chunk->name == "MTLDevice::newAccelerationStructureWithDescriptor")
    {
      descriptorCreated |=
          chunk->name == "MTLDevice::newAccelerationStructureWithDescriptor";
      created++;
      const SDObject *id = chunk->FindChild("Structure");
      const SDObject *size = chunk->FindChild("size");
      if(!id || !size || size->AsUInt64() != 1536) return false;
      if(created == 1) structure = id->AsResourceId();
      else if(created == 2) copyDestination = id->AsResourceId();
      else return false;
    }
    if(chunk->name == "MTLCommandBuffer::accelerationStructureCommandEncoder")
    {
      begun++;
      const SDObject *id = chunk->FindChild("Encoder");
      if(!id) return false;
      encoder = id->AsResourceId();
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildAccelerationStructure" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangle" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBox" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxExtended" ||
       chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxStrided")
    {
      built++;
      indexed = chunk->name == "MTLAccelerationStructureCommandEncoder::buildIndexedTriangle";
      boxed = chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBox" ||
              chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxExtended" ||
              chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxStrided";
      nonOpaque = chunk->name == "MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle";
      const SDObject *e = chunk->FindChild("Encoder");
      const SDObject *as = chunk->FindChild("structure");
      const SDObject *v = chunk->FindChild(boxed ? "boxes" : "vertices");
      const SDObject *s = chunk->FindChild("scratch");
      const SDObject *count = chunk->FindChild(boxed ? "boxCount" : "triangleCount");
      if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxExtended")
      {
        const SDObject *boxOffset = chunk->FindChild("boxOffset");
        const SDObject *scratchOffset = chunk->FindChild("scratchOffset");
        if(!boxOffset || !scratchOffset ||
           (boxOffset->AsUInt64() != 0 && boxOffset->AsUInt64() != 48) ||
           (scratchOffset->AsUInt64() != 0 && scratchOffset->AsUInt64() != 256) ||
           (!boxOffset->AsUInt64() && !scratchOffset->AsUInt64())) return false;
      }
      if(chunk->name == "MTLAccelerationStructureCommandEncoder::buildBoundingBoxStrided")
      {
        const SDObject *stride = chunk->FindChild("boxStride");
        const SDObject *boxOffset = chunk->FindChild("boxOffset");
        const SDObject *scratchOffset = chunk->FindChild("scratchOffset");
        if(!stride || !boxOffset || !scratchOffset || stride->AsUInt64() != 32 ||
           (boxOffset->AsUInt64() != 0 && boxOffset->AsUInt64() != 48) ||
           (scratchOffset->AsUInt64() != 0 && scratchOffset->AsUInt64() != 256) ||
           ((boxOffset->AsUInt64() == 0) != (scratchOffset->AsUInt64() == 0))) return false;
      }
      if(!e || !as || !v || !s || !count ||
         (count->AsUInt64() != 1 &&
          !(boxed && (count->AsUInt64() == 2 || count->AsUInt64() == 3)) &&
          !(!boxed && count->AsUInt64() == 2)) ||
         e->AsResourceId() != encoder || as->AsResourceId() != structure)
        return false;
      multiBox = boxed && count->AsUInt64() > 1;
      multiTriangle = !boxed && !indexed && count->AsUInt64() == 2;
      multiIndexed = indexed && count->AsUInt64() == 2;
      vertices = v->AsResourceId();
      scratch = s->AsResourceId();
      if(indexed)
      {
        const SDObject *i = chunk->FindChild("indices");
        const SDObject *type = chunk->FindChild("indexType");
        if(!i || !type || type->AsUInt64() != 0) return false;
        indices = i->AsResourceId();
      }
    }
    if(chunk->name ==
       "MTLAccelerationStructureCommandEncoder::writeCompactedAccelerationStructureSize")
    {
      written++;
      const SDObject *e = chunk->FindChild("Encoder");
      const SDObject *as = chunk->FindChild("structure");
      const SDObject *b = chunk->FindChild("buffer");
      const SDObject *offset = chunk->FindChild("offset");
      if(!e || !as || !b || !offset || e->AsResourceId() != encoder ||
         as->AsResourceId() != (copied ? copyDestination : structure) ||
         offset->AsUInt64() != 0)
        return false;
      output = b->AsResourceId();
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::copyAccelerationStructure")
    {
      copied++;
      const SDObject *source = chunk->FindChild("source");
      const SDObject *destination = chunk->FindChild("destination");
      if(!source || !destination || source->AsResourceId() != structure ||
         destination->AsResourceId() != copyDestination)
        return false;
    }
    if(chunk->name == "MTLAccelerationStructureCommandEncoder::endEncoding") ended++;
  }
  if(!created && !begun && !built && !copied && !written && !ended) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "Acceleration structure validation failed: %s\n", message);
    return false;
  };
  if(created != (copied ? 2U : 1U) || begun != 1 || built != 1 || copied > 1 ||
     written != 1 || ended != 1 ||
     structure == ResourceId() || encoder == ResourceId() || vertices == ResourceId() ||
     scratch == ResourceId() || output == ResourceId() ||
     (indexed && indices == ResourceId()) || (copied && copyDestination == ResourceId()))
    return fail("AS chunk chain or identities");
  if((multiBox || multiTriangle || multiIndexed) && !descriptorCreated)
    return fail("multi geometry descriptor allocation");
  bool found = false;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.resourceId == structure)
      found = resource.type == ResourceType::AccelerationStructure;
  if(!found) return fail("AS resource type");
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *build = NULL, *copy = NULL, *write = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->customName == (boxed ? "Build Metal Bounding Box Acceleration Structure" :
        indexed ? "Build Metal Indexed Triangle Acceleration Structure" :
        nonOpaque ? "Build Metal Non-Opaque Triangle Acceleration Structure" :
                  "Build Metal Triangle Acceleration Structure")) build = action;
    if(action->customName == "Write Metal Compacted Acceleration Structure Size") write = action;
    if(action->customName == "Copy Metal Acceleration Structure") copy = action;
  }
  if(!build || !write || !(build->flags & ActionFlags::BuildAccStruct) ||
     write->eventId <= build->eventId || (copied && (!copy ||
     copy->eventId <= build->eventId || copy->eventId >= write->eventId)))
    return fail("AS actions missing or out of order");
  for(uint32_t event : {write->eventId, copied ? copy->eventId : build->eventId,
                        write->eventId})
  {
    renderer->SetFrameEvent(event, true);
    const bytebuf bytes = renderer->GetBufferData(output, 0, 8);
    uint64_t size = 0;
    if(bytes.size() != 8) return fail("GPU compacted-size readback length");
    memcpy(&size, bytes.data(), sizeof(size));
    if(size != (event == write->eventId ? 1280ULL : 0ULL))
      return fail("GPU compacted-size replay/rewind value");
  }
  return true;
}

static bool ValidateTileFixture(IReplayController *renderer, ResourceId colorTarget)
{
  unsigned pipelines = 0, singles = 0, offsets = 0, batches = 0, bytes = 0, dispatches = 0;
  unsigned tileTextures = 0, tileTextureBatches = 0, tileSamplers = 0,
           tileSamplerBatches = 0, clampedSamplers = 0, tileMemory = 0;
  unsigned tileVisibleSingles = 0, tileVisibleBatches = 0, tileHandles = 0,
           tileTables = 0, tileTableWrites = 0;
  ResourceId linkedFunction, tilePipeline, tileTable;
  ResourceId counter, sampledTexture, sampledSampler;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    pipelines += chunk->name == "MTLDevice::newRenderPipelineStateWithTileDescriptor" ||
                 chunk->name == "MTLDevice::newRenderPipelineStateWithTileDescriptor(completionHandler)";
    singles += chunk->name == "MTLRenderCommandEncoder::setTileBuffer";
    offsets += chunk->name == "MTLRenderCommandEncoder::setTileBufferOffset";
    batches += chunk->name == "MTLRenderCommandEncoder::setTileBuffers";
    bytes += chunk->name == "MTLRenderCommandEncoder::setTileBytes";
    dispatches += chunk->name == "MTLRenderCommandEncoder::dispatchThreadsPerTile";
    if(chunk->name == "MTLDevice::newRenderPipelineStateWithTileDescriptor" ||
       chunk->name == "MTLDevice::newRenderPipelineStateWithTileDescriptor(completionHandler)")
    {
      const SDObject *links = chunk->FindChild("visibleFunctions");
      if(links && links->NumChildren() == 1)
      {
        linkedFunction = links->GetChild(0)->AsResourceId();
        const SDObject *id = chunk->FindChild("PipelineState");
        if(id) tilePipeline = id->AsResourceId();
      }
    }
    if(chunk->name == "MTLRenderPipelineState::functionHandleWithFunction")
    {
      const SDObject *stage = chunk->FindChild("stageValue");
      if(stage && stage->AsUInt64() == 4)
      {
        tileHandles++;
        const SDObject *function = chunk->FindChild("function");
        const SDObject *pipeline = chunk->FindChild("Pipeline");
        if(!function || !pipeline || function->AsResourceId() != linkedFunction ||
           pipeline->AsResourceId() != tilePipeline) return false;
      }
    }
    if(chunk->name == "MTLRenderPipelineState::newVisibleFunctionTableWithDescriptor")
    {
      const SDObject *stage = chunk->FindChild("stageValue");
      if(stage && stage->AsUInt64() == 4)
      {
        tileTables++;
        const SDObject *table = chunk->FindChild("Table");
        const SDObject *pipeline = chunk->FindChild("Pipeline");
        if(!table || !pipeline || pipeline->AsResourceId() != tilePipeline)
          return false;
        tileTable = table->AsResourceId();
      }
    }
    tileVisibleSingles += chunk->name == "MTLRenderCommandEncoder::setTileVisibleFunctionTable";
    tileVisibleBatches += chunk->name == "MTLRenderCommandEncoder::setTileVisibleFunctionTables";
    tileTableWrites += chunk->name == "MTLVisibleFunctionTable::setFunction";
    if(chunk->name == "MTLRenderCommandEncoder::setThreadgroupMemoryLength")
    {
      tileMemory++;
      const SDObject *length = chunk->FindChild("length");
      const SDObject *offset = chunk->FindChild("offset");
      const SDObject *index = chunk->FindChild("index");
      if(!length || !offset || !index ||
         length->AsUInt64() != (tileMemory == 2 ? 0 : 16) ||
         offset->AsUInt64() != (tileMemory == 3 ? 16 : 0) || index->AsUInt64() != 0)
        return false;
    }
    tileTextures += chunk->name == "MTLRenderCommandEncoder::setTileTexture";
    tileTextureBatches += chunk->name == "MTLRenderCommandEncoder::setTileTextures";
    tileSamplers += chunk->name == "MTLRenderCommandEncoder::setTileSamplerState";
    tileSamplerBatches += chunk->name == "MTLRenderCommandEncoder::setTileSamplerStates";
    if(chunk->name == "MTLRenderCommandEncoder::setTileTexture")
    {
      const SDObject *list = chunk->FindChild("textures");
      if(list && list->NumChildren() == 1) sampledTexture = list->GetChild(0)->AsResourceId();
    }
    if(chunk->name == "MTLRenderCommandEncoder::setTileSamplerState")
    {
      const SDObject *list = chunk->FindChild("samplers");
      if(list && list->NumChildren() == 1) sampledSampler = list->GetChild(0)->AsResourceId();
    }
    if(chunk->name == "MTLRenderCommandEncoder::setTileSamplerState" ||
       chunk->name == "MTLRenderCommandEncoder::setTileSamplerStates")
    {
      const SDObject *clamps = chunk->FindChild("lodMinClamps");
      clampedSamplers += clamps && clamps->NumChildren() != 0;
    }
    if(chunk->name == "MTLRenderCommandEncoder::setTileBuffer" && counter == ResourceId())
    {
      const SDObject *field = chunk->FindChild("buffer");
      if(field) counter = field->AsResourceId();
    }
  }
  if(!pipelines) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T74 tile pipeline failed: %s\n",message); return false;
  };
  if(pipelines != 1 || singles != 3 || offsets != 1 || batches != 2 ||
     bytes != 3 || dispatches != 3 || counter == ResourceId())
    return fail("pipeline, binding or dispatch chunk counts");
  const bool visible = tileVisibleSingles || tileVisibleBatches;
  if(visible && (linkedFunction == ResourceId() || tilePipeline == ResourceId() ||
                 tileTable == ResourceId() || tileHandles != 1 || tileTables != 1 ||
                 tileTableWrites != 1 || tileVisibleSingles + tileVisibleBatches != 3 ||
                 (tileVisibleBatches != 0 && tileVisibleBatches != 1)))
    return fail("tile visible function table resource graph or bindings");
  const bool sampled = tileTextures || tileTextureBatches || tileSamplers || tileSamplerBatches;
  if(sampled && (tileTextures != 2 || tileTextureBatches != 1 ||
                 tileSamplers != 2 || tileSamplerBatches != 2 || clampedSamplers != 2 ||
                 sampledTexture == ResourceId() || sampledSampler == ResourceId()))
    return fail("tile texture/sampler API variants");
  if(tileMemory && tileMemory != 4)
    return fail("three tile threadgroup-memory bindings and one clear required");
  if(sampled)
  {
    const bytebuf sample = renderer->GetTextureData(sampledTexture, {0, 0, 0});
    if(sample.size() != 4 || sample[0] != 2 || sample[1] != 0 ||
       sample[2] != 0 || sample[3] != 255)
      return fail("sampled tile texture bytes");
  }
  bool bufferFound = false;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.resourceId == counter) bufferFound = buffer.length == 12;
  if(!bufferFound) return fail("12-byte output buffer missing");
  rdcarray<const ActionDescription *> actions, tiles, draws;
  FindActions(renderer->GetRootActions(), actions);
  FindDrawActions(renderer->GetRootActions(), draws);
  for(const ActionDescription *action : actions)
    if((action->flags & ActionFlags::Dispatch) &&
       action->customName.contains("dispatchThreadsPerTile"))
      tiles.push_back(action);
  if(tiles.size() != 3 || draws.size() != 1)
    return fail("three tile actions and final draw");
  uint32_t tileCount = 0;
  for(const TextureDescription &target : renderer->GetTextures())
    if(target.resourceId == colorTarget)
      tileCount = ((target.width + 15) / 16) * ((target.height + 15) / 16);
  if(!tileCount) return fail("swap buffer tile dimensions");
  for(const ActionDescription *action : tiles)
    if(action->dispatchDimension[0] * action->dispatchDimension[1] != tileCount ||
       action->dispatchDimension[2] != 1 ||
       action->dispatchThreadsDimension[0] != 16 ||
       action->dispatchThreadsDimension[1] != 16 ||
       action->dispatchThreadsDimension[2] != 1)
      return fail("tile action dimensions");
  for(unsigned phase : {2U,0U,1U,2U})
  {
    renderer->SetFrameEvent(tiles[phase]->eventId,true);
    const bytebuf data = renderer->GetBufferData(counter,0,12);
    if(data.size() != 12) return fail("tile buffer readback size");
    uint32_t values[3] = {};
    memcpy(values,data.data(),12);
    for(unsigned i = 0; i < 3; i++)
      if(values[i] != (i <= phase ? tileCount * (i + 1 + (visible ? 1 : 0)) *
                                   (sampled ? 2 : 1) : 0))
        return fail("tile output across event seek");
  }
  renderer->SetFrameEvent(draws[0]->eventId,true);
  const auto *state = renderer->GetPipelineState().GetMetalPipelineState();
  if(!state || state->fragmentBuffers.empty() ||
     state->fragmentBuffers[0].resourceId != counter ||
     state->fragmentBuffers[0].byteOffset != 8)
    return fail("final draw buffer binding");
  if(!PixelMatches(renderer,colorTarget,draws[0]->eventId,200,150,
                   std::min(float(tileCount)/(visible ? 384.0f : 512.0f),1.0f),0.0f,0.0f))
    return fail("final draw pixel derived from tile dispatch");
  return true;
}

static bool ValidateMeshFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId pipeline, meshFunction, fragmentFunction;
  unsigned pipelines = 0, draws = 0, threadDraws = 0, indirectDraws = 0, asyncPipelines = 0;
  ResourceId indirectArguments;
  unsigned meshBuffers = 0, meshBytes = 0, meshOffsets = 0, meshBatches = 0;
  unsigned meshTextures = 0, meshTextureBatches = 0, meshSamplerSingles = 0;
  unsigned meshSamplerBatches = 0, meshSamplerClamps = 0, meshSamplerBatchClamps = 0;
  bool halfRate = false;
  bool layeredDraw = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    if(chunk->name == "MTLBlitCommandEncoder::copyFromTexture")
    {
      const SDObject *sourceSlice = chunk->FindChild("sourceSlice");
      layeredDraw |= sourceSlice && sourceSlice->AsUInt64() == 1;
    }
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newRasterizationRateMapWithDescriptor")
    {
      const SDObject *samples = chunk->FindChild("horizontal");
      halfRate = samples && samples->NumChildren() == 2 &&
                 samples->GetChild(0)->AsFloat() == 0.5f;
    }
    if(chunk->name == "MTLDevice::newRenderPipelineStateWithMeshDescriptor" ||
       chunk->name == "MTLDevice::newRenderPipelineStateWithMeshDescriptor(completionHandler)")
    {
      pipelines++;
      asyncPipelines += chunk->name ==
          "MTLDevice::newRenderPipelineStateWithMeshDescriptor(completionHandler)";
      const SDObject *p = chunk->FindChild("PipelineState");
      const SDObject *m = chunk->FindChild("meshFunction");
      const SDObject *f = chunk->FindChild("fragmentFunction");
      const SDObject *supported = chunk->FindChild("supported");
      if(p) pipeline = p->AsResourceId();
      if(m) meshFunction = m->AsResourceId();
      if(f) fragmentFunction = f->AsResourceId();
      if(!supported || !supported->AsBool()) return false;
      if(layeredDraw)
      {
        const SDObject *maxGrid = chunk->FindChild("maxMeshGrid");
        if(!maxGrid || maxGrid->AsUInt64() != 2)
          return false;
      }
      if(asyncPipelines)
      {
        const SDObject *formats = chunk->FindChild("colorFormats");
        const SDObject *options = chunk->FindChild("options");
        if(!formats || formats->NumChildren() != 8 ||
           formats->GetChild(0)->AsUInt64() != MTL::PixelFormatBGRA8Unorm ||
           !options || options->AsUInt64() != (uint32_t)MTL::PipelineOptionArgumentInfo)
        {
          fprintf(stderr, "T82 async mesh descriptor snapshot mismatch: formats=%zu first=%llu options=%llu\n",
                  formats ? formats->NumChildren() : 0,
                  formats && formats->NumChildren() ? (unsigned long long)formats->GetChild(0)->AsUInt64() : 0,
                  options ? (unsigned long long)options->AsUInt64() : 0);
          return false;
        }
      }
    }
    draws += chunk->name == "MTLRenderCommandEncoder::drawMeshThreadgroups" ||
             chunk->name == "MTLRenderCommandEncoder::drawMeshThreads" ||
             chunk->name == "MTLRenderCommandEncoder::drawMeshThreadgroups(indirect)";
    threadDraws += chunk->name == "MTLRenderCommandEncoder::drawMeshThreads";
    if(chunk->name == "MTLRenderCommandEncoder::drawMeshThreadgroups(indirect)")
    {
      indirectDraws++;
      const SDObject *buffer = chunk->FindChild("indirectBuffer");
      const SDObject *offset = chunk->FindChild("indirectBufferOffset");
      if(buffer) indirectArguments = buffer->AsResourceId();
      if(!offset || offset->AsUInt64() != 16) return false;
    }
    meshBuffers += chunk->name == "MTLRenderCommandEncoder::setMeshBuffer";
    meshBytes += chunk->name == "MTLRenderCommandEncoder::setMeshBytes";
    meshOffsets += chunk->name == "MTLRenderCommandEncoder::setMeshBufferOffset";
    meshBatches += chunk->name == "MTLRenderCommandEncoder::setMeshBuffers";
    meshTextures += chunk->name == "MTLRenderCommandEncoder::setMeshTexture";
    meshTextureBatches += chunk->name == "MTLRenderCommandEncoder::setMeshTextures";
    meshSamplerSingles += chunk->name == "MTLRenderCommandEncoder::setMeshSamplerState";
    meshSamplerBatches += chunk->name == "MTLRenderCommandEncoder::setMeshSamplerStates";
    meshSamplerClamps += chunk->name == "MTLRenderCommandEncoder::setMeshSamplerState(lodclamp)";
    meshSamplerBatchClamps += chunk->name == "MTLRenderCommandEncoder::setMeshSamplerStates(lodclamp)";
  }
  if(!pipelines) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T78 mesh pipeline failed: %s\n",message); return false;
  };
  const bool bindings = meshBuffers || meshBytes || meshOffsets || meshBatches;
  const bool sampled = meshTextures || meshTextureBatches || meshSamplerSingles ||
                       meshSamplerBatches || meshSamplerClamps || meshSamplerBatchClamps;
  if(pipelines != 1 || draws != (bindings ? 3U : 1U) ||
     threadDraws + indirectDraws > 1 ||
     (indirectDraws && indirectArguments == ResourceId()) ||
     (bindings && (meshBuffers != 2 || meshBytes != 3 || meshOffsets != 1 || meshBatches != 1)) ||
     (sampled && (!bindings || meshTextures != 2 || meshTextureBatches != 1 ||
                  meshSamplerSingles != 1 || meshSamplerBatches != 1 ||
                  meshSamplerClamps != 1 || meshSamplerBatchClamps != 1)) ||
     pipeline == ResourceId() ||
     meshFunction == ResourceId() || fragmentFunction == ResourceId())
    return fail("pipeline/draw chunks or shader identity");
  rdcarray<const ActionDescription *> actions, meshes;
  FindActions(renderer->GetRootActions(), actions);
  const ActionDescription *writer = NULL;
  for(const ActionDescription *action : actions)
  {
    if(action->flags & ActionFlags::MeshDispatch) meshes.push_back(action);
    if(action->flags & ActionFlags::Dispatch) writer = action;
  }
  if(meshes.size() != draws) return fail("mesh action count");
  if(writer && (!indirectDraws || writer->eventId >= meshes[0]->eventId))
    return fail("GPU indirect argument writer order");
  for(size_t phase = 0; phase < meshes.size(); phase++)
  {
    const ActionDescription *action = meshes[phase];
    if(action->dispatchDimension[0] != (indirectDraws ? 0U : threadDraws ? 32U : layeredDraw ? 2U : 1U) ||
       action->dispatchDimension[1] != (indirectDraws ? 0U : 1U) ||
       action->dispatchDimension[2] != (indirectDraws ? 0U : 1U) ||
       bool(action->flags & ActionFlags::Indirect) != bool(indirectDraws) ||
       action->dispatchThreadsDimension[0] != 32 ||
       action->dispatchThreadsDimension[1] != 1 || action->dispatchThreadsDimension[2] != 1)
      return fail("mesh action dimensions and indirect flag");
    renderer->SetFrameEvent(action->eventId,true);
    const auto *state = renderer->GetPipelineState().GetMetalPipelineState();
    if(!state || state->pipelineResourceId != pipeline ||
       state->fragmentShader.resourceId != fragmentFunction)
      return fail("mesh pipeline or fragment shader binding");
    if(indirectDraws &&
       (state->indirectBuffer.resourceId != indirectArguments ||
        state->indirectBuffer.byteOffset != 16 || state->indirectBuffer.byteSize != 12 ||
        !HasUsage(renderer,indirectArguments,ResourceUsage::Indirect)))
      return fail("mesh indirect argument binding and usage");
    if(writer)
    {
      const bytebuf bytes = renderer->GetBufferData(indirectArguments,16,12);
      static const uint32_t expected[3] = {1,1,1};
      if(bytes.size() != sizeof(expected) ||
         memcmp(bytes.data(),expected,sizeof(expected)) != 0 ||
         !HasUsage(renderer,indirectArguments,ResourceUsage::CS_RWResource))
        return fail("GPU-produced mesh arguments at draw event");
    }
    const int x = halfRate ? 100 : sampled ? 170 + int(phase) * 100 :
                  bindings ? 100 + int(phase) * 100 : 200;
    const ResourceId drawTarget = action->outputs[0] == ResourceId() ?
                                      colorTarget : action->outputs[0];
    if(!PixelMatches(renderer,drawTarget,action->eventId,x,150,0.2f,0.7f,0.3f) ||
       !PixelMatches(renderer,drawTarget,action->eventId,10,10,0.0f,0.0f,0.0f))
      return fail("triangle and cleared background pixels");
    if(halfRate && !PixelMatches(renderer,drawTarget,action->eventId,
                                 175,150,0.0f,0.0f,0.0f))
      return fail("half-rate map did not shrink the physical triangle");
    if(layeredDraw)
    {
      const PixelValue secondLayer = renderer->PickPixel(drawTarget,100,150,
                                                         {0,1,0},CompType::Typeless);
      const PixelValue secondBackground = renderer->PickPixel(drawTarget,175,150,
                                                              {0,1,0},CompType::Typeless);
      if(!Near(secondLayer.floatValue[0],0.2f) ||
         !Near(secondLayer.floatValue[1],0.7f) ||
         !Near(secondLayer.floatValue[2],0.3f) ||
         !Near(secondLayer.floatValue[3],1.0f) ||
         !Near(secondBackground.floatValue[0],0.0f) ||
         !Near(secondBackground.floatValue[1],0.0f) ||
         !Near(secondBackground.floatValue[2],0.0f) ||
         !Near(secondBackground.floatValue[3],1.0f))
        return fail("second rate-map layer did not rasterize into slice 1");
    }
    const int absentX = sampled ? (phase == 0 ? 100 : 170 + int(phase - 1) * 100) :
                        phase == 1 ? 100 : 200;
    if(bindings && !PixelMatches(renderer,colorTarget,action->eventId,
                                 absentX,150,0.0f,0.0f,0.0f))
      return fail("mesh buffer offsets did not move triangle");
  }
  return true;
}

static bool ValidateObjectMeshFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId pipeline, objectFunction, meshFunction, fragmentFunction, objectBuffer;
  ResourceId indirectArguments;
  unsigned pipelines = 0, bufferBindings = 0, bytes = 0, offsets = 0, batches = 0, draws = 0;
  unsigned textures = 0, textureBatches = 0, samplerSingles = 0, samplerBatches = 0;
  unsigned samplerClamps = 0, samplerBatchClamps = 0;
  unsigned objectMemory = 0, indirectDraws = 0, objectThreadGridDraws = 0;
  bool objectThreads4 = false;
  uint64_t directGridWidth = 1;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newRenderPipelineStateWithObjectMeshDescriptor" ||
       chunk->name ==
           "MTLDevice::newRenderPipelineStateWithObjectMeshDescriptor(completionHandler)")
    {
      pipelines++;
      const SDObject *p = chunk->FindChild("PipelineState");
      const SDObject *o = chunk->FindChild("objectFunction");
      const SDObject *m = chunk->FindChild("meshFunction");
      const SDObject *f = chunk->FindChild("fragmentFunction");
      const SDObject *payload = chunk->FindChild("payloadLength");
      const SDObject *grid = chunk->FindChild("maxMeshGrid");
      const SDObject *supported = chunk->FindChild("supported");
      if(p) pipeline = p->AsResourceId();
      if(o) objectFunction = o->AsResourceId();
      if(m) meshFunction = m->AsResourceId();
      if(f) fragmentFunction = f->AsResourceId();
      if(!payload || payload->AsUInt64() != 16 || !grid || grid->AsUInt64() != 1 ||
         !supported || !supported->AsBool()) return false;
      if(chunk->name ==
             "MTLDevice::newRenderPipelineStateWithObjectMeshDescriptor(completionHandler)")
      {
        const SDObject *formats = chunk->FindChild("colorFormats");
        const SDObject *options = chunk->FindChild("options");
        if(!formats || formats->NumChildren() != 8 ||
           formats->GetChild(0)->AsUInt64() != MTL::PixelFormatBGRA8Unorm ||
           !options || options->AsUInt64() != (uint32_t)MTL::PipelineOptionArgumentInfo)
          return false;
      }
    }
    if(chunk->name == "MTLRenderCommandEncoder::setObjectBuffer")
    {
      bufferBindings++;
      const SDObject *buffer = chunk->FindChild("buffer");
      if(buffer) objectBuffer = buffer->AsResourceId();
    }
    bytes += chunk->name == "MTLRenderCommandEncoder::setObjectBytes";
    offsets += chunk->name == "MTLRenderCommandEncoder::setObjectBufferOffset";
    batches += chunk->name == "MTLRenderCommandEncoder::setObjectBuffers";
    textures += chunk->name == "MTLRenderCommandEncoder::setObjectTexture";
    textureBatches += chunk->name == "MTLRenderCommandEncoder::setObjectTextures";
    samplerSingles += chunk->name == "MTLRenderCommandEncoder::setObjectSamplerState";
    samplerBatches += chunk->name == "MTLRenderCommandEncoder::setObjectSamplerStates";
    samplerClamps += chunk->name == "MTLRenderCommandEncoder::setObjectSamplerState(lodclamp)";
    samplerBatchClamps += chunk->name == "MTLRenderCommandEncoder::setObjectSamplerStates(lodclamp)";
    if(chunk->name == "MTLRenderCommandEncoder::setObjectThreadgroupMemoryLength")
    {
      objectMemory++;
      const SDObject *length = chunk->FindChild("length");
      const SDObject *index = chunk->FindChild("index");
      if(!length || length->AsUInt64() != 16 * objectMemory ||
         !index || index->AsUInt64() != 0) return false;
    }
    draws += chunk->name == "MTLRenderCommandEncoder::drawMeshThreadgroups" ||
             chunk->name == "MTLRenderCommandEncoder::drawMeshThreadgroups(indirect)" ||
             chunk->name == "MTLRenderCommandEncoder::drawMeshThreads";
    if(chunk->name == "MTLRenderCommandEncoder::drawMeshThreadgroups" ||
       chunk->name == "MTLRenderCommandEncoder::drawMeshThreads")
    {
      const SDObject *group = chunk->FindChild("threadsPerObjectThreadgroup");
      if(group)
      {
        const SDObject *width = group->FindChild("width");
        if(width) objectThreads4 = width->AsUInt64() == 4;
      }
      const SDObject *grid = chunk->FindChild(
          chunk->name == "MTLRenderCommandEncoder::drawMeshThreads" ?
              "threadsPerGrid" : "threadgroupsPerGrid");
      const SDObject *gridWidth = grid ? grid->FindChild("width") : NULL;
      if(gridWidth) directGridWidth = gridWidth->AsUInt64();
      objectThreadGridDraws += chunk->name == "MTLRenderCommandEncoder::drawMeshThreads";
    }
    if(chunk->name == "MTLRenderCommandEncoder::drawMeshThreadgroups(indirect)")
    {
      indirectDraws++;
      const SDObject *buffer = chunk->FindChild("indirectBuffer");
      const SDObject *offset = chunk->FindChild("indirectBufferOffset");
      if(buffer) indirectArguments = buffer->AsResourceId();
      if(!offset || offset->AsUInt64() != 16) return false;
    }
  }
  if(!pipelines) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T83 object/mesh failed: %s\n",message); return false;
  };
  const bool bindingVariants = bytes || offsets || batches;
  const bool sampled = textures || textureBatches || samplerSingles || samplerBatches ||
                       samplerClamps || samplerBatchClamps;
  if(pipelines != 1 || bufferBindings != (bindingVariants ? 2U : 1U) ||
     draws != (bindingVariants ? 3U : 1U) ||
     (bindingVariants && (bytes != 3 || offsets != 1 || batches != 1)) ||
     (sampled && (!bindingVariants || textures != 2 || textureBatches != 1 ||
                  samplerSingles != 1 || samplerBatches != 1 ||
                  samplerClamps != 1 || samplerBatchClamps != 1)) ||
     (objectMemory && (objectMemory != 3 || !bindingVariants || sampled)) ||
     (indirectDraws && (indirectDraws != 1 || indirectArguments == ResourceId())) ||
     (objectThreadGridDraws && (objectThreadGridDraws != 1 || !objectThreads4)) ||
     pipeline == ResourceId() ||
     objectFunction == ResourceId() || meshFunction == ResourceId() ||
     fragmentFunction == ResourceId() || objectBuffer == ResourceId())
    return fail("pipeline/functions/buffer/draw identity");
  rdcarray<const ActionDescription *> actions, meshes;
  FindActions(renderer->GetRootActions(), actions);
  for(const ActionDescription *action : actions)
    if(action->flags & ActionFlags::MeshDispatch) meshes.push_back(action);
  if(meshes.size() != draws) return fail("mesh dispatch count");
  for(size_t phase = 0; phase < meshes.size(); phase++)
  {
    if(meshes[phase]->dispatchDimension[0] !=
           (indirectDraws ? 0U : directGridWidth) ||
       bool(meshes[phase]->flags & ActionFlags::Indirect) != bool(indirectDraws) ||
       meshes[phase]->dispatchThreadsDimension[0] != 32)
      return fail("mesh dispatch dimensions");
    renderer->SetFrameEvent(meshes[phase]->eventId,true);
    const auto *state = renderer->GetPipelineState().GetMetalPipelineState();
    if(!state || state->pipelineResourceId != pipeline ||
       state->fragmentShader.resourceId != fragmentFunction)
      return fail("pipeline and fragment shader state");
    if(indirectDraws &&
       (state->indirectBuffer.resourceId != indirectArguments ||
        state->indirectBuffer.byteOffset != 16 || state->indirectBuffer.byteSize != 12 ||
        !HasUsage(renderer,indirectArguments,ResourceUsage::Indirect)))
      return fail("object/mesh indirect buffer binding and usage");
    const int sampledX[3] = {120,250,370};
    const int x = sampled ? sampledX[phase] :
                  bindingVariants ? 80 + int(phase) * 140 : 200;
    if(!PixelMatches(renderer,colorTarget,meshes[phase]->eventId,x,150,0.7f,0.2f,0.4f) ||
       !PixelMatches(renderer,colorTarget,meshes[phase]->eventId,10,10,0.0f,0.0f,0.0f))
      return fail("object payload triangle or clear pixel");
    const int absentX = sampled ? (phase == 0 ? 70 : phase == 1 ? 200 : 320) :
                        phase ? 80 + int(phase - 1) * 140 : 200;
    if(bindingVariants && !PixelMatches(renderer,colorTarget,meshes[phase]->eventId,
                                        absentX,
                                        150,0.0f,0.0f,0.0f))
      return fail("object binding offset did not change payload");
  }
  return true;
}

static bool ValidateRateMapFixture(IReplayController *renderer, ResourceId colorTarget)
{
  const SDChunk *creation = NULL, *copy = NULL, *pass = NULL;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLDevice::newRasterizationRateMapWithDescriptor") creation = chunk;
    if(chunk->name == "MTLRasterizationRateMap::copyParameterDataToBuffer") copy = chunk;
    if(chunk->name == "MTLCommandBuffer::renderCommandEncoderWithDescriptor") pass = chunk;
  }
  if(!creation) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T95 rasterization rate map failed: %s\n",message); return false;
  };
  const SDObject *map = creation->FindChild("RateMap");
  const SDObject *size = creation->FindChild("screenSize");
  const SDObject *horizontal = creation->FindChild("horizontal");
  const SDObject *vertical = creation->FindChild("vertical");
  const SDObject *supported = creation->FindChild("supported");
  const SDObject *extraHorizontal = creation->FindChild("extraHorizontal");
  const SDObject *extraVertical = creation->FindChild("extraVertical");
  const bool twoLayers = extraHorizontal && extraHorizontal->NumChildren() == 1;
  const bool halfRate = horizontal && horizontal->NumChildren() == 2 &&
                        horizontal->GetChild(0)->AsFloat() == 0.5f;
  if(!map || map->AsResourceId() == ResourceId() || !size ||
     !size->FindChild("width") || size->FindChild("width")->AsUInt64() != 400 ||
     !size->FindChild("height") || size->FindChild("height")->AsUInt64() != 300 ||
     !horizontal || horizontal->NumChildren() != 2 ||
     horizontal->GetChild(0)->AsFloat() != (halfRate ? 0.5f : 1.0f) ||
     horizontal->GetChild(1)->AsFloat() != (halfRate ? 0.5f : 1.0f) ||
     !vertical || vertical->NumChildren() != 2 ||
     vertical->GetChild(0)->AsFloat() != 1.0f ||
     vertical->GetChild(1)->AsFloat() != 1.0f ||
     !supported || !supported->AsBool() || !copy || !pass)
    return fail("descriptor snapshot or creation/copy/pass chunks");
  if(twoLayers)
  {
    const SDObject *secondH = extraHorizontal->GetChild(0);
    const SDObject *secondV = extraVertical && extraVertical->NumChildren() == 1 ?
                                extraVertical->GetChild(0) : NULL;
    if(!secondH || secondH->NumChildren() != 2 ||
       secondH->GetChild(0)->AsFloat() != 0.5f ||
       secondH->GetChild(1)->AsFloat() != 0.5f ||
       !secondV || secondV->NumChildren() != 2 ||
       secondV->GetChild(0)->AsFloat() != 1.0f ||
       secondV->GetChild(1)->AsFloat() != 1.0f)
      return fail("second layer samples were not preserved");
  }
  const ResourceId mapId = map->AsResourceId();
  const SDObject *copyMap = copy->FindChild("RateMap");
  const SDObject *copyBuffer = copy->FindChild("buffer");
  const SDObject *copyOffset = copy->FindChild("offset");
  const SDObject *descriptor = pass->FindChild("descriptor");
  const SDObject *passMap = descriptor ? descriptor->FindChild("rasterizationRateMap") : NULL;
  const SDObject *passMapId = descriptor ? descriptor->FindChild("rasterizationRateMapId") : NULL;
  const bool boundArrayPass = twoLayers && passMapId && passMapId->AsResourceId() == mapId;
  if(!copyMap || copyMap->AsResourceId() != mapId || !copyBuffer ||
     copyBuffer->AsResourceId() == ResourceId() || !copyOffset ||
     copyOffset->AsUInt64() != 4 || !passMap || !passMapId ||
     (twoLayers ? (passMap->AsResourceId() != passMapId->AsResourceId()) :
                  (passMap->AsResourceId() != mapId ||
                   passMapId->AsResourceId() != mapId)))
    return fail("map identity, parameter buffer, or render-pass reference");
  if(boundArrayPass)
  {
    bool secondSlice = false;
    for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
      if(chunk->name == "MTLBlitCommandEncoder::copyFromTexture")
      {
        const SDObject *slice = chunk->FindChild("sourceSlice");
        secondSlice |= slice && slice->AsUInt64() == 1;
      }
    const SDObject *arrayLength = descriptor->FindChild("renderTargetArrayLength");
    const SDObject *attachments = descriptor->FindChild("colorAttachments");
    const SDObject *firstAttachment = attachments && attachments->NumChildren() ?
                                          attachments->GetChild(0) : NULL;
    const SDObject *textureObject = firstAttachment ? firstAttachment->FindChild("texture") : NULL;
    const ResourceId arrayTarget = textureObject ? textureObject->AsResourceId() : ResourceId();
    bool arrayTexture = false;
    for(const TextureDescription &texture : renderer->GetTextures())
      if(texture.resourceId == arrayTarget && texture.width == 400 &&
         texture.height == 300 && texture.arraysize == 2)
        arrayTexture = true;
    if(!arrayLength || arrayLength->AsUInt64() != 2 || !arrayTexture)
      return fail("two-layer pass array target and map binding");
    rdcarray<const ActionDescription *> actions;
    FindActions(renderer->GetRootActions(),actions);
    const ActionDescription *copyAction = NULL;
    for(const ActionDescription *action : actions)
      if((action->flags & ActionFlags::Copy) && action->copySource == arrayTarget &&
         action->copyDestination == colorTarget)
        copyAction = action;
    if(!copyAction || !PixelMatches(renderer,colorTarget,copyAction->eventId,
                                     secondSlice ? 100 : 200,150,0.2f,0.7f,0.3f) ||
       (secondSlice && !PixelMatches(renderer,colorTarget,copyAction->eventId,
                                     175,150,0.0f,0.0f,0.0f)))
      return fail("selected array layer did not copy to the presented drawable");
  }
  const bytebuf data = renderer->GetBufferData(copyBuffer->AsResourceId(),4,64);
  bool nonzero = false;
  for(byte value : data) nonzero |= value != 0;
  if(data.size() != 64 || !nonzero)
    return fail("parameter data was not copied into the shared buffer");
  return true;
}

static bool ValidateSharedTextureHandleFixture(IReplayController *renderer,
                                               ResourceId colorTarget)
{
  unsigned exports = 0, imports = 0;
  ResourceId source, imported;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLTexture::newSharedTextureHandle")
      exports++;
    if(chunk->name == "MTLDevice::newSharedTextureWithHandle")
    {
      imports++;
      const SDObject *sourceField = chunk->FindChild("Source");
      const SDObject *textureField = chunk->FindChild("Texture");
      if(sourceField) source = sourceField->AsResourceId();
      if(textureField) imported = textureField->AsResourceId();
    }
  }
  if(!exports && !imports) return true;
  auto fail = [](const char *message) {
    fprintf(stderr, "T68 shared texture handle failed: %s\n", message); return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(exports != 1 || imports != 1 || source == ResourceId() ||
     imported == ResourceId() || source == imported || draws.size() != 2)
    return fail("one source export, one imported resource, or two draws");
  unsigned found = 0;
  for(const TextureDescription &desc : renderer->GetTextures())
    found += (desc.resourceId == source || desc.resourceId == imported) &&
             desc.width == 1 && desc.height == 1;
  if(found != 2 || !HasUsage(renderer, imported, ResourceUsage::PS_Resource))
    return fail("source/import texture resources or sampling usage");
  const byte rgb[2][3] = {{20,40,60},{70,90,110}};
  for(unsigned phase : {1U,0U,1U})
  {
    renderer->SetFrameEvent(draws[phase]->eventId, true);
    const auto *state = renderer->GetPipelineState().GetMetalPipelineState();
    if(!state || state->fragmentTextures.empty() || state->fragmentTextures[0] != imported)
      return fail("fragment binding does not reference imported texture");
    if(!PixelMatches(renderer, colorTarget, draws[phase]->eventId, 200, 150,
                     rgb[phase][0]/255.0f, rgb[phase][1]/255.0f, rgb[phase][2]/255.0f))
      return fail("source clear was not visible through import across seek");
  }
  return true;
}

static bool ValidateArgumentDataFixture(IReplayController *renderer, ResourceId colorTarget)
{
  uint32_t selections = 0, members = 0, constants = 0, updates = 0;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
  {
    selections += chunk->metadata.chunkID == 1274;
    members += chunk->metadata.chunkID == 1275;
    constants += chunk->metadata.chunkID == 1276;
    updates += chunk->metadata.chunkID == 1198;
  }
  if(!members) return true;
  bool fixture = false;
  for(const auto &buffer : renderer->GetBuffers())
    fixture |= buffer.length == 621;
  if(!fixture) return true;
  auto fail = [](const char *message) {
    fprintf(stderr,"T57 argument data failed: %s\n",message); return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(),draws);
  if(selections != 2 || members != 16 || constants != 4 || updates != 4 || draws.size() != 4)
    return fail("canonical chunk counts or four draw phases");
  ResourceId inputs[3], outer;
  const uint64_t lengths[] = {117,181,213};
  const uint64_t offsets[2][3] = {{16,32,48},{32,48,64}};
  for(const auto &buffer : renderer->GetBuffers())
  {
    for(size_t i = 0; i < 3; i++)
      if(buffer.length == lengths[i]) inputs[i] = buffer.resourceId;
    if(buffer.length == 621) outer = buffer.resourceId;
  }
  if(outer == ResourceId() || inputs[0] == ResourceId() || inputs[1] == ResourceId() || inputs[2] == ResourceId())
    return fail("argument or member resources");
  ResourceId textures[2], samplers[2];
  const unsigned order[] = {3,0,1,2,0,3};
  for(unsigned d : order)
  {
    renderer->SetFrameEvent(draws[d]->eventId,true);
    const auto &pipe = renderer->GetPipelineState();
    const auto *state = pipe.GetMetalPipelineState();
    const auto *reflection = pipe.GetShaderReflection(ShaderStage::Fragment);
    const unsigned packet = d % 2, phase = d / 2;
    if(!state || !reflection || reflection->entryPoint != "fs_argument_data" ||
       reflection->readOnlyResources.size() != 5 || reflection->samplers.size() != 2 ||
       reflection->constantBlocks.size() != 1 || state->fragmentArgumentBuffers.size() != 1 ||
       state->fragmentBuffers[0].resourceId != outer || state->fragmentBuffers[0].byteOffset != 256+packet*96)
      return fail("reflection or selected packet offset");
    const auto &argument = state->fragmentArgumentBuffers[0];
    if(argument.buffer.resourceId != outer || argument.buffer.byteOffset != 256+packet*96 ||
       argument.textures.size() != 6 || argument.samplers.size() != 8)
      return fail("packet descriptor state");
    if(textures[0] == ResourceId())
      for(unsigned i = 0; i < 2; i++)
      {
        textures[i] = argument.textures[4+(i^packet)];
        samplers[i] = argument.samplers[6+(i^packet)];
      }
    for(unsigned i = 0; i < 2; i++)
      if(argument.textures[4+i] != textures[i^packet] ||
         argument.samplers[6+i] != samplers[i^packet])
        return fail("packet offset switch leaked texture/sampler identities");
    const auto resources = pipe.GetReadOnlyResources(ShaderStage::Fragment,true);
    const auto filters = pipe.GetSamplers(ShaderStage::Fragment,true);
    if(resources.size() != 5 || filters.size() != 2)
      return fail("generic resource descriptor count");
    for(unsigned i = 0; i < 3; i++)
    {
      const auto &resource = resources[i];
      if(resource.access.index != i || resource.access.staticallyUnused ||
         resource.descriptor.type != DescriptorType::Buffer || resource.descriptor.resource != inputs[i] ||
         resource.descriptor.byteOffset != offsets[packet][i] ||
         resource.descriptor.byteSize != lengths[i]-offsets[packet][i] ||
         !HasUsage(renderer,inputs[i],ResourceUsage::PS_Resource))
        return fail("pointer member descriptor offset, size or usage");
      const bytebuf bytes = renderer->GetBufferData(inputs[i],0,0);
      bytebuf expected; expected.resize(lengths[i]); memset(expected.data(),0xa5,expected.size());
      for(unsigned p = 0; p < 2; p++)
      {
        const uint32_t v = 1 + i*3 + p*10 + (i == 0 && p == 0 ? phase : 0);
        const uint32_t values[] = {v,v+1,v+2,0};
        memcpy(expected.data()+offsets[p][i],values,16);
      }
      if(bytes != expected)
      {
        fprintf(stderr,"T57 draw %u member %u bytes %zu expected %zu\n",d,i,bytes.size(),expected.size());
        for(size_t b = 0; b < bytes.size() && b < expected.size(); b++)
          if(bytes[b] != expected[b])
          { fprintf(stderr,"T57 first mismatch at %zu: %u expected %u\n",b,bytes[b],expected[b]); break; }
        return fail("member CPU updates or padding on backward/forward seek");
      }
    }
    for(unsigned i = 0; i < 2; i++)
      if(resources[3+i].descriptor.resource != textures[i^packet] ||
         filters[i].sampler.object != samplers[i^packet] ||
         filters[i].sampler.filter.minify != ((i^packet) ? FilterMode::Linear : FilterMode::Point) ||
         !HasUsage(renderer,textures[i],ResourceUsage::PS_Resource))
        return fail("texture/sampler generic descriptor or usage");
    if(reflection->readOnlyResources[0].name != "packet.single" ||
       reflection->readOnlyResources[0].fixedBindNumber != 0 ||
       reflection->readOnlyResources[1].name != "packet.inputs[0]" ||
       reflection->readOnlyResources[1].fixedBindNumber != 2 ||
       reflection->readOnlyResources[2].name != "packet.inputs[1]" ||
       reflection->readOnlyResources[2].fixedBindNumber != 3 ||
       !HasUsage(renderer,outer,ResourceUsage::PS_Constants))
      return fail("pointer scalar/array member reflection or outer usage");
    const bytebuf bytes = renderer->GetBufferData(outer,0,0);
    if(bytes.size() != 621) return fail("argument buffer readback size");
    for(size_t i = 0; i < bytes.size(); i++)
      if((i < 256 || i >= 448) && bytes[i] != 0xa5)
        return fail("relocation modified surrounding padding");
    for(unsigned p = 0; p < 2; p++)
    {
      const uint32_t bias[] = {p*10,5+p*10,10+p*10,0};
      uint32_t delta = 0; memcpy(&delta,bytes.data()+336+p*96,4);
      if(memcmp(bytes.data()+320+p*96,bias,16) || delta != (phase ? (p ? 11U : 5U) : 0U))
        return fail("initial vector or scalar CPU constant update was overwritten by relocation");
    }
    const float left = phase ? 23.0f : 17.0f;
    const float right = phase ? 68.0f : 57.0f;
    if(!PixelMatches(renderer,colorTarget,draws[d]->eventId,100,150,left/255,(left+10)/255,(left+20)/255) ||
       !PixelMatches(renderer,colorTarget,draws[d]->eventId,300,150,
                     packet ? right/255 : 0,packet ? (right+10)/255 : 0,packet ? (right+20)/255 : 0))
      return fail("four-stage pixel result or replay address relocation");
  }
  fprintf(stdout,"T57 passed pointer arrays/constants/two packets, four stages, CPU updates, padding and rewind\n");
  return true;
}

static bool ValidateArgumentBufferFixture(IReplayController *renderer, ResourceId colorTarget,
                                          const char *savePath)
{
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1)
    return true;

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(state == NULL || state->fragmentArgumentBuffers.empty())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal argument-buffer fixture validation failed: %s\n", message);
    return false;
  };

  if(state->fragmentArgumentBuffers.size() != 1 || state->fragmentBuffers.size() != 1)
    return fail("fragment argument-buffer slot count is wrong");
  const MetalPipe::ArgumentBuffer &argument = state->fragmentArgumentBuffers[0];
  bool nested = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    nested |= chunk->name == "MTLArgumentEncoder::newArgumentEncoderForBufferAtIndex";
  if(nested)
  {
    ResourceId inner, texture;
    for(const BufferDescription &buffer : renderer->GetBuffers())
      if(buffer.length == 16) inner = buffer.resourceId;
    for(const TextureDescription &candidate : renderer->GetTextures())
      if(candidate.width == 4 && candidate.height == 4 &&
         candidate.format.compCount == 4) texture = candidate.resourceId;
    if(argument.buffer.resourceId == ResourceId() || argument.buffer.byteSize != 8 ||
       argument.buffer.resourceId != state->fragmentBuffers[0].resourceId ||
       inner == ResourceId() || texture == ResourceId() ||
       !HasUsage(renderer, argument.buffer.resourceId, ResourceUsage::PS_Constants) ||
       !HasUsage(renderer, inner, ResourceUsage::PS_Resource) ||
       !HasUsage(renderer, texture, ResourceUsage::PS_Resource))
      return fail("T118 nested outer/inner buffer or texture resources missing");
    const bytebuf texels = renderer->GetTextureData(texture, {0, 0, 0});
    const byte firstPixel[] = {248, 40, 24, 255};
    if(texels.size() != 64 || memcmp(texels.data(), firstPixel, 4) != 0)
      return fail("T118 nested texture contents missing");
    if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 75,
                     248.0f / 255.0f, 40.0f / 255.0f, 24.0f / 255.0f) ||
       !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 75,
                     24.0f / 255.0f, 216.0f / 255.0f, 56.0f / 255.0f) ||
       !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 225,
                     32.0f / 255.0f, 72.0f / 255.0f, 248.0f / 255.0f) ||
       !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 225,
                     232.0f / 255.0f, 200.0f / 255.0f, 40.0f / 255.0f))
      return fail("T118 nested argument draw pixels incorrect");
    const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
    if(!clear || !PixelMatches(renderer, colorTarget, clear->eventId, 200, 150,
                               0.025f, 0.035f, 0.055f) ||
       !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 75,
                     248.0f / 255.0f, 40.0f / 255.0f, 24.0f / 255.0f))
      return fail("T118 nested argument seek incorrect");
    fprintf(stdout, "T118 nested argument-buffer GPU pixels and rewind passed\n");
    return true;
  }
  if(argument.textures.size() == 4)
  {
    if(argument.buffer.resourceId == ResourceId() ||
       argument.buffer.resourceId != state->fragmentBuffers[0].resourceId ||
       argument.buffer.byteOffset != 0 || argument.buffer.byteSize == 0 ||
       argument.textures[0] != ResourceId() || argument.textures[1] != ResourceId() ||
       argument.textures[2] == ResourceId() || argument.textures[3] == ResourceId() ||
       argument.textures[2] == argument.textures[3] || argument.samplers.size() != 8 ||
       argument.samplers[6] == ResourceId() || argument.samplers[7] == ResourceId() ||
       argument.samplers[6] == argument.samplers[7])
      return fail("T56 batch ranges or distinct member identities are wrong");
    for(size_t i = 0; i < 6; i++)
      if(argument.samplers[i] != ResourceId())
        return fail("T56 sampler batch modified a member outside its range");
    const ShaderReflection *fragment = pipe.GetShaderReflection(ShaderStage::Fragment);
    if(!fragment || fragment->constantBlocks.size() != 1 ||
       fragment->readOnlyResources.size() != 2 || fragment->samplers.size() != 2)
      return fail("T56 array member reflection is incomplete");
    const auto textures = pipe.GetReadOnlyResources(ShaderStage::Fragment, true);
    const auto samplers = pipe.GetSamplers(ShaderStage::Fragment, true);
    if(textures.size() != 2 || samplers.size() != 2 ||
       !HasUsage(renderer, argument.buffer.resourceId, ResourceUsage::PS_Constants))
      return fail("T56 descriptor counts or outer buffer usage are wrong");
    for(size_t i = 0; i < 2; i++)
    {
      if(fragment->readOnlyResources[i].name !=
             (i ? "arguments.colours[1]" : "arguments.colours[0]") ||
         fragment->readOnlyResources[i].fixedBindNumber != 2 + i ||
         fragment->samplers[i].name != (i ? "arguments.filters[1]" : "arguments.filters[0]") ||
         fragment->samplers[i].fixedBindNumber != 6 + i ||
         textures[i].descriptor.resource != argument.textures[2 + i] ||
         textures[i].access.index != i || textures[i].access.staticallyUnused ||
         samplers[i].sampler.object != argument.samplers[6 + i] ||
         samplers[i].access.index != i || samplers[i].access.staticallyUnused ||
         samplers[i].sampler.filter.minify != (i ? FilterMode::Linear : FilterMode::Point) ||
         samplers[i].sampler.addressU != (i ? AddressMode::Wrap : AddressMode::ClampEdge) ||
         !HasUsage(renderer, argument.textures[2 + i], ResourceUsage::PS_Resource))
        return fail("T56 reflection, generic descriptors or member usage are wrong");
    }
    const bytebuf first = renderer->GetTextureData(argument.textures[2], {0, 0, 0});
    const bytebuf second = renderer->GetTextureData(argument.textures[3], {0, 0, 0});
    const byte firstPixel[] = {248, 40, 24, 255};
    const byte secondPixel[] = {40, 80, 120, 255};
    if(first.size() != 64 || second.size() != 64 || memcmp(first.data(), firstPixel, 4))
      return fail("T56 first texture bytes are wrong");
    for(size_t i = 0; i < second.size(); i += 4)
      if(memcmp(second.data() + i, secondPixel, 4))
        return fail("T56 second texture bytes are wrong");
    const float expected[][3] = {{144, 60, 72}, {32, 148, 88}, {36, 76, 184}, {136, 140, 80}};
    for(size_t i = 0; i < 4; i++)
      if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, i % 2 ? 300 : 100,
                       i / 2 ? 225 : 75, expected[i][0] / 255.0f,
                       expected[i][1] / 255.0f, expected[i][2] / 255.0f))
        return fail("T56 two-member sampling pixel is wrong");
    const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
    if(!clear || !PixelMatches(renderer, colorTarget, clear->eventId, 200, 150,
                               0.025f, 0.035f, 0.055f) ||
       !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 75,
                     144.0f / 255.0f, 60.0f / 255.0f, 72.0f / 255.0f))
      return fail("T56 backward/forward seek is wrong");
    fprintf(stdout, "T56 argument batch: distinct resources, arrays, samplers, pixels and rewind passed\n");
    return true;
  }
  if(argument.buffer.resourceId == ResourceId() ||
     argument.buffer.resourceId != state->fragmentBuffers[0].resourceId ||
     argument.buffer.byteOffset != 0 || argument.buffer.byteSize == 0 ||
     argument.textures.size() != 1 || argument.textures[0] == ResourceId() ||
     argument.samplers.size() != 2 || argument.samplers[1] == ResourceId())
    return fail("buffer, texture id(0), or sampler id(1) member is missing");
  if(GetResourceType(renderer, argument.buffer.resourceId) != ResourceType::Buffer ||
     GetResourceType(renderer, argument.textures[0]) != ResourceType::Texture ||
     GetResourceType(renderer, argument.samplers[1]) != ResourceType::Sampler)
    return fail("argument member resource types are wrong");

  BufferDescription argumentBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.resourceId == argument.buffer.resourceId)
      argumentBuffer = buffer;
  TextureDescription sampledTexture;
  for(const TextureDescription &texture : renderer->GetTextures())
    if(texture.resourceId == argument.textures[0])
      sampledTexture = texture;
  if(argumentBuffer.length != argument.buffer.byteSize || sampledTexture.width != 4 ||
     sampledTexture.height != 4 || sampledTexture.format.compCount != 4 ||
     sampledTexture.format.compByteWidth != 1)
    return fail("argument buffer range or sampled texture description is wrong");
  if(!HasUsage(renderer, argument.buffer.resourceId, ResourceUsage::PS_Constants) ||
     !HasUsage(renderer, argument.textures[0], ResourceUsage::PS_Resource))
    return fail("draw usage does not reference the argument buffer and texture member");

  const ShaderReflection *fragment = pipe.GetShaderReflection(ShaderStage::Fragment);
  if(fragment == NULL || fragment->constantBlocks.size() != 1 ||
     fragment->constantBlocks[0].name != "arguments" ||
     fragment->constantBlocks[0].fixedBindNumber != 0 ||
     fragment->readOnlyResources.size() != 1 || fragment->samplers.size() != 1 ||
     fragment->readOnlyResources[0].name != "arguments.colourTexture" ||
     fragment->readOnlyResources[0].fixedBindNumber != 0 ||
     fragment->samplers[0].name != "arguments.colourSampler" ||
     fragment->samplers[0].fixedBindNumber != 1)
    return fail("fragment reflection does not expose the argument buffer at slot 0");

  const rdcarray<UsedDescriptor> textures =
      pipe.GetReadOnlyResources(ShaderStage::Fragment, true);
  const rdcarray<UsedDescriptor> samplers = pipe.GetSamplers(ShaderStage::Fragment, true);
  if(textures.size() != 1 || textures[0].descriptor.resource != argument.textures[0] ||
     textures[0].access.index != 0 || textures[0].access.staticallyUnused ||
     samplers.size() != 1 || samplers[0].sampler.object != argument.samplers[1] ||
     samplers[0].access.index != 0 || samplers[0].access.staticallyUnused ||
     samplers[0].sampler.filter.minify != FilterMode::Point ||
     samplers[0].sampler.filter.magnify != FilterMode::Point ||
     samplers[0].sampler.addressU != AddressMode::ClampEdge ||
     samplers[0].sampler.addressV != AddressMode::ClampEdge)
    return fail("generic descriptors do not expose the argument texture and sampler members");

  const byte expectedTexture[][4] = {{248, 40, 24, 255}, {24, 216, 56, 255},
                                     {32, 72, 248, 255}, {232, 200, 40, 255}};
  const bytebuf textureBytes = renderer->GetTextureData(argument.textures[0], {0, 0, 0});
  if(textureBytes.size() != 64 || memcmp(textureBytes.data(), expectedTexture[0], 4) != 0 ||
     memcmp(textureBytes.data() + 12, expectedTexture[1], 4) != 0 ||
     memcmp(textureBytes.data() + 48, expectedTexture[2], 4) != 0 ||
     memcmp(textureBytes.data() + 60, expectedTexture[3], 4) != 0)
    return fail("argument texture readback does not match fixed texels");
  if(!ValidateHeadlessThumbnail(renderer, argument.textures[0], draws[0]->eventId, true))
    return fail("argument texture thumbnail is blank");

  if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 75, 248.0f / 255.0f,
                   40.0f / 255.0f, 24.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 75, 24.0f / 255.0f,
                   216.0f / 255.0f, 56.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 225, 32.0f / 255.0f,
                   72.0f / 255.0f, 248.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 225, 232.0f / 255.0f,
                   200.0f / 255.0f, 40.0f / 255.0f))
    return fail("draw output does not match argument-buffer texture quadrants");

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear action is missing");
  renderer->SetFrameEvent(clear->eventId, true);
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 200, 150, 0.025f, 0.035f, 0.055f))
    return fail("rewind to clear did not restore the background");
  if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 75, 248.0f / 255.0f,
                   40.0f / 255.0f, 24.0f / 255.0f))
    return fail("forward seek did not restore the argument-buffer draw");

  if(savePath)
  {
    TextureSave save;
    save.resourceId = argument.textures[0];
    ResultDetails result = renderer->SaveTexture(save, savePath);
    if(!result.OK())
      return fail("argument texture DDS save failed");
    std::ifstream file(savePath, std::ios::binary | std::ios::ate);
    if(!file || file.tellg() != std::streampos(128 + 64))
      return fail("argument texture DDS byte size is wrong");
  }

  return true;
}

static bool ValidateDynamicUniformFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId uniformBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 512)
    {
      uniformBuffer = buffer.resourceId;
      break;
    }
  }

  // Other fixtures do not contain T04's aligned 512-byte uniform buffer.
  if(uniformBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal dynamic uniform fixture validation failed: %s\n", message);
    return false;
  };

  byte expected[512] = {};
  const float left[4] = {1.0f, 0.125f, 0.0625f, 1.0f};
  const float right[4] = {0.0625f, 0.875f, 0.1875f, 1.0f};
  memcpy(expected, left, sizeof(left));
  memcpy(expected + 256, right, sizeof(right));
  const bytebuf data = renderer->GetBufferData(uniformBuffer, 0, 0);
  if(data.size() != sizeof(expected) || memcmp(data.data(), expected, sizeof(expected)) != 0)
    return fail("uniform buffer bytes do not match offsets 0/256");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 2)
    return fail("expected two draw actions");

  for(size_t drawIndex = 0; drawIndex < draws.size(); drawIndex++)
  {
    if((draws[drawIndex]->flags & ActionFlags::Indexed) || draws[drawIndex]->numIndices != 3 ||
       draws[drawIndex]->numInstances != 1)
      return fail("draw metadata does not match the fullscreen triangle");

    renderer->SetFrameEvent(draws[drawIndex]->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const MetalPipe::State *metal = pipe.GetMetalPipelineState();
    const uint64_t expectedOffset = drawIndex == 0 ? 0 : 256;
    const uint64_t expectedSize = 512 - expectedOffset;
    if(metal == NULL || metal->fragmentBuffers.size() != 1 ||
       metal->fragmentBuffers[0].resourceId != uniformBuffer ||
       metal->fragmentBuffers[0].byteOffset != expectedOffset ||
       metal->fragmentBuffers[0].byteSize != expectedSize)
      return fail("event fragment buffer binding/offset does not match");

    const rdcarray<UsedDescriptor> blocks =
        pipe.GetConstantBlocks(ShaderStage::Fragment, false);
    const rdcarray<UsedDescriptor> usedBlocks =
        pipe.GetConstantBlocks(ShaderStage::Fragment, true);
    if(blocks.size() != 1 || usedBlocks.size() != 1 || blocks[0].access.index != 0 ||
       blocks[0].access.type != DescriptorType::ConstantBuffer ||
       blocks[0].access.staticallyUnused || blocks[0].descriptor.resource != uniformBuffer ||
       blocks[0].descriptor.type != DescriptorType::ConstantBuffer ||
       blocks[0].descriptor.byteOffset != expectedOffset ||
       blocks[0].descriptor.byteSize != expectedSize)
      return fail("generic constant buffer descriptor does not match");

    const ShaderReflection *reflection = pipe.GetShaderReflection(ShaderStage::Fragment);
    if(reflection == NULL || reflection->constantBlocks.size() != 1 ||
       reflection->constantBlocks[0].name != "uniforms" ||
       reflection->constantBlocks[0].fixedBindNumber != 0 ||
       reflection->constantBlocks[0].bindArraySize != 1 ||
       reflection->constantBlocks[0].byteSize != 16 ||
       !reflection->constantBlocks[0].bufferBacked)
      return fail("fragment constant block reflection does not match MSL");

    const Viewport viewport = pipe.GetViewport(0);
    const float expectedX = drawIndex == 0 ? 0.0f : 200.0f;
    if(!viewport.enabled || viewport.x != expectedX || viewport.y != 0.0f ||
       viewport.width != 200.0f || viewport.height != 300.0f)
      return fail("draw viewport does not match the left/right split");
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear action is unavailable");

  const float bgR = 0.025f, bgG = 0.035f, bgB = 0.055f;
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 300, 150, bgR, bgG, bgB))
    return fail("clear image does not contain two background halves");
  if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 150, left[0], left[1], left[2]) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 150, bgR, bgG, bgB))
    return fail("first draw did not update only the left half");
  if(!PixelMatches(renderer, colorTarget, draws[1]->eventId, 100, 150, left[0], left[1], left[2]) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 300, 150, right[0], right[1], right[2]))
    return fail("second draw did not preserve left and update right half");
  if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 150, left[0], left[1], left[2]) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 150, bgR, bgG, bgB))
    return fail("rewinding to the first draw did not restore its image");

  return true;
}

static bool ValidateInstancedFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId positionBuffer;
  ResourceId instanceBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 3 * 2 * sizeof(float))
      positionBuffer = buffer.resourceId;
    else if(buffer.length == 4 * 6 * sizeof(float))
      instanceBuffer = buffer.resourceId;
  }

  // Other fixtures do not contain T05's paired 24-byte/96-byte vertex buffers.
  if(positionBuffer == ResourceId() || instanceBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal instanced fixture validation failed: %s\n", message);
    return false;
  };

  static const float expectedPositions[] = {
      -0.18f, -0.18f, 0.18f, -0.18f, 0.0f, 0.22f,
  };
  static const float expectedInstances[] = {
      0.00f, 1.40f, 1.00f, 0.00f, 1.00f, 1.00f,
      -0.55f, 0.00f, 1.00f, 0.125f, 0.0625f, 1.00f,
      0.00f, 0.00f, 0.0625f, 0.875f, 0.1875f, 1.00f,
      0.55f, 0.00f, 0.09375f, 0.25f, 1.00f, 1.00f,
  };
  const bytebuf positions = renderer->GetBufferData(positionBuffer, 0, 0);
  const bytebuf instances = renderer->GetBufferData(instanceBuffer, 0, 0);
  if(positions.size() != sizeof(expectedPositions) ||
     memcmp(positions.data(), expectedPositions, sizeof(expectedPositions)) != 0)
    return fail("position buffer bytes do not match");
  if(instances.size() != sizeof(expectedInstances) ||
     memcmp(instances.data(), expectedInstances, sizeof(expectedInstances)) != 0)
    return fail("instance buffer bytes do not match");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1)
    return fail("expected one draw action");

  const ActionDescription *draw = draws[0];
  if((draw->flags & ActionFlags::Indexed) || !(draw->flags & ActionFlags::Instanced) ||
     draw->numIndices != 3 || draw->numInstances != 3 || draw->vertexOffset != 0 ||
     draw->instanceOffset != 1)
    return fail("instanced action metadata does not match");

  renderer->SetFrameEvent(draw->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(state == NULL || state->vertexAttributes.size() != 3 || state->vertexBuffers.size() != 2)
    return fail("Metal vertex input state shape does not match");

  if(state->vertexAttributes[0].attributeIndex != 0 ||
     state->vertexAttributes[0].bufferIndex != 0 ||
     state->vertexAttributes[0].byteOffset != 0 ||
     state->vertexAttributes[0].format.compType != CompType::Float ||
     state->vertexAttributes[0].format.compByteWidth != 4 ||
     state->vertexAttributes[0].format.compCount != 2 ||
     state->vertexAttributes[1].attributeIndex != 1 ||
     state->vertexAttributes[1].bufferIndex != 1 ||
     state->vertexAttributes[1].byteOffset != 0 ||
     state->vertexAttributes[1].format.compType != CompType::Float ||
     state->vertexAttributes[1].format.compByteWidth != 4 ||
     state->vertexAttributes[1].format.compCount != 2 ||
     state->vertexAttributes[2].attributeIndex != 2 ||
     state->vertexAttributes[2].bufferIndex != 1 ||
     state->vertexAttributes[2].byteOffset != 8 ||
     state->vertexAttributes[2].format.compType != CompType::Float ||
     state->vertexAttributes[2].format.compByteWidth != 4 ||
     state->vertexAttributes[2].format.compCount != 4)
    return fail("Metal attributes do not match the two-buffer descriptor");

  if(state->vertexBuffers[0].resourceId != positionBuffer ||
     state->vertexBuffers[0].byteOffset != 0 || state->vertexBuffers[0].byteSize != 24 ||
     state->vertexBuffers[0].byteStride != 8 || state->vertexBuffers[0].perInstance ||
     state->vertexBuffers[0].stepRate != 1 ||
     state->vertexBuffers[1].resourceId != instanceBuffer ||
     state->vertexBuffers[1].byteOffset != 0 || state->vertexBuffers[1].byteSize != 96 ||
     state->vertexBuffers[1].byteStride != 24 || !state->vertexBuffers[1].perInstance ||
     state->vertexBuffers[1].stepRate != 1)
    return fail("Metal vertex buffer ranges/step modes do not match");

  const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
  const rdcarray<BoundVBuffer> vertexBuffers = pipe.GetVBuffers();
  if(inputs.size() != 3 || vertexBuffers.size() != 2 || inputs[0].name != "attr0" ||
     inputs[0].vertexBuffer != 0 || inputs[0].byteOffset != 0 || inputs[0].perInstance ||
     inputs[0].instanceRate != 1 || inputs[1].name != "attr1" ||
     inputs[1].vertexBuffer != 1 || inputs[1].byteOffset != 0 || !inputs[1].perInstance ||
     inputs[1].instanceRate != 1 || inputs[2].name != "attr2" ||
     inputs[2].vertexBuffer != 1 || inputs[2].byteOffset != 8 || !inputs[2].perInstance ||
     inputs[2].instanceRate != 1 || vertexBuffers[0].resourceId != positionBuffer ||
     vertexBuffers[1].resourceId != instanceBuffer)
    return fail("generic vertex inputs do not preserve per-instance mapping");

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear action is unavailable");

  const float bgR = 0.025f, bgG = 0.035f, bgB = 0.055f;
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 90, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 310, 150, bgR, bgG, bgB))
    return fail("clear image does not contain the expected background");
  if(!PixelMatches(renderer, colorTarget, draw->eventId, 90, 150, 1.0f, 0.125f, 0.0625f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 200, 150, 0.0625f, 0.875f, 0.1875f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 310, 150, 0.09375f, 0.25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 10, 10, bgR, bgG, bgB))
    return fail("instanced replay image does not contain the three expected colours");
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 200, 150, 0.0625f, 0.875f, 0.1875f))
    return fail("clear/draw event seek did not restore the instanced image");

  return true;
}

static bool ValidateMRTBlendFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 6 * 10 * sizeof(float))
    {
      vertexBuffer = buffer.resourceId;
      break;
    }
  }

  // Other fixtures do not contain T06's 240-byte interleaved vertex buffer.
  if(vertexBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal MRT/blend fixture validation failed: %s\n", message);
    return false;
  };

  TextureDescription firstTarget;
  TextureDescription secondTarget;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(texture.resourceId == colorTarget)
      firstTarget = texture;
    else if(!(texture.creationFlags & TextureCategory::SwapBuffer) && texture.width == 400 &&
            texture.height == 300 && texture.depth == 1 && texture.mips == 1 &&
            texture.arraysize == 1 && texture.format.type == ResourceFormatType::Regular &&
            texture.format.compType == CompType::UNorm && texture.format.compByteWidth == 1 &&
            texture.format.compCount == 4 && !texture.format.BGRAOrder())
      secondTarget = texture;
  }

  if(firstTarget.resourceId == ResourceId() || secondTarget.resourceId == ResourceId() ||
     !firstTarget.format.BGRAOrder())
    return fail("BGRA8 swapbuffer/RGBA8 secondary target pair is unavailable");

  static const float expectedVertices[] = {
      -1.0f, -1.0f, 1.0f, 0.125f, 0.0625f, 0.5f, 0.09375f, 0.25f, 1.0f, 0.75f,
      3.0f,  -1.0f, 1.0f, 0.125f, 0.0625f, 0.5f, 0.09375f, 0.25f, 1.0f, 0.75f,
      -1.0f, 3.0f,  1.0f, 0.125f, 0.0625f, 0.5f, 0.09375f, 0.25f, 1.0f, 0.75f,
      -0.45f, -0.45f, 0.0625f, 0.875f, 0.1875f, 0.25f, 0.9375f, 0.8125f, 0.125f, 0.25f,
      0.45f,  -0.45f, 0.0625f, 0.875f, 0.1875f, 0.25f, 0.9375f, 0.8125f, 0.125f, 0.25f,
      0.0f,   0.55f, 0.0625f, 0.875f, 0.1875f, 0.25f, 0.9375f, 0.8125f, 0.125f, 0.25f,
  };
  const bytebuf vertexData = renderer->GetBufferData(vertexBuffer, 0, 0);
  if(vertexData.size() != sizeof(expectedVertices) ||
     memcmp(vertexData.data(), expectedVertices, sizeof(expectedVertices)) != 0)
    return fail("interleaved position/dual-colour vertex bytes do not match");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 2)
    return fail("expected two draw actions");

  for(size_t drawIndex = 0; drawIndex < draws.size(); drawIndex++)
  {
    const ActionDescription *draw = draws[drawIndex];
    if((draw->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
       draw->numIndices != 3 || draw->numInstances != 1 ||
       draw->vertexOffset != drawIndex * 3 || draw->outputs[0] != colorTarget ||
       draw->outputs[1] != secondTarget.resourceId)
      return fail("draw metadata/output slots do not match");

    renderer->SetFrameEvent(draw->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const MetalPipe::State *state = pipe.GetMetalPipelineState();
    const rdcarray<Descriptor> targets = pipe.GetOutputTargets();
    const rdcarray<ColorBlend> blends = pipe.GetColorBlends();
    const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
    const rdcarray<BoundVBuffer> buffers = pipe.GetVBuffers();
    if(state == NULL || targets.size() != 2 || targets[0].resource != colorTarget ||
       targets[1].resource != secondTarget.resourceId || targets[0].format.BGRAOrder() == false ||
       targets[1].format.BGRAOrder() || state->colorBlends.size() != 2 || blends.size() != 2)
      return fail("two output targets/generic blend array do not match");

    if(!blends[0].enabled || blends[0].colorBlend.source != BlendMultiplier::SrcAlpha ||
       blends[0].colorBlend.destination != BlendMultiplier::InvSrcAlpha ||
       blends[0].colorBlend.operation != BlendOperation::Add ||
       blends[0].alphaBlend.source != BlendMultiplier::One ||
       blends[0].alphaBlend.destination != BlendMultiplier::Zero ||
       blends[0].alphaBlend.operation != BlendOperation::Add || blends[0].writeMask != 0xf ||
       blends[1].enabled || blends[1].writeMask != 0x7)
      return fail("per-attachment blend factors/operations/write masks do not match");

    if(inputs.size() != 3 || buffers.size() != 1 || inputs[0].vertexBuffer != 0 ||
       inputs[0].byteOffset != 0 || inputs[0].format.compCount != 2 ||
       inputs[1].vertexBuffer != 0 || inputs[1].byteOffset != 8 ||
       inputs[1].format.compCount != 4 || inputs[2].vertexBuffer != 0 ||
       inputs[2].byteOffset != 24 || inputs[2].format.compCount != 4 ||
       buffers[0].resourceId != vertexBuffer || buffers[0].byteOffset != 0 ||
       buffers[0].byteSize != sizeof(expectedVertices) || buffers[0].byteStride != 40)
      return fail("Float2/Float4/Float4 vertex input does not match");
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL || clear->outputs[0] != colorTarget ||
     clear->outputs[1] != secondTarget.resourceId)
    return fail("clear action does not expose both output slots");

  const uint32_t outsideX = 20, outsideY = 20, centreX = 200, centreY = 150;
  if(!PixelMatchesRGBA(renderer, colorTarget, clear->eventId, outsideX, outsideY, 0.10f, 0.20f,
                       0.30f, 1.0f) ||
     !PixelMatchesRGBA(renderer, secondTarget.resourceId, clear->eventId, outsideX, outsideY,
                       0.02f, 0.04f, 0.06f, 1.0f))
    return fail("clear image does not preserve both attachment clear colours");

  if(!PixelMatchesRGBA(renderer, colorTarget, draws[0]->eventId, centreX, centreY, 0.55f,
                       0.1625f, 0.18125f, 0.5f) ||
     !PixelMatchesRGBA(renderer, secondTarget.resourceId, draws[0]->eventId, centreX, centreY,
                       0.09375f, 0.25f, 1.0f, 1.0f))
    return fail("first draw output does not match blend/write-mask result");

  if(!PixelMatchesRGBA(renderer, colorTarget, draws[1]->eventId, outsideX, outsideY, 0.55f,
                       0.1625f, 0.18125f, 0.5f) ||
     !PixelMatchesRGBA(renderer, colorTarget, draws[1]->eventId, centreX, centreY, 0.428125f,
                       0.340625f, 0.1828125f, 0.25f) ||
     !PixelMatchesRGBA(renderer, secondTarget.resourceId, draws[1]->eventId, outsideX, outsideY,
                       0.09375f, 0.25f, 1.0f, 1.0f) ||
     !PixelMatchesRGBA(renderer, secondTarget.resourceId, draws[1]->eventId, centreX, centreY,
                       0.9375f, 0.8125f, 0.125f, 1.0f))
    return fail("second draw output does not preserve outside/overlap MRT results");

  if(!PixelMatchesRGBA(renderer, colorTarget, draws[0]->eventId, centreX, centreY, 0.55f,
                       0.1625f, 0.18125f, 0.5f) ||
     !PixelMatchesRGBA(renderer, secondTarget.resourceId, draws[0]->eventId, centreX, centreY,
                       0.09375f, 0.25f, 1.0f, 1.0f))
    return fail("rewinding to the first draw did not restore both attachments");

  return true;
}

static bool ValidateDepthStencilFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 15 * 7 * sizeof(float))
    {
      vertexBuffer = buffer.resourceId;
      break;
    }
  }

  // Other fixtures do not contain T07's 420-byte position/colour vertex buffer.
  if(vertexBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal depth/stencil fixture validation failed: %s\n", message);
    return false;
  };

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 5)
    return fail("expected five draw actions");

  static const uint32_t expectedVertices[] = {3, 3, 3, 3, 3};
  static const uint32_t expectedOffsets[] = {0, 3, 6, 9, 12};
  for(size_t i = 0; i < draws.size(); i++)
  {
    if((draws[i]->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
       draws[i]->numIndices != expectedVertices[i] ||
       draws[i]->vertexOffset != expectedOffsets[i] || draws[i]->depthOut == ResourceId())
      return fail("draw metadata/depth output does not match");

    renderer->SetFrameEvent(draws[i]->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const MetalPipe::State *state = pipe.GetMetalPipelineState();
    const DepthTestState depth = pipe.GetDepthTestState();
    const rdcpair<StencilFace, StencilFace> faces = pipe.GetStencilFaces();
    if(state == NULL || state->depthStencil.resourceId == ResourceId() ||
       pipe.GetDepthTarget().resource != draws[i]->depthOut || !pipe.IsStencilTestEnabled())
      return fail("combined depth/stencil target or state is unavailable");

    if(i < 2)
    {
      if(depth.depthFunction != CompareFunction::AlwaysTrue || depth.depthWrites ||
         faces.first.reference != (i == 0 ? 5U : 9U) ||
         faces.second.reference != (i == 0 ? 5U : 9U) ||
         faces.first.function != CompareFunction::AlwaysTrue ||
         faces.first.failOperation != StencilOperation::Zero ||
         faces.first.depthFailOperation != StencilOperation::IncSat ||
         faces.first.passOperation != StencilOperation::Replace || faces.first.compareMask != 0x3f ||
         faces.first.writeMask != 0xff ||
         faces.second.function != CompareFunction::AlwaysTrue ||
         faces.second.failOperation != StencilOperation::Invert ||
         faces.second.depthFailOperation != StencilOperation::DecWrap ||
         faces.second.passOperation != StencilOperation::Replace ||
         faces.second.compareMask != 0x7f || faces.second.writeMask != 0xff)
        return fail("stencil-write front/back state does not match");
    }
    else
    {
      const uint32_t expectedReference = i == 3 ? 9 : 5;
      if(depth.depthFunction != CompareFunction::Less || !depth.depthWrites ||
         faces.first.reference != expectedReference ||
         faces.second.reference != expectedReference ||
         faces.first.function != CompareFunction::Equal ||
         faces.second.function != CompareFunction::Equal || faces.first.compareMask != 0xff ||
         faces.second.compareMask != 0xff || faces.first.writeMask != 0 ||
         faces.second.writeMask != 0 ||
         faces.first.depthFailOperation != StencilOperation::IncSat ||
         faces.second.depthFailOperation != StencilOperation::DecSat)
        return fail("depth/stencil-test state or dynamic reference does not match");
    }
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL || !(clear->flags & ActionFlags::ClearDepthStencil) ||
     clear->depthOut != draws[0]->depthOut)
    return fail("clear action does not expose the combined depth/stencil target");
  const ActionDescription *end = FindNamedAction(renderer->GetRootActions(), "End Metal Render Pass");
  fprintf(stderr, "T07 scope: %s / %s usage=%d,%d\n", clear->customName.c_str(),
          end ? end->customName.c_str() : "missing",
          HasUsageAt(renderer, colorTarget, ResourceUsage::Clear, clear->eventId),
          HasUsageAt(renderer, draws[0]->depthOut, ResourceUsage::Clear, clear->eventId));
  if(clear->customName.find("C0=Clear, D=Clear, S=Clear") == -1 ||
     !(clear->flags & (ActionFlags::PassBoundary | ActionFlags::BeginPass)) || !end ||
     end->customName.find("C0=Store, D=Store, S=Store") == -1 ||
     !(end->flags & (ActionFlags::PassBoundary | ActionFlags::EndPass)) ||
     !HasUsageAt(renderer, colorTarget, ResourceUsage::Clear, clear->eventId) ||
     !HasUsageAt(renderer, draws[0]->depthOut, ResourceUsage::Clear, clear->eventId))
    return fail("render pass load/store labels, boundary flags or clear usage incorrect");

  const float bgR = 0.025f, bgG = 0.035f, bgB = 0.055f;
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 300, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 300, 150, bgR, bgG, bgB))
    return fail("clear/stencil-write events changed the color target");
  if(!PixelMatches(renderer, colorTarget, draws[2]->eventId, 100, 150, 0.0625f, 0.875f,
                   0.1875f) ||
     !PixelMatches(renderer, colorTarget, draws[2]->eventId, 300, 150, bgR, bgG, bgB))
    return fail("ref=5 draw did not update only the left mask");
  if(!PixelMatches(renderer, colorTarget, draws[3]->eventId, 100, 150, 0.0625f, 0.875f,
                   0.1875f) ||
     !PixelMatches(renderer, colorTarget, draws[3]->eventId, 300, 150, 0.09375f, 0.25f, 1.0f))
    return fail("ref=9 draw did not preserve left and update right mask");
  if(!PixelMatches(renderer, colorTarget, draws[4]->eventId, 100, 150, 0.0625f, 0.875f,
                   0.1875f) ||
     !PixelMatches(renderer, colorTarget, draws[4]->eventId, 300, 150, 0.09375f, 0.25f, 1.0f))
    return fail("far red draw did not fail depth and preserve both masks");
  if(!PixelMatches(renderer, colorTarget, draws[2]->eventId, 100, 150, 0.0625f, 0.875f,
                   0.1875f) ||
     !PixelMatches(renderer, colorTarget, draws[2]->eventId, 300, 150, bgR, bgG, bgB))
    return fail("rewinding to the first color draw did not restore its image");

  return true;
}

static bool ValidateMSAAResolveFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 9 * 6 * sizeof(float))
    {
      vertexBuffer = buffer.resourceId;
      break;
    }
  }

  // Other fixtures do not contain T08's 216-byte position/colour vertex buffer.
  if(vertexBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal MSAA resolve fixture validation failed: %s\n", message);
    return false;
  };

  TextureDescription multisampleTarget;
  TextureDescription resolveTarget;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(texture.width != 400 || texture.height != 300)
      continue;

    if(texture.type == TextureType::Texture2DMS && texture.msSamp == 4 &&
       texture.byteSize == 400 * 300 * 4 * 4)
      multisampleTarget = texture;
    if((texture.creationFlags & TextureCategory::SwapBuffer) && texture.msSamp == 1)
      resolveTarget = texture;
  }
  if(multisampleTarget.resourceId == ResourceId() || resolveTarget.resourceId != colorTarget)
    return fail("4x multisample or single-sample resolve texture is unavailable");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 3)
    return fail("expected three draw actions");

  static const uint32_t expectedOffsets[] = {0, 3, 6};
  for(size_t i = 0; i < draws.size(); i++)
  {
    if((draws[i]->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
       draws[i]->numIndices != 3 || draws[i]->vertexOffset != expectedOffsets[i] ||
       draws[i]->outputs[0] != resolveTarget.resourceId)
      return fail("draw metadata or resolved action output does not match");

    renderer->SetFrameEvent(draws[i]->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const MetalPipe::State *state = pipe.GetMetalPipelineState();
    const rdcarray<Descriptor> colorTargets = pipe.GetOutputTargets();
    if(state == NULL || state->sampleCount != 4 || !state->alphaToCoverageEnabled ||
       state->alphaToOneEnabled || colorTargets.size() != 1 ||
       colorTargets[0].resource != multisampleTarget.resourceId ||
       colorTargets[0].textureType != TextureType::Texture2DMS ||
       state->resolveTargets.size() != 1 ||
       state->resolveTargets[0].resource != resolveTarget.resourceId ||
       state->resolveTargets[0].textureType != TextureType::Texture2D)
      return fail("pipeline multisample attachment/resolve state does not match");

    const rdcarray<BoundVBuffer> vertexBuffers = pipe.GetVBuffers();
    if(vertexBuffers.size() != 1 || vertexBuffers[0].resourceId != vertexBuffer ||
       vertexBuffers[0].byteOffset != 0 || vertexBuffers[0].byteSize != 216 ||
       vertexBuffers[0].byteStride != 24)
      return fail("vertex buffer range or stride does not match");
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL || clear->outputs[0] != resolveTarget.resourceId)
    return fail("clear action does not expose the resolve target");
  const ActionDescription *end = FindNamedAction(renderer->GetRootActions(), "End Metal Render Pass");
  fprintf(stderr, "T08 scope: %s / %s usage=%d,%d,%d\n", clear->customName.c_str(),
          end ? end->customName.c_str() : "missing",
          HasUsageAt(renderer, multisampleTarget.resourceId, ResourceUsage::Clear, clear->eventId),
          end && HasUsageAt(renderer, multisampleTarget.resourceId, ResourceUsage::ResolveSrc, end->eventId),
          end && HasUsageAt(renderer, resolveTarget.resourceId, ResourceUsage::ResolveDst, end->eventId));
  if(clear->customName.find("C0=Clear") == -1 || !end ||
     end->customName.find("C0=Resolve") == -1 ||
     !HasUsageAt(renderer, multisampleTarget.resourceId, ResourceUsage::Clear, clear->eventId) ||
     !HasUsageAt(renderer, multisampleTarget.resourceId, ResourceUsage::ResolveSrc, end->eventId) ||
     !HasUsageAt(renderer, resolveTarget.resourceId, ResourceUsage::ResolveDst, end->eventId))
    return fail("MSAA pass load/store labels or resolve usage incorrect");

  const float bgR = 0.03f, bgG = 0.04f, bgB = 0.06f;
  if(!PixelMatches(renderer, resolveTarget.resourceId, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, resolveTarget.resourceId, clear->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, resolveTarget.resourceId, clear->eventId, 300, 150, bgR, bgG, bgB))
    return fail("resolved clear image does not contain the expected background");
  if(!PixelMatches(renderer, resolveTarget.resourceId, draws[0]->eventId, 100, 150, 1.0f,
                   0.125f, 0.0625f) ||
     !PixelMatches(renderer, resolveTarget.resourceId, draws[0]->eventId, 300, 150, bgR, bgG,
                   bgB))
    return fail("first draw did not resolve only the left triangle");
  if(!PixelMatches(renderer, resolveTarget.resourceId, draws[1]->eventId, 100, 150, 1.0f,
                   0.125f, 0.0625f) ||
     !PixelMatches(renderer, resolveTarget.resourceId, draws[1]->eventId, 300, 150, 0.09375f,
                   0.25f, 1.0f))
    return fail("second draw did not resolve both side triangles");
  if(!PixelMatches(renderer, resolveTarget.resourceId, draws[2]->eventId, 200, 150, 0.0625f,
                   0.875f, 0.1875f))
    return fail("third draw did not resolve the centre overlay");
  if(!PixelMatches(renderer, resolveTarget.resourceId, draws[0]->eventId, 100, 150, 1.0f,
                   0.125f, 0.0625f) ||
     !PixelMatches(renderer, resolveTarget.resourceId, draws[0]->eventId, 300, 150, bgR, bgG,
                   bgB))
    return fail("rewinding to the first draw did not restore its resolve image");

  return true;
}

static bool T02PixelIsBackground(const bytebuf &data, uint32_t width, uint32_t x, uint32_t y)
{
  const size_t offset = (size_t(y) * width + x) * 4;
  return offset + 3 < data.size() && data[offset + 0] == 0x0e && data[offset + 1] == 0x09 &&
         data[offset + 2] == 0x06 && data[offset + 3] == 0xff;
}

static bool ValidateIndexedEventImage(IReplayController *renderer, ResourceId texture,
                                      const TextureDescription &desc, uint32_t eventId,
                                      bool expectLeftBackground, bool expectRightBackground)
{
  renderer->SetFrameEvent(eventId, true);
  bytebuf data = renderer->GetTextureData(texture, {0, 0, 0});
  if(desc.width != 400 || desc.height != 300 || data.size() != size_t(400 * 300 * 4))
    return false;

  const bool leftBackground = T02PixelIsBackground(data, desc.width, 100, 150);
  const bool rightBackground = T02PixelIsBackground(data, desc.width, 300, 150);
  return leftBackground == expectLeftBackground && rightBackground == expectRightBackground;
}

static bool ValidateIndexedFixture(IReplayController *renderer, ResourceId colorTarget,
                                   const TextureDescription &desc)
{
  auto fail = [](const char *message) {
    fprintf(stderr, "Metal indexed fixture validation failed: %s\n", message);
    return false;
  };

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.empty() || !(draws[0]->flags & ActionFlags::Indexed) ||
     draws[0]->numIndices != 36)
    return true;

  if(draws.size() != 2)
    return fail("expected two draw actions");

  for(size_t drawIndex = 0; drawIndex < draws.size(); drawIndex++)
  {
    const ActionDescription *draw = draws[drawIndex];
    if(!(draw->flags & ActionFlags::Indexed) || draw->numIndices != 36 ||
       draw->numInstances != 1 || draw->indexOffset != 0)
      return fail("indexed action metadata does not match");

    renderer->SetFrameEvent(draw->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    if(pipe.GetPrimitiveTopology() != Topology::TriangleList ||
       pipe.GetDepthTarget().resource == ResourceId())
    {
      fprintf(stderr, "T02 state: topology=%u depthPresent=%d\n",
              (uint32_t)pipe.GetPrimitiveTopology(),
              pipe.GetDepthTarget().resource != ResourceId() ? 1 : 0);
      return fail("indexed action pipeline topology/depth target does not match");
    }

    const MetalPipe::State *state = pipe.GetMetalPipelineState();
    if(state == NULL)
      return fail("Metal pipeline state is unavailable");
    if(state->vertexAttributes.size() != 2 || state->vertexBuffers.size() != 1)
      return fail("vertex descriptor shape does not match");
    if(state->vertexAttributes[0].attributeIndex != 0 ||
       state->vertexAttributes[0].bufferIndex != 0 ||
       state->vertexAttributes[0].byteOffset != 0 ||
       state->vertexAttributes[0].format.type != ResourceFormatType::Regular ||
       state->vertexAttributes[0].format.compType != CompType::Float ||
       state->vertexAttributes[0].format.compByteWidth != 4 ||
       state->vertexAttributes[0].format.compCount != 3 ||
       state->vertexAttributes[1].attributeIndex != 1 ||
       state->vertexAttributes[1].bufferIndex != 0 ||
       state->vertexAttributes[1].byteOffset != 12 ||
       state->vertexAttributes[1].format.compType != CompType::Float ||
       state->vertexAttributes[1].format.compByteWidth != 4 ||
       state->vertexAttributes[1].format.compCount != 4)
      return fail("vertex attributes do not match Float3/Float4 interleaved input");

    const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
    if(inputs.size() != 2 || inputs[0].name != "attr0" || inputs[0].vertexBuffer != 0 ||
       inputs[0].byteOffset != 0 || inputs[0].perInstance || inputs[0].instanceRate != 1 ||
       inputs[0].format.compType != CompType::Float || inputs[0].format.compByteWidth != 4 ||
       inputs[0].format.compCount != 3 || !inputs[0].used || inputs[0].genericEnabled ||
       inputs[1].name != "attr1" || inputs[1].vertexBuffer != 0 ||
       inputs[1].byteOffset != 12 || inputs[1].perInstance || inputs[1].instanceRate != 1 ||
       inputs[1].format.compType != CompType::Float || inputs[1].format.compByteWidth != 4 ||
       inputs[1].format.compCount != 4 || !inputs[1].used || inputs[1].genericEnabled)
      return fail("generic vertex inputs do not match the Metal descriptor");
    if(state->vertexBuffers[0].resourceId == ResourceId() ||
       state->vertexBuffers[0].byteOffset != 0 || state->vertexBuffers[0].byteSize != 224 ||
       state->vertexBuffers[0].byteStride != 28 || state->vertexBuffers[0].perInstance ||
       state->vertexBuffers[0].stepRate != 1)
      return fail("vertex buffer layout does not match");

    const BoundVBuffer indexBuffer = pipe.GetIBuffer();
    const uint32_t expectedIndexStride = drawIndex == 0 ? 2 : 4;
    const uint64_t expectedIndexSize = drawIndex == 0 ? 72 : 144;
    if(indexBuffer.resourceId == ResourceId() || indexBuffer.byteOffset != 0 ||
       indexBuffer.byteStride != expectedIndexStride || indexBuffer.byteSize != expectedIndexSize)
      return fail("indexed draw buffer binding does not match");

    const DepthTestState depth = pipe.GetDepthTestState();
    if(state->depthStencil.resourceId == ResourceId() || !depth.depthEnable ||
       !depth.depthWrites || depth.depthFunction != CompareFunction::Less)
      return fail("depth state does not match less/write");

    const RasterState raster = pipe.GetRasterState();
    const Viewport viewport = pipe.GetViewport(0);
    const Scissor scissor = pipe.GetScissor(0);
    const float expectedX = drawIndex == 0 ? 0.0f : 200.0f;
    if(raster.cullMode != CullMode::Back || !raster.frontCCW || !viewport.enabled ||
       viewport.x != expectedX || viewport.y != 0.0f || viewport.width != 200.0f ||
       viewport.height != 300.0f || viewport.minDepth != 0.0f || viewport.maxDepth != 1.0f ||
       !scissor.enabled || scissor.x != (int32_t)expectedX || scissor.y != 0 ||
       scissor.width != 200 || scissor.height != 300)
    {
      fprintf(stderr,
              "T02 raster[%zu]: cull=%u ccw=%d vp=%d %.1f %.1f %.1f %.1f %.1f %.1f "
              "sc=%d %d %d %d %d\n",
              drawIndex, (uint32_t)raster.cullMode, raster.frontCCW ? 1 : 0,
              viewport.enabled ? 1 : 0, viewport.x, viewport.y, viewport.width, viewport.height,
              viewport.minDepth, viewport.maxDepth, scissor.enabled ? 1 : 0, scissor.x, scissor.y,
              scissor.width, scissor.height);
      return fail("viewport/scissor/cull/front-face state does not match");
    }
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear action is unavailable");
  if(!ValidateIndexedEventImage(renderer, colorTarget, desc, clear->eventId, true, true))
    return fail("clear event image does not contain two background halves");
  if(!ValidateIndexedEventImage(renderer, colorTarget, desc, draws[0]->eventId, false, true))
    return fail("UInt16 draw event did not update only the left half");
  if(!ValidateIndexedEventImage(renderer, colorTarget, desc, draws[1]->eventId, false, false))
    return fail("UInt32 draw event did not preserve left and update right half");
  if(!ValidateIndexedEventImage(renderer, colorTarget, desc, draws[0]->eventId, false, true))
    return fail("rewinding to the UInt16 draw did not restore its event image");

  static const uint16_t expected16[] = {
      0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 1, 5, 6, 1, 6, 2,
      4, 0, 3, 4, 3, 7, 3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
  };
  static constexpr size_t indexCount = sizeof(expected16) / sizeof(expected16[0]);
  uint32_t expected32[indexCount] = {};
  for(size_t i = 0; i < indexCount; i++)
    expected32[i] = expected16[i];

  bool found16 = false;
  bool found32 = false;
  bool foundVertices = false;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    bytebuf data = renderer->GetBufferData(buffer.resourceId, 0, 0);
    if(buffer.length == sizeof(expected16))
      found16 = data.size() == sizeof(expected16) &&
                memcmp(data.data(), expected16, sizeof(expected16)) == 0;
    else if(buffer.length == sizeof(expected32))
      found32 = data.size() == sizeof(expected32) &&
                memcmp(data.data(), expected32, sizeof(expected32)) == 0;
    else if(buffer.length == 8 * (3 + 4) * sizeof(float))
      foundVertices = data.size() == buffer.length;
  }

  if(!found16)
    return fail("UInt16 index bytes do not match");
  if(!found32)
    return fail("UInt32 index bytes do not match");
  if(!foundVertices)
    return fail("vertex buffer bytes are unavailable");
  return true;
}

static bool ValidateIndirectFixture(IReplayController *renderer, ResourceId colorTarget,
                                    const char *savePath)
{
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.empty() || !draws[0]->customName.contains("drawPrimitives(indirect"))
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal indirect fixture validation failed: %s\n", message);
    return false;
  };

  if(draws.size() != 1)
    return fail("expected one indirect draw action");
  const ActionDescription *draw = draws[0];
  if(!(draw->flags & ActionFlags::Instanced) || (draw->flags & ActionFlags::Indexed) ||
     draw->numIndices != 3 || draw->numInstances != 2 || draw->vertexOffset != 1 ||
     draw->instanceOffset != 1)
    return fail("indirect action metadata does not match all four argument fields");

  renderer->SetFrameEvent(draw->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(state == NULL || pipe.GetPrimitiveTopology() != Topology::TriangleList ||
     state->vertexAttributes.size() != 3 || state->vertexBuffers.size() != 2)
    return fail("indirect draw pipeline topology or vertex input shape is wrong");

  const MetalPipe::BufferBinding &indirect = state->indirectBuffer;
  if(indirect.resourceId == ResourceId() || indirect.byteOffset != 16 ||
     indirect.byteSize != 16)
    return fail("indirect buffer binding does not expose the exact argument subrange");

  BufferDescription description;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.resourceId == indirect.resourceId)
      description = buffer;
  if(description.resourceId == ResourceId() || description.length != 48 ||
     !(description.creationFlags & BufferCategory::Indirect))
    return fail("indirect buffer description is missing its category or full length");
  if(!HasUsage(renderer, indirect.resourceId, ResourceUsage::Indirect))
    return fail("indirect argument usage is missing");

  static const uint32_t expected[] = {3, 2, 1, 1};
  const bytebuf arguments = renderer->GetBufferData(indirect.resourceId, 16, 16);
  if(arguments.size() != sizeof(expected) ||
     memcmp(arguments.data(), expected, sizeof(expected)) != 0)
    return fail("indirect argument bytes do not match 3/2/1/1");

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear action is unavailable");
  const float bgR = 0.025f, bgG = 0.035f, bgB = 0.055f;
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 300, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 100, 150, 1.0f, 0.125f, 0.0625f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 300, 150, 0.09375f, 0.25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 300, 150, 0.09375f, 0.25f, 1.0f))
    return fail("clear/draw/clear/draw event seek or indirect output pixels are wrong");

  if(savePath)
  {
    std::ofstream file(savePath, std::ios::binary | std::ios::trunc);
    file.write((const char *)arguments.data(), arguments.size());
    file.close();
    std::ifstream saved(savePath, std::ios::binary | std::ios::ate);
    if(!saved || saved.tellg() != std::streampos(sizeof(expected)))
      return fail("indirect argument raw save did not contain exactly 16 bytes");
  }

  return true;
}

static bool ValidateICBFixture(IReplayController *renderer, ResourceId colorTarget,
                               const char *savePath)
{
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1 || !draws[0]->customName.contains("ICB[0] drawPrimitives"))
    return true;

  const ActionDescription *execute = NULL;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  for(const ActionDescription *action : actions)
    if(action->customName.contains("executeCommandsInBuffer(location="))
      execute = action;
  auto fail = [](const char *message) {
    fprintf(stderr, "Metal ICB validation failed: %s\n", message);
    return false;
  };
  if(!execute || !execute->customName.contains("location=0, length=1") ||
     !(execute->flags & ActionFlags::MultiAction) || execute->children.size() != 1 ||
     execute->children[0].eventId != draws[0]->eventId)
    return fail("ICB execute marker or its range is missing");
  if(draws.size() != 1 || !draws[0]->customName.contains("ICB[0] drawPrimitives(3)") ||
     draws[0]->eventId != execute->eventId + 1 ||
     !(draws[0]->flags & ActionFlags::Indirect) ||
     (draws[0]->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
     draws[0]->vertexOffset != 1 || draws[0]->numIndices != 3 ||
     draws[0]->numInstances != 1 || draws[0]->outputs[0] != colorTarget)
    return fail("execute/draw actions or range are wrong");

  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 152)
      vertexBuffer = buffer.resourceId;
  if(vertexBuffer == ResourceId())
    return fail("152-byte vertex buffer is missing");

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  const rdcarray<BoundVBuffer> vbs = pipe.GetVBuffers();
  const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
  if(!state || pipe.GetPrimitiveTopology() != Topology::TriangleList ||
     pipe.GetGraphicsPipelineObject() == ResourceId() ||
     vbs.size() != 1 || vbs[0].resourceId != vertexBuffer ||
     vbs[0].byteOffset != 16 || vbs[0].byteSize != 136 || vbs[0].byteStride != 24 ||
     inputs.size() != 2 || inputs[0].byteOffset != 0 || inputs[1].byteOffset != 8 ||
     !HasUsage(renderer, vertexBuffer, ResourceUsage::VertexBuffer))
    return fail("draw IA/Mesh state or vertex usage is wrong");

  ResourceId icb;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.name.contains("Indirect Command Buffer"))
      icb = resource.resourceId;
  if(icb == ResourceId() || !HasUsage(renderer, icb, ResourceUsage::Indirect))
    return fail("ICB resource or indirect usage is missing");

  const bytebuf bytes = renderer->GetBufferData(vertexBuffer, 0, 0);
  const uint32_t prefix[] = {0x13572468, 0x24681357, 0x89abcdef, 0xfedcba98};
  const uint32_t suffix[] = {0x10203040, 0x50607080, 0x90a0b0c0, 0xd0e0f000};
  const float first[] = {2.0f, 2.0f};
  const float selected[] = {-0.6f, -0.5f};
  if(bytes.size() != 152 || memcmp(bytes.data(), prefix, sizeof(prefix)) != 0 ||
     memcmp(bytes.data() + 16, first, sizeof(first)) != 0 ||
     memcmp(bytes.data() + 40, selected, sizeof(selected)) != 0 ||
     memcmp(bytes.data() + 136, suffix, sizeof(suffix)) != 0 ||
     renderer->GetBufferData(vertexBuffer, 152, 4).size() != 0)
    return fail("raw vertex bytes or sentinels are wrong");

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(!clear || !PixelMatches(renderer, colorTarget, clear->eventId, 200, 150,
                             .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, execute->eventId, 200, 150,
                   1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 200, 150,
                   1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 20, 20,
                   .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 200, 150,
                   .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 200, 150,
                   1.0f, .125f, .0625f))
    return fail("clear/draw/rewind pixels are wrong");

  if(savePath)
  {
    std::ofstream saved(savePath, std::ios::binary);
    saved.write((const char *)bytes.data(), bytes.size());
    if(!saved)
      return fail("raw vertex export failed");
  }
  return true;
}

static bool ValidateInheritPipelineFixture(IReplayController *renderer, ResourceId colorTarget,
                                           const char *savePath)
{
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 2 || !draws[0]->customName.contains("ICB[0] drawPrimitives(3)") ||
     !draws[1]->customName.contains("ICB[0] drawPrimitives(3)"))
    return true;
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 24)
      vertexBuffer = buffer.resourceId;
  if(vertexBuffer == ResourceId())
    return true;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  rdcarray<const ActionDescription *> markers;
  for(const ActionDescription *action : actions)
    if(action->customName.contains("executeCommandsInBuffer(location=0, length=1)"))
      markers.push_back(action);
  if(markers.size() != 2 || draws[0]->eventId != markers[0]->eventId + 1 ||
     draws[1]->eventId != markers[1]->eventId + 1 ||
     !(draws[0]->flags & ActionFlags::Indirect) ||
     !(draws[1]->flags & ActionFlags::Indirect) ||
     !HasUsage(renderer, vertexBuffer, ResourceUsage::VertexBuffer))
    return false;
  ResourceId pipelines[2] = {};
  for(unsigned i = 0; i < 2; ++i)
  {
    renderer->SetFrameEvent(draws[i]->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const rdcarray<BoundVBuffer> vbs = pipe.GetVBuffers();
    pipelines[i] = pipe.GetGraphicsPipelineObject();
    if(pipelines[i] == ResourceId() || vbs.size() != 1 ||
       vbs[0].resourceId != vertexBuffer || vbs[0].byteOffset != 0 ||
       vbs[0].byteStride != 8)
      return false;
  }
  if(pipelines[0] == pipelines[1]) return false;
  ResourceId icb;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.name.contains("Indirect Command Buffer")) icb = resource.resourceId;
  if(icb == ResourceId() || !HasUsage(renderer, icb, ResourceUsage::Indirect))
    return false;
  const bytebuf bytes = renderer->GetBufferData(vertexBuffer, 0, 0);
  const float first[] = {-0.22f, -0.40f};
  if(bytes.size() != 24 || memcmp(bytes.data(), first, sizeof(first)) != 0 ||
     !renderer->GetBufferData(vertexBuffer, 24, 4).empty())
    return false;
  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(!clear || !PixelMatches(renderer, colorTarget, clear->eventId, 110, 150,
                             .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, markers[0]->eventId, 110, 150,
                   1, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 110, 150,
                   1, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 290, 150,
                   .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 290, 150,
                   .0625f, .125f, 1) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 110, 150,
                   .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 110, 150,
                   1, .125f, .0625f))
    return false;
  if(savePath)
  {
    std::ofstream saved(savePath, std::ios::binary);
    saved.write((const char *)bytes.data(), bytes.size());
    if(!saved) return false;
  }
  return true;
}

static bool ValidateInheritBuffersFixture(IReplayController *renderer, ResourceId colorTarget,
                                          const char *savePath)
{
  auto fail = [](const char *message) {
    fprintf(stderr, "T27 inherited buffers validation failed: %s\n", message);
    return false;
  };
  ResourceId buffers[2] = {};
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 104)
    {
      const bytebuf bytes = renderer->GetBufferData(buffer.resourceId, 0, 4);
      uint32_t prefix = 0;
      if(bytes.size() == 4) memcpy(&prefix, bytes.data(), 4);
      if(prefix == 0x27000000) buffers[0] = buffer.resourceId;
      if(prefix == 0x27010000) buffers[1] = buffer.resourceId;
    }
  if(buffers[0] == ResourceId() && buffers[1] == ResourceId()) return true;
  if(buffers[0] == ResourceId() || buffers[1] == ResourceId()) return fail("resource ID");
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  rdcarray<const ActionDescription *> markers;
  for(const ActionDescription *action : actions)
    if(action->customName.contains("executeCommandsInBuffer(location=0, length=1)"))
      markers.push_back(action);
  if(draws.size() != 2 || markers.size() != 2 ||
     draws[0]->eventId != markers[0]->eventId + 1 ||
     draws[1]->eventId != markers[1]->eventId + 1 ||
     !draws[0]->customName.contains("ICB[0] drawPrimitives(3)") ||
     !draws[1]->customName.contains("ICB[0] drawPrimitives(3)"))
    return fail("actions");
  ResourceId pipeline;
  for(unsigned i = 0; i < 2; ++i)
  {
    if(!(draws[i]->flags & ActionFlags::Indirect) ||
       !HasUsage(renderer, buffers[i], ResourceUsage::VertexBuffer))
      return fail("usage");
    renderer->SetFrameEvent(draws[i]->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const rdcarray<BoundVBuffer> vbs = pipe.GetVBuffers();
    if(pipe.GetGraphicsPipelineObject() == ResourceId() || vbs.size() != 1 ||
       vbs[0].resourceId != buffers[i] || vbs[0].byteOffset != 16 ||
       vbs[0].byteSize != 88 || vbs[0].byteStride != 24)
      return fail("IA");
    if(i == 0) pipeline = pipe.GetGraphicsPipelineObject();
    else if(pipe.GetGraphicsPipelineObject() != pipeline) return fail("pipeline");
  }
  ResourceId icb;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.name.contains("Indirect Command Buffer")) icb = resource.resourceId;
  if(icb == ResourceId() || !HasUsage(renderer, icb, ResourceUsage::Indirect))
    return fail("ICB usage");
  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(!clear || !PixelMatches(renderer, colorTarget, clear->eventId, 110, 150,
                             .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, markers[0]->eventId, 110, 150,
                   1, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 110, 150,
                   1, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 290, 150,
                   .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 290, 150,
                   .0625f, .125f, 1) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 110, 150,
                   .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 110, 150,
                   1, .125f, .0625f))
    return fail("pixels");
  if(savePath)
  {
    std::ofstream saved(savePath, std::ios::binary);
    for(ResourceId buffer : buffers)
    {
      const bytebuf bytes = renderer->GetBufferData(buffer, 0, 0);
      if(bytes.size() != 104 || renderer->GetBufferData(buffer, 104, 4).size() != 0)
        return false;
      saved.write((const char *)bytes.data(), bytes.size());
    }
    if(!saved) return false;
  }
  return true;
}

static bool ValidateMultiICBFixture(IReplayController *renderer, ResourceId colorTarget,
                                    const char *savePath)
{
  rdcarray<ResourceId> buffers;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 104)
    {
      const bytebuf bytes = renderer->GetBufferData(buffer.resourceId, 0, 4);
      uint32_t prefix = 0;
      if(bytes.size() == 4)
        memcpy(&prefix, bytes.data(), 4);
      if((prefix & 0xff000000U) == 0x22000000U)
        buffers.push_back(buffer.resourceId);
    }
  if(buffers.empty())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal multi-command ICB validation failed: %s\n", message);
    return false;
  };
  if(buffers.size() != 3)
    return fail("expected three 104-byte vertex packets");

  const ActionDescription *execute = NULL;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  for(const ActionDescription *action : actions)
    if(action->customName.contains("executeCommandsInBuffer(location="))
      execute = action;
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(!execute || !execute->customName.contains("location=1, length=2") || draws.size() != 2 ||
     !(execute->flags & ActionFlags::MultiAction) || execute->children.size() != 2 ||
     !draws[0]->customName.contains("ICB[1] drawPrimitives(3)") ||
     !draws[1]->customName.contains("ICB[2] drawPrimitives(3)") ||
     draws[0]->eventId != execute->eventId + 1 ||
     draws[1]->eventId != draws[0]->eventId + 1)
    return fail("execute marker or ordered draw actions are wrong");
  fprintf(stderr, "T22 ICB EIDs execute=%u child1=%u child2=%u\n",
          execute->eventId, draws[0]->eventId, draws[1]->eventId);

  ResourceId vertexBuffers[3] = {};
  const uint32_t prefixes[] = {0x22000000, 0x22010000, 0x22020000};
  const uint32_t suffixes[] = {0x22ff0000, 0x22ff0100, 0x22ff0200};
  for(ResourceId id : buffers)
  {
    const bytebuf bytes = renderer->GetBufferData(id, 0, 0);
    if(bytes.size() != 104)
      return fail("vertex packet byte size is wrong");
    uint32_t prefix = 0;
    uint32_t suffix = 0;
    memcpy(&prefix, bytes.data(), 4);
    memcpy(&suffix, bytes.data() + 88, 4);
    bool matched = false;
    for(size_t i = 0; i < 3; ++i)
      if(prefix == prefixes[i] && suffix == suffixes[i])
      {
        vertexBuffers[i] = id;
        matched = true;
      }
    if(!matched || renderer->GetBufferData(id, 104, 4).size() != 0)
      return fail("vertex packet sentinels or range are wrong");
  }
  if(vertexBuffers[0] == ResourceId() || vertexBuffers[1] == ResourceId() ||
     vertexBuffers[2] == ResourceId())
    return fail("one or more command vertex buffers are missing");

  ResourceId icb;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.name.contains("Indirect Command Buffer"))
      icb = resource.resourceId;
  if(icb == ResourceId() || !HasUsage(renderer, icb, ResourceUsage::Indirect))
    return fail("ICB resource or indirect usage is missing");

  for(size_t i = 0; i < 2; ++i)
  {
    const ActionDescription *draw = draws[i];
    if(!(draw->flags & ActionFlags::Indirect) ||
       (draw->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
       draw->numIndices != 3 || draw->numInstances != 1 || draw->vertexOffset != 0 ||
       draw->outputs[0] != colorTarget ||
       !HasUsage(renderer, vertexBuffers[i + 1], ResourceUsage::VertexBuffer))
      return fail("draw metadata, output or vertex usage is wrong");
    renderer->SetFrameEvent(draw->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const rdcarray<BoundVBuffer> vbs = pipe.GetVBuffers();
    const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
    if(pipe.GetPrimitiveTopology() != Topology::TriangleList ||
       pipe.GetGraphicsPipelineObject() == ResourceId() || vbs.size() != 1 ||
       vbs[0].resourceId != vertexBuffers[i + 1] || vbs[0].byteOffset != 16 ||
       vbs[0].byteSize != 88 || vbs[0].byteStride != 24 || inputs.size() != 2 ||
       inputs[0].byteOffset != 0 || inputs[1].byteOffset != 8)
      return fail("per-command IA/Mesh state is wrong");
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(!clear || !PixelMatches(renderer, colorTarget, clear->eventId, 120, 150,
                             .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, execute->eventId, 120, 150,
                   1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, execute->eventId, 280, 150,
                   .0625f, .125f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 120, 150,
                   1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 280, 150,
                   .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 120, 150,
                   1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 200, 150,
                   .0625f, .125f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 280, 150,
                   .0625f, .125f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 350, 37,
                   .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 200, 150,
                   .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 200, 150,
                   .0625f, .125f, 1.0f))
    return fail("clear/first draw/second draw/rewind pixels are wrong");

  if(savePath)
  {
    std::ofstream saved(savePath, std::ios::binary);
    for(size_t i = 0; i < 3; ++i)
    {
      const bytebuf bytes = renderer->GetBufferData(vertexBuffers[i], 0, 0);
      saved.write((const char *)bytes.data(), bytes.size());
    }
    if(!saved)
      return fail("raw vertex packet export failed");
  }
  return true;
}

static bool ValidateICBResetFixture(IReplayController *renderer, ResourceId colorTarget,
                                    const char *savePath)
{
  rdcarray<ResourceId> buffers;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length != 104)
      continue;
    const bytebuf bytes = renderer->GetBufferData(buffer.resourceId, 0, 4);
    uint32_t prefix = 0;
    if(bytes.size() == 4)
      memcpy(&prefix, bytes.data(), 4);
    if((prefix & 0xff000000U) == 0x24000000U)
      buffers.push_back(buffer.resourceId);
  }
  if(buffers.empty())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal ICB reset validation failed: %s\n", message);
    return false;
  };
  if(buffers.size() != 4)
    return fail("expected four 104-byte old/final vertex packets");

  const ActionDescription *execute = NULL;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  for(const ActionDescription *action : actions)
    if(action->customName.contains("executeCommandsInBuffer(location="))
      execute = action;
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(!execute || !execute->customName.contains("location=0, length=3") || draws.size() != 3 ||
     !(execute->flags & ActionFlags::MultiAction) || execute->children.size() != 3 ||
     !draws[0]->customName.contains("ICB[0] drawPrimitives(3)") ||
     !draws[1]->customName.contains("ICB[1] drawPrimitives(3)") ||
     !draws[2]->customName.contains("ICB[2] drawPrimitives(3)") ||
     draws[0]->eventId != execute->eventId + 1 ||
     draws[1]->eventId != draws[0]->eventId + 1 ||
     draws[2]->eventId != draws[1]->eventId + 1)
    return fail("reset execute marker or ordered replacement actions are wrong");

  ResourceId vertexBuffers[4] = {};
  for(ResourceId id : buffers)
  {
    const bytebuf bytes = renderer->GetBufferData(id, 0, 0);
    if(bytes.size() != 104)
      return fail("vertex packet byte size is wrong");
    uint32_t prefix = 0, suffix = 0;
    memcpy(&prefix, bytes.data(), 4);
    memcpy(&suffix, bytes.data() + 88, 4);
    const uint32_t index = (prefix >> 16) & 0xff;
    if(index >= 4 || prefix != 0x24000000U + index * 0x10000U ||
       suffix != 0x24ff0000U + index * 0x100U || vertexBuffers[index] != ResourceId())
      return fail("vertex packet sentinels are wrong");
    vertexBuffers[index] = id;
  }

  ResourceId icb;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.name.contains("Indirect Command Buffer"))
      icb = resource.resourceId;
  if(icb == ResourceId() || !HasUsage(renderer, icb, ResourceUsage::Indirect))
    return fail("ICB resource or indirect usage is missing");

  const size_t packetIndices[] = {0, 3, 2};
  for(size_t i = 0; i < 3; ++i)
  {
    const ActionDescription *draw = draws[i];
    if(!(draw->flags & ActionFlags::Indirect) ||
       (draw->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
       draw->numIndices != 3 || draw->numInstances != 1 || draw->vertexOffset != 0 ||
       draw->outputs[0] != colorTarget ||
       !HasUsage(renderer, vertexBuffers[packetIndices[i]], ResourceUsage::VertexBuffer))
      return fail("replacement draw metadata, output or vertex usage is wrong");
    renderer->SetFrameEvent(draw->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const rdcarray<BoundVBuffer> vbs = pipe.GetVBuffers();
    const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
    if(pipe.GetPrimitiveTopology() != Topology::TriangleList ||
       pipe.GetGraphicsPipelineObject() == ResourceId() || vbs.size() != 1 ||
       vbs[0].resourceId != vertexBuffers[packetIndices[i]] || vbs[0].byteOffset != 16 ||
       vbs[0].byteSize != 88 || vbs[0].byteStride != 24 || inputs.size() != 2 ||
       inputs[0].byteOffset != 0 || inputs[1].byteOffset != 8)
      return fail("per-command replacement IA/Mesh state is wrong");
  }
  if(HasUsage(renderer, vertexBuffers[1], ResourceUsage::VertexBuffer))
    return fail("reset old command vertex buffer is still used by an action");

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  const float bgR = .025f, bgG = .035f, bgB = .055f;
  if(!clear ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 60, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, execute->eventId, 60, 150,
                   1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 60, 150, 1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 200, 150, .0625f, 1.0f, .125f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 340, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[2]->eventId, 340, 150, .0625f, .125f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draws[2]->eventId, 150, 175, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[2]->eventId, 200, 150, .0625f, 1.0f, .125f))
    return fail("clear/neighbour/replacement/rewind pixels are wrong");

  if(savePath)
  {
    std::ofstream saved(savePath, std::ios::binary | std::ios::trunc);
    for(size_t i = 0; i < 4; ++i)
    {
      const bytebuf bytes = renderer->GetBufferData(vertexBuffers[i], 0, 0);
      saved.write((const char *)bytes.data(), bytes.size());
    }
    if(!saved)
      return fail("old/final vertex packet export failed");
  }
  return true;
}

static bool ValidateMixedICBFixture(IReplayController *renderer, ResourceId colorTarget,
                                    const char *savePath)
{
  ResourceId directBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length != 104)
      continue;
    const bytebuf bytes = renderer->GetBufferData(buffer.resourceId, 0, 4);
    uint32_t prefix = 0;
    if(bytes.size() == 4)
      memcpy(&prefix, bytes.data(), 4);
    if(prefix == 0x25000000U)
      directBuffer = buffer.resourceId;
  }
  if(directBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal mixed ICB validation failed: %s\n", message);
    return false;
  };
  const ActionDescription *execute = NULL;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  for(const ActionDescription *action : actions)
    if(action->customName.contains("executeCommandsInBuffer(location="))
      execute = action;
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(!execute || !execute->customName.contains("location=0, length=2") || draws.size() != 2 ||
     !(execute->flags & ActionFlags::MultiAction) || execute->children.size() != 2 ||
     !draws[0]->customName.contains("ICB[0] drawPrimitives(3)") ||
     !draws[1]->customName.contains("ICB[1] drawIndexedPrimitives(3)") ||
     draws[0]->eventId != execute->eventId + 1 ||
     draws[1]->eventId != draws[0]->eventId + 1)
    return fail("mixed execute marker or ordered draw actions are wrong");
  fprintf(stderr, "T25 ICB EIDs execute=%u direct=%u indexed=%u\n",
          execute->eventId, draws[0]->eventId, draws[1]->eventId);
  if(!(draws[0]->flags & ActionFlags::Indirect) ||
     (draws[0]->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
     draws[0]->numIndices != 3 || draws[0]->numInstances != 1 ||
     draws[0]->vertexOffset != 0 || draws[0]->outputs[0] != colorTarget ||
     !(draws[1]->flags & ActionFlags::Indirect) ||
     !(draws[1]->flags & ActionFlags::Indexed) ||
     !(draws[1]->flags & ActionFlags::Instanced) ||
     draws[1]->numIndices != 3 || draws[1]->numInstances != 2 ||
     !draws[1]->customName.contains("instances=2") ||
     draws[1]->baseVertex != 1 || draws[1]->instanceOffset != 1 ||
     draws[1]->outputs[0] != colorTarget)
    return fail("mixed action flags, arguments or outputs are wrong");

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &directPipe = renderer->GetPipelineState();
  const rdcarray<BoundVBuffer> directVertices = directPipe.GetVBuffers();
  const rdcarray<VertexInputAttribute> directInputs = directPipe.GetVertexInputs();
  if(directPipe.GetPrimitiveTopology() != Topology::TriangleList ||
     directPipe.GetGraphicsPipelineObject() == ResourceId() || directVertices.size() != 1 ||
     directVertices[0].resourceId != directBuffer || directVertices[0].byteOffset != 16 ||
     directVertices[0].byteSize != 88 || directVertices[0].byteStride != 24 ||
     directInputs.size() != 2 || directPipe.GetIBuffer().resourceId != ResourceId())
    return fail("non-indexed command inherited index state or has wrong IA/Mesh state");

  renderer->SetFrameEvent(draws[1]->eventId, true);
  const PipeState &indexedPipe = renderer->GetPipelineState();
  const MetalPipe::State *indexedState = indexedPipe.GetMetalPipelineState();
  const BoundVBuffer index = indexedPipe.GetIBuffer();
  const rdcarray<BoundVBuffer> vertices = indexedPipe.GetVBuffers();
  const rdcarray<VertexInputAttribute> inputs = indexedPipe.GetVertexInputs();
  if(!indexedState || indexedPipe.GetPrimitiveTopology() != Topology::TriangleList ||
     indexedPipe.GetGraphicsPipelineObject() == ResourceId() || vertices.size() != 2 ||
     vertices[0].resourceId == ResourceId() || vertices[1].resourceId == ResourceId() ||
     vertices[0].byteOffset != 0 || vertices[0].byteStride != 8 ||
     vertices[1].byteOffset != 0 || vertices[1].byteStride != 24 ||
     indexedState->vertexBuffers.size() != 2 || !indexedState->vertexBuffers[1].perInstance ||
     inputs.size() != 3 || index.resourceId == ResourceId() || index.byteOffset != 4 ||
     index.byteStride != 2 || index.byteSize != 6 ||
     indexedState->indirectBuffer.resourceId != ResourceId())
    return fail("indexed command IA/Mesh or exact index state is wrong");

  const uint32_t directPrefix[] = {0x25000000, 0x25000001, 0x25000002, 0x25000003};
  const uint32_t directSuffix[] = {0x25ff0000, 0x25ff0001, 0x25ff0002, 0x25ff0003};
  const float expectedPositions[] = {2.0f, 2.0f, -.12f, -.18f, .12f, -.18f,
                                     0.0f, .22f, -2.0f, -2.0f};
  const float expectedInstances[] = {
      0.0f, 1.4f, 1.0f, 0.0f, 1.0f, 1.0f,
      .35f, 0.0f, .0625f, 1.0f, .125f, 1.0f,
      .72f, 0.0f, .09375f, .25f, 1.0f, 1.0f,
      0.0f, -1.4f, 1.0f, 1.0f, 0.0f, 1.0f,
  };
  const uint16_t expectedIndices[] = {4, 4, 0, 1, 2, 4};
  const bytebuf directData = renderer->GetBufferData(directBuffer, 0, 0);
  const bytebuf positionData = renderer->GetBufferData(vertices[0].resourceId, 0, 0);
  const bytebuf instanceData = renderer->GetBufferData(vertices[1].resourceId, 0, 0);
  const bytebuf indexData = renderer->GetBufferData(index.resourceId, 0, 0);
  if(directData.size() != 104 || memcmp(directData.data(), directPrefix, 16) != 0 ||
     memcmp(directData.data() + 88, directSuffix, 16) != 0 ||
     positionData.size() != sizeof(expectedPositions) ||
     memcmp(positionData.data(), expectedPositions, sizeof(expectedPositions)) != 0 ||
     instanceData.size() != sizeof(expectedInstances) ||
     memcmp(instanceData.data(), expectedInstances, sizeof(expectedInstances)) != 0 ||
     indexData.size() != sizeof(expectedIndices) ||
     memcmp(indexData.data(), expectedIndices, sizeof(expectedIndices)) != 0)
    return fail("mixed direct/index/vertex/instance raw bytes are wrong");

  ResourceId icb;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.name.contains("Indirect Command Buffer"))
      icb = resource.resourceId;
  if(icb == ResourceId() || !HasUsage(renderer, icb, ResourceUsage::Indirect) ||
     !HasUsage(renderer, directBuffer, ResourceUsage::VertexBuffer) ||
     !HasUsage(renderer, vertices[0].resourceId, ResourceUsage::VertexBuffer) ||
     !HasUsage(renderer, vertices[1].resourceId, ResourceUsage::VertexBuffer) ||
     !HasUsage(renderer, index.resourceId, ResourceUsage::IndexBuffer))
    return fail("mixed ICB resource descriptions or usages are missing");

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  const float bgR = .025f, bgG = .035f, bgB = .055f;
  if(!clear ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 70, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, execute->eventId, 70, 150,
                   1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, execute->eventId, 270, 150,
                   .0625f, 1.0f, .125f) ||
     !PixelMatches(renderer, colorTarget, execute->eventId, 344, 150,
                   .09375f, .25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 70, 150, 1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 270, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 270, 150, .0625f, 1.0f, .125f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 344, 150, .09375f, .25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 344, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 344, 150, .09375f, .25f, 1.0f))
    return fail("mixed clear/direct/indexed/rewind pixels are wrong");

  if(savePath)
  {
    std::ofstream saved(savePath, std::ios::binary | std::ios::trunc);
    saved.write((const char *)directData.data(), directData.size());
    saved.write((const char *)positionData.data(), positionData.size());
    saved.write((const char *)instanceData.data(), instanceData.size());
    saved.write((const char *)indexData.data(), indexData.size());
    if(!saved)
      return fail("mixed ICB raw resource export failed");
  }
  return true;
}

static bool ValidateIndexedICBFixture(IReplayController *renderer, ResourceId colorTarget,
                                      const char *savePath)
{
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length != 104)
      continue;
    const bytebuf bytes = renderer->GetBufferData(buffer.resourceId, 0, 4);
    uint32_t prefix = 0;
    if(bytes.size() == 4)
      memcpy(&prefix, bytes.data(), 4);
    if(prefix == 0x25000000U)
      return true;
  }
  ResourceId icb;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.name.contains("Indirect Command Buffer"))
      icb = resource.resourceId;

  bool hasT23IndexPacket = false;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 12)
      hasT23IndexPacket = true;
  if(icb == ResourceId() || !hasT23IndexPacket)
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal indexed ICB validation failed: %s\n", message);
    return false;
  };

  const ActionDescription *execute = NULL;
  rdcarray<const ActionDescription *> actions;
  FindActions(renderer->GetRootActions(), actions);
  for(const ActionDescription *action : actions)
    if(action->customName.contains("executeCommandsInBuffer(location="))
      execute = action;
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(!execute || !execute->customName.contains("location=1, length=1") || draws.size() != 1 ||
     !(execute->flags & ActionFlags::MultiAction) || execute->children.size() != 1 ||
     !draws[0]->customName.contains("ICB[1] drawIndexedPrimitives(3)"))
    return fail("execute marker or indexed draw action is missing");

  const ActionDescription *draw = draws[0];
  if(draw->eventId != execute->eventId + 1 ||
     !(draw->flags & ActionFlags::Indexed) ||
     !(draw->flags & ActionFlags::Indirect) ||
     !(draw->flags & ActionFlags::Instanced) ||
     draw->numIndices != 3 || draw->numInstances != 2 ||
     !draw->customName.contains("instances=2") ||
     draw->baseVertex != 1 || draw->instanceOffset != 1 ||
     draw->indexOffset != 0 || draw->outputs[0] != colorTarget)
    return fail("indexed ICB action flags, arguments or output are wrong");

  renderer->SetFrameEvent(draw->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  const BoundVBuffer index = pipe.GetIBuffer();
  const rdcarray<BoundVBuffer> vertices = pipe.GetVBuffers();
  const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
  if(!state || pipe.GetPrimitiveTopology() != Topology::TriangleList ||
     pipe.GetGraphicsPipelineObject() == ResourceId() ||
     state->vertexAttributes.size() != 3 || state->vertexBuffers.size() != 2 ||
     !state->vertexBuffers[1].perInstance || vertices.size() != 2 ||
     vertices[0].resourceId == ResourceId() || vertices[1].resourceId == ResourceId() ||
     vertices[0].byteOffset != 0 || vertices[0].byteStride != 8 ||
     vertices[1].byteOffset != 0 || vertices[1].byteStride != 24 ||
     inputs.size() != 3 || inputs[0].byteOffset != 0 ||
     inputs[1].byteOffset != 0 || inputs[2].byteOffset != 8 ||
     index.resourceId == ResourceId() || index.byteOffset != 4 ||
     index.byteStride != 2 || index.byteSize != 6 ||
     state->indirectBuffer.resourceId != ResourceId())
    return fail("indexed IA/Mesh layout or exact index range is wrong");

  const uint16_t expectedIndices[] = {4, 4, 0, 1, 2, 4};
  const float expectedPositions[] = {2.0f, 2.0f, -.18f, -.18f, .18f, -.18f,
                                     0.0f, .22f, -2.0f, -2.0f};
  const float expectedInstances[] = {
      0.0f, 1.4f, 1.0f, 0.0f, 1.0f, 1.0f,
      -.55f, 0.0f, 1.0f, .125f, .0625f, 1.0f,
      .55f, 0.0f, .09375f, .25f, 1.0f, 1.0f,
      0.0f, -1.4f, 1.0f, 1.0f, 0.0f, 1.0f,
  };
  const bytebuf indexData = renderer->GetBufferData(index.resourceId, 0, 0);
  const bytebuf selectedIndices = renderer->GetBufferData(index.resourceId, 4, 6);
  const bytebuf positionData = renderer->GetBufferData(vertices[0].resourceId, 0, 0);
  const bytebuf instanceData = renderer->GetBufferData(vertices[1].resourceId, 0, 0);
  if(indexData.size() != sizeof(expectedIndices) ||
     memcmp(indexData.data(), expectedIndices, sizeof(expectedIndices)) != 0 ||
     selectedIndices.size() != 6 ||
     memcmp(selectedIndices.data(), expectedIndices + 2, 6) != 0 ||
     positionData.size() != sizeof(expectedPositions) ||
     memcmp(positionData.data(), expectedPositions, sizeof(expectedPositions)) != 0 ||
     instanceData.size() != sizeof(expectedInstances) ||
     memcmp(instanceData.data(), expectedInstances, sizeof(expectedInstances)) != 0 ||
     renderer->GetBufferData(index.resourceId, 12, 2).size() != 0 ||
     renderer->GetBufferData(vertices[0].resourceId, 40, 4).size() != 0 ||
     renderer->GetBufferData(vertices[1].resourceId, 96, 4).size() != 0)
    return fail("index, position or per-instance bytes are wrong");

  bool indexDescription = false;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.resourceId == index.resourceId)
      indexDescription = buffer.length == 12 &&
                         bool(buffer.creationFlags & BufferCategory::Index);
  if(!indexDescription ||
     !HasUsage(renderer, icb, ResourceUsage::Indirect) ||
     !HasUsage(renderer, index.resourceId, ResourceUsage::IndexBuffer) ||
     !HasUsage(renderer, vertices[0].resourceId, ResourceUsage::VertexBuffer) ||
     !HasUsage(renderer, vertices[1].resourceId, ResourceUsage::VertexBuffer))
    return fail("indexed ICB resource descriptions or usages are missing");

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  const float bgR = .025f, bgG = .035f, bgB = .055f;
  if(!clear ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, execute->eventId, 100, 150,
                   1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 100, 150,
                   1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 300, 150,
                   .09375f, .25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 300, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 300, 150,
                   .09375f, .25f, 1.0f))
    return fail("clear/execute/draw/rewind output pixels are wrong");

  if(savePath)
  {
    std::ofstream saved(savePath, std::ios::binary | std::ios::trunc);
    saved.write((const char *)selectedIndices.data(), selectedIndices.size());
    saved.close();
    std::ifstream check(savePath, std::ios::binary | std::ios::ate);
    if(!check || check.tellg() != std::streampos(6))
      return fail("selected UInt16 index export is not six bytes");
  }
  return true;
}

static bool ValidateIndexedInstancingFixture(IReplayController *renderer, ResourceId colorTarget,
                                             const char *savePath)
{
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1 ||
     !draws[0]->customName.contains("drawIndexedPrimitives(3,"))
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal indexed instancing validation failed: %s\n", message);
    return false;
  };
  const ActionDescription *draw = draws[0];
  bool shortOverload = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    shortOverload |= chunk->metadata.chunkID == 1147;
  if(!(draw->flags & ActionFlags::Instanced) || draw->numIndices != 3 ||
     draw->numInstances != 2 || draw->baseVertex != (shortOverload ? 0 : 1) ||
     draw->instanceOffset != (shortOverload ? 0U : 1U) || draw->indexOffset != 0)
    return fail("indexed/instanced action metadata is wrong");

  renderer->SetFrameEvent(draw->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  const BoundVBuffer index = pipe.GetIBuffer();
  const rdcarray<BoundVBuffer> vertices = pipe.GetVBuffers();
  if(state == NULL || pipe.GetPrimitiveTopology() != Topology::TriangleList ||
     state->vertexAttributes.size() != 3 || vertices.size() < 2 ||
     index.resourceId == ResourceId() || index.byteOffset != 4 || index.byteStride != 2 ||
     index.byteSize != 6 || vertices[0].resourceId == ResourceId() ||
     vertices[1].resourceId == ResourceId() || !state->vertexBuffers[1].perInstance ||
     vertices[0].byteOffset != (shortOverload ? 8U : 0U) ||
     vertices[1].byteOffset != (shortOverload ? 24U : 0U))
    return fail("IA binding, index subrange or vertex layout is wrong");

  const uint16_t expectedIndices[] = {4, 4, 0, 1, 2, 4};
  const bytebuf indexData = renderer->GetBufferData(index.resourceId, 0, 0);
  if(indexData.size() != sizeof(expectedIndices) ||
     memcmp(indexData.data(), expectedIndices, sizeof(expectedIndices)) != 0)
    return fail("full index buffer does not contain the offset sentinels");
  const bytebuf selected = renderer->GetBufferData(index.resourceId, index.byteOffset, 6);
  if(selected.size() != 6 || memcmp(selected.data(), expectedIndices + 2, 6) != 0)
    return fail("selected index subrange is wrong");
  if(renderer->GetBufferData(vertices[0].resourceId, 0, 0).size() != 40 ||
     renderer->GetBufferData(vertices[1].resourceId, 0, 0).size() != 96 ||
     !HasUsage(renderer, index.resourceId, ResourceUsage::IndexBuffer) ||
     !HasUsage(renderer, vertices[0].resourceId, ResourceUsage::VertexBuffer) ||
     !HasUsage(renderer, vertices[1].resourceId, ResourceUsage::VertexBuffer))
    return fail("index/vertex/instance data or usage is missing");

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear action is unavailable");
  const float bgR = 0.025f, bgG = 0.035f, bgB = 0.055f;
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 100, 150, 1.0f, 0.125f, 0.0625f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 300, 150, 0.09375f, 0.25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 300, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 300, 150, 0.09375f, 0.25f, 1.0f))
    return fail("clear/draw/rewind output pixels are wrong");

  if(savePath)
  {
    std::ofstream file(savePath, std::ios::binary | std::ios::trunc);
    file.write((const char *)selected.data(), selected.size());
    file.close();
    std::ifstream saved(savePath, std::ios::binary | std::ios::ate);
    if(!saved || saved.tellg() != std::streampos(6))
      return fail("raw index export is not exactly six bytes");
  }
  return true;
}

static bool ValidateIndexedIndirectFixture(IReplayController *renderer, ResourceId colorTarget,
                                           const char *savePath)
{
  bool isFixture = false;
  for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    if(chunk->name == "MTLRenderCommandEncoder::drawIndexedPrimitives" &&
       chunk->FindChild("indirectBuffer") != NULL)
      isFixture = true;
  if(!isFixture)
    return true;

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal indexed indirect validation failed: %s\n", message);
    return false;
  };
  if(draws.size() != 1 ||
     !draws[0]->customName.contains("drawIndexedPrimitives(indirect"))
    return fail("expected exactly one indexed indirect action");
  const ActionDescription *draw = draws[0];
  if(!(draw->flags & ActionFlags::Indexed) ||
     !(draw->flags & ActionFlags::Indirect) ||
     !(draw->flags & ActionFlags::Instanced) ||
     !draw->customName.contains("indexStart 1, baseVertex 1, baseInstance 1") ||
     draw->numIndices != 3 || draw->numInstances != 2 ||
     draw->baseVertex != 1 || draw->instanceOffset != 1 ||
     draw->indexOffset != 0 || draw->outputs[0] != colorTarget)
    return fail("action flags, five arguments or output are wrong");

  renderer->SetFrameEvent(draw->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  const BoundVBuffer index = pipe.GetIBuffer();
  const rdcarray<BoundVBuffer> vertices = pipe.GetVBuffers();
  if(!state || pipe.GetPrimitiveTopology() != Topology::TriangleList ||
     pipe.GetGraphicsPipelineObject() == ResourceId() ||
     state->vertexAttributes.size() != 3 ||
     state->vertexBuffers.size() != 2 || vertices.size() != 2 ||
     !state->vertexBuffers[1].perInstance ||
     index.resourceId == ResourceId() || index.byteOffset != 6 ||
     index.byteStride != 2 || index.byteSize != 6 ||
     state->indirectBuffer.resourceId == ResourceId() ||
     state->indirectBuffer.byteOffset != 16 ||
     state->indirectBuffer.byteSize != 20)
    return fail("IA/Mesh bindings or exact index/argument ranges are wrong");

  const uint16_t expectedIndices[] = {4, 4, 4, 0, 1, 2, 4};
  const uint32_t expectedArguments[] = {3, 2, 1, 1, 1};
  const uint32_t prefix[] = {0x13572468, 0x24681357, 0x89abcdef, 0xfedcba98};
  const uint32_t suffix[] = {0x10203040, 0x50607080, 0x90a0b0c0, 0xd0e0f000};
  const bytebuf indices = renderer->GetBufferData(index.resourceId, 0, 0);
  const bytebuf selected = renderer->GetBufferData(index.resourceId, 6, 6);
  const bytebuf packet = renderer->GetBufferData(state->indirectBuffer.resourceId, 0, 0);
  if(indices.size() != sizeof(expectedIndices) ||
     memcmp(indices.data(), expectedIndices, sizeof(expectedIndices)) != 0 ||
     selected.size() != 6 || memcmp(selected.data(), expectedIndices + 3, 6) != 0 ||
     packet.size() != 52 || memcmp(packet.data(), prefix, sizeof(prefix)) != 0 ||
     memcmp(packet.data() + 16, expectedArguments, sizeof(expectedArguments)) != 0 ||
     memcmp(packet.data() + 36, suffix, sizeof(suffix)) != 0 ||
     renderer->GetBufferData(index.resourceId, sizeof(expectedIndices), 2).size() != 0 ||
     renderer->GetBufferData(state->indirectBuffer.resourceId, 52, 4).size() != 0)
    return fail("raw index/argument bytes or sentinels are wrong");

  bool indexDescription = false, indirectDescription = false;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.resourceId == index.resourceId)
      indexDescription = buffer.length == sizeof(expectedIndices) &&
                         bool(buffer.creationFlags & BufferCategory::Index);
    if(buffer.resourceId == state->indirectBuffer.resourceId)
      indirectDescription = buffer.length == 52 &&
                            bool(buffer.creationFlags & BufferCategory::Indirect);
  }
  if(!indexDescription || !indirectDescription ||
     !HasUsage(renderer, index.resourceId, ResourceUsage::IndexBuffer) ||
     !HasUsage(renderer, state->indirectBuffer.resourceId, ResourceUsage::Indirect) ||
     vertices[0].resourceId == ResourceId() || vertices[1].resourceId == ResourceId() ||
     renderer->GetBufferData(vertices[0].resourceId, 0, 0).size() != 40 ||
     renderer->GetBufferData(vertices[1].resourceId, 0, 0).size() != 96 ||
     !HasUsage(renderer, vertices[0].resourceId, ResourceUsage::VertexBuffer) ||
     !HasUsage(renderer, vertices[1].resourceId, ResourceUsage::VertexBuffer))
  {
    fprintf(stderr, "T21 detail: descriptions %d/%d, usages %d/%d/%d/%d, vertex bytes %zu/%zu\n",
            int(indexDescription), int(indirectDescription),
            int(HasUsage(renderer, index.resourceId, ResourceUsage::IndexBuffer)),
            int(HasUsage(renderer, state->indirectBuffer.resourceId, ResourceUsage::Indirect)),
            int(HasUsage(renderer, vertices[0].resourceId, ResourceUsage::VertexBuffer)),
            int(HasUsage(renderer, vertices[1].resourceId, ResourceUsage::VertexBuffer)),
            renderer->GetBufferData(vertices[0].resourceId, 0, 0).size(),
            renderer->GetBufferData(vertices[1].resourceId, 0, 0).size());
    return fail("buffer descriptions, vertex data or resource usages are wrong");
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  const float bgR = .025f, bgG = .035f, bgB = .055f;
  if(!clear || !PixelMatches(renderer, colorTarget, clear->eventId, 100, 150,
                             bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 100, 150,
                   1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 300, 150,
                   .09375f, .25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 200, 150,
                   bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 300, 150,
                   bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 300, 150,
                   .09375f, .25f, 1.0f))
    return fail("clear/draw/rewind pixels are wrong");

  if(savePath)
  {
    std::ofstream file(savePath, std::ios::binary | std::ios::trunc);
    file.write((const char *)packet.data() + 16, sizeof(expectedArguments));
    file.close();
    std::ifstream saved(savePath, std::ios::binary | std::ios::ate);
    if(!saved || saved.tellg() != std::streampos(sizeof(expectedArguments)))
      return fail("raw argument export did not contain 20 bytes");
  }
  return true;
}

static bool ValidatePointLineFixture(IReplayController *renderer, ResourceId colorTarget,
                                     const char *savePath)
{
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 11 * 6 * sizeof(float))
      vertexBuffer = buffer.resourceId;
  if(vertexBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal point/line fixture validation failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 3)
    return fail("expected three draw events");
  const Topology topologies[] = {Topology::PointList, Topology::LineList, Topology::LineStrip};
  const uint32_t starts[] = {1, 3, 6};
  const uint32_t counts[] = {1, 2, 3};
  const char *names[] = {"Point", "Line", "Line Strip"};
  for(size_t i = 0; i < 3; i++)
  {
    const ActionDescription *draw = draws[i];
    if((draw->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
       draw->eventId != draws[0]->eventId + i || draw->vertexOffset != starts[i] ||
       draw->numIndices != counts[i] || draw->numInstances != 1 ||
       !draw->customName.contains(names[i]) || draw->outputs[0] != colorTarget)
      return fail("draw event metadata, order or output is wrong");

    renderer->SetFrameEvent(draw->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const MetalPipe::State *state = pipe.GetMetalPipelineState();
    const rdcarray<BoundVBuffer> vbs = pipe.GetVBuffers();
    const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
    if(state == NULL || pipe.GetPrimitiveTopology() != topologies[i] ||
       state->vertexAttributes.size() != 2 || vbs.size() != 1 ||
       vbs[0].resourceId != vertexBuffer || vbs[0].byteOffset != 0 ||
       vbs[0].byteSize != 264 || vbs[0].byteStride != 24 || inputs.size() != 2 ||
       inputs[0].vertexBuffer != 0 || inputs[0].byteOffset != 0 ||
       inputs[0].format.compCount != 2 || inputs[1].vertexBuffer != 0 ||
       inputs[1].byteOffset != 8 || inputs[1].format.compCount != 4)
      return fail("Pipeline or standard VS Input does not show point/line topology and vertex input");

    const size_t selectedOffset = starts[i] * 24;
    const bytebuf selected = renderer->GetBufferData(vertexBuffer, selectedOffset, counts[i] * 24);
    if(selected.size() != counts[i] * 24)
      return fail("selected Buffer Viewer vertex range is unavailable");
    float firstX = 0.0f;
    memcpy(&firstX, selected.data(), sizeof(firstX));
    const float expectedX[] = {-0.5f, -0.75f, 0.25f};
    if(!Near(firstX, expectedX[i]))
      return fail("selected vertex bytes do not match vertexStart");
  }

  const bytebuf all = renderer->GetBufferData(vertexBuffer, 0, 0);
  if(all.size() != 264 || !HasUsage(renderer, vertexBuffer, ResourceUsage::VertexBuffer))
    return fail("full vertex buffer or resource usage is missing");

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear event is unavailable");
  const float bgR = 0.025f, bgG = 0.035f, bgB = 0.055f;
  auto nearby = [&](uint32_t eventId, uint32_t x, uint32_t y, float r, float g, float b) {
    renderer->SetFrameEvent(eventId, true);
    for(uint32_t yy = y - 2; yy <= y + 2; yy++)
      for(uint32_t xx = x - 2; xx <= x + 2; xx++)
      {
        const PixelValue pixel = renderer->PickPixel(colorTarget, xx, yy, {0, 0, 0}, CompType::Typeless);
        if(Near(pixel.floatValue[0], r) && Near(pixel.floatValue[1], g) &&
           Near(pixel.floatValue[2], b))
          return true;
      }
    return false;
  };
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 75, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 275, 225, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 75, 1.0f, 0.125f, 0.0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 150, bgR, bgG, bgB) ||
     !nearby(draws[1]->eventId, 100, 150, 0.0625f, 0.875f, 0.1875f) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 275, 225, bgR, bgG, bgB) ||
     !nearby(draws[2]->eventId, 275, 225, 0.09375f, 0.25f, 1.0f) ||
     !nearby(draws[2]->eventId, 300, 250, 0.09375f, 0.25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 100, 75, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[2]->eventId, 100, 75, 1.0f, 0.125f, 0.0625f))
    return fail("clear/draw/rewind pixels or primitive connectivity are wrong");

  if(savePath)
  {
    renderer->SetFrameEvent(draws[2]->eventId, true);
    TextureSave save;
    save.resourceId = colorTarget;
    save.destType = FileType::DDS;
    save.mip = 0;
    save.slice.sliceIndex = 0;
    if(!renderer->SaveTexture(save, savePath).OK())
      return fail("standard DDS export failed");
    std::ifstream saved(savePath, std::ios::binary | std::ios::ate);
    if(!saved || saved.tellg() != std::streampos(400 * 300 * 4 + 128))
      return fail("standard DDS export size is wrong");
  }
  return true;
}

static bool ValidateVertexTextureFixture(IReplayController *renderer, ResourceId colorTarget,
                                         const char *savePath)
{
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 24 * 4 * sizeof(float))
      vertexBuffer = buffer.resourceId;
  if(vertexBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal vertex texture fixture validation failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1 || draws[0]->numIndices != 24 || draws[0]->numInstances != 1 ||
     draws[0]->outputs[0] != colorTarget)
    return fail("draw action or output does not match");
  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear event is missing");
  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(state == NULL || pipe.GetPrimitiveTopology() != Topology::TriangleList ||
     state->vertexTextures.size() != 1 || state->vertexTextures[0] == ResourceId() ||
     state->vertexSamplers.size() != 1 || state->vertexSamplers[0] == ResourceId() ||
     !state->fragmentTextures.empty() || !state->fragmentSamplers.empty())
    return fail("vertex bindings or topology are wrong");
  const ResourceId texture = state->vertexTextures[0];
  const ResourceId sampler = state->vertexSamplers[0];
  const ShaderReflection *reflection = pipe.GetShaderReflection(ShaderStage::Vertex);
  if(reflection == NULL || reflection->readOnlyResources.size() != 1 ||
     reflection->samplers.size() != 1 ||
     reflection->readOnlyResources[0].name != "vertexTexture" ||
     reflection->readOnlyResources[0].fixedBindNumber != 0 ||
     reflection->samplers[0].name != "vertexSampler" ||
     reflection->samplers[0].fixedBindNumber != 0)
    return fail("vertex shader reflection is wrong");
  const rdcarray<UsedDescriptor> resources = pipe.GetReadOnlyResources(ShaderStage::Vertex, true);
  const rdcarray<UsedDescriptor> samplers = pipe.GetSamplers(ShaderStage::Vertex, true);
  if(resources.size() != 1 || resources[0].access.index != 0 ||
     resources[0].access.staticallyUnused || resources[0].descriptor.resource != texture ||
     resources[0].descriptor.textureType != TextureType::Texture2D ||
     samplers.size() != 1 || samplers[0].access.index != 0 ||
     samplers[0].access.staticallyUnused || samplers[0].sampler.object != sampler ||
     samplers[0].sampler.filter.minify != FilterMode::Point ||
     samplers[0].sampler.addressU != AddressMode::ClampEdge)
    return fail("standard vertex descriptors are wrong");
  const byte expectedTexture[] = {255, 32, 16, 255, 16, 224, 48, 255,
                                  24, 64, 255, 255, 240, 208, 32, 255};
  const bytebuf textureBytes = renderer->GetTextureData(texture, {0, 0, 0});
  if(textureBytes.size() != sizeof(expectedTexture) ||
     memcmp(textureBytes.data(), expectedTexture, sizeof(expectedTexture)) != 0 ||
     !HasUsage(renderer, texture, ResourceUsage::VS_Resource) ||
     !HasUsage(renderer, vertexBuffer, ResourceUsage::VertexBuffer))
    return fail("texture bytes or resource usage are wrong");
  if(!ValidateHeadlessThumbnail(renderer, texture, draws[0]->eventId, true))
    return fail("vertex texture thumbnail is blank");
  const rdcarray<BoundVBuffer> vbs = pipe.GetVBuffers();
  const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
  const bytebuf vertexBytes = renderer->GetBufferData(vertexBuffer, 0, 0);
  if(vbs.size() != 1 || vbs[0].resourceId != vertexBuffer || vbs[0].byteStride != 16 ||
     inputs.size() != 2 || inputs[0].byteOffset != 0 || inputs[1].byteOffset != 8 ||
     vertexBytes.size() != 384)
    return fail("VS Input or standard Buffer Viewer data is wrong");
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 75, .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 75, 1.0f,
                   32.0f / 255.0f, 16.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 75,
                   16.0f / 255.0f, 224.0f / 255.0f, 48.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 225,
                   24.0f / 255.0f, 64.0f / 255.0f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 225,
                   240.0f / 255.0f, 208.0f / 255.0f, 32.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 100, 75, .025f, .035f, .055f))
    return fail("clear/draw/rewind pixels are wrong");
  if(savePath)
  {
    renderer->SetFrameEvent(draws[0]->eventId, true);
    TextureSave save;
    save.resourceId = colorTarget;
    save.destType = FileType::DDS;
    save.mip = 0;
    save.slice.sliceIndex = 0;
    if(!renderer->SaveTexture(save, savePath).OK())
      return fail("standard DDS export failed");
    std::ifstream saved(savePath, std::ios::binary | std::ios::ate);
    if(!saved || saved.tellg() != std::streampos(400 * 300 * 4 + 128))
      return fail("DDS size is wrong");
  }
  return true;
}

static bool ValidateBatchTextureFixture(IReplayController *renderer, ResourceId colorTarget,
                                        const char *savePath)
{
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 25 * 4 * sizeof(float))
      vertexBuffer = buffer.resourceId;
  if(vertexBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal batch texture fixture validation failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL || draws.size() != 1 || draws[0]->numIndices != 24 ||
     draws[0]->outputs[0] != colorTarget)
    return fail("clear/draw action is wrong");
  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(state == NULL || pipe.GetPrimitiveTopology() != Topology::TriangleList ||
     state->vertexTextures.size() != 4 || state->vertexSamplers.size() != 4 ||
     state->fragmentTextures.size() != 6 || state->fragmentSamplers.size() != 6 ||
     state->vertexTextures[1] != ResourceId() || state->vertexSamplers[1] != ResourceId() ||
     state->fragmentTextures[3] != ResourceId() || state->fragmentSamplers[3] != ResourceId() ||
     state->vertexTextures[2] == ResourceId() || state->vertexTextures[3] == ResourceId() ||
     state->fragmentTextures[4] == ResourceId() || state->fragmentTextures[5] == ResourceId() ||
     state->vertexSamplers[2] == ResourceId() || state->vertexSamplers[3] == ResourceId() ||
     state->fragmentSamplers[4] == ResourceId() || state->fragmentSamplers[5] == ResourceId())
    return fail("batch slot range or cleared slots are wrong");

  const ShaderReflection *vs = pipe.GetShaderReflection(ShaderStage::Vertex);
  const ShaderReflection *fs = pipe.GetShaderReflection(ShaderStage::Fragment);
  if(vs == NULL || fs == NULL || vs->readOnlyResources.empty() || vs->samplers.empty() ||
     fs->readOnlyResources.empty() || fs->samplers.empty() ||
     vs->readOnlyResources[0].fixedBindNumber != 2 || vs->samplers[0].fixedBindNumber != 2 ||
     fs->readOnlyResources[0].fixedBindNumber != 4 || fs->samplers[0].fixedBindNumber != 4)
    return fail("VS/FS reflection slot numbers are wrong");
  const rdcarray<UsedDescriptor> vsTextures = pipe.GetReadOnlyResources(ShaderStage::Vertex, false);
  const rdcarray<UsedDescriptor> vsSamplers = pipe.GetSamplers(ShaderStage::Vertex, false);
  const rdcarray<UsedDescriptor> fsTextures = pipe.GetReadOnlyResources(ShaderStage::Fragment, false);
  const rdcarray<UsedDescriptor> fsSamplers = pipe.GetSamplers(ShaderStage::Fragment, false);
  if(vsTextures.size() != 2 || vsSamplers.size() != 2 || fsTextures.size() != 2 ||
     fsSamplers.size() != 2 || vsTextures[0].descriptor.resource != state->vertexTextures[2] ||
     vsTextures[0].access.staticallyUnused || !vsTextures[1].access.staticallyUnused ||
     vsSamplers[0].sampler.object != state->vertexSamplers[2] ||
     vsSamplers[0].sampler.addressU != AddressMode::ClampEdge ||
     vsSamplers[0].access.staticallyUnused || !vsSamplers[1].access.staticallyUnused ||
     fsTextures[0].descriptor.resource != state->fragmentTextures[4] ||
     fsTextures[0].access.staticallyUnused || !fsTextures[1].access.staticallyUnused ||
     fsSamplers[0].sampler.object != state->fragmentSamplers[4] ||
     fsSamplers[0].sampler.addressU != AddressMode::Wrap ||
     fsSamplers[0].access.staticallyUnused || !fsSamplers[1].access.staticallyUnused ||
     pipe.GetReadOnlyResources(ShaderStage::Vertex, true).size() != 1 ||
     pipe.GetSamplers(ShaderStage::Vertex, true).size() != 1 ||
     pipe.GetReadOnlyResources(ShaderStage::Fragment, true).size() != 1 ||
     pipe.GetSamplers(ShaderStage::Fragment, true).size() != 1)
    return fail("standard used/unused descriptors or sampler states are wrong");

  const byte expectedVertex[] = {255, 32, 16, 255, 16, 224, 48, 255,
                                 24, 64, 255, 255, 240, 208, 32, 255};
  const byte expectedFragment[] = {255, 0, 255, 255, 0, 255, 255, 255,
                                   255, 255, 0, 255, 255, 255, 255, 255};
  const bytebuf vertexBytes = renderer->GetTextureData(state->vertexTextures[2], {0, 0, 0});
  const bytebuf fragmentBytes = renderer->GetTextureData(state->fragmentTextures[4], {0, 0, 0});
  if(vertexBytes.size() != sizeof(expectedVertex) ||
     memcmp(vertexBytes.data(), expectedVertex, sizeof(expectedVertex)) != 0 ||
     fragmentBytes.size() != sizeof(expectedFragment) ||
     memcmp(fragmentBytes.data(), expectedFragment, sizeof(expectedFragment)) != 0 ||
     renderer->GetBufferData(vertexBuffer, 0, 0).size() != 400 ||
     !HasUsage(renderer, state->vertexTextures[2], ResourceUsage::VS_Resource) ||
     !HasUsage(renderer, state->fragmentTextures[4], ResourceUsage::PS_Resource) ||
     !HasUsage(renderer, vertexBuffer, ResourceUsage::VertexBuffer))
    return fail("texture/buffer data or usage is wrong");
  if(!ValidateHeadlessThumbnail(renderer, state->vertexTextures[2], draws[0]->eventId, true) ||
     !ValidateHeadlessThumbnail(renderer, state->fragmentTextures[4], draws[0]->eventId, true))
    return fail("batch vertex/fragment texture thumbnails are blank");
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 75, .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 75, 0.0f,
                   32.0f / 255.0f, 16.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 75,
                   16.0f / 255.0f, 0.0f, 48.0f / 255.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 225,
                   24.0f / 255.0f, 64.0f / 255.0f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 225,
                   240.0f / 255.0f, 208.0f / 255.0f, 0.0f) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 100, 75, .025f, .035f, .055f))
    return fail("clear/draw/rewind pixels are wrong");
  if(savePath)
  {
    renderer->SetFrameEvent(draws[0]->eventId, true);
    TextureSave save;
    save.resourceId = colorTarget;
    save.destType = FileType::DDS;
    save.mip = 0;
    save.slice.sliceIndex = 0;
    if(!renderer->SaveTexture(save, savePath).OK())
      return fail("standard DDS export failed");
    std::ifstream saved(savePath, std::ios::binary | std::ios::ate);
    if(!saved || saved.tellg() != std::streampos(400 * 300 * 4 + 128))
      return fail("DDS size is wrong");
  }
  return true;
}

static bool ValidateFragmentStorageBufferFixture(IReplayController *renderer,
                                                 ResourceId colorTarget, const char *savePath)
{
  ResourceId storage;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 640)
      storage = buffer.resourceId;
  if(storage == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal fragment storage-buffer validation failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL || draws.size() != 1 || draws[0]->numIndices != 3 ||
     draws[0]->outputs[0] != colorTarget)
    return fail("clear/draw action or output is wrong");

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(state == NULL || state->fragmentBuffers.size() != 4 ||
     state->fragmentBuffers[3].resourceId != storage ||
     state->fragmentBuffers[3].byteOffset != 256 ||
     state->fragmentBuffers[3].byteSize != 384 ||
     state->fragmentBuffers[0].resourceId != ResourceId() ||
     state->fragmentBuffers[1].resourceId != ResourceId() ||
     state->fragmentBuffers[2].resourceId != ResourceId())
    return fail("physical slot, range, or empty slots are wrong");
  const ShaderReflection *reflection = pipe.GetShaderReflection(ShaderStage::Fragment);
  if(reflection == NULL || !reflection->constantBlocks.empty() ||
     reflection->readOnlyResources.size() != 2 ||
     reflection->readOnlyResources[0].name != "colours" ||
     reflection->readOnlyResources[0].fixedBindNumber != 3 ||
     reflection->readOnlyResources[0].descriptorType != DescriptorType::Buffer ||
     reflection->readOnlyResources[0].isTexture ||
     reflection->readOnlyResources[1].name != "unusedColours" ||
     reflection->readOnlyResources[1].fixedBindNumber != 5 ||
     reflection->readOnlyResources[1].descriptorType != DescriptorType::Buffer)
    return fail("storage buffer reflection was misclassified");
  const rdcarray<UsedDescriptor> allResources =
      pipe.GetReadOnlyResources(ShaderStage::Fragment, false);
  if(allResources.size() != 1)
    return fail("unbound storage-buffer shader slot must not appear as a bound descriptor");
  const rdcarray<UsedDescriptor> resources =
      pipe.GetReadOnlyResources(ShaderStage::Fragment, true);
  if(resources.size() != 1 || resources[0].access.type != DescriptorType::Buffer ||
     resources[0].access.index != 0 || resources[0].access.byteOffset != 0x203 ||
     resources[0].access.staticallyUnused ||
     resources[0].descriptor.type != DescriptorType::Buffer ||
     resources[0].descriptor.resource != storage ||
     resources[0].descriptor.byteOffset != 256 || resources[0].descriptor.byteSize != 384 ||
     !pipe.GetConstantBlocks(ShaderStage::Fragment, true).empty())
    return fail("generic storage-buffer descriptor or used state is wrong");

  const bytebuf bytes = renderer->GetBufferData(storage, 0, 0);
  const float guard[4] = {1.0f, 0.0f, 1.0f, 1.0f};
  const float colours[4][4] = {{1.0f, 0.125f, 0.0625f, 1.0f},
                               {0.0625f, 0.875f, 0.1875f, 1.0f},
                               {0.125f, 0.25f, 1.0f, 1.0f},
                               {0.9375f, 0.8125f, 0.0f, 1.0f}};
  if(bytes.size() != 640 || memcmp(bytes.data(), guard, sizeof(guard)) != 0 ||
     memcmp(bytes.data() + 256, colours, sizeof(colours)) != 0 ||
     memcmp(bytes.data() + 256 + sizeof(colours), guard, sizeof(guard)) != 0 ||
     renderer->GetBufferData(storage, 640, 16).size() != 0 ||
     !HasUsage(renderer, storage, ResourceUsage::PS_Resource) ||
     HasUsage(renderer, storage, ResourceUsage::PS_Constants))
    return fail("raw bytes, out-of-range read, or resource usage are wrong");

  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 75, .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 75, .125f, .25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 75, .9375f, .8125f, 0.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 225, 1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 225, .0625f, .875f, .1875f) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 100, 75, .025f, .035f, .055f))
    return fail("clear/draw/rewind pixels are wrong");
  if(savePath)
  {
    std::ofstream saved(savePath, std::ios::binary);
    saved.write((const char *)bytes.data(), bytes.size());
    if(!saved)
      return fail("raw buffer export failed");
  }
  return true;
}

static bool ValidateVertexStorageBufferFixture(IReplayController *renderer,
                                               ResourceId colorTarget, const char *savePath)
{
  ResourceId storage;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 768)
      storage = buffer.resourceId;
  if(storage == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal vertex storage-buffer validation failed: %s\n", message);
    return false;
  };
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL || draws.size() != 1 || draws[0]->numIndices != 24 ||
     draws[0]->outputs[0] != colorTarget)
    return fail("clear/draw action or output is wrong");

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(state == NULL || state->vertexStorageBuffers.size() != 7 ||
     state->vertexStorageBuffers[4].resourceId != storage ||
     state->vertexStorageBuffers[4].byteOffset != 256 ||
     state->vertexStorageBuffers[4].byteSize != 512 ||
     state->vertexStorageBuffers[6].resourceId != storage ||
     state->vertexStorageBuffers[6].byteOffset != 320 ||
     state->vertexStorageBuffers[6].byteSize != 448 ||
     state->vertexStorageBuffers[0].resourceId != ResourceId() ||
     state->vertexStorageBuffers[1].resourceId != ResourceId() ||
     state->vertexStorageBuffers[2].resourceId != ResourceId() ||
     state->vertexStorageBuffers[3].resourceId != ResourceId() ||
     state->vertexStorageBuffers[5].resourceId != ResourceId())
    return fail("physical slots, ranges, or empty slots are wrong");
  for(const MetalPipe::VertexBuffer &buffer : state->vertexBuffers)
    if(buffer.resourceId == storage)
      return fail("vertex storage buffer was misclassified as an IA vertex buffer");
  const ShaderReflection *reflection = pipe.GetShaderReflection(ShaderStage::Vertex);
  if(reflection == NULL || !reflection->constantBlocks.empty() ||
     reflection->readOnlyResources.size() != 2 ||
     reflection->readOnlyResources[0].name != "positions" ||
     reflection->readOnlyResources[0].fixedBindNumber != 4 ||
     reflection->readOnlyResources[0].descriptorType != DescriptorType::Buffer ||
     reflection->readOnlyResources[0].isTexture ||
     reflection->readOnlyResources[1].name != "unusedPositions" ||
     reflection->readOnlyResources[1].fixedBindNumber != 6 ||
     reflection->readOnlyResources[1].descriptorType != DescriptorType::Buffer)
    return fail("storage buffer reflection was misclassified");
  const rdcarray<UsedDescriptor> allResources =
      pipe.GetReadOnlyResources(ShaderStage::Vertex, false);
  const rdcarray<UsedDescriptor> usedResources =
      pipe.GetReadOnlyResources(ShaderStage::Vertex, true);
  if(allResources.size() != 2 || usedResources.size() != 1 ||
     allResources[0].access.type != DescriptorType::Buffer ||
     allResources[0].access.index != 0 || allResources[0].access.byteOffset != 0xF04 ||
     allResources[0].access.staticallyUnused ||
     allResources[0].descriptor.resource != storage ||
     allResources[0].descriptor.byteOffset != 256 ||
     allResources[0].descriptor.byteSize != 512 ||
     allResources[1].access.type != DescriptorType::Buffer ||
     allResources[1].access.index != 1 || allResources[1].access.byteOffset != 0xF06 ||
     !allResources[1].access.staticallyUnused ||
     allResources[1].descriptor.resource != storage ||
     allResources[1].descriptor.byteOffset != 320 ||
     allResources[1].descriptor.byteSize != 448 ||
     usedResources[0].access.byteOffset != 0xF04 ||
     !pipe.GetConstantBlocks(ShaderStage::Vertex, true).empty())
    return fail("generic storage-buffer descriptors or used state are wrong");

  const bytebuf bytes = renderer->GetBufferData(storage, 0, 0);
  const float guard[4] = {2.0f, 2.0f, 2.0f, 2.0f};
  const float firstPosition[4] = {-1.0f, 0.0f, 0.0f, 1.0f};
  const float lastPosition[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  if(bytes.size() != 768 || memcmp(bytes.data(), guard, sizeof(guard)) != 0 ||
     memcmp(bytes.data() + 256, firstPosition, sizeof(firstPosition)) != 0 ||
     memcmp(bytes.data() + 256 + 23 * sizeof(firstPosition), lastPosition,
            sizeof(lastPosition)) != 0 ||
     memcmp(bytes.data() + 256 + 24 * sizeof(firstPosition), guard, sizeof(guard)) != 0 ||
     renderer->GetBufferData(storage, 768, 16).size() != 0 ||
     !HasUsage(renderer, storage, ResourceUsage::VS_Resource) ||
     HasUsage(renderer, storage, ResourceUsage::VertexBuffer))
    return fail("raw bytes, sentinels, out-of-range read, or resource usage are wrong");

  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 75, .025f, .035f, .055f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 75, .125f, .25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 75, .9375f, .8125f, 0.0f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 225, 1.0f, .125f, .0625f) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 225, .0625f, .875f, .1875f) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 100, 75, .025f, .035f, .055f))
    return fail("clear/draw/rewind pixels are wrong");
  if(savePath)
  {
    std::ofstream saved(savePath, std::ios::binary);
    saved.write((const char *)bytes.data(), bytes.size());
    if(!saved)
      return fail("raw buffer export failed");
  }
  return true;
}

static bool ValidatePointLineMeshPreview(IReplayController *renderer, WindowingData window)
{
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 264)
      vertexBuffer = buffer.resourceId;
  if(vertexBuffer == ResourceId())
    return true;

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 3)
    return false;

  for(size_t i = 0; i < 3; i++)
  {
    renderer->SetFrameEvent(draws[i]->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
    const rdcarray<BoundVBuffer> vbs = pipe.GetVBuffers();
    if(inputs.empty() || vbs.empty() || inputs[0].vertexBuffer != 0)
      return false;

    MeshDisplay mesh;
    mesh.type = MeshDataStage::VSIn;
    mesh.wireframeDraw = true;
    mesh.position.vertexResourceId = vertexBuffer;
    mesh.position.vertexByteOffset = vbs[0].byteOffset + inputs[0].byteOffset +
                                     draws[i]->vertexOffset * vbs[0].byteStride;
    mesh.position.vertexByteStride = vbs[0].byteStride;
    mesh.position.vertexByteSize = vbs[0].byteSize;
    mesh.position.format = inputs[0].format;
    mesh.position.topology = pipe.GetPrimitiveTopology();
    mesh.position.numIndices = draws[i]->numIndices;

    IReplayOutput *output = renderer->CreateOutput(window, ReplayOutputType::Mesh);
    if(output == NULL)
      return false;
    output->SetMeshDisplay(mesh);
    output->Display();
    const bytebuf pixels = output->ReadbackOutputTexture();
    output->Shutdown();
    if(pixels.size() != size_t(640 * 480 * 3))
      return false;
    size_t changed = 0;
    for(size_t p = 3; p + 2 < pixels.size(); p += 3)
      if(pixels[p] != pixels[0] || pixels[p + 1] != pixels[1] || pixels[p + 2] != pixels[2])
        changed++;
    if(changed < (i == 0 ? 30U : 60U))
    {
      fprintf(stderr, "T15 %s Mesh Viewer preview changed only %zu pixels\n",
              i == 0 ? "Point" : i == 1 ? "Line" : "Line Strip", changed);
      return false;
    }
  }
  return true;
}

static bool ValidateMeshPreview(IReplayController *renderer, WindowingData window)
{
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.empty())
    return true;

  const bool indexedFixture = draws.size() == 2 && (draws[0]->flags & ActionFlags::Indexed);
  const bool instancedFixture =
      draws.size() == 1 && (draws[0]->flags & ActionFlags::Instanced) &&
      draws[0]->numInstances == 3 && draws[0]->instanceOffset == 1;
  const bool indexedInstancedFixture =
      draws.size() == 1 && (draws[0]->flags & ActionFlags::Indexed) &&
      (draws[0]->flags & ActionFlags::Instanced) && draws[0]->numInstances == 2 &&
      draws[0]->baseVertex == 1;
  if(!indexedFixture && !instancedFixture && !indexedInstancedFixture)
    return true;

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
  const rdcarray<BoundVBuffer> vertexBuffers = pipe.GetVBuffers();
  const BoundVBuffer indexBuffer = pipe.GetIBuffer();
  if(inputs.empty() || inputs[0].vertexBuffer < 0 ||
     size_t(inputs[0].vertexBuffer) >= vertexBuffers.size())
    return false;

  const BoundVBuffer &vertexBuffer = vertexBuffers[inputs[0].vertexBuffer];
  MeshDisplay mesh;
  mesh.type = MeshDataStage::VSIn;
  mesh.wireframeDraw = true;
  mesh.curInstance = instancedFixture || indexedInstancedFixture ? 1 : 0;
  mesh.position.vertexResourceId = vertexBuffer.resourceId;
  mesh.position.vertexByteOffset = vertexBuffer.byteOffset + inputs[0].byteOffset;
  mesh.position.vertexByteStride = vertexBuffer.byteStride;
  mesh.position.vertexByteSize = vertexBuffer.byteSize;
  mesh.position.indexResourceId = indexBuffer.resourceId;
  mesh.position.indexByteOffset = indexBuffer.byteOffset;
  mesh.position.indexByteStride = indexBuffer.byteStride;
  mesh.position.indexByteSize = indexBuffer.byteSize;
  mesh.position.format = inputs[0].format;
  mesh.position.topology = pipe.GetPrimitiveTopology();
  mesh.position.numIndices = draws[0]->numIndices;
  mesh.position.baseVertex = draws[0]->baseVertex;

  IReplayOutput *output = renderer->CreateOutput(window, ReplayOutputType::Mesh);
  if(output == NULL)
    return false;
  output->SetMeshDisplay(mesh);
  output->Display();
  const bytebuf pixels = output->ReadbackOutputTexture();
  output->Shutdown();

  if(pixels.size() != size_t(640 * 480 * 3))
    return false;

  size_t changedPixels = 0;
  for(size_t i = 3; i + 2 < pixels.size(); i += 3)
  {
    if(pixels[i + 0] != pixels[0] || pixels[i + 1] != pixels[1] ||
       pixels[i + 2] != pixels[2])
      changedPixels++;
  }

  if(changedPixels < 100)
  {
    fprintf(stderr, "Metal mesh preview validation failed: only %zu pixels changed\n",
            changedPixels);
    return false;
  }
  return true;
}

int main(int argc, char **argv)
{
  if(argc != 3 && argc != 4 && argc != 6)
    return 2;

  GlobalEnvironment env;
  env.enumerateGPUs = false;
  rdcarray<rdcstr> args;
  args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(env, args);

  ICaptureFile *file = RENDERDOC_OpenCaptureFile();
  ResultDetails result = file->OpenFile(argv[1], "rdc", NULL);
  if(!result.OK())
    return 3;

  IReplayController *renderer = NULL;
  rdctie(result, renderer) = file->OpenCapture(ReplayOptions(), NULL);
  file->Shutdown();
  if(!result.OK() || renderer == NULL)
    return 4;

  TextureDisplay display;
  display.subresource = {0, 0, 0};
  display.scale = -1.0f;
  display.rangeMin = 0.0f;
  display.rangeMax = 1.0f;
  display.red = display.green = display.blue = true;
  display.alpha = false;

  TextureDescription swapBuffer;

  for(const TextureDescription &desc : renderer->GetTextures())
  {
    if(desc.creationFlags & TextureCategory::SwapBuffer)
    {
      display.resourceId = desc.resourceId;
      swapBuffer = desc;
      break;
    }
  }

  if(display.resourceId == ResourceId())
  {
    renderer->Shutdown();
    return 5;
  }

  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  CA::MetalLayer *layer = CA::MetalLayer::layer();
  layer->setDrawableSize(CGSizeMake(640, 480));

  WindowingData window = {};
  window.system = WindowingSystem::MacOS;
  window.macOS.layer = layer;

  IReplayOutput *output = renderer->CreateOutput(window, ReplayOutputType::Texture);
  if(output == NULL)
  {
    pool->release();
    renderer->Shutdown();
    return 6;
  }

  output->SetTextureDisplay(display);

  bool success = false;
  if(argc == 3 || argc == 4)
  {
    bool rayRefitFixture = false;
    bool rayInstanceFixture = false;
    bool boxRayFixture = false;
    bool boxRefitFixture = false;
    bool indexedRefitFixture = false;
    for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
    {
      rayRefitFixture |=
          chunk->name == "MTLAccelerationStructureCommandEncoder::refitTriangle" ||
          chunk->name == "MTLAccelerationStructureCommandEncoder::refitTriangleExtended" ||
          chunk->name == "MTLAccelerationStructureCommandEncoder::refitTriangleNoDuplicate" ||
          chunk->name == "MTLAccelerationStructureCommandEncoder::refitFormattedTriangle";
      rayInstanceFixture |=
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildInstance" ||
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildInstances" ||
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildDistinctInstances" ||
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildMultipleDistinctInstances" ||
          chunk->name == "MTLAccelerationStructureCommandEncoder::buildRepeatedDistinctInstances";
      boxRayFixture |=
          chunk->name == "MTLComputePipelineState::newIntersectionFunctionTableWithDescriptor";
      boxRefitFixture |=
          chunk->name == "MTLAccelerationStructureCommandEncoder::refitBoundingBox";
      indexedRefitFixture |=
          chunk->name == "MTLAccelerationStructureCommandEncoder::refitIndexedTriangle";
    }
    if(rayRefitFixture || rayInstanceFixture || boxRayFixture || boxRefitFixture ||
       indexedRefitFixture)
    {
      success = boxRefitFixture ? ValidateBoxRefitFixture(renderer) :
                indexedRefitFixture ? ValidateIndexedRefitFixture(renderer) :
                boxRayFixture ? ValidateBoxRayFixture(renderer) :
                rayRefitFixture ? ValidateRayRefitFixture(renderer) :
                                  ValidateRayInstanceFixture(renderer);
      if(success) success = WriteOutput(renderer, output, 10000000, argv[2]);
    }
    else
    {
    const bool commandCreationFixture = IsCommandCreationFixture(renderer);
    success = ValidateMetalEventSequence(renderer) &&
              ValidateCounterStageFixture(renderer, display.resourceId) &&
              ValidateDynamicLibraryFixture(renderer, display.resourceId) &&
              ValidateCommandCreationFixture(renderer) &&
              ValidateRenderInlineBatchFixture(renderer, display.resourceId) &&
              ValidateRenderDynamicStateFixture(renderer, display.resourceId) &&
              ValidateBlitTransferFixture(renderer, display.resourceId) &&
              ValidateBlitOptimizationFixture(renderer) &&
              ValidateResourceBarrierFixture(renderer, display.resourceId) &&
              ValidateCommandHandlersFixture(renderer, display.resourceId) &&
              ValidateTextureReadbackFixture(renderer, display.resourceId) &&
              ValidateAsyncCreationFixture(renderer, display.resourceId) &&
              ValidateFunctionVariantsFixture(renderer, display.resourceId) &&
              ValidateArgumentDataFixture(renderer, display.resourceId) &&
              ValidateTextureViewsFixture(renderer, display.resourceId) &&
              ValidateBufferTextureFixture(renderer, display.resourceId) &&
              ValidateDeviceArgumentEncoderFixture(renderer, display.resourceId) &&
              ValidateNoCopyBufferFixture(renderer, display.resourceId) &&
              ValidatePurgeableStateFixture(renderer, display.resourceId) &&
              ValidateDynamicVertexStrideFixture(renderer, display.resourceId) &&
              ValidateSharedTextureFixture(renderer, display.resourceId) &&
              ValidateHeapAliasFixture(renderer) &&
              ValidateAccelerationStructureFixture(renderer) &&
              ValidateTileFixture(renderer, display.resourceId) &&
              ValidateMeshFixture(renderer, display.resourceId) &&
              ValidateRateMapFixture(renderer, display.resourceId) &&
              ValidateObjectMeshFixture(renderer, display.resourceId) &&
              ValidateSharedTextureHandleFixture(renderer, display.resourceId) &&
              ValidateSharedEventFixture(renderer, display.resourceId) &&
              ValidateTessellationFixture(renderer, display.resourceId) &&
              ValidateTessellationVariantsFixture(renderer, display.resourceId) &&
              ValidateEventSyncFixture(renderer, display.resourceId) &&
              ValidateICBOperationsFixture(renderer, display.resourceId) &&
              ValidateBinaryLibraryFixture(renderer, display.resourceId) &&
              ValidateBinaryArchiveFixture(renderer, display.resourceId) &&
              ValidateStitchedLibraryFixture(renderer, display.resourceId) &&
              ValidatePipelineVariantsFixture(renderer, display.resourceId) &&
              ValidateFencePresentFixture(renderer, display.resourceId) &&
              ValidateSamplerLODFixture(renderer, display.resourceId) &&
              ValidatePrivateBufferFixture(renderer, display.resourceId) &&
              ValidateComputeInlineFixture(renderer, display.resourceId) &&
              ValidateComputeIndirectDispatchFixture(renderer, argc == 4 ? argv[3] : NULL) &&
              ValidateIndexedFixture(renderer, display.resourceId, swapBuffer) &&
              ValidateDynamicUniformFixture(renderer, display.resourceId) &&
              ValidateInstancedFixture(renderer, display.resourceId) &&
              ValidateMRTBlendFixture(renderer, display.resourceId) &&
              ValidateDepthStencilFixture(renderer, display.resourceId) &&
              ValidateMSAAResolveFixture(renderer, display.resourceId) &&
              ValidateTexturedFixture(renderer) &&
              ValidateTextureSubresourceFixture(renderer, output, display.resourceId,
                                                argc == 4 ? argv[3] : NULL) &&
              ValidateBlitFixture(renderer, display.resourceId, argc == 4 ? argv[3] : NULL) &&
              ValidateComputeFixture(renderer, display.resourceId, argc == 4 ? argv[3] : NULL) &&
              ValidateComputeSamplerFixture(renderer, display.resourceId,
                                            argc == 4 ? argv[3] : NULL) &&
              ValidateComputeBatchFixture(renderer, display.resourceId,
                                          argc == 4 ? argv[3] : NULL) &&
              ValidateDispatchThreadsFixture(renderer, display.resourceId,
                                             argc == 4 ? argv[3] : NULL) &&
              ValidateComputeBufferFixture(renderer, display.resourceId,
                                           argc == 4 ? argv[3] : NULL) &&
              ValidateArgumentBufferFixture(renderer, display.resourceId,
                                            argc == 4 ? argv[3] : NULL) &&
              ValidateIndirectFixture(renderer, display.resourceId,
                                      argc == 4 ? argv[3] : NULL) &&
              ValidateICBFixture(renderer, display.resourceId,
                                 argc == 4 ? argv[3] : NULL) &&
              ValidateInheritPipelineFixture(renderer, display.resourceId,
                                             argc == 4 ? argv[3] : NULL) &&
              ValidateInheritBuffersFixture(renderer, display.resourceId,
                                            argc == 4 ? argv[3] : NULL) &&
              ValidateMultiICBFixture(renderer, display.resourceId,
                                      argc == 4 ? argv[3] : NULL) &&
              ValidateICBResetFixture(renderer, display.resourceId,
                                      argc == 4 ? argv[3] : NULL) &&
              ValidateMixedICBFixture(renderer, display.resourceId,
                                      argc == 4 ? argv[3] : NULL) &&
              ValidateIndexedICBFixture(renderer, display.resourceId,
                                        argc == 4 ? argv[3] : NULL) &&
              ValidateIndexedInstancingFixture(renderer, display.resourceId,
                                               argc == 4 ? argv[3] : NULL) &&
              ValidateIndexedIndirectFixture(renderer, display.resourceId,
                                             argc == 4 ? argv[3] : NULL) &&
              ValidatePointLineFixture(renderer, display.resourceId,
                                       argc == 4 ? argv[3] : NULL) &&
              ValidateVertexTextureFixture(renderer, display.resourceId,
                                           argc == 4 ? argv[3] : NULL) &&
              ValidateBatchTextureFixture(renderer, display.resourceId,
                                          argc == 4 ? argv[3] : NULL) &&
              ValidateFragmentStorageBufferFixture(renderer, display.resourceId,
                                                  argc == 4 ? argv[3] : NULL) &&
              ValidateVertexStorageBufferFixture(renderer, display.resourceId,
                                                 argc == 4 ? argv[3] : NULL) &&
              ValidatePointLineMeshPreview(renderer, window) &&
              ValidateMeshPreview(renderer, window);
    bool meshFixture = false;
    for(const SDChunk *chunk : renderer->GetStructuredFile().chunks)
      meshFixture |= chunk->name == "MTLDevice::newRenderPipelineStateWithMeshDescriptor" ||
                     chunk->name ==
                         "MTLDevice::newRenderPipelineStateWithMeshDescriptor(completionHandler)" ||
                     chunk->name == "MTLDevice::newRenderPipelineStateWithObjectMeshDescriptor" ||
                     chunk->name ==
                         "MTLDevice::newRenderPipelineStateWithObjectMeshDescriptor(completionHandler)";
    if(success && !commandCreationFixture && !meshFixture)
    {
      rdcarray<const ActionDescription *> draws;
      FindDrawActions(renderer->GetRootActions(), draws);
      // Earlier draws may only write storage buffers with rasterization disabled.
      success = !draws.empty() &&
                ValidateHeadlessThumbnail(renderer, display.resourceId, draws.back()->eventId,
                                          true, {0, 0, 0}, true);
    }
    if(success)
    {
      output->SetTextureDisplay(display);
      success = WriteOutput(renderer, output, 10000000, argv[2]);
    }
    }
  }
  else
  {
    const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
    const ActionDescription *draw = FindAction(renderer->GetRootActions(), ActionFlags::Drawcall);
    if(!clear || !draw)
    {
      output->Shutdown();
      renderer->Shutdown();
      pool->release();
      RENDERDOC_ShutdownReplay();
      return 8;
    }

    success = ValidateShaders(renderer) &&
              ValidatePipelineState(renderer, swapBuffer.resourceId, clear->eventId,
                                    draw->eventId) &&
              ValidateTextureData(renderer, display.resourceId, swapBuffer, clear->eventId, true) &&
              ValidateTextureData(renderer, display.resourceId, swapBuffer, draw->eventId, false) &&
              ValidateTextureSave(renderer, display.resourceId, draw->eventId, argv[5]);
    for(int i = 0; i < 10 && success; i++)
    {
      success = WriteOutput(renderer, output, clear->eventId, argv[2]) &&
                WriteOutput(renderer, output, draw->eventId, argv[3]) &&
                WriteOutput(renderer, output, clear->eventId, argv[4]);
    }
  }

  if(success)
    success = ValidateFakePassMarkers(renderer);

  output->Shutdown();
  renderer->Shutdown();
  pool->release();
  RENDERDOC_ShutdownReplay();
  return success ? 0 : 7;
}
