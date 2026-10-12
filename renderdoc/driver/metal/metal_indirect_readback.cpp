// SPDX-License-Identifier: MIT
#include "metal_indirect_readback.h"
#include "common/common.h"
#include <cstring>
#include "metal_device.h"
#include "metal_command_buffer.h"
#include "metal_texture.h"
#include "metal_buffer.h"

MTL::ComputePipelineState *CreateMetalIndirectReadbackPipeline(MTL::Device *device)
{
  if(!device) return NULL;
  const char *code=R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void rdoc_indirect_readback(device const uint *source [[buffer(0)]],
                                  device uint *destination [[buffer(1)]])
{ destination[0]=source[0]; destination[1]=source[1]; destination[2]=source[2];
  destination[3]=0x52444349u; }
)MSL";
  NS::Error *error = NULL;
  auto library = NS::TransferPtr(device->newLibrary(NS::String::string(code, NS::UTF8StringEncoding), NULL, &error));
  if(!library) return NULL;
  auto function = NS::TransferPtr(library->newFunction(NS::String::string("rdoc_indirect_readback", NS::UTF8StringEncoding)));
  return function ? device->newComputePipelineState(function.get(), &error) : NULL;
}

MTL::ComputePipelineState *CreateMetalIndirectReplayPipeline(MTL::Device *device)
{
  if(!device) return NULL;
  const char *code=R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void rdoc_indirect_replay(device const uint *source [[buffer(0)]],
                                 device uint *snapshot [[buffer(1)]],
                                 constant uint *limits [[buffer(3)]])
{
  uint3 groups=uint3(source[0],source[1],source[2]);
  uint2 work=uint2(limits[0],0);
  bool valid=limits[0] && (!limits[2] || all(groups==uint3(limits[3],limits[4],limits[5])));
  // Original Native GPU counts are execution inputs, not CPU analysis work.
  // Preserve checked 64-bit extent arithmetic and the frozen RT invocation
  // contract. A zero axis executes no shader, just like a direct zero grid.
  if(all(groups != uint3(0)))
    for(uint i=0;i<3;i++)
    {
      // Use 32-bit limbs: Native pipeline compilation of dynamic ulong
      // division fails on some Metal compiler services. mulhi preserves exact
      // checked 64-bit multiplication without requiring 64-bit GPU division.
      uint high=work.y*groups[i],carry=mulhi(work.x,groups[i]);
      if(mulhi(work.y,groups[i]) || high > 0xffffffffu-carry) valid=false;
      if(valid) work=uint2(work.x*groups[i],high+carry);
    }
  snapshot[0]=groups.x; snapshot[1]=groups.y; snapshot[2]=groups.z;
  snapshot[3]=valid?0x52444349u:0u;
  snapshot[4]=valid?groups.x:0u;
  snapshot[5]=valid?groups.y:1u;
  snapshot[6]=valid?groups.z:1u;
}
)MSL";
  NS::Error *error=NULL;
  auto library=NS::TransferPtr(device->newLibrary(NS::String::string(code,NS::UTF8StringEncoding),NULL,&error));
  if(!library)
  { RDCERR("Metal indirect guard library compilation failed: %s", error ? error->localizedDescription()->utf8String() : "unknown"); return NULL; }
  auto function=NS::TransferPtr(library->newFunction(NS::String::string("rdoc_indirect_replay",NS::UTF8StringEncoding)));
  MTL::ComputePipelineState *pipeline=function?device->newComputePipelineState(function.get(),&error):NULL;
  if(!pipeline) RDCERR("Metal indirect guard pipeline compilation failed: %s", error ? error->localizedDescription()->utf8String() : "missing function");
  return pipeline;
}

