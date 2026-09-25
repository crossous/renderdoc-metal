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
                                      const Subresource &sub = {0, 0, 0})
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
  const bool hasContent = pixels[center] || pixels[center + 1] || pixels[center + 2];
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
  if(mipTexture.resourceId == ResourceId() && arrayTexture.resourceId == ResourceId() &&
     cubeTexture.resourceId == ResourceId())
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
  if(!(draw->flags & ActionFlags::Instanced) || draw->numIndices != 3 ||
     draw->numInstances != 2 || draw->baseVertex != 1 ||
     draw->instanceOffset != 1 || draw->indexOffset != 0)
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
     vertices[1].resourceId == ResourceId() || !state->vertexBuffers[1].perInstance)
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
  for(const BufferDescription &buffer : renderer->GetBuffers())
    if(buffer.length == 52)
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
    success = ValidateMetalEventSequence(renderer) &&
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
    if(success)
    {
      const ActionDescription *lastDraw = FindAction(renderer->GetRootActions(), ActionFlags::Drawcall);
      success = lastDraw &&
                ValidateHeadlessThumbnail(renderer, display.resourceId, lastDraw->eventId, true);
    }
    if(success)
    {
      output->SetTextureDisplay(display);
      success = WriteOutput(renderer, output, 10000000, argv[2]);
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