void MetalComputeIndirectCapture::BindPipeline(MTL::ComputePipelineState *pipeline)
{
  m_Pipeline = NS::RetainPtr(pipeline);
}
void MetalComputeIndirectCapture::BindBuffer(MTL::Buffer *buffer, uint64_t offset, uint32_t index)
{
  if(index >= 2) return;
  auto &binding=m_Bindings[index];
  binding.buffer=NS::RetainPtr(buffer); binding.offset=offset; binding.bytes.clear(); binding.known=true;
}
void MetalComputeIndirectCapture::BindBytes(const rdcarray<byte> &data, uint32_t index)
{
  if(index >= 2) return;
  auto &binding=m_Bindings[index];
  binding.buffer.reset(); binding.offset=0; binding.bytes=data; binding.known=true;
}
void MetalComputeIndirectCapture::SetBufferOffset(uint64_t offset, uint32_t index)
{
  if(index >= 2) return;
  auto &binding=m_Bindings[index];
  binding.offset=offset;
  if(!binding.buffer || !binding.bytes.empty()) binding.known=false;
}
void MetalComputeIndirectCapture::InvalidateSlot(uint32_t index)
{
  if(index < 2) m_Bindings[index].known=false;
}
void MetalComputeIndirectCapture::Clear()
{
  m_Pipeline.reset(); m_CopyPipeline.reset(); m_ByteCopyPipeline.reset();
  for(auto &binding : m_Bindings) binding=Binding();
}

bool MetalComputeIndirectCapture::SnapshotBytes(MTL::Device *device,
    MTL::ComputeCommandEncoder *encoder, MTL::Buffer *source, MetalIndirectReadback &result,
    uint64_t offset, uint64_t length)
{
  if(source && !length && offset <= source->length()) length = source->length() - offset;
  if(!device || !encoder || !m_Pipeline || m_Pipeline->device() != device ||
     !source || source->device() != device || !length || length > 1048576 ||
     offset > source->length() || length > source->length() - offset)
    return false;
  for(const auto &binding : m_Bindings)
    if(!binding.known || binding.bytes.size() > 4096 || (!binding.buffer && binding.offset) ||
       (binding.buffer && (binding.buffer->device() != device || binding.offset >= binding.buffer->length())))
      return false;
  if(!m_ByteCopyPipeline)
  {
    const char *code = R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void rdoc_input_bytes(device const uchar *source [[buffer(0)]],
                             device uchar *destination [[buffer(1)]],
                             uint index [[thread_position_in_grid]])
{ destination[index] = source[index]; }
)MSL";
    NS::Error *error = NULL;
    auto library = NS::TransferPtr(device->newLibrary(NS::String::string(code, NS::UTF8StringEncoding), NULL, &error));
    auto function = library ? NS::TransferPtr(library->newFunction(NS::String::string("rdoc_input_bytes", NS::UTF8StringEncoding))) : NS::SharedPtr<MTL::Function>();
    if(function) m_ByteCopyPipeline = NS::TransferPtr(device->newComputePipelineState(function.get(), &error));
    if(!m_ByteCopyPipeline) return false;
  }
  auto saved = NS::TransferPtr(device->newBuffer(length, MTL::ResourceStorageModeShared));
  if(!saved) return false;
  encoder->memoryBarrier(MTL::BarrierScope(MTL::BarrierScopeBuffers | MTL::BarrierScopeTextures));
  encoder->setComputePipelineState(m_ByteCopyPipeline.get());
  encoder->setBuffer(source, offset, 0); encoder->setBuffer(saved.get(), 0, 1);
  encoder->dispatchThreads(MTL::Size(length, 1, 1), MTL::Size(64, 1, 1));
  encoder->memoryBarrier(MTL::BarrierScope(MTL::BarrierScopeBuffers | MTL::BarrierScopeTextures));
  encoder->setComputePipelineState(m_Pipeline.get());
  for(uint32_t slot = 0; slot < 2; slot++)
  {
    const auto &binding = m_Bindings[slot];
    if(!binding.bytes.empty()) encoder->setBytes(binding.bytes.data(), binding.bytes.size(), slot);
    else encoder->setBuffer(binding.buffer.get(), binding.offset, slot);
  }
  result.snapshot = saved; result.source = NS::RetainPtr(source); result.copyPipeline = m_ByteCopyPipeline;
  return true;
}

bool MetalComputeIndirectCapture::Snapshot(MTL::Device *device, MTL::ComputeCommandEncoder *encoder,
    MTL::Buffer *source, uint64_t offset, MetalIndirectReadback &result)
{
  if(!device || !encoder || !m_Pipeline || m_Pipeline->device()!=device || !source || source->device()!=device ||
     (offset & 3) || offset > source->length() || 12 > source->length()-offset)
    return false;
  for(const auto &binding : m_Bindings)
    if(!binding.known || binding.bytes.size()>4096 || (!binding.buffer && binding.offset) ||
       (binding.buffer && (binding.buffer->device()!=device || binding.offset>=binding.buffer->length())))
      return false;
  if(!m_CopyPipeline) m_CopyPipeline=NS::TransferPtr(CreateMetalIndirectReadbackPipeline(device));
  if(!m_CopyPipeline) return false;
  auto snapshot=NS::TransferPtr(device->newBuffer(16, MTL::ResourceStorageModeShared));
  if(!snapshot) return false;
  memset(snapshot->contents(), 0, 16);
  encoder->memoryBarrier(MTL::BarrierScope(MTL::BarrierScopeBuffers | MTL::BarrierScopeTextures));
  encoder->setComputePipelineState(m_CopyPipeline.get());
  encoder->setBuffer(source, offset, 0); encoder->setBuffer(snapshot.get(), 0, 1);
  encoder->dispatchThreadgroups(MTL::Size(1,1,1), MTL::Size(1,1,1));
  encoder->memoryBarrier(MTL::BarrierScope(MTL::BarrierScopeBuffers | MTL::BarrierScopeTextures));
  encoder->setComputePipelineState(m_Pipeline.get());
  for(uint32_t slot=0;slot<2;slot++) {
    const auto &binding=m_Bindings[slot];
    if(!binding.bytes.empty()) encoder->setBytes(binding.bytes.data(), binding.bytes.size(), slot);
    else encoder->setBuffer(binding.buffer.get(), binding.offset, slot);
  }
  result.snapshot=snapshot; result.source=NS::RetainPtr(source); result.copyPipeline=m_CopyPipeline;
  return true;
}

// Diagnostic locator only. Save one actual encoder-end output in its original
// command stream, before any subsequent command can change its physical bytes.
// Export on completion; never wait here or substitute application resources.
void SnapshotMetalDiagnosticTexture(WrappedMTLDevice *device,
    WrappedMTLCommandBuffer *owner, ResourceId encoder, const char *environmentPrefix)
{
  const rdcstr directory = Process::GetEnvVariable(StringFormat::Fmt("%s_DIR", environmentPrefix).c_str());
  if(directory.empty() || directory[0] != '/' ||
     Process::GetEnvVariable(StringFormat::Fmt("%s_ENCODER", environmentPrefix).c_str()) != ToStr(encoder)) return;
  const rdcstr selected = Process::GetEnvVariable(StringFormat::Fmt("%s_TEXTURE", environmentPrefix).c_str());
  char *end = NULL; const uint64_t number = strtoull(selected.c_str(), &end, 10);
  ResourceId id; memcpy(&id, &number, sizeof(id));
  auto resource = device->GetResourceManager()->GetResource(id, true);
  auto texture = resource && resource->m_Type == eResTexture && resource->m_Real ? Unwrap((WrappedMTLTexture *)resource) : NULL;
  uint32_t bw = 0, bh = 0, bytes = 0;
  if(!number || !end || *end || !texture || !owner || texture->framebufferOnly() ||
     texture->textureType() != MTL::TextureType2D || texture->arrayLength() != 1 ||
     texture->mipmapLevelCount() != 1 || texture->sampleCount() != 1 ||
     !texture->width() || !texture->height() ||
     !GetTextureDataBlockShape(texture->pixelFormat(), bw, bh, bytes) || bw != 1 || bh != 1 ||
     MakeResourceFormat(texture->pixelFormat()).compType == CompType::Depth) return;
  RDMTL::TextureDescriptor layout;
  layout.textureType = texture->textureType(); layout.pixelFormat = texture->pixelFormat();
  layout.width = texture->width(); layout.height = texture->height(); layout.depth = 1;
  layout.arrayLength = 1; layout.mipmapLevelCount = 1; layout.sampleCount = 1;
  layout.storageMode = texture->storageMode();
  uint64_t logical = 0;
  if(!MetalTextureReplayLayout(layout, logical)) return;
  auto nativeDevice = Unwrap(device);
  const uint64_t row = uint64_t(texture->width()) * bytes;
  const uint64_t alignment = RDCMAX<uint64_t>(1, nativeDevice->minimumLinearTextureAlignmentForPixelFormat(texture->pixelFormat()));
  if(row > UINT64_MAX - (alignment - 1)) return;
  const uint64_t pitch = AlignUp(row, alignment);
  if(!pitch || texture->height() > UINT64_MAX / pitch) return;
  MetalReplayAllocationBudget budget; budget.Add(budget.snapshotBytes, pitch * texture->height());
  if(!budget.Fits(nativeDevice->recommendedMaxWorkingSetSize(), nativeDevice->currentAllocatedSize())) return;
  auto saved = NS::TransferPtr(nativeDevice->newBuffer(pitch * texture->height(), MTL::ResourceStorageModeShared));
  auto retainedTexture = NS::RetainPtr(texture);
  auto command = Unwrap(owner);
  auto blit = saved && saved->contents() && command ? command->blitCommandEncoder() : NULL;
  if(!blit) return;
  blit->copyFromTexture(texture, 0, 0, MTL::Origin(0,0,0),
      MTL::Size(texture->width(), texture->height(), 1), saved.get(), 0, pitch,
      pitch * texture->height(), MTL::BlitOptionNone);
  blit->endEncoding();
  const uint64_t epoch = device->GetReplayEpoch(), height = texture->height();
  const rdcstr path = StringFormat::Fmt("%s/epoch-%llu-command-%s-encoder-%s-texture-%s.bin",
      directory.c_str(), epoch, ToStr(GetResID(owner)).c_str(), ToStr(encoder).c_str(), ToStr(id).c_str());
  command->addCompletedHandler([saved, retainedTexture, path, row, pitch, height, logical](MTL::CommandBuffer *completed) {
    if(completed->status() != MTL::CommandBufferStatusCompleted || completed->error()) return;
    FILE *file = fopen(path.c_str(), "wbx"); bool good = file != NULL;
    for(uint64_t y = 0; y < height && good; y++)
      good = fwrite((byte *)saved->contents() + y * pitch, 1, row, file) == row;
    if(file && fclose(file)) good = false;
    fprintf(stderr, "Metal diagnostic texture snapshot: bytes=%llu result=%s path=%s; Native stream copy, no CPU wait\n",
        logical, good ? "EXPORTED" : "FAILED", path.c_str());
  });
}

void SnapshotMetalDiagnosticResources(WrappedMTLDevice *device,
    WrappedMTLCommandBuffer *owner, ResourceId encoder, const rdcstr &pipelineLabel,
    bool capture, const char *prefix, const rdcarray<ResourceId> &residentResources)
{
  const rdcstr directory = Process::GetEnvVariable(StringFormat::Fmt("%s_DIR", prefix).c_str());
  const rdcstr selected = Process::GetEnvVariable(StringFormat::Fmt("%s_ENCODER", prefix).c_str());
  const rdcstr label = Process::GetEnvVariable(StringFormat::Fmt("%s_PIPELINE", prefix).c_str());
  if(directory.empty() || directory[0] != '/' || !owner ||
     (!selected.empty() ? selected != ToStr(encoder) :
       label.empty() || label != pipelineLabel)) return;
  const rdcstr request = Process::GetEnvVariable(StringFormat::Fmt("%s_RESOURCES", prefix).c_str());
  struct Copy
  {
    ResourceId id;
    NS::SharedPtr<MTL::Resource> source;
    NS::SharedPtr<MTL::Buffer> saved;
    bool texture = false, linear = false;
    uint64_t offset = 0, row = 0, pitch = 0, height = 1, depth = 1, width = 0;
    uint64_t bytes = 0, staging = 0, format = 0, type = 0;
  };
  rdcarray<Copy> copies;
  uint64_t total = 0;
  auto fail = [&](const char *reason) {
    fprintf(stderr, "Metal boundary observation FAILED: encoder=%s reason=%s request=%s; no application input substituted\n",
        ToStr(encoder).c_str(), reason, request.c_str());
  };
  const char *cursor = request.c_str();
  if(!*cursor) { fail("empty-request"); return; }
  while(*cursor)
  {
    Copy copy;
    bool resident = false;
    if(!strncmp(cursor, "texture-resident:", 17)) { copy.texture = true; resident = true; cursor += 17; }
    else if(!strncmp(cursor, "buffer-resident:", 16)) { resident = true; cursor += 16; }
    else if(!strncmp(cursor, "texture:", 8)) { copy.texture = true; cursor += 8; }
    else if(!strncmp(cursor, "buffer:", 7)) cursor += 7;
    else { fail("request-kind"); return; }
    char *end = NULL;
    const uint64_t number = strtoull(cursor, &end, 10);
    if((!resident && !number) || cursor == end || copies.size() >= 16) { fail("request-identity-or-count"); return; }
    if(resident)
    {
      if(number >= residentResources.size()) { fail("residency-ordinal-unavailable"); return; }
      copy.id = residentResources[(size_t)number];
    }
    else memcpy(&copy.id, &number, sizeof(copy.id));
    cursor = end;
    auto object = device->GetResourceManager()->GetResource(copy.id, true);
    if(!object || !object->m_Real) { fail("resource-unavailable"); return; }
    if(copy.texture)
    {
      if(object->m_Type != eResTexture) { fail("texture-identity"); return; }
      MTL::Texture *texture = Unwrap((WrappedMTLTexture *)object);
      uint32_t bw = 0, bh = 0, bytes = 0;
      copy.type = texture->textureType(); copy.format = texture->pixelFormat();
      if(texture->framebufferOnly() || texture->sampleCount() != 1 ||
         texture->mipmapLevelCount() != 1 || texture->arrayLength() != 1 ||
         (copy.type != MTL::TextureType2D && copy.type != MTL::TextureType3D &&
          copy.type != MTL::TextureTypeTextureBuffer && copy.type != MTL::TextureType1D) ||
         !GetTextureDataBlockShape(texture->pixelFormat(), bw, bh, bytes) || bw != 1 || bh != 1 ||
         MakeResourceFormat(texture->pixelFormat()).compType == CompType::Depth)
      { fail("texture-logical-layout-unobserved"); return; }
      copy.width = texture->width(); copy.height = texture->height(); copy.depth = texture->depth();
      copy.row = copy.width * bytes;
      const uint64_t alignment = RDCMAX(1ULL, uint64_t(Unwrap(device)->minimumLinearTextureAlignmentForPixelFormat(texture->pixelFormat())));
      copy.pitch = AlignUp(copy.row, alignment);
      copy.linear = texture->buffer() != NULL;
      if(copy.linear)
      {
        if(copy.height != 1 || copy.depth != 1) { fail("buffer-texture-layout"); return; }
        copy.offset = texture->bufferOffset(); copy.pitch = copy.row;
        if(copy.offset > texture->buffer()->length() || copy.row > texture->buffer()->length() - copy.offset)
        { fail("buffer-texture-range"); return; }
      }
      if(!copy.row || !copy.pitch || !copy.height || !copy.depth ||
         copy.depth > (128ULL << 20) / copy.pitch / copy.height)
      { fail("texture-observation-budget"); return; }
      copy.bytes = copy.row * copy.height * copy.depth;
      copy.staging = copy.pitch * copy.height * copy.depth;
      copy.source = NS::RetainPtr((MTL::Resource *)texture);
    }
    else
    {
      if(*cursor != ':') { fail("buffer-offset"); return; } cursor++;
      copy.offset = strtoull(cursor, &end, 10);
      if(cursor == end || *end != ':') { fail("buffer-offset"); return; } cursor = end + 1;
      copy.bytes = strtoull(cursor, &end, 10);
      if(cursor == end || !copy.bytes || object->m_Type != eResBuffer) { fail("buffer-length-or-identity"); return; }
      cursor = end;
      MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)object);
      if((copy.offset & 3) || (copy.bytes & 3) || copy.offset > buffer->length() ||
         copy.bytes > buffer->length() - copy.offset) { fail("buffer-native-range"); return; }
      copy.row = copy.pitch = copy.staging = copy.bytes;
      copy.source = NS::RetainPtr((MTL::Resource *)buffer);
    }
    if((*cursor && *cursor != ',') || copy.staging > (128ULL << 20) - total)
    { fail("request-separator-or-aggregate-budget"); return; }
    total += copy.staging; copies.push_back(copy);
    if(*cursor) { cursor++; if(!*cursor) { fail("trailing-separator"); return; } }
  }
  const uint64_t recommended = Unwrap(device)->recommendedMaxWorkingSetSize();
  const uint64_t allocated = Unwrap(device)->currentAllocatedSize();
  // Capture observes an already-running Native application. Its existing
  // allocations are not a replay frame awaiting admission. Keep the explicit
  // 128 MiB staging cap and reserve Native working-set headroom; replay still
  // uses the stricter replay allocation policy. Shared staging is GPU memory
  // as well as CPU-visible snapshot storage.
  MetalReplayAllocationBudget budget;
  budget.Add(budget.nativeBytes, total); budget.Add(budget.snapshotBytes, total);
  const uint64_t reserve = 128ULL << 20;
  const bool captureFits = recommended >= reserve && allocated <= recommended - reserve &&
      total <= recommended - reserve - allocated;
  if(capture ? !captureFits : !budget.Fits(recommended, allocated))
  {
    fprintf(stderr, "Metal boundary allocation: capture=%d allocated=%llu staging=%llu recommended=%llu reserve=%llu\n",
        capture, allocated, total, recommended, reserve);
    fail("native-allocation-budget"); return;
  }
  for(Copy &copy : copies)
  {
    copy.saved = NS::TransferPtr(Unwrap(device)->newBuffer(copy.staging, MTL::ResourceStorageModeShared));
    if(!copy.saved || !copy.saved->contents()) { fail("snapshot-allocation"); return; }
  }
  auto command = Unwrap(owner);
  auto blit = command ? command->blitCommandEncoder() : NULL;
  if(!blit) { fail("native-blit-encoder"); return; }
  for(const Copy &copy : copies)
  {
    if(copy.texture && !copy.linear)
      blit->copyFromTexture((MTL::Texture *)copy.source.get(), 0, 0, MTL::Origin(0,0,0),
          MTL::Size(copy.width,copy.height,copy.depth), copy.saved.get(), 0, copy.pitch,
          copy.pitch * copy.height, MTL::BlitOptionNone);
    else
      blit->copyFromBuffer(copy.texture ? ((MTL::Texture *)copy.source.get())->buffer() :
          (MTL::Buffer *)copy.source.get(), copy.offset, copy.saved.get(), 0, copy.bytes);
  }
  blit->endEncoding();
  const uint64_t epoch = capture ? device->GetCaptureEpoch() : device->GetReplayEpoch();
  const ResourceId commandID = GetResID(owner);
  command->addCompletedHandler([copies, directory, commandID, encoder, epoch, capture](MTL::CommandBuffer *completed) {
    if(completed->status() != MTL::CommandBufferStatusCompleted || completed->error())
    { fprintf(stderr, "Metal boundary observation FAILED: native-command-not-completed\n"); return; }
    for(const Copy &copy : copies)
    {
      const rdcstr path = StringFormat::Fmt("%s/%s-epoch-%llu-command-%s-encoder-%s-%s-%s-offset-%llu-length-%llu.bin",
          directory.c_str(), capture ? "capture" : "replay", epoch, ToStr(commandID).c_str(),
          ToStr(encoder).c_str(), copy.texture ? "texture" : "buffer", ToStr(copy.id).c_str(), copy.offset, copy.bytes);
      FILE *file = fopen(path.c_str(), "wbx"); bool good = file != NULL;
      for(uint64_t z = 0; z < copy.depth && good; z++)
        for(uint64_t y = 0; y < copy.height && good; y++)
          good = fwrite((byte *)copy.saved->contents() + (z * copy.height + y) * copy.pitch, 1, copy.row, file) == copy.row;
      if(file && fclose(file)) good = false;
      fprintf(stderr, "Metal boundary observation: encoder=%s resource=%s kind=%s format=%llu type=%llu extent=%llux%llux%llu offset=%llu logical=%llu pitch=%llu result=%s path=%s; original command completion, no CPU wait\n",
          ToStr(encoder).c_str(), ToStr(copy.id).c_str(), copy.texture ? "texture" : "buffer", copy.format,
          copy.type, copy.width, copy.height, copy.depth, copy.offset, copy.bytes, copy.pitch, good ? "EXPORTED" : "FAILED", path.c_str());
    }
  });
}
