/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2026 Baldur Karlsson
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

#include "metal_replay.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "replay/dummy_driver.h"
#include "serialise/rdcfile.h"
#include "metal_buffer.h"
#include "metal_argument_encoder.h"
#include "metal_acceleration_structure.h"
#include "metal_visible_function_table.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_compute_pipeline_state.h"
#include "metal_texture.h"
#include "metal_sampler_state.h"
#include "metal_render_command_encoder.h"
#include "metal_parallel_render_command_encoder.h"
#include "metal_command_buffer.h"
#include "metal_rate_map.h"
#include <cmath>
#include <cfloat>

MetalReplay::MetalReplay(WrappedMTLDevice *wrappedMTLDevice)
{
  m_pDriver = wrappedMTLDevice;
  m_DriverInfo.vendor = GPUVendor::Unknown;
  memset(m_DriverInfo.version, 0, sizeof(m_DriverInfo.version));
  snprintf(m_DriverInfo.version, sizeof(m_DriverInfo.version), "Apple Metal");
}

MetalReplay::~MetalReplay()
{
  ResetMetalFXTemporal();
  for(auto &entry : m_ShaderPipelineTemplates)
    entry.second.descriptor->release();
  for(auto &entry : m_TargetShaderLibraries)
    entry.second->release();
  ClearPendingComputeIndirectActions();
  if(m_IndirectReadbackPipeline) m_IndirectReadbackPipeline->release();
  if(m_IndirectReplayPipeline) m_IndirectReplayPipeline->release();
  for(auto &entry : m_TextureViewSourceInitial)
    if(entry.second.privateCopy)
      entry.second.privateCopy->release();
  for(auto &it : m_OutputWindows)
  {
    if(it.second.texture)
      it.second.texture->release();
    if(it.second.layer)
      it.second.layer->release();
  }
  m_OutputWindows.clear();

  if(m_OutputPipeline)
    m_OutputPipeline->release();
  if(m_RawOutputPipeline)
    m_RawOutputPipeline->release();
  if(m_BackgroundPipeline)
    m_BackgroundPipeline->release();
  if(m_MeshPipeline)
    m_MeshPipeline->release();
  if(m_OutputQueue)
    m_OutputQueue->release();
}

void MetalReplay::Shutdown()
{
  delete m_pDriver;
}

IReplayDriver *MetalReplay::MakeDummyDriver()
{
  // Use the shared failure driver, as D3D12/Vulkan do. Metal stores reflections by value,
  // so give the dummy owned copies before the native replay device is destroyed.
  rdcarray<const ShaderReflection *> shaders;
  for(const auto &entry : m_Shaders)
    shaders.push_back(new ShaderReflection(entry.second));
  return new DummyDriver(this, shaders, m_pDriver->DetachStructuredFile(), {});
}

APIProperties MetalReplay::GetAPIProperties()
{
  APIProperties ret;
  ret.pipelineType = GraphicsAPI::Metal;
  ret.localRenderer = GraphicsAPI::Metal;
  ret.vendor = GPUVendor::Unknown;
  ret.remoteReplay = m_Proxy;
  ret.degraded = false;
  ret.shaderDebugging = false;
  ret.pixelHistory = false;
  return ret;
}

rdcarray<GPUDevice> MetalReplay::GetAvailableGPUs()
{
  if(!m_pDriver || !Unwrap(m_pDriver))
    return {};

  GPUDevice gpu;
  gpu.vendor = GPUVendor::Unknown;
  gpu.name = Unwrap(m_pDriver)->name()->utf8String();
  gpu.apis = {GraphicsAPI::Metal};
  return {gpu};
}

ResourceDescription &MetalReplay::GetResourceDesc(ResourceId id)
{
  auto it = m_ResourceIdx.find(id);
  if(it == m_ResourceIdx.end())
  {
    m_ResourceIdx[id] = m_Resources.size();
    m_Resources.push_back(ResourceDescription());
    m_Resources.back().resourceId = id;
    return m_Resources.back();
  }

  return m_Resources[it->second];
}

void MetalReplay::AddBuffer(ResourceId id, uint64_t length)
{
  for(BufferDescription &buf : m_Buffers)
  {
    if(buf.resourceId == id)
    {
      buf.length = length;
      return;
    }
  }

  BufferDescription desc;
  desc.resourceId = id;
  desc.length = length;
  m_Buffers.push_back(desc);
}

static TextureType MakeShaderTextureType(MTL::TextureType type);

void MetalReplay::AddTexture(ResourceId id, MTL::Texture *texture, bool swapBuffer, MTL::StorageMode originalStorage)
{
  m_TextureStorage[id] = originalStorage == MTL::StorageMode(0xff) ? texture->storageMode() : originalStorage;
  TextureDescription desc;
  desc.resourceId = id;
  desc.dimension = texture->textureType() == MTL::TextureType3D ? 3 : 2;
  desc.type = MakeShaderTextureType(texture->textureType());
  desc.width = (uint32_t)texture->width();
  desc.height = (uint32_t)texture->height();
  desc.depth = (uint32_t)texture->depth();
  desc.mips = (uint32_t)texture->mipmapLevelCount();
  desc.cubemap = texture->textureType() == MTL::TextureTypeCube ||
                 texture->textureType() == MTL::TextureTypeCubeArray;
  desc.arraysize = (uint32_t)texture->arrayLength() * (desc.cubemap ? 6U : 1U);
  desc.msSamp = (uint32_t)texture->sampleCount();
  desc.msQual = 0;
  desc.format = MakeResourceFormat(texture->pixelFormat());
  desc.byteSize = 0;
  for(uint32_t mip = 0; mip < desc.mips; mip++)
    desc.byteSize += GetByteSize(desc.width, desc.height, desc.depth, texture->pixelFormat(), mip) *
                     RDCMAX(1U, desc.msSamp) * RDCMAX(1U, desc.arraysize);
  desc.creationFlags = TextureCategory::NoFlags;
  if(texture->usage() & MTL::TextureUsageShaderRead)
    desc.creationFlags |= TextureCategory::ShaderRead;
  if(texture->usage() & MTL::TextureUsageShaderWrite)
    desc.creationFlags |= TextureCategory::ShaderReadWrite;
  if(texture->usage() & MTL::TextureUsageRenderTarget)
    desc.creationFlags |= TextureCategory::ColorTarget;
  if(swapBuffer)
    desc.creationFlags |= TextureCategory::SwapBuffer;

  for(TextureDescription &tex : m_Textures)
  {
    if(tex.resourceId == id)
    {
      tex = desc;
      return;
    }
  }

  m_Textures.push_back(desc);
}

void MetalReplay::RegisterTextureViewSource(ResourceId id)
{
  m_TextureViewSourceInitial.emplace(id, TextureViewSourceInitial());
}

bool MetalReplay::SnapshotTextureViewSources()
{
  // Views alias their parent allocation. Keep one initial snapshot of each source, before the
  // frame's GPU writes, so partial replay can restore every mip/slice seen through any view.
  for(auto &entry : m_TextureViewSourceInitial)
  {
    if(entry.second.capturedParent || !entry.second.sharedData.empty() || entry.second.privateCopy)
      continue;
    if(m_pDriver->HasReplayTextureInitialContents(entry.first))
    {
      // RestoreReplayTextureInitialContents already restores the shared base allocation
      // before every seek. Views observe that allocation; no duplicate snapshot is needed.
      entry.second.capturedParent = true;
      continue;
    }
    WrappedMTLObject *object = m_pDriver->GetResourceManager()->GetResource(entry.first, true);
    if(!object || object->m_Type != eResTexture || !object->m_Real)
      return false;
    MTL::Texture *texture = Unwrap((WrappedMTLTexture *)object);
    const uint64_t mips = texture->mipmapLevelCount();
    const MTL::TextureType type = texture->textureType();
    const uint64_t slices = type == MTL::TextureTypeCube ? 6 :
                            type == MTL::TextureType2D ? 1 : texture->arrayLength();
    if(texture->storageMode() == MTL::StorageModePrivate)
    {
      if((type != MTL::TextureType2D && type != MTL::TextureType2DArray &&
          type != MTL::TextureTypeCube) ||
         mips == 0 || mips > 16 || slices == 0 || slices > 16 || mips * slices > 256 ||
         texture->sampleCount() != 1 || texture->width() > 8192 || texture->height() > 8192)
        return false;
      MTL::Device *device = Unwrap(m_pDriver);
      MTL::TextureDescriptor *descriptor = MTL::TextureDescriptor::alloc()->init();
      descriptor->setTextureType(type);
      descriptor->setPixelFormat(texture->pixelFormat());
      descriptor->setWidth(texture->width());
      descriptor->setHeight(texture->height());
      descriptor->setDepth(texture->depth());
      descriptor->setMipmapLevelCount(mips);
      descriptor->setArrayLength(texture->arrayLength());
      descriptor->setSampleCount(1);
      descriptor->setStorageMode(MTL::StorageModePrivate);
      // This allocation is only used by blits. ShaderWrite/RenderTarget usages can
      // reject otherwise valid sampled formats and are not needed for the baseline copy.
      descriptor->setUsage(MTL::TextureUsage(MTL::TextureUsageShaderRead |
                                             MTL::TextureUsagePixelFormatView));
      MTL::Texture *copy = device->newTexture(descriptor);
      descriptor->release();
      if(!copy || !InitialiseOutputResources())
      {
        if(copy) copy->release();
        return false;
      }
      MTL::CommandBuffer *command = m_OutputQueue->commandBuffer();
      MTL::BlitCommandEncoder *blit = command ? command->blitCommandEncoder() : NULL;
      if(!blit)
      {
        copy->release();
        return false;
      }
      for(uint64_t slice = 0; slice < slices; slice++)
        for(uint64_t mip = 0; mip < mips; mip++)
          blit->copyFromTexture(texture, slice, mip, MTL::Origin::Make(0, 0, 0),
                                MTL::Size::Make(RDCMAX(1ULL, texture->width() >> mip),
                                                RDCMAX(1ULL, texture->height() >> mip), 1),
                                copy, slice, mip, MTL::Origin::Make(0, 0, 0));
      blit->endEncoding();
      command->commit();
      command->waitUntilCompleted();
      if(command->status() == MTL::CommandBufferStatusError)
      {
        copy->release();
        return false;
      }
      entry.second.privateCopy = copy;
      continue;
    }
    if(texture->storageMode() != MTL::StorageModeShared || mips == 0 || mips > 16 ||
       slices == 0 || slices > 2048 || mips * slices > 4096)
      return false;
    uint64_t total = 0;
    for(uint64_t slice = 0; slice < slices; slice++)
      for(uint64_t mip = 0; mip < mips; mip++)
      {
        const uint64_t width = RDCMAX(1ULL, texture->width() >> mip);
        const uint64_t height = RDCMAX(1ULL, texture->height() >> mip);
        if(width > UINT64_MAX / 4 || height > (64ULL << 20) / (width * 4))
          return false;
        const uint64_t bytes = width * height * 4;
        if(bytes > (64ULL << 20) - total)
          return false;
        total += bytes;
        bytebuf data;
        data.resize(bytes);
        const MTL::Region region = MTL::Region::Make2D(0, 0, width, height);
        if(texture->textureType() == MTL::TextureType2D)
          texture->getBytes(data.data(), width * 4, region, mip);
        else
          texture->getBytes(data.data(), width * 4, bytes, region, mip, slice);
        entry.second.sharedData.push_back(data);
      }
  }
  return true;
}

bool MetalReplay::ResetTextureViewSources()
{
  for(const auto &entry : m_TextureViewSourceInitial)
  {
    if(entry.second.capturedParent) continue;
    WrappedMTLObject *object = m_pDriver->GetResourceManager()->GetResource(entry.first, true);
    if(!object || object->m_Type != eResTexture || !object->m_Real)
      return false;
    MTL::Texture *texture = Unwrap((WrappedMTLTexture *)object);
    const uint64_t mips = texture->mipmapLevelCount();
    const MTL::TextureType type = texture->textureType();
    const uint64_t slices = type == MTL::TextureTypeCube ? 6 :
                            type == MTL::TextureType2D ? 1 : texture->arrayLength();
    if(entry.second.privateCopy)
    {
      if(!InitialiseOutputResources()) return false;
      MTL::CommandBuffer *command = m_OutputQueue->commandBuffer();
      MTL::BlitCommandEncoder *blit = command ? command->blitCommandEncoder() : NULL;
      if(!blit) return false;
      for(uint64_t slice = 0; slice < slices; slice++)
        for(uint64_t mip = 0; mip < mips; mip++)
          blit->copyFromTexture(entry.second.privateCopy, slice, mip,
                                MTL::Origin::Make(0, 0, 0),
                                MTL::Size::Make(RDCMAX(1ULL, texture->width() >> mip),
                                                RDCMAX(1ULL, texture->height() >> mip), 1),
                                texture, slice, mip, MTL::Origin::Make(0, 0, 0));
      blit->endEncoding();
      command->commit();
      command->waitUntilCompleted();
      if(command->status() == MTL::CommandBufferStatusError) return false;
      continue;
    }
    if(entry.second.sharedData.size() != mips * slices)
      return false;
    for(uint64_t slice = 0; slice < slices; slice++)
      for(uint64_t mip = 0; mip < mips; mip++)
      {
        const uint64_t width = RDCMAX(1ULL, texture->width() >> mip);
        const uint64_t height = RDCMAX(1ULL, texture->height() >> mip);
        const bytebuf &data = entry.second.sharedData[slice * mips + mip];
        if(data.size() != width * height * 4)
          return false;
        const MTL::Region region = MTL::Region::Make2D(0, 0, width, height);
        if(texture->textureType() == MTL::TextureType2D)
          texture->replaceRegion(region, mip, data.data(), width * 4);
        else
          texture->replaceRegion(region, mip, slice, data.data(), width * 4, data.size());
      }
  }
  return true;
}

BufferDescription MetalReplay::GetBuffer(ResourceId id)
{
  for(const BufferDescription &buf : m_Buffers)
    if(buf.resourceId == id)
      return buf;
  return {};
}

TextureDescription MetalReplay::GetTexture(ResourceId id)
{
  for(const TextureDescription &tex : m_Textures)
    if(tex.resourceId == id)
      return tex;
  return {};
}

void MetalReplay::AddShaderLibrary(ResourceId id, const rdcstr &source)
{
  m_LibrarySources[id] = source;
}

static ShaderStage MakeShaderStage(MTL::FunctionType type)
{
  switch(type)
  {
    case MTL::FunctionTypeVertex: return ShaderStage::Vertex;
    case MTL::FunctionTypeFragment: return ShaderStage::Fragment;
    case MTL::FunctionTypeKernel: return ShaderStage::Compute;
    case MTL::FunctionTypeVisible: return ShaderStage::Callable;
    case MTL::FunctionTypeIntersection: return ShaderStage::Intersection;
    case MTL::FunctionTypeMesh: return ShaderStage::Mesh;
    case MTL::FunctionTypeObject: return ShaderStage::Task;
  }

  return ShaderStage::Invalid;
}

static uint32_t FindEntryLine(const rdcstr &source, const rdcstr &entryPoint)
{
  int32_t offset = source.find(entryPoint);
  if(offset < 0)
    return 0;

  uint32_t line = 1;
  for(int32_t i = 0; i < offset; i++)
    if(source[(size_t)i] == '\n')
      line++;
  return line;
}

void MetalReplay::AddShader(ResourceId id, ResourceId library, MTL::Function *function,
                            const rdcstr &entryPoint)
{
  if(id == ResourceId() || function == NULL || entryPoint.empty())
    return;

  ShaderReflection &reflection = m_Shaders[id];
  reflection = ShaderReflection();
  m_ShaderBindingUsage[id] = ShaderBindingUsage();
  m_ShaderLibraries[id] = library;
  if(!m_FunctionObjects.count(id)) m_FunctionObjects[id] = function;
  reflection.debugInfo.compileFlags.flags.push_back({"@metal_shader", ToStr(id)});
  reflection.resourceId = id;
  reflection.entryPoint = entryPoint;
  reflection.stage = MakeShaderStage(function->functionType());
  reflection.debugInfo.entrySourceName = entryPoint;
  reflection.debugInfo.debuggable = false;
  reflection.debugInfo.debugStatus = "Metal shader debugging is not implemented.";

  auto binary = m_LibraryBinaries.find(library);
  if(binary != m_LibraryBinaries.end())
  {
    reflection.rawBytes = binary->second;
    reflection.encoding = ShaderEncoding::MetalLib;
  }
  auto debug = m_LibraryDebugSources.find(library);
  if(debug != m_LibraryDebugSources.end() && !debug->second.empty())
  {
    reflection.debugInfo.encoding = ShaderEncoding::MSL;
    reflection.debugInfo.files = debug->second;
    for(size_t i = 0; i < debug->second.size(); i++)
      if(debug->second[i].contents.contains(entryPoint + "("))
      {
        reflection.debugInfo.editBaseFile = (int32_t)i;
        reflection.debugInfo.entryLocation.fileIndex = (int32_t)i;
        reflection.debugInfo.entryLocation.lineStart = FindEntryLine(debug->second[i].contents, entryPoint);
        reflection.debugInfo.entryLocation.lineEnd = reflection.debugInfo.entryLocation.lineStart;
        break;
      }
  }
  auto source = m_LibrarySources.find(library);
  if(source != m_LibrarySources.end() && !source->second.empty())
  {
    reflection.encoding = ShaderEncoding::MSL;
    reflection.rawBytes.assign((const byte *)source->second.data(), source->second.size());
    reflection.debugInfo.encoding = ShaderEncoding::MSL;
    reflection.debugInfo.files.push_back({"captured.metal", source->second});
    reflection.debugInfo.editBaseFile = 0;
    reflection.debugInfo.entryLocation.fileIndex = 0;
    reflection.debugInfo.entryLocation.lineStart = FindEntryLine(source->second, entryPoint);
    reflection.debugInfo.entryLocation.lineEnd = reflection.debugInfo.entryLocation.lineStart;
  }
}

static TextureType MakeShaderTextureType(MTL::TextureType type)
{
  switch(type)
  {
    case MTL::TextureType1D: return TextureType::Texture1D;
    case MTL::TextureType1DArray: return TextureType::Texture1DArray;
    case MTL::TextureType2D: return TextureType::Texture2D;
    case MTL::TextureType2DArray: return TextureType::Texture2DArray;
    case MTL::TextureType2DMultisample: return TextureType::Texture2DMS;
    case MTL::TextureTypeCube: return TextureType::TextureCube;
    case MTL::TextureTypeCubeArray: return TextureType::TextureCubeArray;
    case MTL::TextureType3D: return TextureType::Texture3D;
    case MTL::TextureType2DMultisampleArray: return TextureType::Texture2DMSArray;
    case MTL::TextureTypeTextureBuffer: return TextureType::Buffer;
  }

  return TextureType::Unknown;
}

void MetalReplay::AddShaderBindings(ResourceId shader, NS::Array *arguments)
{
  auto shaderIt = m_Shaders.find(shader);
  if(shaderIt == m_Shaders.end())
    return;

  ShaderReflection &reflection = shaderIt->second;
  reflection.constantBlocks.clear();
  reflection.samplers.clear();
  reflection.readOnlyResources.clear();
  reflection.readWriteResources.clear();
  m_ShaderArgumentSlots.erase(shader);
  m_ShaderBufferMinimums[shader].clear();
  m_ShaderIndirectReadOnly[shader] = true;

  ShaderBindingUsage &usage = m_ShaderBindingUsage[shader];
  usage = ShaderBindingUsage();
  usage.available = true;

  // Native pipeline reflection uses a nil array for an empty stage. Record that
  // successful zero-binding result, as the other APIs' empty binding lists do.
  for(NS::UInteger i = 0; arguments && i < arguments->count(); i++)
  {
    MTL::Argument *argument = arguments->object<MTL::Argument>(i);
    if(argument == NULL)
      continue;

    const bool modern = ((BOOL (*)(id, SEL, SEL))objc_msgSend)(
        (id)argument, sel_registerName("respondsToSelector:"), sel_registerName("isUsed"));
    const bool active = modern ? ((BOOL (*)(id, SEL))objc_msgSend)((id)argument, sel_registerName("isUsed"))
                               : argument->active();
    if(active)
    {
      if(argument->type() == MTL::ArgumentTypeBuffer && argument->access() == MTL::BindingAccessReadOnly)
        m_ShaderBufferMinimums[shader][(uint32_t)argument->index()] =
            {RDCMAX(1ULL, (uint64_t)argument->bufferDataSize()), RDCMAX(1ULL, (uint64_t)argument->bufferAlignment())};
      else if(uint32_t(argument->type()) != 34) // Object payload is not a resource binding.
        m_ShaderIndirectReadOnly[shader] = false;
    }

    const rdcstr name = argument->name() ? argument->name()->utf8String() : "";
    const uint32_t bind = (uint32_t)argument->index();
    const bool hasArray = ((BOOL (*)(id, SEL, SEL))objc_msgSend)(
        (id)argument, sel_registerName("respondsToSelector:"), sel_registerName("arrayLength"));
    const uint32_t arraySize = hasArray ? RDCMAX(1U, (uint32_t)argument->arrayLength()) : 1;

    if(argument->type() == MTL::ArgumentTypePrimitiveAccelerationStructure ||
       argument->type() == MTL::ArgumentTypeInstanceAccelerationStructure)
    {
      ShaderResource resource;
      resource.name = name;
      resource.fixedBindNumber = bind;
      resource.bindArraySize = arraySize;
      resource.isReadOnly = true;
      resource.descriptorType = DescriptorType::AccelerationStructure;
      reflection.readOnlyResources.push_back(resource);
      usage.readOnlyResources.push_back(active);
    }
    else if(argument->type() == MTL::ArgumentTypeBuffer)
    {
      // Metal reports a device float4 pointer as Float4 rather than DataTypePointer here.
      // Struct-backed constant/argument buffers continue through the constant-block path.
      if(argument->bufferDataType() != MTL::DataTypeStruct || argument->access() != MTL::BindingAccessReadOnly)
      {
        ShaderResource resource;
        resource.name = name;
        resource.fixedBindNumber = bind;
        resource.bindArraySize = arraySize;
        resource.isTexture = false;
        resource.isReadOnly = argument->access() == MTL::BindingAccessReadOnly;
        resource.descriptorType = resource.isReadOnly ? DescriptorType::Buffer
                                                      : DescriptorType::ReadWriteBuffer;
        if(resource.isReadOnly)
        {
          reflection.readOnlyResources.push_back(resource);
          usage.readOnlyResources.push_back(active);
        }
        else
        {
          reflection.readWriteResources.push_back(resource);
          usage.readWriteResources.push_back(active);
        }
        continue;
      }
      ConstantBlock block;
      block.name = name;
      block.fixedBindNumber = bind;
      block.bindArraySize = arraySize;
      block.byteSize = (uint32_t)argument->bufferDataSize();
      block.bufferBacked = true;
      reflection.constantBlocks.push_back(block);
      usage.constantBlocks.push_back(active);

      MTL::StructType *structure = argument->bufferStructType();
      NS::Array *members = structure ? structure->members() : NULL;
      for(NS::UInteger memberIndex = 0; members && memberIndex < members->count(); memberIndex++)
      {
        MTL::StructMember *member = members->object<MTL::StructMember>(memberIndex);
        if(member == NULL)
          continue;
        const rdcstr memberName =
            member->name() ? member->name()->utf8String() : StringFormat::Fmt("id%llu", memberIndex);
        MTL::ArrayType *array = MetalArgumentArray(member);
        const MTL::DataType memberType = array ? array->elementType() : member->dataType();
        // Reflection can describe members of an inactive top-level argument. Keep their names in
        // the shader reflection, but require a bound packet only when Metal actually uses it.
        if(active &&
           (memberType == MTL::DataTypeTexture || memberType == MTL::DataTypeSampler ||
            memberType == MTL::DataTypePointer))
          m_ShaderArgumentSlots[shader][bind] = name;
        const NS::UInteger count = array ? array->arrayLength() : 1;
        const NS::UInteger stride = array ? array->argumentIndexStride() : 1;
        // Flatten top-level resource arrays to the same per-id descriptor layout as scalars.
        for(NS::UInteger element = 0; element < RDCMIN(count, (NS::UInteger)32); element++)
        {
          const uint64_t id = member->argumentIndex() + element * stride;
          if(id >= 32)
            continue;
          const uint32_t argumentId = (uint32_t)id;
          const rdcstr qualifiedName = name + "." + memberName +
              (array ? StringFormat::Fmt("[%llu]", element) : rdcstr());
          if(memberType == MTL::DataTypeTexture)
          {
            MTL::TextureReferenceType *texture =
                array ? array->elementTextureReferenceType() : member->textureReferenceType();
            if(texture == NULL)
              continue;
            ShaderResource resource;
            resource.name = qualifiedName;
            resource.fixedBindNumber = argumentId;
            resource.bindArraySize = 1;
            resource.textureType = MakeShaderTextureType(texture->textureType());
            resource.isTexture = true;
            resource.isReadOnly = texture->access() == MTL::BindingAccessReadOnly;
            resource.descriptorType = resource.isReadOnly ? DescriptorType::Image
                                                          : DescriptorType::ReadWriteImage;
            if(resource.isReadOnly)
            {
              reflection.readOnlyResources.push_back(resource);
              usage.readOnlyResources.push_back(active);
            }
            else
            {
              reflection.readWriteResources.push_back(resource);
              usage.readWriteResources.push_back(active);
            }
          }
          else if(memberType == MTL::DataTypeSampler)
          {
            ShaderSampler sampler;
            sampler.name = qualifiedName;
            sampler.fixedBindNumber = argumentId;
            sampler.bindArraySize = 1;
            reflection.samplers.push_back(sampler);
            usage.samplers.push_back(active);
          }
          else if(memberType == MTL::DataTypePointer)
          {
            MTL::PointerType *pointer = array ? array->elementPointerType() : member->pointerType();
            if(!pointer || pointer->elementIsArgumentBuffer() ||
               pointer->access() != MTL::BindingAccessReadOnly)
              continue;
            ShaderResource resource;
            resource.name = qualifiedName;
            resource.fixedBindNumber = argumentId;
            resource.bindArraySize = 1;
            resource.isTexture = false;
            resource.isReadOnly = true;
            resource.descriptorType = DescriptorType::Buffer;
            reflection.readOnlyResources.push_back(resource);
            usage.readOnlyResources.push_back(active);
          }
        }
      }
    }
    else if(argument->type() == MTL::ArgumentTypeSampler)
    {
      ShaderSampler sampler;
      sampler.name = name;
      sampler.fixedBindNumber = bind;
      sampler.bindArraySize = arraySize;
      reflection.samplers.push_back(sampler);
      usage.samplers.push_back(active);
    }
    else if(argument->type() == MTL::ArgumentTypeTexture)
    {
      ShaderResource resource;
      resource.name = name;
      resource.fixedBindNumber = bind;
      resource.bindArraySize = arraySize;
      resource.textureType = MakeShaderTextureType(argument->textureType());
      resource.isTexture = true;
      resource.hasSampler = false;
      resource.isInputAttachment = false;
      resource.isReadOnly = argument->access() == MTL::BindingAccessReadOnly;
      resource.descriptorType = resource.isReadOnly ? DescriptorType::Image
                                                    : DescriptorType::ReadWriteImage;

      if(resource.isReadOnly)
      {
        reflection.readOnlyResources.push_back(resource);
        usage.readOnlyResources.push_back(active);
      }
      else
      {
        reflection.readWriteResources.push_back(resource);
        usage.readWriteResources.push_back(active);
      }
    }
  }
}

rdcarray<ShaderEntryPoint> MetalReplay::GetShaderEntryPoints(ResourceId shader)
{
  auto it = m_Shaders.find(shader);
  if(it == m_Shaders.end())
    return {};

  return {{it->second.entryPoint, it->second.stage}};
}

const ShaderReflection *MetalReplay::GetShader(ResourceId pipeline, ResourceId shader,
                                               ShaderEntryPoint entry)
{
  const bool replaced = m_ShaderReplacements.count(shader) != 0;
  if(replaced) shader = m_ShaderReplacements[shader];
  auto it = m_Shaders.find(shader);
  if(it == m_Shaders.end())
    return NULL;

  if(!entry.name.empty() &&
     ((!replaced && entry.name != it->second.entryPoint) || entry.stage != it->second.stage))
    return NULL;

  return &it->second;
}

void MetalReplay::AddRenderPipeline(ResourceId id,
                                    const RDMTL::RenderPipelineDescriptor &descriptor,
                                    MTL::RenderPipelineReflection *reflection)
{
  RenderPipelineInfo &pipeline = m_RenderPipelines[id];
  pipeline.descriptor = descriptor;
  pipeline.vertexFunction = GetResID(descriptor.vertexFunction);
  pipeline.fragmentFunction = GetResID(descriptor.fragmentFunction);
  pipeline.vertexDescriptor = descriptor.vertexDescriptor;
  pipeline.sampleCount = (uint32_t)descriptor.rasterSampleCount;
  pipeline.rasterizationEnabled = descriptor.rasterizationEnabled;
  pipeline.alphaToCoverageEnabled = descriptor.alphaToCoverageEnabled;
  pipeline.alphaToOneEnabled = descriptor.alphaToOneEnabled;
  pipeline.colorAttachments = descriptor.colorAttachments;
  pipeline.depthAttachmentPixelFormat = descriptor.depthAttachmentPixelFormat;
  pipeline.stencilAttachmentPixelFormat = descriptor.stencilAttachmentPixelFormat;

  if(reflection)
  {
    AddShaderBindings(pipeline.vertexFunction, reflection->vertexArguments());
    AddShaderBindings(pipeline.fragmentFunction, reflection->fragmentArguments());
  }
}

void MetalReplay::AddTilePipeline(ResourceId id, ResourceId function,
                                  MTL::RenderPipelineReflection *reflection)
{
  // Tile pipelines share MTLRenderPipelineState's native class, but are not vertex/fragment
  // pipelines. Keep their resource identity without inventing either graphics shader stage.
  m_RenderPipelines[id] = RenderPipelineInfo();
  m_TilePipelines[id] = function;
  if(reflection)
    AddShaderBindings(function, reflection->tileArguments());
}

void MetalReplay::AddMeshPipeline(ResourceId pipelineId, ResourceId meshFunction,
                                  ResourceId fragmentFunction, uint32_t sampleCount,
                                  MTL::RenderPipelineReflection *reflection, const rdcarray<uint32_t> &colorFormats,
                                  ResourceId objectFunction)
{
  RenderPipelineInfo &pipeline = m_RenderPipelines[pipelineId];
  pipeline.fragmentFunction = fragmentFunction;
  pipeline.sampleCount = sampleCount;
  m_MeshPipelines[pipelineId] = meshFunction;
  m_ObjectPipelines[pipelineId] = objectFunction;
  pipeline.colorAttachments.resize(colorFormats.size());
  for(size_t i = 0; i < colorFormats.size(); i++)
    pipeline.colorAttachments[i].pixelFormat = MTL::PixelFormat(colorFormats[i]);
  if(reflection)
  {
    auto bindings = [reflection](const char *selector) {
      return (NS::Array *)((id (*)(id, SEL))objc_msgSend)((id)reflection, sel_registerName(selector));
    };
    AddShaderBindings(meshFunction, bindings("meshBindings"));
    if(objectFunction != ResourceId()) AddShaderBindings(objectFunction, bindings("objectBindings"));
  }
  if(reflection && fragmentFunction != ResourceId())
    AddShaderBindings(fragmentFunction, reflection->fragmentArguments());
}

void MetalReplay::AddComputePipeline(ResourceId id, ResourceId function,
                                     MTL::ComputePipelineReflection *reflection,
                                     MTL::ComputePipelineState *pipeline, bool threadExecutionMultiple)
{
  m_ComputePipelines[id] = function;
  m_ComputeReadOnlyBuffers[id].clear();
  if(reflection && reflection->arguments())
    for(NS::UInteger i = 0; i < reflection->arguments()->count(); i++)
    {
      MTL::Argument *argument = reflection->arguments()->object<MTL::Argument>(i);
      if(argument && argument->type() == MTL::ArgumentTypeBuffer &&
         argument->access() == MTL::BindingAccessReadOnly)
        m_ComputeReadOnlyBuffers[id].insert((uint32_t)argument->index());
    }
  // Pipelines created inside a captured frame can be recreated on event replay.
  m_ComputeBufferMinimums.erase(id);
  m_ComputeRequiredTextures.erase(id);
  m_ComputeRequiredSamplers.erase(id);
  m_ComputeRequiredAS.erase(id);
  m_ComputeThreadgroupMinimums.erase(id);
  m_ComputeThreadgroupLimits[id] = {(uint64_t)pipeline->staticThreadgroupMemoryLength(),
                                   (uint64_t)pipeline->maxTotalThreadsPerThreadgroup()};
  m_ComputeThreadExecutionMultiples[id] =
      threadExecutionMultiple ? (uint64_t)pipeline->threadExecutionWidth() : 1;
  if(reflection)
  {
    // A successful Native query with zero active buffers is a known empty
    // binding set. Keep an absent reflection object distinct from that result.
    m_ComputeBufferMinimums[id].clear();
    AddShaderBindings(function, reflection->arguments());
    NS::Array *arguments = reflection->arguments();
    for(NS::UInteger i = 0; arguments && i < arguments->count(); i++)
    {
      MTL::Argument *argument = arguments->object<MTL::Argument>(i);
      if(!argument || !argument->active())
        continue;
      if(argument->type() == MTL::ArgumentTypeBuffer)
        m_ComputeBufferMinimums[id][(uint32_t)argument->index()] =
            {RDCMAX(1ULL, (uint64_t)argument->bufferDataSize()),
             RDCMAX(1ULL, (uint64_t)argument->bufferAlignment())};
      else if(argument->type() == MTL::ArgumentTypeTexture)
        m_ComputeRequiredTextures[id].push_back((uint32_t)argument->index());
      else if(argument->type() == MTL::ArgumentTypeSampler)
        m_ComputeRequiredSamplers[id].push_back((uint32_t)argument->index());
      else if(argument->type() == MTL::ArgumentTypePrimitiveAccelerationStructure ||
              argument->type() == MTL::ArgumentTypeInstanceAccelerationStructure)
        m_ComputeRequiredAS[id][(uint32_t)argument->index()] = argument->type();
      else if(argument->type() == MTL::ArgumentTypeThreadgroupMemory)
        m_ComputeThreadgroupMinimums[id][(uint32_t)argument->index()] =
            RDCMAX(1ULL, (uint64_t)argument->threadgroupMemoryDataSize());
    }
  }
}

bool MetalReplay::ValidateComputeRayBindings() const
{
  auto required = m_ComputeRequiredAS.find(m_CurrentPipelineState.computePipelineResourceId);
  if(required == m_ComputeRequiredAS.end() || required->second.empty() ||
     !ValidateComputeBufferBindings()) return false;
  for(const auto &slot : required->second)
  {
    const auto &bindings = m_CurrentPipelineState.computeAccelerationStructures;
    if(slot.first >= bindings.size() || bindings[slot.first] == ResourceId()) return false;
    auto object = m_pDriver->GetResourceManager()->GetResource(bindings[slot.first], true);
    if(!object || object->m_Type != eResAccelerationStructure || !object->m_Real) return false;
    auto structure = (WrappedMTLAccelerationStructure *)object;
    if(!structure->m_LastBuildKind ||
       (slot.second == MTL::ArgumentTypeInstanceAccelerationStructure) !=
         (structure->m_LastBuildKind == 5)) return false;
  }
  return true;
}

bool MetalReplay::ValidateComputeBufferBindings(bool allowMissingBufferReflection) const
{
  auto pipeline = m_ComputeBufferMinimums.find(m_CurrentPipelineState.computePipelineResourceId);
  if(pipeline == m_ComputeBufferMinimums.end() && !allowMissingBufferReflection)
  {
    fprintf(stderr, "Metal compute binding validation: missing buffer reflection map\n");
    return false;
  }
  if(pipeline != m_ComputeBufferMinimums.end())
    for(const auto &required : pipeline->second)
  {
    MetalPipe::BufferBinding binding = GetComputeBuffer(required.first);
    if((binding.resourceId == ResourceId() &&
        m_CurrentComputeInlineBytes.find(required.first) == m_CurrentComputeInlineBytes.end()) ||
       binding.byteSize < required.second.first ||
       binding.byteOffset % required.second.second != 0)
    {
      fprintf(stderr, "Metal compute binding validation: buffer slot=%u id=%d size=%llu min=%llu offset=%llu align=%llu\n",
              required.first, binding.resourceId != ResourceId(), binding.byteSize,
              required.second.first, binding.byteOffset, required.second.second);
      return false;
    }
  }
  auto textures = m_ComputeRequiredTextures.find(m_CurrentPipelineState.computePipelineResourceId);
  if(textures != m_ComputeRequiredTextures.end())
    for(uint32_t slot : textures->second)
      if(GetComputeTexture(slot) == ResourceId())
      {
        fprintf(stderr, "Metal compute binding validation: texture slot=%u missing\n", slot);
        return false;
      }
  auto samplers = m_ComputeRequiredSamplers.find(m_CurrentPipelineState.computePipelineResourceId);
  if(samplers != m_ComputeRequiredSamplers.end())
    for(uint32_t slot : samplers->second)
      if(slot >= m_CurrentPipelineState.computeSamplers.size() ||
         m_CurrentPipelineState.computeSamplers[slot] == ResourceId())
      {
        fprintf(stderr, "Metal compute binding validation: sampler slot=%u missing\n", slot);
        return false;
      }
  return true;
}

bool MetalReplay::ValidateComputeBufferSnapshot(ResourceId pipeline,
    const std::map<uint32_t, MetalPipe::BufferBinding> &buffers,
    const std::map<uint32_t, uint64_t> &bytes) const
{
  auto requiredBuffers = m_ComputeBufferMinimums.find(pipeline);
  auto textures = m_ComputeRequiredTextures.find(pipeline);
  auto samplers = m_ComputeRequiredSamplers.find(pipeline);
  if(requiredBuffers == m_ComputeBufferMinimums.end() ||
     (textures != m_ComputeRequiredTextures.end() && !textures->second.empty()) ||
     (samplers != m_ComputeRequiredSamplers.end() && !samplers->second.empty()))
  {
    if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
      fprintf(stderr,"Metal compute snapshot reflection: pipeline=%s known=%d textures=%zu samplers=%zu\n",
          ToStr(pipeline).c_str(),requiredBuffers!=m_ComputeBufferMinimums.end(),
          textures==m_ComputeRequiredTextures.end()?0:textures->second.size(),
          samplers==m_ComputeRequiredSamplers.end()?0:samplers->second.size());
    return false;
  }
  for(const auto &required : requiredBuffers->second)
  {
    auto buffer = buffers.find(required.first);
    auto inlineBytes = bytes.find(required.first);
    if(buffer != buffers.end() && buffer->second.resourceId != ResourceId())
    {
      if(buffer->second.byteSize < required.second.first ||
         buffer->second.byteOffset % required.second.second != 0)
      {
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal compute snapshot binding: slot=%u length=%llu minimum=%llu offset=%llu alignment=%llu\n",
              required.first,(unsigned long long)buffer->second.byteSize,(unsigned long long)required.second.first,
              (unsigned long long)buffer->second.byteOffset,(unsigned long long)required.second.second);
        return false;
      }
    }
    else if(inlineBytes == bytes.end() || inlineBytes->second < required.second.first)
    {
      if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal compute snapshot inline: slot=%u bytes=%llu minimum=%llu\n",required.first,
            (unsigned long long)(inlineBytes==bytes.end()?0:inlineBytes->second),(unsigned long long)required.second.first);
      return false;
    }
  }
  return true;
}

bool MetalReplay::ValidateGraphicsTargets(ResourceId id, const std::map<uint32_t, MTL::PixelFormat> &targets, uint32_t maxTargets,
    MTL::PixelFormat depth, MTL::PixelFormat stencil, bool allowAttachmentless) const
{
  auto pipeline = m_RenderPipelines.find(id);
  if(pipeline == m_RenderPipelines.end() || (targets.empty() && depth == MTL::PixelFormatInvalid && !allowAttachmentless) || targets.size() > maxTargets ||
     pipeline->second.depthAttachmentPixelFormat != depth ||
     pipeline->second.stencilAttachmentPixelFormat != stencil) return false;
  if(targets.empty() && depth==MTL::PixelFormatInvalid &&
     (!pipeline->second.rasterizationEnabled || pipeline->second.vertexFunction==ResourceId() ||
      pipeline->second.fragmentFunction==ResourceId() || pipeline->second.sampleCount!=1))return false;
  for(const auto &target : targets)
    if(target.first >= pipeline->second.colorAttachments.size() ||
       pipeline->second.colorAttachments[target.first].pixelFormat != target.second) return false;
  for(uint32_t index = 0; index < pipeline->second.colorAttachments.size(); index++)
    if(pipeline->second.colorAttachments[index].pixelFormat != MTL::PixelFormatInvalid && !targets.count(index))
      return false;
  return true;
}

bool MetalReplay::ValidateGraphicsBufferSnapshot(ResourceId id, uint32_t stage,
    const std::map<uint32_t, MetalPipe::BufferBinding> &buffers,
    const std::map<uint32_t, uint64_t> &bytes, bool allowAbsentFragment) const
{
  auto pipeline = m_RenderPipelines.find(id);
  if(pipeline == m_RenderPipelines.end() || stage < 1 || stage > 4 ||
     pipeline->second.sampleCount != 1 || IsTilePipeline(id) ||
     (IsMeshPipeline(id) ? stage == 1 : stage > 2)) { fprintf(stderr,"Metal graphics snapshot: invalid pipeline/stage\n"); return false; }
  for(const auto &attribute : pipeline->second.vertexDescriptor.attributes)
    if(attribute.format != MTL::VertexFormatInvalid) { fprintf(stderr,"Metal graphics snapshot: vertex fetch unsupported\n"); return false; }
  ResourceId shader = stage == 3 ? m_ObjectPipelines.at(id) :
                      stage == 4 ? m_MeshPipelines.at(id) :
                      stage == 1 ? pipeline->second.vertexFunction : pipeline->second.fragmentFunction;
  if(stage == 3 && shader == ResourceId()) return buffers.empty() && bytes.empty();
  // A Native vertex-only pipeline has no fragment resource accesses, even
  // when valid fragment bindings remain set or colour attachments are present.
  // The pipeline and bound objects have already been restored independently.
  if(stage == 2 && shader == ResourceId() && allowAbsentFragment)
    return pipeline->second.vertexFunction != ResourceId();
  auto required = m_ShaderBufferMinimums.find(shader);
  auto indirect = m_ShaderIndirectReadOnly.find(shader);
  if(required == m_ShaderBufferMinimums.end() || indirect == m_ShaderIndirectReadOnly.end() || !indirect->second)
  { fprintf(stderr,"Metal graphics snapshot: stage=%u shader=%d buffers=%d indirect=%d readonly=%d\n",stage,shader!=ResourceId(),required!=m_ShaderBufferMinimums.end(),indirect!=m_ShaderIndirectReadOnly.end(),indirect!=m_ShaderIndirectReadOnly.end()&&indirect->second); return false; }
  for(const auto &entry : required->second)
  {
    auto buffer = buffers.find(entry.first);
    auto inlineBytes = bytes.find(entry.first);
    if(buffer != buffers.end() && buffer->second.resourceId != ResourceId())
    {
      if(buffer->second.byteSize < entry.second.first || buffer->second.byteOffset % entry.second.second != 0)
        return false;
    }
    else if(inlineBytes == bytes.end() || inlineBytes->second < entry.second.first) { fprintf(stderr,"Metal graphics snapshot: stage=%u slot=%u bytes=%llu minimum=%llu\n",stage,entry.first,inlineBytes==bytes.end()?0ULL:inlineBytes->second,entry.second.first); return false; }
  }
  return true;
}

bool MetalReplay::ValidateComputeThreadgroup(const MTL::Size &threads, const MTL::Size *grid) const
{
  return ValidateComputeThreadgroupSnapshot(m_CurrentPipelineState.computePipelineResourceId,
      threads, m_CurrentComputeThreadgroupMemory, grid);
}

bool MetalReplay::ValidateComputeThreadgroupSnapshot(ResourceId pipeline, const MTL::Size &threads,
    const std::map<uint32_t, uint64_t> &memory, const MTL::Size *grid) const
{
  auto limits = m_ComputeThreadgroupLimits.find(pipeline);
  if(limits == m_ComputeThreadgroupLimits.end())
    return false;
  const auto abi = m_pDriver->m_IRComputeRuntimeABIs.find(pipeline);
  if(abi != m_pDriver->m_IRComputeRuntimeABIs.end() &&
     (threads.width != abi->second.threads[0] || threads.height != abi->second.threads[1] ||
      threads.depth != abi->second.threads[2]))
    return false;
  const MTL::Size maximum = Unwrap(m_pDriver)->maxThreadsPerThreadgroup();
  if(!threads.width || !threads.height || !threads.depth || threads.width > maximum.width ||
     threads.height > maximum.height || threads.depth > maximum.depth ||
     threads.width > limits->second.second / threads.height ||
     threads.width * threads.height > limits->second.second / threads.depth)
    return false;
  const uint64_t maximumMemory = Unwrap(m_pDriver)->maxThreadgroupMemoryLength();
  auto multiple =
      m_ComputeThreadExecutionMultiples.find(pipeline);
  if(multiple == m_ComputeThreadExecutionMultiples.end() || !multiple->second ||
     (threads.width * threads.height * threads.depth) % multiple->second != 0)
    return false;
  if(grid && multiple->second > 1)
  {
    // Non-uniform edge groups must also honour the descriptor's compiler promise.
    const uint64_t widths[] = {threads.width, grid->width % threads.width};
    const uint64_t heights[] = {threads.height, grid->height % threads.height};
    const uint64_t depths[] = {threads.depth, grid->depth % threads.depth};
    for(uint64_t width : widths)
      for(uint64_t height : heights)
        for(uint64_t depth : depths)
          if(width && height && depth && (width * height * depth) % multiple->second != 0)
            return false;
  }
  uint64_t total = limits->second.first;
  for(const auto &binding : memory)
  {
    if(total > maximumMemory || binding.second > maximumMemory - total)
      return false;
    total += binding.second;
  }
  if(total > maximumMemory)
    return false;
  auto required = m_ComputeThreadgroupMinimums.find(pipeline);
  if(required != m_ComputeThreadgroupMinimums.end())
    for(const auto &minimum : required->second)
    {
      auto binding = memory.find(minimum.first);
      if(binding == memory.end() || binding->second < minimum.second)
        return false;
    }
  return true;
}

void MetalReplay::SetComputeThreadgroupMemory(uint32_t index, uint64_t length)
{
  m_CurrentComputeThreadgroupMemory[index] = length;
}

void MetalReplay::BeginComputePass()
{
  m_CurrentPipelineState = MetalPipe::State();
  m_CurrentGraphicsInlineData.clear();
  m_CurrentSamplerLOD.clear();
  m_CurrentComputeInlineBytes.clear();
  m_CurrentComputeInlineData.clear();
  m_CurrentComputeThreadgroupMemory.clear();
}

void MetalReplay::SetComputePipeline(ResourceId id)
{
  m_CurrentPipelineState.computePipelineResourceId = id;
  m_CurrentPipelineState.computeShader = MetalPipe::Shader();
  auto pipeline = m_ComputePipelines.find(id);
  if(pipeline == m_ComputePipelines.end())
    return;
  const ResourceId function = pipeline->second;
  m_CurrentPipelineState.computeShader.resourceId = function;
  m_CurrentPipelineState.computeShader.stage = ShaderStage::Compute;
  auto shader = m_Shaders.find(function);
  if(shader != m_Shaders.end())
  {
    m_CurrentPipelineState.computeShader.reflection = &shader->second;
    m_CurrentPipelineState.computeShader.entryPoint = shader->second.entryPoint;
  }
}

bool MetalReplay::IsComputeBufferActive(ResourceId pipeline, uint32_t slot) const
{
  auto required=m_ComputeBufferMinimums.find(pipeline);
  return required!=m_ComputeBufferMinimums.end() && required->second.count(slot)!=0;
}

bool MetalReplay::IsComputeBufferReadOnly(ResourceId pipeline, uint32_t slot) const
{
  const auto active=m_ComputeBufferMinimums.find(pipeline);
  if(active!=m_ComputeBufferMinimums.end() && !active->second.count(slot)) return true;
  auto entry = m_ComputeReadOnlyBuffers.find(pipeline);
  return entry != m_ComputeReadOnlyBuffers.end() && entry->second.count(slot);
}

void MetalReplay::SetComputeTexture(uint32_t index, ResourceId id)
{
  if(index < 128)
  {
    m_CurrentPipelineState.computeTextures.resize_for_index(index);
    m_CurrentPipelineState.computeTextures[index] = id;
  }
}

void MetalReplay::BindComputeSampler(uint32_t index, ResourceId id)
{
  m_CurrentSamplerLOD.erase(0x1000 + index);
  m_CurrentPipelineState.computeSamplers.resize_for_index(index);
  m_CurrentPipelineState.computeSamplers[index] = id;
}

void MetalReplay::BindExtendedBuffer(ShaderStage stage, uint32_t index, ResourceId id, uint64_t offset)
{
  auto &buffers = stage == ShaderStage::Task ? m_CurrentPipelineState.taskBuffers : m_CurrentPipelineState.meshBuffers;
  buffers.resize_for_index(index);
  auto &binding = buffers[index]; binding.resourceId = id; binding.byteOffset = offset;
  const auto description = GetBuffer(id);
  binding.byteSize = offset < description.length ? description.length - offset : 0;
  m_CurrentGraphicsInlineData.erase({stage == ShaderStage::Task ? 3U : 4U, index});
}

void MetalReplay::BindExtendedBytes(ShaderStage stage, uint32_t index, const rdcarray<byte> &data, ResourceId encoder)
{
  BindExtendedBuffer(stage, index, ResourceId(), 0);
  auto &buffers = stage == ShaderStage::Task ? m_CurrentPipelineState.taskBuffers : m_CurrentPipelineState.meshBuffers;
  buffers[index].byteSize = data.size();
  SaveShaderInlineData(stage == ShaderStage::Task ? 3U : 4U, index, data, encoder);
}

void MetalReplay::BindExtendedTexture(ShaderStage stage, uint32_t index, ResourceId id)
{
  auto &textures = stage == ShaderStage::Task ? m_CurrentPipelineState.taskTextures : m_CurrentPipelineState.meshTextures;
  textures.resize_for_index(index); textures[index] = id;
}

void MetalReplay::BindExtendedSampler(ShaderStage stage, uint32_t index, ResourceId id)
{
  auto &samplers = stage == ShaderStage::Task ? m_CurrentPipelineState.taskSamplers : m_CurrentPipelineState.meshSamplers;
  samplers.resize_for_index(index); samplers[index] = id;
  m_CurrentSamplerLOD.erase(0x10000 | (uint32_t(stage) << 12) | 0x300 | index);
}

void MetalReplay::BindAccelerationStructure(ShaderStage stage, uint32_t index, ResourceId id)
{
  auto &structures = stage == ShaderStage::Compute ? m_CurrentPipelineState.computeAccelerationStructures :
                     stage == ShaderStage::Vertex ? m_CurrentPipelineState.vertexAccelerationStructures :
                                                   m_CurrentPipelineState.fragmentAccelerationStructures;
  structures.resize_for_index(index); structures[index] = id;
}

void MetalReplay::SetSamplerLOD(ShaderStage stage, uint32_t index, float minimum, float maximum)
{
  const uint32_t base = stage == ShaderStage::Vertex ? 0xE00 :
                        stage == ShaderStage::Task || stage == ShaderStage::Mesh
                            ? (0x10000 | uint32_t(stage) << 12 | 0x300) :
                        stage == ShaderStage::Compute ? 0x1000 : 0x100;
  m_CurrentSamplerLOD[base + index] = {minimum, maximum};
}

ResourceId MetalReplay::GetComputeTexture(uint32_t index) const
{
  return index < m_CurrentPipelineState.computeTextures.size()
             ? m_CurrentPipelineState.computeTextures[index]
             : ResourceId();
}

ResourceId MetalReplay::GetComputeTextureForAccess(bool write) const
{
  const ShaderReflection *reflection = m_CurrentPipelineState.computeShader.reflection;
  if(reflection)
  {
    const rdcarray<ShaderResource> &resources =
        write ? reflection->readWriteResources : reflection->readOnlyResources;
    for(const ShaderResource &resource : resources)
      if(resource.isTexture)
        return GetComputeTexture(resource.fixedBindNumber);
    // An authoritative reflection with no resource for this access means none.
    // Falling back to slot 1 invents a writable texture for texture-to-buffer kernels.
    return ResourceId();
  }
  return GetComputeTexture(write ? 1 : 0);
}

void MetalReplay::BindComputeBuffer(uint32_t index, ResourceId id, uint64_t offset)
{
  m_CurrentGraphicsInlineData.erase({0, index});
  m_CurrentComputeInlineBytes.erase(index);
  m_CurrentComputeInlineData.erase(index);
  m_CurrentPipelineState.computeBuffers.resize_for_index(index);
  MetalPipe::BufferBinding &binding = m_CurrentPipelineState.computeBuffers[index];
  binding.resourceId = id;
  binding.byteOffset = offset;
  const BufferDescription buffer = GetBuffer(id);
  binding.byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                         ? buffer.length - offset
                         : 0;
}

void MetalReplay::BindComputeBytes(uint32_t index, const rdcarray<byte> &data)
{
  const uint64_t length = data.size();
  m_CurrentPipelineState.computeBuffers.resize_for_index(index);
  m_CurrentPipelineState.computeBuffers[index] = MetalPipe::BufferBinding();
  m_CurrentComputeInlineData[index] = data;
  m_CurrentComputeInlineBytes[index] = length;
  m_CurrentPipelineState.computeBuffers[index].byteSize = length;
}

bool MetalReplay::SetComputeBufferOffset(uint32_t index, uint64_t offset)
{
  if(index >= m_CurrentPipelineState.computeBuffers.size())
    return false;
  MetalPipe::BufferBinding &binding = m_CurrentPipelineState.computeBuffers[index];
  // Metal API validation requires an existing MTLBuffer, not a setBytes allocation.
  if(binding.resourceId == ResourceId())
    return false;
  const uint64_t length = GetBuffer(binding.resourceId).length;
  if(offset >= length)
    return false;
  binding.byteOffset = offset;
  binding.byteSize = length - offset;
  return true;
}

MetalPipe::BufferBinding MetalReplay::GetComputeBuffer(uint32_t index) const
{
  return index < m_CurrentPipelineState.computeBuffers.size()
             ? m_CurrentPipelineState.computeBuffers[index]
             : MetalPipe::BufferBinding();
}

MetalPipe::BufferBinding MetalReplay::GetComputeBufferForAccess(bool write) const
{
  const ShaderReflection *reflection = m_CurrentPipelineState.computeShader.reflection;
  if(reflection)
  {
    const rdcarray<ShaderResource> &resources =
        write ? reflection->readWriteResources : reflection->readOnlyResources;
    for(const ShaderResource &resource : resources)
      if(!resource.isTexture && (resource.descriptorType == DescriptorType::Buffer ||
                                 resource.descriptorType == DescriptorType::ReadWriteBuffer))
        return GetComputeBuffer(resource.fixedBindNumber);
    return {};
  }
  return GetComputeBuffer(write ? 4 : 2);
}

void MetalReplay::AddDepthStencilState(ResourceId id,
                                       const RDMTL::DepthStencilDescriptor &descriptor)
{
  m_DepthStencilStates[id] = descriptor;
}

void MetalReplay::AddSamplerState(ResourceId id, const RDMTL::SamplerDescriptor &descriptor)
{
  m_SamplerStates[id] = descriptor;
}

static ResourceFormat MakeVertexFormat(MTL::VertexFormat vertexFormat)
{
  ResourceFormat ret;
  ret.type = ResourceFormatType::Regular;

  switch(vertexFormat)
  {
    case MTL::VertexFormatFloat:
      ret.compCount = 1;
      ret.compByteWidth = 4;
      ret.compType = CompType::Float;
      break;
    case MTL::VertexFormatFloat2:
      ret.compCount = 2;
      ret.compByteWidth = 4;
      ret.compType = CompType::Float;
      break;
    case MTL::VertexFormatFloat3:
      ret.compCount = 3;
      ret.compByteWidth = 4;
      ret.compType = CompType::Float;
      break;
    case MTL::VertexFormatFloat4:
      ret.compCount = 4;
      ret.compByteWidth = 4;
      ret.compType = CompType::Float;
      break;
    default: ret = ResourceFormat(); break;
  }

  return ret;
}

static CompareFunction MakeCompareFunction(MTL::CompareFunction function)
{
  switch(function)
  {
    case MTL::CompareFunctionNever: return CompareFunction::Never;
    case MTL::CompareFunctionLess: return CompareFunction::Less;
    case MTL::CompareFunctionEqual: return CompareFunction::Equal;
    case MTL::CompareFunctionLessEqual: return CompareFunction::LessEqual;
    case MTL::CompareFunctionGreater: return CompareFunction::Greater;
    case MTL::CompareFunctionNotEqual: return CompareFunction::NotEqual;
    case MTL::CompareFunctionGreaterEqual: return CompareFunction::GreaterEqual;
    case MTL::CompareFunctionAlways: return CompareFunction::AlwaysTrue;
  }

  return CompareFunction::AlwaysTrue;
}

static StencilOperation MakeStencilOperation(MTL::StencilOperation operation)
{
  switch(operation)
  {
    case MTL::StencilOperationKeep: return StencilOperation::Keep;
    case MTL::StencilOperationZero: return StencilOperation::Zero;
    case MTL::StencilOperationReplace: return StencilOperation::Replace;
    case MTL::StencilOperationIncrementClamp: return StencilOperation::IncSat;
    case MTL::StencilOperationDecrementClamp: return StencilOperation::DecSat;
    case MTL::StencilOperationIncrementWrap: return StencilOperation::IncWrap;
    case MTL::StencilOperationDecrementWrap: return StencilOperation::DecWrap;
    case MTL::StencilOperationInvert: return StencilOperation::Invert;
  }

  return StencilOperation::Keep;
}

static StencilFace MakeStencilFace(const RDMTL::StencilDescriptor &descriptor)
{
  StencilFace ret;
  ret.function = MakeCompareFunction(descriptor.stencilCompareFunction);
  ret.failOperation = MakeStencilOperation(descriptor.stencilFailureOperation);
  ret.depthFailOperation = MakeStencilOperation(descriptor.depthFailureOperation);
  ret.passOperation = MakeStencilOperation(descriptor.depthStencilPassOperation);
  ret.compareMask = descriptor.readMask;
  ret.writeMask = descriptor.writeMask;
  return ret;
}

static Descriptor MakeRenderTargetDescriptor(MetalReplay *replay,
                                             const RDMTL::RenderPassAttachmentDescriptor &attachment)
{
  Descriptor ret;
  ret.type = DescriptorType::ReadWriteImage;
  ret.resource = attachment.texture ? GetResID(attachment.texture) : ResourceId();
  ret.firstMip = (uint8_t)attachment.level;
  ret.firstSlice = (uint16_t)attachment.slice;

  TextureDescription texture = replay->GetTexture(ret.resource);
  if(texture.resourceId != ResourceId())
  {
    ret.format = texture.format;
    ret.textureType = texture.type;
  }

  return ret;
}

static Descriptor MakeResolveTargetDescriptor(MetalReplay *replay,
                                              const RDMTL::RenderPassAttachmentDescriptor &attachment)
{
  Descriptor ret;
  ret.type = DescriptorType::ReadWriteImage;
  ret.resource = attachment.resolveTexture ? GetResID(attachment.resolveTexture) : ResourceId();
  ret.firstMip = (uint8_t)attachment.resolveLevel;
  ret.firstSlice = (uint16_t)attachment.resolveSlice;

  TextureDescription texture = replay->GetTexture(ret.resource);
  if(texture.resourceId != ResourceId())
  {
    ret.format = texture.format;
    ret.textureType = texture.type;
  }

  return ret;
}

MetalReplay::EncoderReplayState MetalReplay::SaveEncoderState() const
{
  EncoderReplayState state;
  state.pipeline = m_CurrentPipelineState;
  state.renderPass = m_CurrentRenderPassDescriptor;
  state.computeInlineBytes = m_CurrentComputeInlineBytes;
  state.computeInlineData = m_CurrentComputeInlineData;
  state.graphicsInlineData = m_CurrentGraphicsInlineData;
  state.computeThreadgroupMemory = m_CurrentComputeThreadgroupMemory;
  state.samplerLOD = m_CurrentSamplerLOD;
  state.vertexAttributeStrides = m_CurrentVertexAttributeStrides;
  state.viewportCount = m_CurrentViewportCount;
  state.scissorCount = m_CurrentScissorCount;
  return state;
}

void MetalReplay::RestoreEncoderState(const EncoderReplayState &state)
{
  m_CurrentPipelineState = state.pipeline;
  m_CurrentRenderPassDescriptor = state.renderPass;
  m_CurrentComputeInlineBytes = state.computeInlineBytes;
  m_CurrentComputeInlineData = state.computeInlineData;
  m_CurrentGraphicsInlineData = state.graphicsInlineData;
  m_CurrentComputeThreadgroupMemory = state.computeThreadgroupMemory;
  m_CurrentSamplerLOD = state.samplerLOD;
  m_CurrentVertexAttributeStrides = state.vertexAttributeStrides;
  m_CurrentViewportCount = state.viewportCount;
  m_CurrentScissorCount = state.scissorCount;
}

void MetalReplay::ActivateEncoderContext(ResourceId commandBuffer)
{
  if(m_ActiveEncoderContext == commandBuffer)
    return;
  if(m_ActiveEncoderContext != ResourceId())
    m_EncoderContexts[m_ActiveEncoderContext] = SaveEncoderState();
  auto it = m_EncoderContexts.find(commandBuffer);
  RestoreEncoderState(it == m_EncoderContexts.end() ? EncoderReplayState() : it->second);
  m_ActiveEncoderContext = commandBuffer;
}

void MetalReplay::ClearEncoderContexts()
{
  m_ActiveEncoderContext = ResourceId();
  m_EncoderContexts.clear();
  RestoreEncoderState(EncoderReplayState());
}

void MetalReplay::BeginRenderPass(const RDMTL::RenderPassDescriptor &descriptor)
{
  m_CurrentViewportCount = m_CurrentScissorCount = 1;
  m_CurrentSamplerLOD.clear();
  m_CurrentVertexAttributeStrides.clear();
  m_CurrentRenderPassDescriptor = descriptor;
  m_CurrentPipelineState = MetalPipe::State();
  m_CurrentGraphicsInlineData.clear();

  if(descriptor.rasterizationRateMap)
  {
    auto map = descriptor.rasterizationRateMap;
    auto real = Unwrap(map);
    auto &r = m_CurrentPipelineState.rasterizer;
    r.rasterizationRateMap = GetResID(map);
    auto size = real->screenSize();
    r.rateMapScreenSize = {uint32_t(size.width), uint32_t(size.height)};
    r.rateMapHorizontal = map->horizontal;
    r.rateMapVertical = map->vertical;
    for(NS::UInteger layer = 0; layer < real->layerCount(); layer++)
    {
      auto physical = real->physicalSize(layer);
      r.rateMapPhysicalSizes.push_back(uint32_t(physical.width));
      r.rateMapPhysicalSizes.push_back(uint32_t(physical.height));
    }
  }
  m_CurrentPipelineState.imageblockSampleLength = descriptor.imageblockSampleLength;
  m_CurrentPipelineState.threadgroupMemoryLength = descriptor.threadgroupMemoryLength;
  auto storage = [this](WrappedMTLTexture *texture) {
    auto it = m_TextureStorage.find(GetResID(texture));
    return texture ? ToStr(it == m_TextureStorage.end() ? Unwrap(texture)->storageMode() : it->second) : rdcstr();
  };
  m_CurrentPipelineState.depthStorage = storage(descriptor.depthAttachment.texture);
  m_CurrentPipelineState.depthLoad = ToStr(descriptor.depthAttachment.loadAction);
  m_CurrentPipelineState.depthStore = ToStr(descriptor.depthAttachment.storeAction);
  m_CurrentPipelineState.stencilStorage = storage(descriptor.stencilAttachment.texture);
  m_CurrentPipelineState.stencilLoad = ToStr(descriptor.stencilAttachment.loadAction);
  m_CurrentPipelineState.stencilStore = ToStr(descriptor.stencilAttachment.storeAction);
  m_CurrentPipelineState.colorTargets.resize(descriptor.colorAttachments.size());
  m_CurrentPipelineState.resolveTargets.resize(descriptor.colorAttachments.size());
  for(size_t i = 0; i < descriptor.colorAttachments.size(); i++)
  {
    m_CurrentPipelineState.attachmentStorage.push_back(storage(descriptor.colorAttachments[i].texture));
    m_CurrentPipelineState.attachmentLoad.push_back(ToStr(descriptor.colorAttachments[i].loadAction));
    m_CurrentPipelineState.attachmentStore.push_back(ToStr(descriptor.colorAttachments[i].storeAction));
    m_CurrentPipelineState.attachmentStoreOptions.push_back(ToStr(descriptor.colorAttachments[i].storeActionOptions));
    m_CurrentPipelineState.colorTargets[i] =
        MakeRenderTargetDescriptor(this, descriptor.colorAttachments[i]);
    m_CurrentPipelineState.resolveTargets[i] =
        MakeResolveTargetDescriptor(this, descriptor.colorAttachments[i]);
  }

  m_CurrentPipelineState.depthTarget = MakeRenderTargetDescriptor(this, descriptor.depthAttachment);
  if(m_CurrentPipelineState.depthTarget.resource == ResourceId())
    m_CurrentPipelineState.depthTarget =
        MakeRenderTargetDescriptor(this, descriptor.stencilAttachment);

  // Metal starts each encoder with a viewport/scissor covering the render area, even when
  // no setViewport/setScissorRect call was recorded. Expose those defaults for inspection.
  uint32_t width = descriptor.renderTargetWidth ? (uint32_t)descriptor.renderTargetWidth : UINT32_MAX;
  uint32_t height = descriptor.renderTargetHeight ? (uint32_t)descriptor.renderTargetHeight : UINT32_MAX;
  auto attachmentSize = [this, &width, &height](const Descriptor &target) {
    if(target.resource == ResourceId()) return;
    const auto texture = GetTexture(target.resource);
    width = RDCMIN(width, RDCMAX(1U, texture.width >> target.firstMip));
    height = RDCMIN(height, RDCMAX(1U, texture.height >> target.firstMip));
  };
  for(const auto &target : m_CurrentPipelineState.colorTargets) attachmentSize(target);
  attachmentSize(m_CurrentPipelineState.depthTarget);
  attachmentSize(MakeRenderTargetDescriptor(this, descriptor.stencilAttachment));
  if(width != UINT32_MAX && height != UINT32_MAX)
  {
    auto &raster = m_CurrentPipelineState.rasterizer;
    raster.viewport = Viewport(0, 0, (float)width, (float)height, 0, 1, true);
    raster.scissor = Scissor(0, 0, (int32_t)width, (int32_t)height, true);
    raster.viewports = {raster.viewport};
    raster.scissors = {raster.scissor};
  }
}

void MetalReplay::EndRenderPass()
{
  m_CurrentSamplerLOD.clear();
  m_CurrentVertexAttributeStrides.clear();
  m_CurrentPipelineState = MetalPipe::State();
  m_CurrentGraphicsInlineData.clear();
}

void MetalReplay::SetRenderPassStoreAction(uint32_t attachment, MTL::StoreAction action)
{
  if(attachment < m_CurrentRenderPassDescriptor.colorAttachments.size())
  {
    m_CurrentRenderPassDescriptor.colorAttachments[attachment].storeAction = action;
    m_CurrentPipelineState.attachmentStore[attachment] = ToStr(action);
  }
  else if(attachment == 8)
  {
    m_CurrentRenderPassDescriptor.depthAttachment.storeAction = action;
    m_CurrentPipelineState.depthStore = ToStr(action);
  }
  else if(attachment == 9)
  {
    m_CurrentRenderPassDescriptor.stencilAttachment.storeAction = action;
    m_CurrentPipelineState.stencilStore = ToStr(action);
  }
}

void MetalReplay::SetRenderPassStoreOptions(uint32_t attachment, MTL::StoreActionOptions options)
{
  if(attachment < m_CurrentRenderPassDescriptor.colorAttachments.size())
  {
    m_CurrentRenderPassDescriptor.colorAttachments[attachment].storeActionOptions = options;
    m_CurrentPipelineState.attachmentStoreOptions[attachment] = ToStr(options);
  }
  else if(attachment == 8)
    m_CurrentRenderPassDescriptor.depthAttachment.storeActionOptions = options;
  else if(attachment == 9)
    m_CurrentRenderPassDescriptor.stencilAttachment.storeActionOptions = options;
}

void MetalReplay::SetMetalFXSpatial(ResourceId input, ResourceId output, const rdcarray<uint64_t> &parameters)
{
  m_CurrentPipelineState = MetalPipe::State();
  m_CurrentGraphicsInlineData.clear();
  m_CurrentPipelineState.metalFXSpatial = parameters;
  m_CurrentPipelineState.metalFXInput = input;
  m_CurrentPipelineState.metalFXOutput = output;
}

void MetalReplay::SetTileDispatch(const MTL::Size &threads, uint32_t width, uint32_t height)
{
  m_CurrentPipelineState.tileWidth = width;
  m_CurrentPipelineState.tileHeight = height;
  m_CurrentPipelineState.tileThreads[0] = (uint32_t)threads.width;
  m_CurrentPipelineState.tileThreads[1] = (uint32_t)threads.height;
  m_CurrentPipelineState.tileThreads[2] = (uint32_t)threads.depth;
}

void MetalReplay::SetTileMemory(uint32_t index, uint64_t length, uint64_t offset)
{
  m_CurrentPipelineState.tileMemoryLengths.resize_for_index(index);
  m_CurrentPipelineState.tileMemoryOffsets.resize_for_index(index);
  m_CurrentPipelineState.tileMemoryLengths[index] = length;
  m_CurrentPipelineState.tileMemoryOffsets[index] = offset;
}

void MetalReplay::BindRenderPipeline(ResourceId id)
{
  const size_t oldVertexBindingCount =
      RDCMAX(m_CurrentPipelineState.vertexBuffers.size(),
             m_CurrentPipelineState.vertexStorageBuffers.size());
  rdcarray<MetalPipe::BufferBinding> oldVertexBindings;
  oldVertexBindings.resize(oldVertexBindingCount);
  for(size_t slot = 0; slot < oldVertexBindingCount; slot++)
  {
    if(slot < m_CurrentPipelineState.vertexBuffers.size() &&
       m_CurrentPipelineState.vertexBuffers[slot].resourceId != ResourceId())
    {
      oldVertexBindings[slot].resourceId = m_CurrentPipelineState.vertexBuffers[slot].resourceId;
      oldVertexBindings[slot].byteOffset = m_CurrentPipelineState.vertexBuffers[slot].byteOffset;
      oldVertexBindings[slot].byteSize = m_CurrentPipelineState.vertexBuffers[slot].byteSize;
    }
    else if(slot < m_CurrentPipelineState.vertexStorageBuffers.size())
    {
      oldVertexBindings[slot] = m_CurrentPipelineState.vertexStorageBuffers[slot];
    }
  }

  m_CurrentPipelineState.pipelineResourceId = id;
  m_CurrentPipelineState.tileDispatch = IsTilePipeline(id);
  m_CurrentPipelineState.computePipelineResourceId = ResourceId();
  m_CurrentPipelineState.computeShader = MetalPipe::Shader();
  m_CurrentPipelineState.tileMaxThreads = 0;
  m_CurrentPipelineState.tileSizeMatches = false;
  memset(m_CurrentPipelineState.tileThreads, 0, sizeof(m_CurrentPipelineState.tileThreads));
  m_CurrentPipelineState.vertexShader = MetalPipe::Shader();
  m_CurrentPipelineState.fragmentShader = MetalPipe::Shader();
  m_CurrentPipelineState.taskShader = MetalPipe::Shader();
  m_CurrentPipelineState.meshShader = MetalPipe::Shader();
  m_CurrentPipelineState.sampleCount = 1;
  m_CurrentPipelineState.alphaToCoverageEnabled = false;
  m_CurrentPipelineState.alphaToOneEnabled = false;
  m_CurrentPipelineState.vertexAttributes.clear();
  m_CurrentPipelineState.colorBlends.clear();
  for(MetalPipe::VertexBuffer &binding : m_CurrentPipelineState.vertexBuffers)
  {
    binding.byteStride = 0;
    binding.perInstance = false;
    binding.stepRate = 1;
  }

  auto pipeline = m_RenderPipelines.find(id);
  if(pipeline == m_RenderPipelines.end())
    return;

  auto bindShader = [this](ResourceId shader, ShaderStage stage) {
    MetalPipe::Shader ret;
    ret.resourceId = shader;
    ret.stage = stage;

    auto reflection = m_Shaders.find(shader);
    if(reflection != m_Shaders.end())
    {
      ret.reflection = &reflection->second;
      ret.entryPoint = reflection->second.entryPoint;
    }

    PopulateShaderFeatures(ret);
    return ret;
  };

  if(m_CurrentPipelineState.tileDispatch)
  {
    // Kernel compatibility for common shader/descriptor viewers; the Metal UI uses Tile.
    m_CurrentPipelineState.computePipelineResourceId = id;
    m_CurrentPipelineState.computeShader = bindShader(m_TilePipelines[id], ShaderStage::Compute);
    auto native = m_ShaderPipelineTemplates.find(id);
    if(native != m_ShaderPipelineTemplates.end())
    {
      auto tile = (MTL::TileRenderPipelineDescriptor *)native->second.descriptor;
      m_CurrentPipelineState.tileMaxThreads = tile->maxTotalThreadsPerThreadgroup();
      m_CurrentPipelineState.tileSizeMatches = tile->threadgroupSizeMatchesTileSize();
      m_CurrentPipelineState.sampleCount = (uint32_t)tile->rasterSampleCount();
    }
    return;
  }

  m_CurrentPipelineState.vertexShader =
      bindShader(pipeline->second.vertexFunction, ShaderStage::Vertex);
  m_CurrentPipelineState.fragmentShader =
      bindShader(pipeline->second.fragmentFunction, ShaderStage::Fragment);
  if(m_MeshPipelines.count(id))
  {
    m_CurrentPipelineState.meshShader = bindShader(m_MeshPipelines[id], ShaderStage::Mesh);
    m_CurrentPipelineState.taskShader = bindShader(m_ObjectPipelines[id], ShaderStage::Task);
  }
  m_CurrentPipelineState.rasterizer.rasterizationEnabled = pipeline->second.rasterizationEnabled;
  const auto &descriptor = pipeline->second.descriptor;
  m_CurrentPipelineState.tessellationPartitionMode = ToStr(descriptor.tessellationPartitionMode);
  m_CurrentPipelineState.tessellationStepFunction = ToStr(descriptor.tessellationFactorStepFunction);
  m_CurrentPipelineState.tessellationOutputWinding = ToStr(descriptor.tessellationOutputWindingOrder);
  m_CurrentPipelineState.maxTessellationFactor = (uint32_t)descriptor.maxTessellationFactor;
  m_CurrentPipelineState.tessellationFactorScaleEnabled = descriptor.tessellationFactorScaleEnabled;
  m_CurrentPipelineState.sampleCount = pipeline->second.sampleCount;
  m_CurrentPipelineState.alphaToCoverageEnabled = pipeline->second.alphaToCoverageEnabled;
  m_CurrentPipelineState.alphaToOneEnabled = pipeline->second.alphaToOneEnabled;

  const RDMTL::VertexDescriptor &vertexDescriptor = pipeline->second.vertexDescriptor;
  for(size_t attributeIndex = 0; attributeIndex < vertexDescriptor.attributes.size();
      attributeIndex++)
  {
    const RDMTL::VertexAttributeDescriptor &attribute = vertexDescriptor.attributes[attributeIndex];
    if(attribute.format == MTL::VertexFormatInvalid)
      continue;

    m_CurrentPipelineState.vertexAttributes.push_back(MetalPipe::VertexAttribute());
    MetalPipe::VertexAttribute &state = m_CurrentPipelineState.vertexAttributes.back();
    state.attributeIndex = (uint32_t)attributeIndex;
    state.bufferIndex = (uint32_t)attribute.bufferIndex;
    state.byteOffset = (uint32_t)attribute.offset;
    state.format = MakeVertexFormat(attribute.format);
  }

  for(size_t slot = 0; slot < vertexDescriptor.layouts.size(); slot++)
  {
    const RDMTL::VertexBufferLayoutDescriptor &layout = vertexDescriptor.layouts[slot];
    if(layout.stride == 0)
      continue;

    m_CurrentPipelineState.vertexBuffers.resize_for_index(slot);
    MetalPipe::VertexBuffer &binding = m_CurrentPipelineState.vertexBuffers[slot];
    binding.byteStride = layout.stride == MTL::BufferLayoutStrideDynamic &&
                                 slot < m_CurrentVertexAttributeStrides.size() &&
                                 m_CurrentVertexAttributeStrides[slot]
                             ? m_CurrentVertexAttributeStrides[slot]
                             : (uint32_t)layout.stride;
    binding.stepFunction = ToStr(layout.stepFunction);
    binding.perInstance = layout.stepFunction == MTL::VertexStepFunctionPerInstance;
    binding.stepRate = (uint32_t)layout.stepRate;
  }

  for(const RDMTL::RenderPipelineColorAttachmentDescriptor &attachment :
      pipeline->second.colorAttachments)
  {
    ColorBlend blend;
    blend.enabled = attachment.blendingEnabled;
    blend.colorBlend.source = MakeBlendMultiplier(attachment.sourceRGBBlendFactor);
    blend.colorBlend.destination =
        MakeBlendMultiplier(attachment.destinationRGBBlendFactor);
    blend.colorBlend.operation = MakeBlendOp(attachment.rgbBlendOperation);
    blend.alphaBlend.source = MakeBlendMultiplier(attachment.sourceAlphaBlendFactor);
    blend.alphaBlend.destination =
        MakeBlendMultiplier(attachment.destinationAlphaBlendFactor);
    blend.alphaBlend.operation = MakeBlendOp(attachment.alphaBlendOperation);
    blend.writeMask = MakeWriteMask(attachment.writeMask);
    m_CurrentPipelineState.colorBlends.push_back(blend);
  }

  for(size_t slot = 0; slot < oldVertexBindings.size(); slot++)
  {
    const MetalPipe::BufferBinding &binding = oldVertexBindings[slot];
    if(binding.resourceId != ResourceId())
      BindVertexBuffer((uint32_t)slot, binding.resourceId, binding.byteOffset);
  }
}

bool MetalReplay::IsVertexStorageBufferSlot(uint32_t index) const
{
  auto shader = m_Shaders.find(m_CurrentPipelineState.vertexShader.resourceId);
  if(shader == m_Shaders.end())
    return false;

  for(const ShaderResource &resource : shader->second.readOnlyResources)
    if(!resource.isTexture && resource.descriptorType == DescriptorType::Buffer &&
       resource.fixedBindNumber == index)
      return true;

  for(const ShaderResource &resource : shader->second.readWriteResources)
    if(!resource.isTexture && resource.descriptorType == DescriptorType::ReadWriteBuffer &&
       resource.fixedBindNumber == index)
      return true;

  return false;
}

bool MetalReplay::IsVertexInputBufferSlot(uint32_t index) const
{
  for(const MetalPipe::VertexAttribute &attribute : m_CurrentPipelineState.vertexAttributes)
    if(attribute.bufferIndex == index)
      return true;

  return false;
}

void MetalReplay::BindDepthStencilState(ResourceId id)
{
  const uint32_t frontReference = m_CurrentPipelineState.depthStencil.frontFace.reference;
  const uint32_t backReference = m_CurrentPipelineState.depthStencil.backFace.reference;
  m_CurrentPipelineState.depthStencil = MetalPipe::DepthStencil();
  m_CurrentPipelineState.depthStencil.resourceId = id;
  m_CurrentPipelineState.depthStencil.frontFace.reference = frontReference;
  m_CurrentPipelineState.depthStencil.backFace.reference = backReference;

  auto state = m_DepthStencilStates.find(id);
  if(state != m_DepthStencilStates.end())
  {
    m_CurrentPipelineState.depthStencil.depthFunction =
        MakeCompareFunction(state->second.depthCompareFunction);
    m_CurrentPipelineState.depthStencil.depthWrites = state->second.depthWriteEnabled;
    m_CurrentPipelineState.depthStencil.stencilEnabled =
        state->second.frontFaceStencil.enabled || state->second.backFaceStencil.enabled;
    m_CurrentPipelineState.depthStencil.frontFace = MakeStencilFace(state->second.frontFaceStencil);
    m_CurrentPipelineState.depthStencil.backFace = MakeStencilFace(state->second.backFaceStencil);
    m_CurrentPipelineState.depthStencil.frontFace.reference = frontReference;
    m_CurrentPipelineState.depthStencil.backFace.reference = backReference;
  }
}

void MetalReplay::SetStencilReferenceValue(uint32_t referenceValue)
{
  SetStencilReferenceValues(referenceValue, referenceValue);
}

void MetalReplay::SetStencilReferenceValues(uint32_t frontReferenceValue,
                                            uint32_t backReferenceValue)
{
  m_CurrentPipelineState.depthStencil.frontFace.reference = frontReferenceValue;
  m_CurrentPipelineState.depthStencil.backFace.reference = backReferenceValue;
}

void MetalReplay::BindVertexBuffer(uint32_t index, ResourceId id, uint64_t offset)
{
  m_CurrentGraphicsInlineData.erase({1, index});
  BufferDescription buffer = GetBuffer(id);
  const uint64_t byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                                ? buffer.length - offset
                                : 0;
  const bool storage = IsVertexStorageBufferSlot(index);
  const bool vertexInput = !storage || IsVertexInputBufferSlot(index);

  if(vertexInput)
  {
    m_CurrentPipelineState.vertexBuffers.resize_for_index(index);
    MetalPipe::VertexBuffer &binding = m_CurrentPipelineState.vertexBuffers[index];
    binding.resourceId = id;
    binding.byteOffset = offset;
    binding.byteSize = byteSize;
  }
  else if(index < m_CurrentPipelineState.vertexBuffers.size())
  {
    m_CurrentPipelineState.vertexBuffers[index] = MetalPipe::VertexBuffer();
  }

  if(storage)
  {
    m_CurrentPipelineState.vertexStorageBuffers.resize_for_index(index);
    MetalPipe::BufferBinding &binding = m_CurrentPipelineState.vertexStorageBuffers[index];
    binding.resourceId = id;
    binding.byteOffset = offset;
    binding.byteSize = byteSize;
  }
  else if(index < m_CurrentPipelineState.vertexStorageBuffers.size())
  {
    m_CurrentPipelineState.vertexStorageBuffers[index] = MetalPipe::BufferBinding();
  }
}

void MetalReplay::BindGraphicsBytes(uint32_t stage, uint32_t index,
                                    const rdcarray<byte> &data, ResourceId encoder)
{
  const uint64_t length = data.size();
  if(stage == 1)
  {
    BindVertexBuffer(index, ResourceId(), 0);
    if(index < m_CurrentPipelineState.vertexBuffers.size())
      m_CurrentPipelineState.vertexBuffers[index].byteSize = length;
    if(index < m_CurrentPipelineState.vertexStorageBuffers.size())
      m_CurrentPipelineState.vertexStorageBuffers[index].byteSize = length;
  }
  else if(stage == 2)
  {
    BindFragmentBuffer(index, ResourceId(), 0);
    m_CurrentPipelineState.fragmentBuffers[index].byteSize = length;
  }
  SaveShaderInlineData(stage, index, data, encoder);
}

void MetalReplay::SaveShaderInlineData(uint32_t stage, uint32_t index,
                                      const rdcarray<byte> &data, ResourceId encoder)
{
  auto &saved = m_CurrentGraphicsInlineData[{stage, index}];
  saved = EncoderReplayState::InlineData();
  saved.data.assign(data.data(), data.size());
  auto layout = m_pDriver->m_DescriptorInlineShadow.find({encoder, (uint64_t(stage) << 32) | index});
  if(layout != m_pDriver->m_DescriptorInlineShadow.end())
    for(const auto &source : layout->second.sources)
      saved.pointers[source.first * layout->second.stride] =
          {source.second.resource, source.second.offset};

}

void MetalReplay::SetVertexBufferOffset(uint32_t index, uint64_t offset)
{
  if(index < m_CurrentPipelineState.vertexBuffers.size())
  {
    MetalPipe::VertexBuffer &binding = m_CurrentPipelineState.vertexBuffers[index];
    binding.byteOffset = offset;
    BufferDescription buffer = GetBuffer(binding.resourceId);
    binding.byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                           ? buffer.length - offset
                           : 0;
  }

  if(index < m_CurrentPipelineState.vertexStorageBuffers.size())
  {
    MetalPipe::BufferBinding &binding = m_CurrentPipelineState.vertexStorageBuffers[index];
    binding.byteOffset = offset;
    BufferDescription buffer = GetBuffer(binding.resourceId);
    binding.byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                           ? buffer.length - offset
                           : 0;
  }
}

void MetalReplay::SetVertexBufferStride(uint32_t index, uint32_t stride)
{
  m_CurrentVertexAttributeStrides.resize_for_index(index);
  m_CurrentVertexAttributeStrides[index] = stride;
  m_CurrentPipelineState.vertexBuffers.resize_for_index(index);
  m_CurrentPipelineState.vertexBuffers[index].byteStride = stride;
}

uint64_t MetalReplay::GetVertexLayoutStride(ResourceId pipeline, uint32_t slot) const
{
  auto it = m_RenderPipelines.find(pipeline);
  if(it == m_RenderPipelines.end() || slot >= it->second.vertexDescriptor.layouts.size())
    return 0;
  return it->second.vertexDescriptor.layouts[slot].stride;
}

void MetalReplay::BindFragmentBuffer(uint32_t index, ResourceId id, uint64_t offset)
{
  m_CurrentGraphicsInlineData.erase({2, index});
  m_CurrentPipelineState.fragmentBuffers.resize_for_index(index);
  MetalPipe::BufferBinding &binding = m_CurrentPipelineState.fragmentBuffers[index];
  binding.resourceId = id;
  binding.byteOffset = offset;

  BufferDescription buffer = GetBuffer(id);
  binding.byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                         ? buffer.length - offset
                         : 0;

  auto argumentBuffer = m_ArgumentBuffers.find({id, offset});
  if(argumentBuffer != m_ArgumentBuffers.end())
  {
    m_CurrentPipelineState.fragmentArgumentBuffers.resize_for_index(index);
    m_CurrentPipelineState.fragmentArgumentBuffers[index] = argumentBuffer->second.binding;
    m_CurrentPipelineState.fragmentArgumentBuffers[index].buffer = binding;
  }
  else if(index < m_CurrentPipelineState.fragmentArgumentBuffers.size())
    m_CurrentPipelineState.fragmentArgumentBuffers[index] = MetalPipe::ArgumentBuffer();
}

void MetalReplay::SetFragmentBufferOffset(uint32_t index, uint64_t offset)
{
  m_CurrentPipelineState.fragmentBuffers.resize_for_index(index);
  BindFragmentBuffer(index, m_CurrentPipelineState.fragmentBuffers[index].resourceId, offset);
}

bool MetalReplay::IsFragmentBufferOffsetValid(uint32_t index, uint64_t offset) const
{
  if(index >= 31 || index >= m_CurrentPipelineState.fragmentBuffers.size())
    return false;
  const MetalPipe::BufferBinding &binding = m_CurrentPipelineState.fragmentBuffers[index];
  return binding.resourceId != ResourceId() && offset % 4 == 0 &&
         (offset < binding.byteOffset || offset - binding.byteOffset < binding.byteSize);
}

void MetalReplay::BindFragmentTexture(uint32_t index, ResourceId id)
{
  m_CurrentPipelineState.fragmentTextures.resize_for_index(index);
  m_CurrentPipelineState.fragmentTextures[index] = id;
}

void MetalReplay::BindVertexTexture(uint32_t index, ResourceId id)
{
  m_CurrentPipelineState.vertexTextures.resize_for_index(index);
  m_CurrentPipelineState.vertexTextures[index] = id;
}

void MetalReplay::BindVertexSampler(uint32_t index, ResourceId id)
{
  m_CurrentSamplerLOD.erase(0xE00 + index);
  m_CurrentPipelineState.vertexSamplers.resize_for_index(index);
  m_CurrentPipelineState.vertexSamplers[index] = id;
}

void MetalReplay::BindFragmentSampler(uint32_t index, ResourceId id)
{
  m_CurrentSamplerLOD.erase(0x100 + index);
  m_CurrentPipelineState.fragmentSamplers.resize_for_index(index);
  m_CurrentPipelineState.fragmentSamplers[index] = id;
}

bool MetalReplay::RegisterArgumentBuffer(WrappedMTLArgumentEncoder *encoder, ResourceId buffer,
                                         uint64_t offset)
{
  const uint64_t length = Unwrap(encoder)->encodedLength();
  for(const auto &entry : m_ArgumentBuffers)
  {
    if(entry.first.first != buffer)
      continue;
    if(entry.first.second == offset)
    {
      if(entry.second.encoder == encoder)
        return true;
      RDCERR("Unsupported Metal argument packet reinterpreted with a different encoder");
      return false;
    }
    const uint64_t other = entry.first.second;
    if((offset < other && other - offset < length) ||
       (offset > other && offset - other < Unwrap(entry.second.encoder)->encodedLength()))
    {
      RDCERR("Invalid overlapping Metal argument packets");
      return false;
    }
  }
  ArgumentPacket &packet = m_ArgumentBuffers[{buffer, offset}];
  packet.encoder = encoder;
  packet.binding.buffer.resourceId = buffer;
  packet.binding.buffer.byteOffset = offset;
  packet.binding.buffer.byteSize = length;
  return true;
}

void MetalReplay::SetArgumentBufferTexture(ResourceId argumentBuffer, uint64_t offset, uint32_t index,
                                           ResourceId texture)
{
  MetalPipe::ArgumentBuffer &binding = m_ArgumentBuffers[{argumentBuffer, offset}].binding;
  binding.textures.resize_for_index(index);
  binding.textures[index] = texture;
}

void MetalReplay::SetArgumentBufferSampler(ResourceId argumentBuffer, uint64_t offset, uint32_t index,
                                           ResourceId sampler)
{
  MetalPipe::ArgumentBuffer &binding = m_ArgumentBuffers[{argumentBuffer, offset}].binding;
  binding.samplers.resize_for_index(index);
  binding.samplers[index] = sampler;
}

void MetalReplay::SetArgumentBufferMember(ResourceId argumentBuffer, uint64_t offset, uint32_t index,
                                           ResourceId buffer, uint64_t bufferOffset)
{
  ArgumentPacket &packet = m_ArgumentBuffers[{argumentBuffer, offset}];
  packet.buffers.resize_for_index(index);
  MetalPipe::BufferBinding &binding = packet.buffers[index];
  binding.resourceId = buffer;
  binding.byteOffset = bufferOffset;
  BufferDescription description = GetBuffer(buffer);
  binding.byteSize = description.resourceId != ResourceId() && bufferOffset < description.length
                         ? description.length - bufferOffset : 0;
}

rdcarray<ResourceId> MetalReplay::GetArgumentBuffers() const
{
  rdcarray<ResourceId> result;
  for(const auto &entry : m_ArgumentBuffers)
    if(result.empty() || result.back() != entry.first.first)
      result.push_back(entry.first.first);
  return result;
}

bool MetalReplay::RestoreArgumentBufferResources(ResourceId id)
{
  // Raw initial/CPU data contains capture-process GPU addresses. Re-encode only resource fields,
  // leaving inline constants and padding intact. Packet selections are immutable after loading.
  for(const auto &entry : m_ArgumentBuffers)
  {
    if(entry.first.first != id)
      continue;
    const ArgumentPacket &packet = entry.second;
    auto rm = m_pDriver->GetResourceManager();
    WrappedMTLObject *object = rm->GetResource(id, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real || !packet.encoder)
      return false;
    MTL::ArgumentEncoder *encoder = Unwrap(packet.encoder);
    encoder->setArgumentBuffer(Unwrap((WrappedMTLBuffer *)object), entry.first.second);
    for(uint32_t i = 0; i < 32; i++)
    {
      const MTL::DataType type = packet.encoder->GetMemberType(i);
      ResourceId resource;
      if(type == MTL::DataTypeTexture && i < packet.binding.textures.size())
        resource = packet.binding.textures[i];
      else if(type == MTL::DataTypeSampler && i < packet.binding.samplers.size())
        resource = packet.binding.samplers[i];
      else if(type == MTL::DataTypePointer && i < packet.buffers.size())
        resource = packet.buffers[i].resourceId;
      else if(packet.rayResources.count(i))
        resource = packet.rayResources.at(i);
      else if(type != MTL::DataTypeTexture && type != MTL::DataTypeSampler && type != MTL::DataTypePointer)
        continue;
      object = resource == ResourceId() ? NULL : rm->GetResource(resource, true);
      if(resource != ResourceId() && (!object || !object->m_Real))
        return false;
      if(type == MTL::DataTypeTexture)
        encoder->setTexture(Unwrap((WrappedMTLTexture *)object), i);
      else if(type == MTL::DataTypeSampler)
        encoder->setSamplerState(Unwrap((WrappedMTLSamplerState *)object), i);
      else if(type == MTL::DataTypeVisibleFunctionTable)
        encoder->setVisibleFunctionTable(Unwrap((WrappedMTLVisibleFunctionTable *)object), i);
      else if(type == MTL::DataTypeIntersectionFunctionTable)
        encoder->setIntersectionFunctionTable(Unwrap((WrappedMTLIntersectionFunctionTable *)object), i);
      else if(type == MTL::DataTypePrimitiveAccelerationStructure ||
              type == MTL::DataTypeInstanceAccelerationStructure)
        encoder->setAccelerationStructure(Unwrap((WrappedMTLAccelerationStructure *)object), i);
      else
        encoder->setBuffer(Unwrap((WrappedMTLBuffer *)object), object ? packet.buffers[i].byteOffset : 0, i);
    }
  }
  return true;
}

void MetalReplay::SetArgumentBufferRayResource(ResourceId buffer, uint64_t offset, uint32_t index,
                                              ResourceId resource, MTL::DataType type)
{
  auto &packet = m_ArgumentBuffers[{buffer, offset}];
  RDCASSERT(packet.encoder && packet.encoder->GetMemberType(index) == type);
  packet.rayResources[index] = resource;
}

bool MetalReplay::ValidateComputeArgumentRayBindings() const
{
  auto rm = m_pDriver->GetResourceManager();
  for(const auto &binding : m_CurrentPipelineState.computeBuffers)
  {
    auto found = m_ArgumentBuffers.find({binding.resourceId, binding.byteOffset});
    if(found == m_ArgumentBuffers.end()) continue;
    const auto &packet = found->second;
    for(uint32_t index = 0; index < 32; index++)
    {
      const auto type = packet.encoder->GetMemberType(index);
      if(type != MTL::DataTypeVisibleFunctionTable && type != MTL::DataTypeIntersectionFunctionTable &&
         type != MTL::DataTypePrimitiveAccelerationStructure && type != MTL::DataTypeInstanceAccelerationStructure)
        continue;
      auto field = packet.rayResources.find(index);
      if(field == packet.rayResources.end()) return false;
      if(field->second == ResourceId()) continue; // Explicit null, not an undeclared/stale field.
      auto object = rm->GetResource(field->second, true);
      if(!object || !object->m_Real) return false;
      if(type == MTL::DataTypeVisibleFunctionTable || type == MTL::DataTypeIntersectionFunctionTable)
      {
        const auto pipeline = type == MTL::DataTypeVisibleFunctionTable ?
            ((WrappedMTLVisibleFunctionTable *)object)->m_Pipeline :
            ((WrappedMTLIntersectionFunctionTable *)object)->m_Pipeline;
        if(GetResID(pipeline) != m_CurrentPipelineState.computePipelineResourceId) return false;
      }
      else
      {
        auto structure = (WrappedMTLAccelerationStructure *)object;
        if(!structure->m_LastBuildKind ||
           (type == MTL::DataTypeInstanceAccelerationStructure) != (structure->m_LastBuildKind == 5))
          return false;
      }
    }
  }
  return true;
}

bool MetalReplay::ValidateArgumentBufferBindings() const
{
  auto slots = m_ShaderArgumentSlots.find(m_CurrentPipelineState.fragmentShader.resourceId);
  if(slots == m_ShaderArgumentSlots.end())
    return true;
  for(const auto &slot : slots->second)
  {
    if(slot.first >= m_CurrentPipelineState.fragmentBuffers.size())
      return false;
    const MetalPipe::BufferBinding &binding = m_CurrentPipelineState.fragmentBuffers[slot.first];
    auto entry = m_ArgumentBuffers.find({binding.resourceId, binding.byteOffset});
    if(entry == m_ArgumentBuffers.end())
    {
      // A raw typed table is restored by descriptor publication/relocation, not
      // by ArgumentEncoder calls. Match the validated consumer and its actual
      // pipeline/stage/slot/resource/offset; a declaration alone is no proof.
      if(m_pDriver->HasRestoredRuntimeTableBinding(m_CurrentPipelineState.pipelineResourceId,
          2, slot.first, binding.resourceId, binding.byteOffset))
        continue;
      RDCERR("Missing Metal argument packet or restored typed table at shader buffer slot %u", slot.first);
      return false;
    }
    const ArgumentPacket &packet = entry->second;
    for(uint32_t i = 0; i < 32; i++)
    {
      const MTL::DataType type = packet.encoder->GetMemberType(i);
      if((type == MTL::DataTypePointer &&
          (i >= packet.buffers.size() || packet.buffers[i].resourceId == ResourceId())) ||
         (type == MTL::DataTypeTexture &&
          (i >= packet.binding.textures.size() || packet.binding.textures[i] == ResourceId())) ||
         (type == MTL::DataTypeSampler &&
          (i >= packet.binding.samplers.size() || packet.binding.samplers[i] == ResourceId())))
      {
        RDCERR("Missing Metal argument resource member %u", i);
        return false;
      }
    }
  }
  return true;
}

void MetalReplay::BindIndexBuffer(ResourceId id, uint64_t offset, MTL::IndexType indexType,
                                  uint64_t indexCount)
{
  MetalPipe::VertexBuffer &binding = m_CurrentPipelineState.indexBuffer;
  binding.resourceId = id;
  binding.byteOffset = offset;
  binding.byteStride = indexType == MTL::IndexTypeUInt16 ? 2 : 4;

  BufferDescription buffer = GetBuffer(id);
  binding.byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                         ? buffer.length - offset
                         : 0;
  if(indexCount > 0)
    binding.byteSize = RDCMIN(binding.byteSize, indexCount * binding.byteStride);
  for(BufferDescription &description : m_Buffers)
    if(description.resourceId == id)
    {
      description.creationFlags |= BufferCategory::Index;
      break;
    }
}

void MetalReplay::SetIndirectBuffer(ResourceId id, uint64_t offset, uint64_t size)
{
  MetalPipe::BufferBinding &binding = m_CurrentPipelineState.indirectBuffer;
  binding.resourceId = id;
  binding.byteOffset = offset;
  binding.byteSize = size;

  if(id == ResourceId())
    return;

  for(BufferDescription &buffer : m_Buffers)
  {
    if(buffer.resourceId == id)
    {
      buffer.creationFlags |= BufferCategory::Indirect;
      break;
    }
  }
}

void MetalReplay::SetViewport(const MTL::Viewport &viewport, uint32_t count)
{
  m_CurrentViewportCount = count;
  m_CurrentPipelineState.rasterizer.viewport =
      Viewport((float)viewport.originX, (float)viewport.originY, (float)viewport.width,
               (float)viewport.height, (float)viewport.znear, (float)viewport.zfar, true);
  m_CurrentPipelineState.rasterizer.viewports = {m_CurrentPipelineState.rasterizer.viewport};
}

void MetalReplay::SetScissor(const MTL::ScissorRect &scissor, uint32_t count)
{
  m_CurrentScissorCount = count;
  m_CurrentPipelineState.rasterizer.scissor =
      Scissor((int32_t)scissor.x, (int32_t)scissor.y, (int32_t)scissor.width,
              (int32_t)scissor.height, true);
  m_CurrentPipelineState.rasterizer.scissors = {m_CurrentPipelineState.rasterizer.scissor};
}

void MetalReplay::SetInspectionViewports(const rdcarray<MTL::Viewport> &viewports)
{
  auto &result = m_CurrentPipelineState.rasterizer.viewports;
  result.clear();
  for(const auto &v : viewports)
    result.push_back(Viewport((float)v.originX, (float)v.originY, (float)v.width,
                             (float)v.height, (float)v.znear, (float)v.zfar, true));
}

void MetalReplay::SetInspectionScissors(const rdcarray<MTL::ScissorRect> &scissors)
{
  auto &result = m_CurrentPipelineState.rasterizer.scissors;
  result.clear();
  for(const auto &s : scissors)
    result.push_back(Scissor((int32_t)s.x, (int32_t)s.y, (int32_t)s.width, (int32_t)s.height, true));
}

void MetalReplay::SetFrontFacingWinding(MTL::Winding winding)
{
  m_CurrentPipelineState.rasterizer.frontCCW = winding == MTL::WindingCounterClockwise;
}

void MetalReplay::SetCullMode(MTL::CullMode cullMode)
{
  switch(cullMode)
  {
    case MTL::CullModeNone: m_CurrentPipelineState.rasterizer.cullMode = CullMode::NoCull; break;
    case MTL::CullModeFront: m_CurrentPipelineState.rasterizer.cullMode = CullMode::Front; break;
    case MTL::CullModeBack: m_CurrentPipelineState.rasterizer.cullMode = CullMode::Back; break;
  }
}

void MetalReplay::SetInspectionTessellationBuffer(ResourceId id, uint64_t offset, uint64_t stride)
{
  auto &binding = m_CurrentPipelineState.tessellationFactors;
  binding.resourceId = id;
  binding.byteOffset = offset;
  const auto buffer = GetBuffer(id);
  binding.byteSize = offset < buffer.length ? buffer.length - offset : 0;
  m_CurrentPipelineState.tessellationInstanceStride = stride;
}

void MetalReplay::SetPrimitiveTopology(MTL::PrimitiveType primitiveType)
{
  m_CurrentPipelineState.patchControlPoints = 0;
  switch(primitiveType)
  {
    case MTL::PrimitiveTypePoint: m_CurrentPipelineState.topology = Topology::PointList; break;
    case MTL::PrimitiveTypeLine: m_CurrentPipelineState.topology = Topology::LineList; break;
    case MTL::PrimitiveTypeLineStrip: m_CurrentPipelineState.topology = Topology::LineStrip; break;
    case MTL::PrimitiveTypeTriangle:
      m_CurrentPipelineState.topology = Topology::TriangleList;
      break;
    case MTL::PrimitiveTypeTriangleStrip:
      m_CurrentPipelineState.topology = Topology::TriangleStrip;
      break;
  }
}

void MetalReplay::SavePipelineState(uint32_t eventId)
{
  auto continuation = m_ContinuationEvents.find(eventId);
  if(continuation != m_ContinuationEvents.end()) eventId = continuation->second;
  auto lod = m_EventSamplerLOD.upper_bound(eventId);
  m_SelectedSamplerLOD = lod == m_EventSamplerLOD.begin() ? m_CurrentSamplerLOD : (--lod)->second;
  if(m_MetalPipelineState)
  {
    auto state = m_EventPipelineStates.upper_bound(eventId);
    if(state != m_EventPipelineStates.begin())
    {
      --state;
      *m_MetalPipelineState = state->second;
    }
    else
    {
      *m_MetalPipelineState = m_CurrentPipelineState;
    }
  }
  if(m_MetalPipelineState)
    for(MetalPipe::Shader *shader : {&m_MetalPipelineState->vertexShader,
                                   &m_MetalPipelineState->fragmentShader,
                                   &m_MetalPipelineState->computeShader,
                                   &m_MetalPipelineState->taskShader,
                                   &m_MetalPipelineState->meshShader})
      if(shader->resourceId != ResourceId())
      {
        // Captured event bindings remain immutable, as with other APIs. Reflection must
        // describe the live edited function, not a freed target or the old captured shader.
        shader->reflection = GetShader(ResourceId(), shader->resourceId, {});
        if(shader->reflection) shader->entryPoint = shader->reflection->entryPoint;
        PopulateShaderFeatures(*shader);
      }
  auto data = m_EventGraphicsInlineData.upper_bound(eventId);
  m_SelectedGraphicsInlineData = data == m_EventGraphicsInlineData.begin()
                                    ? GraphicsInlineData() : (--data)->second;
  m_SelectedBindlessDescriptors.clear();

}

void MetalReplay::SetActionOutputs(ActionDescription &action) const
{
  const size_t count = RDCMIN(action.outputs.size(), m_CurrentPipelineState.colorTargets.size());
  for(size_t i = 0; i < count; i++)
  {
    if(i < m_CurrentPipelineState.resolveTargets.size() &&
       m_CurrentPipelineState.resolveTargets[i].resource != ResourceId())
      action.outputs[i] = m_CurrentPipelineState.resolveTargets[i].resource;
    else
      action.outputs[i] = m_CurrentPipelineState.colorTargets[i].resource;
  }
  action.depthOut = m_CurrentPipelineState.depthTarget.resource;
}

rdcarray<Descriptor> MetalReplay::GetDescriptors(ResourceId descriptorStore,
                                                 const rdcarray<DescriptorRange> &ranges)
{
  static const uint32_t SamplerOffset = 0x100;
  static const uint32_t BufferOffset = 0x200;
  static const uint32_t ComputeReadOffset = 0x300;
  static const uint32_t ComputeWriteOffset = 0x400;
  static const uint32_t ComputeReadBufferOffset = 0xB00;
  static const uint32_t ComputeWriteBufferOffset = 0xC00;
  static const uint32_t ArgumentTextureOffset = 0x500;
  static const uint32_t VertexTextureOffset = 0xD00;
  static const uint32_t VertexBufferOffset = 0xF00;
  static const uint32_t ArgumentBufferOffset = 0x2000;
  if(m_MetalPipelineState == NULL) return {};
  if(descriptorStore != GetResID(m_pDriver))
  {
    rdcarray<Descriptor> result;
    for(const DescriptorRange &range : ranges)
      for(uint32_t i = 0; i < range.count; i++)
      {
        Descriptor descriptor;
        for(const auto &binding : m_SelectedBindlessDescriptors)
          if(binding.access.descriptorStore == descriptorStore &&
             binding.access.byteOffset == range.offset + i * range.descriptorSize &&
             binding.access.type == range.type)
            descriptor = binding.descriptor;
        result.push_back(descriptor);
      }
    return result;
  }

  size_t count = 0;
  for(const DescriptorRange &range : ranges)
    count += range.count;
  rdcarray<Descriptor> ret;
  ret.resize(count);

  size_t dst = 0;
  for(const DescriptorRange &range : ranges)
  {
    for(uint32_t i = 0; i < range.count; i++, dst++)
    {
      const uint32_t offset = range.offset + i * range.descriptorSize;
      Descriptor &descriptor = ret[dst];
      if(offset >= 0x50000 && offset < 0x50006 &&
         (m_MetalPipelineState->metalFXSpatial.size() == 9 || m_MetalPipelineState->metalFXTemporal.size() == 17))
      {
        const bool output = offset == 0x50001;
        const DescriptorType type = output ? DescriptorType::ReadWriteImage : DescriptorType::Image;
        if(range.type == type)
        {
          descriptor.type = type;
          descriptor.resource = output ? m_MetalPipelineState->metalFXOutput :
              offset == 0x50000 ? m_MetalPipelineState->metalFXInput :
              (m_MetalPipelineState->metalFXTemporalInputs.size() == 5 ?
                 m_MetalPipelineState->metalFXTemporalInputs[offset-0x50001] : ResourceId());
          const TextureDescription texture = GetTexture(descriptor.resource);
          descriptor.format = texture.format;
          descriptor.textureType = texture.type;
          descriptor.numMips = uint8_t(texture.mips);
          descriptor.numSlices = uint16_t(texture.arraysize);
        }
      }
      else if(offset >= 0x40000 && offset < 0x40008 && range.type == DescriptorType::Image)
      {
        uint32_t slot = offset - 0x40000;
        if(slot < m_MetalPipelineState->colorTargets.size())
        {
          descriptor = m_MetalPipelineState->colorTargets[slot];
          descriptor.type = DescriptorType::Image;
        }
      }
      else if(offset >= 0x20000 && offset < 0x21000)
      {
        const ShaderStage stage = ShaderStage((offset - 0x20000) >> 8);
        const uint32_t slot = offset & 0xff;
        const auto &structures = stage == ShaderStage::Compute ? m_MetalPipelineState->computeAccelerationStructures :
                                 stage == ShaderStage::Vertex ? m_MetalPipelineState->vertexAccelerationStructures :
                                                               m_MetalPipelineState->fragmentAccelerationStructures;
        if(slot < structures.size()) { descriptor.type = DescriptorType::AccelerationStructure; descriptor.resource = structures[slot]; }
      }
      else if(offset >= 0x10000 && offset < 0x18000)
      {
        const ShaderStage stage = ShaderStage((offset - 0x10000) >> 12);
        const uint32_t slot = offset & 0xff, category = offset & 0xf00;
        const auto &buffers = stage == ShaderStage::Task ? m_MetalPipelineState->taskBuffers : m_MetalPipelineState->meshBuffers;
        const auto &textures = stage == ShaderStage::Task ? m_MetalPipelineState->taskTextures : m_MetalPipelineState->meshTextures;
        descriptor.type = range.type;
        if(category == 0 && slot < buffers.size())
        { descriptor.resource = buffers[slot].resourceId; descriptor.byteOffset = buffers[slot].byteOffset; descriptor.byteSize = buffers[slot].byteSize; }
        else if(category == 0x100 && slot < textures.size())
        {
          descriptor.resource = textures[slot]; const auto texture = GetTexture(descriptor.resource);
          descriptor.format = texture.format; descriptor.textureType = texture.type;
          descriptor.numMips = uint8_t(texture.mips); descriptor.numSlices = uint16_t(texture.arraysize);
        }
      }
      else if(range.type == DescriptorType::Buffer && offset >= ArgumentBufferOffset &&
         offset < ArgumentBufferOffset + 32 * 32)
      {
        const uint32_t encoded = offset - ArgumentBufferOffset;
        const uint32_t slot = encoded / 32, member = encoded % 32;
        if(slot < m_MetalPipelineState->fragmentArgumentBuffers.size())
        {
          const MetalPipe::BufferBinding &outer = m_MetalPipelineState->fragmentArgumentBuffers[slot].buffer;
          auto packet = m_ArgumentBuffers.find({outer.resourceId, outer.byteOffset});
          if(packet != m_ArgumentBuffers.end() && member < packet->second.buffers.size())
          {
            const MetalPipe::BufferBinding &binding = packet->second.buffers[member];
            descriptor.type = DescriptorType::Buffer;
            descriptor.resource = binding.resourceId;
            descriptor.byteOffset = binding.byteOffset;
            descriptor.byteSize = binding.byteSize;
          }
        }
      }
      else if((range.type == DescriptorType::Buffer || range.type == DescriptorType::ReadWriteBuffer) &&
         offset >= VertexBufferOffset)
      {
        const uint32_t slot = offset - VertexBufferOffset;
        if(slot < m_MetalPipelineState->vertexStorageBuffers.size())
        {
          const MetalPipe::BufferBinding &binding =
              m_MetalPipelineState->vertexStorageBuffers[slot];
          descriptor.type = range.type;
          descriptor.resource = binding.resourceId;
          descriptor.byteOffset = binding.byteOffset;
          descriptor.byteSize = binding.byteSize;
        }
      }
      else if((range.type == DescriptorType::Image || range.type == DescriptorType::ReadWriteImage) && offset >= VertexTextureOffset)
      {
        const uint32_t slot = offset - VertexTextureOffset;
        if(slot < m_MetalPipelineState->vertexTextures.size())
        {
          descriptor.type = range.type;
          descriptor.resource = m_MetalPipelineState->vertexTextures[slot];
          TextureDescription texture = GetTexture(descriptor.resource);
          if(texture.resourceId != ResourceId())
          {
            descriptor.format = texture.format;
            descriptor.textureType = texture.type;
            descriptor.numMips = (uint8_t)texture.mips;
            descriptor.numSlices = (uint16_t)texture.arraysize;
          }
        }
      }
      else if((range.type == DescriptorType::Image || range.type == DescriptorType::ReadWriteImage) && offset < SamplerOffset &&
         offset < m_MetalPipelineState->fragmentTextures.size())
      {
        descriptor.type = range.type;
        descriptor.resource = m_MetalPipelineState->fragmentTextures[offset];
        TextureDescription texture = GetTexture(descriptor.resource);
        if(texture.resourceId != ResourceId())
        {
          descriptor.format = texture.format;
          descriptor.textureType = texture.type;
          descriptor.numMips = (uint8_t)texture.mips;
          descriptor.numSlices = (uint16_t)texture.arraysize;
        }
      }
      else if((range.type == DescriptorType::ConstantBuffer ||
               range.type == DescriptorType::Buffer ||
               range.type == DescriptorType::ReadWriteBuffer) &&
              offset >= BufferOffset && offset < ComputeReadOffset)
      {
        const uint32_t slot = offset - BufferOffset;
        if(slot < m_MetalPipelineState->fragmentBuffers.size())
        {
          const MetalPipe::BufferBinding &binding =
              m_MetalPipelineState->fragmentBuffers[slot];
          descriptor.type = range.type;
          descriptor.resource = binding.resourceId;
          descriptor.byteOffset = binding.byteOffset;
          descriptor.byteSize = binding.byteSize;
        }
      }
      else if(((range.type == DescriptorType::Buffer &&
                offset >= ComputeReadBufferOffset && offset < ComputeWriteBufferOffset) ||
               (range.type == DescriptorType::ReadWriteBuffer &&
                offset >= ComputeWriteBufferOffset && offset < VertexTextureOffset)))
      {
        const uint32_t slot = offset - (range.type == DescriptorType::Buffer
                                            ? ComputeReadBufferOffset
                                            : ComputeWriteBufferOffset);
        if(slot < m_MetalPipelineState->computeBuffers.size())
        {
          const MetalPipe::BufferBinding &binding = m_MetalPipelineState->computeBuffers[slot];
          descriptor.type = range.type;
          descriptor.resource = binding.resourceId;
          descriptor.byteOffset = binding.byteOffset;
          descriptor.byteSize = binding.byteSize;
        }
      }
      else if((range.type == DescriptorType::Image && offset >= ComputeReadOffset &&
               offset < ComputeWriteOffset) ||
              (range.type == DescriptorType::ReadWriteImage && offset >= ComputeWriteOffset))
      {
        const uint32_t slot = offset - (range.type == DescriptorType::Image
                                            ? ComputeReadOffset
                                            : ComputeWriteOffset);
        if(slot < m_MetalPipelineState->computeTextures.size())
        {
          descriptor.type = range.type;
          descriptor.resource = m_MetalPipelineState->computeTextures[slot];
          TextureDescription texture = GetTexture(descriptor.resource);
          if(texture.resourceId != ResourceId())
          {
            descriptor.format = texture.format;
            descriptor.textureType = texture.type;
            descriptor.numMips = (uint8_t)texture.mips;
            descriptor.numSlices = (uint16_t)texture.arraysize;
          }
        }
      }
      else if(range.type == DescriptorType::Image && offset >= ArgumentTextureOffset)
      {
        const uint32_t encoded = offset - ArgumentTextureOffset;
        const uint32_t bufferSlot = encoded / 32;
        const uint32_t member = encoded % 32;
        if(bufferSlot < m_MetalPipelineState->fragmentArgumentBuffers.size() &&
           member < m_MetalPipelineState->fragmentArgumentBuffers[bufferSlot].textures.size())
        {
          descriptor.type = DescriptorType::Image;
          descriptor.resource =
              m_MetalPipelineState->fragmentArgumentBuffers[bufferSlot].textures[member];
          TextureDescription texture = GetTexture(descriptor.resource);
          if(texture.resourceId != ResourceId())
          {
            descriptor.format = texture.format;
            descriptor.textureType = texture.type;
            descriptor.numMips = (uint8_t)texture.mips;
            descriptor.numSlices = (uint16_t)texture.arraysize;
          }
        }
      }
    }
  }
  return ret;
}

static AddressMode MakeAddressMode(MTL::SamplerAddressMode mode)
{
  switch(mode)
  {
    case MTL::SamplerAddressModeClampToEdge: return AddressMode::ClampEdge;
    case MTL::SamplerAddressModeMirrorClampToEdge: return AddressMode::MirrorOnce;
    case MTL::SamplerAddressModeRepeat: return AddressMode::Wrap;
    case MTL::SamplerAddressModeMirrorRepeat: return AddressMode::Mirror;
    case MTL::SamplerAddressModeClampToZero:
    case MTL::SamplerAddressModeClampToBorderColor: return AddressMode::ClampBorder;
  }
  return AddressMode::Wrap;
}

static FilterMode MakeFilterMode(MTL::SamplerMinMagFilter filter)
{
  return filter == MTL::SamplerMinMagFilterLinear ? FilterMode::Linear : FilterMode::Point;
}

static FilterMode MakeFilterMode(MTL::SamplerMipFilter filter)
{
  if(filter == MTL::SamplerMipFilterLinear)
    return FilterMode::Linear;
  if(filter == MTL::SamplerMipFilterNearest)
    return FilterMode::Point;
  return FilterMode::NoFilter;
}

rdcarray<SamplerDescriptor> MetalReplay::GetSamplerDescriptors(ResourceId descriptorStore,
                                                               const rdcarray<DescriptorRange> &ranges)
{
  static const uint32_t SamplerOffset = 0x100;
  static const uint32_t ArgumentSamplerOffset = 0x900;
  static const uint32_t VertexSamplerOffset = 0xE00;
  static const uint32_t ComputeSamplerOffset = 0x1000;

  size_t count = 0;
  for(const DescriptorRange &range : ranges)
    count += range.count;
  rdcarray<SamplerDescriptor> ret;
  ret.resize(count);
  if(m_MetalPipelineState == NULL) return ret;
  const bool tableSampler=descriptorStore!=GetResID(m_pDriver);

  size_t dst = 0;
  for(const DescriptorRange &range : ranges)
  {
    for(uint32_t i = 0; i < range.count; i++, dst++)
    {
      const uint32_t offset = range.offset + i * range.descriptorSize;
      if(range.type != DescriptorType::Sampler || (!tableSampler && offset < SamplerOffset))
        continue;

      ResourceId samplerId;
      if(tableSampler)
      {
        for(const auto &binding:m_SelectedBindlessDescriptors)
          if(binding.access.type==DescriptorType::Sampler &&
             binding.access.descriptorStore==descriptorStore && binding.access.byteOffset==offset)
            samplerId=binding.descriptor.resource;
      }
      else if(offset >= 0x10000 && offset < 0x18000)
      {
        const ShaderStage stage = ShaderStage((offset - 0x10000) >> 12);
        const auto &samplers = stage == ShaderStage::Task ? m_MetalPipelineState->taskSamplers : m_MetalPipelineState->meshSamplers;
        const uint32_t slot = offset & 0xff;
        if(slot < samplers.size()) samplerId = samplers[slot];
      }
      else if(offset >= ComputeSamplerOffset)
      {
        const uint32_t slot = offset - ComputeSamplerOffset;
        if(slot < m_MetalPipelineState->computeSamplers.size())
          samplerId = m_MetalPipelineState->computeSamplers[slot];
      }
      else if(offset >= VertexSamplerOffset)
      {
        const uint32_t slot = offset - VertexSamplerOffset;
        if(slot < m_MetalPipelineState->vertexSamplers.size())
          samplerId = m_MetalPipelineState->vertexSamplers[slot];
      }
      else if(offset >= ArgumentSamplerOffset)
      {
        const uint32_t encoded = offset - ArgumentSamplerOffset;
        const uint32_t bufferSlot = encoded / 32;
        const uint32_t member = encoded % 32;
        if(bufferSlot < m_MetalPipelineState->fragmentArgumentBuffers.size() &&
           member < m_MetalPipelineState->fragmentArgumentBuffers[bufferSlot].samplers.size())
          samplerId = m_MetalPipelineState->fragmentArgumentBuffers[bufferSlot].samplers[member];
      }
      else
      {
        const uint32_t slot = offset - SamplerOffset;
        if(slot < m_MetalPipelineState->fragmentSamplers.size())
          samplerId = m_MetalPipelineState->fragmentSamplers[slot];
      }
      auto captured = m_SamplerStates.find(samplerId);
      if(captured == m_SamplerStates.end())
        continue;

      const RDMTL::SamplerDescriptor &source = captured->second;
      SamplerDescriptor &sampler = ret[dst];
      sampler.type = DescriptorType::Sampler;
      sampler.object = samplerId;
      sampler.addressU = MakeAddressMode(source.sAddressMode);
      sampler.addressV = MakeAddressMode(source.tAddressMode);
      sampler.addressW = MakeAddressMode(source.rAddressMode);
      sampler.compareFunction = MakeCompareFunction(source.compareFunction);
      sampler.filter.minify = MakeFilterMode(source.minFilter);
      sampler.filter.magnify = MakeFilterMode(source.magFilter);
      sampler.filter.mip = MakeFilterMode(source.mipFilter);
      sampler.filter.filter = source.compareFunction == MTL::CompareFunctionNever
                                  ? FilterFunction::Normal
                                  : FilterFunction::Comparison;
      sampler.maxAnisotropy = (float)source.maxAnisotropy;
      sampler.minLOD = source.lodMinClamp;
      sampler.maxLOD = source.lodMaxClamp;
      auto overrideLOD = m_SelectedSamplerLOD.find(offset);
      if(!tableSampler && overrideLOD != m_SelectedSamplerLOD.end())
      {
        sampler.minLOD = overrideLOD->second.first;
        sampler.maxLOD = overrideLOD->second.second;
      }
      sampler.unnormalized = !source.normalizedCoordinates;
      if(source.borderColor == MTL::SamplerBorderColorOpaqueBlack)
        sampler.borderColorValue.floatValue[3] = 1.0f;
      else if(source.borderColor == MTL::SamplerBorderColorOpaqueWhite)
      {
        for(float &component : sampler.borderColorValue.floatValue)
          component = 1.0f;
      }
    }
  }
  return ret;
}

rdcarray<DescriptorAccess> MetalReplay::GetDescriptorAccess(uint32_t eventId)
{
  static const uint32_t SamplerOffset = 0x100;
  static const uint32_t BufferOffset = 0x200;
  static const uint32_t ComputeReadOffset = 0x300;
  static const uint32_t ComputeWriteOffset = 0x400;
  static const uint32_t ComputeReadBufferOffset = 0xB00;
  static const uint32_t ComputeWriteBufferOffset = 0xC00;
  static const uint32_t ArgumentTextureOffset = 0x500;
  static const uint32_t ArgumentSamplerOffset = 0x900;
  static const uint32_t VertexTextureOffset = 0xD00;
  static const uint32_t VertexSamplerOffset = 0xE00;
  static const uint32_t VertexBufferOffset = 0xF00;
  static const uint32_t ComputeSamplerOffset = 0x1000;
  const MetalPipe::State *state = m_MetalPipelineState;
  if(state == NULL)
    return {};

  rdcarray<DescriptorAccess> ret;
  const ResourceId store = GetResID(m_pDriver);
  if(state->metalFXSpatial.size() == 9 || state->metalFXTemporal.size() == 17)
  {
    // Opaque operation resources, exposed through the common inspection API without
    // inventing an internal shader or a texture(n) shader binding.
    for(uint32_t i = 0; i < (state->metalFXTemporal.size() == 17 ? 6U : 2U); i++)
    {
      ResourceId resource = i == 1 ? state->metalFXOutput : i == 0 ? state->metalFXInput :
                            state->metalFXTemporalInputs[i-1];
      if(resource == ResourceId())
        continue;
      DescriptorAccess access;
      access.stage = ShaderStage::Compute;
      access.type = i == 1 ? DescriptorType::ReadWriteImage : DescriptorType::Image;
      access.descriptorStore = store;
      access.byteOffset = 0x50000 + i;
      access.byteSize = 1;
      access.index = DescriptorAccess::NoShaderBinding;
      access.arrayElement = i;
      ret.push_back(access);
    }
    return ret;
  }
  for(uint32_t slot : state->fragmentShader.framebufferFetch)
  {
    if(slot >= state->colorTargets.size() || state->colorTargets[slot].resource == ResourceId()) continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Fragment;
    access.type = DescriptorType::Image;
    access.descriptorStore = store;
    access.byteOffset = 0x40000 + slot;
    access.byteSize = 1;
    access.index = DescriptorAccess::NoShaderBinding;
    access.arrayElement = slot;
    ret.push_back(access);
  }
  const ResourceId vertexShader = state->vertexShader.resourceId;
  auto vertexReflectionIt = m_Shaders.find(vertexShader);
  auto vertexUsageIt = m_ShaderBindingUsage.find(vertexShader);
  const bool hasVertexReflection = vertexReflectionIt != m_Shaders.end() &&
                                   vertexUsageIt != m_ShaderBindingUsage.end() &&
                                   vertexUsageIt->second.available;
  for(size_t slot = 0; slot < state->vertexStorageBuffers.size(); slot++)
  {
    if(state->vertexStorageBuffers[slot].resourceId == ResourceId())
      continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Vertex;
    access.type = DescriptorType::Buffer;
    access.index = DescriptorAccess::NoShaderBinding;
    if(hasVertexReflection)
    {
      const rdcarray<ShaderResource> &resources = vertexReflectionIt->second.readOnlyResources;
      for(size_t bind = 0; bind < resources.size(); bind++)
        if(!resources[bind].isTexture && resources[bind].descriptorType == DescriptorType::Buffer &&
           resources[bind].fixedBindNumber == slot)
        {
          access.index = (uint16_t)bind;
          access.staticallyUnused = bind >= vertexUsageIt->second.readOnlyResources.size() ||
                                    !vertexUsageIt->second.readOnlyResources[bind];
          break;
        }
      if(access.index == DescriptorAccess::NoShaderBinding)
      {
        const auto &writes = vertexReflectionIt->second.readWriteResources;
        for(size_t bind = 0; bind < writes.size(); bind++)
          if(!writes[bind].isTexture && writes[bind].descriptorType == DescriptorType::ReadWriteBuffer &&
             writes[bind].fixedBindNumber == slot)
          {
            access.type = DescriptorType::ReadWriteBuffer;
            access.index = (uint16_t)bind;
            access.staticallyUnused = bind >= vertexUsageIt->second.readWriteResources.size() ||
                                      !vertexUsageIt->second.readWriteResources[bind];
            break;
          }
      }
      if(access.index == DescriptorAccess::NoShaderBinding)
        access.staticallyUnused = true;
    }
    else
    {
      access.index = (uint16_t)slot;
    }
    access.descriptorStore = store;
    access.byteOffset = VertexBufferOffset + (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  for(size_t slot = 0; slot < state->vertexTextures.size(); slot++)
  {
    if(state->vertexTextures[slot] == ResourceId())
      continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Vertex;
    access.type = DescriptorType::Image;
    access.index = DescriptorAccess::NoShaderBinding;
    if(hasVertexReflection)
    {
      const rdcarray<ShaderResource> &resources = vertexReflectionIt->second.readOnlyResources;
      for(size_t bind = 0; bind < resources.size(); bind++)
        if(resources[bind].fixedBindNumber == slot)
        {
          access.index = (uint16_t)bind;
          access.staticallyUnused = bind >= vertexUsageIt->second.readOnlyResources.size() ||
                                    !vertexUsageIt->second.readOnlyResources[bind];
          break;
        }
      if(access.index == DescriptorAccess::NoShaderBinding)
        access.staticallyUnused = true;
    }
    else
    {
      access.index = (uint16_t)slot;
    }
    access.descriptorStore = store;
    access.byteOffset = VertexTextureOffset + (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  for(size_t slot = 0; slot < state->vertexSamplers.size(); slot++)
  {
    if(state->vertexSamplers[slot] == ResourceId())
      continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Vertex;
    access.type = DescriptorType::Sampler;
    access.index = DescriptorAccess::NoShaderBinding;
    if(hasVertexReflection)
    {
      const rdcarray<ShaderSampler> &samplers = vertexReflectionIt->second.samplers;
      for(size_t bind = 0; bind < samplers.size(); bind++)
        if(samplers[bind].fixedBindNumber == slot)
        {
          access.index = (uint16_t)bind;
          access.staticallyUnused = bind >= vertexUsageIt->second.samplers.size() ||
                                    !vertexUsageIt->second.samplers[bind];
          break;
        }
      if(access.index == DescriptorAccess::NoShaderBinding)
        access.staticallyUnused = true;
    }
    else
    {
      access.index = (uint16_t)slot;
    }
    access.descriptorStore = store;
    access.byteOffset = VertexSamplerOffset + (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  const ResourceId fragmentShader = state->fragmentShader.resourceId;
  auto reflectionIt = m_Shaders.find(fragmentShader);
  auto usageIt = m_ShaderBindingUsage.find(fragmentShader);
  const bool hasReflection = reflectionIt != m_Shaders.end() &&
                             usageIt != m_ShaderBindingUsage.end() &&
                             usageIt->second.available;
  for(size_t slot = 0; slot < state->fragmentBuffers.size(); slot++)
  {
    if(state->fragmentBuffers[slot].resourceId == ResourceId())
      continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Fragment;
    access.type = DescriptorType::ConstantBuffer;
    access.index = DescriptorAccess::NoShaderBinding;
    if(hasReflection)
    {
      const rdcarray<ConstantBlock> &blocks = reflectionIt->second.constantBlocks;
      for(size_t bind = 0; bind < blocks.size(); bind++)
      {
        if(blocks[bind].fixedBindNumber == slot)
        {
          access.index = (uint16_t)bind;
          access.staticallyUnused = bind >= usageIt->second.constantBlocks.size() ||
                                    !usageIt->second.constantBlocks[bind];
          break;
        }
      }
      if(access.index == DescriptorAccess::NoShaderBinding)
      {
        const rdcarray<ShaderResource> &resources = reflectionIt->second.readOnlyResources;
        for(size_t bind = 0; bind < resources.size(); bind++)
          if(!resources[bind].isTexture && resources[bind].fixedBindNumber == slot &&
             resources[bind].descriptorType == DescriptorType::Buffer)
          {
            access.type = DescriptorType::Buffer;
            access.index = (uint16_t)bind;
            access.staticallyUnused = bind >= usageIt->second.readOnlyResources.size() ||
                                      !usageIt->second.readOnlyResources[bind];
            break;
          }
        if(access.index == DescriptorAccess::NoShaderBinding)
          access.staticallyUnused = true;
      }
    }
    else
    {
      access.index = (uint16_t)slot;
    }
    access.descriptorStore = store;
    access.byteOffset = BufferOffset + (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  for(size_t slot = 0; slot < state->fragmentTextures.size(); slot++)
  {
    if(state->fragmentTextures[slot] == ResourceId())
      continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Fragment;
    access.type = DescriptorType::Image;
    access.index = DescriptorAccess::NoShaderBinding;
    if(hasReflection)
    {
      const rdcarray<ShaderResource> &resources = reflectionIt->second.readOnlyResources;
      for(size_t bind = 0; bind < resources.size(); bind++)
      {
        if(resources[bind].fixedBindNumber == slot)
        {
          access.index = (uint16_t)bind;
          access.staticallyUnused = bind >= usageIt->second.readOnlyResources.size() ||
                                    !usageIt->second.readOnlyResources[bind];
          break;
        }
      }
      if(access.index == DescriptorAccess::NoShaderBinding)
        access.staticallyUnused = true;
    }
    else
    {
      access.index = (uint16_t)slot;
    }
    access.descriptorStore = store;
    access.byteOffset = (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  for(size_t slot = 0; slot < state->fragmentSamplers.size(); slot++)
  {
    if(state->fragmentSamplers[slot] == ResourceId())
      continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Fragment;
    access.type = DescriptorType::Sampler;
    access.index = DescriptorAccess::NoShaderBinding;
    if(hasReflection)
    {
      const rdcarray<ShaderSampler> &samplers = reflectionIt->second.samplers;
      for(size_t bind = 0; bind < samplers.size(); bind++)
      {
        if(samplers[bind].fixedBindNumber == slot)
        {
          access.index = (uint16_t)bind;
          access.staticallyUnused = bind >= usageIt->second.samplers.size() ||
                                    !usageIt->second.samplers[bind];
          break;
        }
      }
      if(access.index == DescriptorAccess::NoShaderBinding)
        access.staticallyUnused = true;
    }
    else
    {
      access.index = (uint16_t)slot;
    }
    access.descriptorStore = store;
    access.byteOffset = SamplerOffset + (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  for(size_t bufferSlot = 0; bufferSlot < state->fragmentArgumentBuffers.size(); bufferSlot++)
  {
    const MetalPipe::ArgumentBuffer &argument = state->fragmentArgumentBuffers[bufferSlot];
    rdcstr prefix;
    auto slots = m_ShaderArgumentSlots.find(fragmentShader);
    if(slots != m_ShaderArgumentSlots.end())
    {
      auto name = slots->second.find((uint32_t)bufferSlot);
      if(name != slots->second.end())
        prefix = name->second + ".";
    }
    auto packet = m_ArgumentBuffers.find({argument.buffer.resourceId, argument.buffer.byteOffset});
    if(packet != m_ArgumentBuffers.end())
      for(size_t member = 0; member < packet->second.buffers.size(); member++)
      {
        if(packet->second.buffers[member].resourceId == ResourceId())
          continue;
        DescriptorAccess access;
        access.stage = ShaderStage::Fragment;
        access.type = DescriptorType::Buffer;
        access.index = DescriptorAccess::NoShaderBinding;
        access.staticallyUnused = true;
        if(hasReflection && !prefix.empty())
          for(size_t bind = 0; bind < reflectionIt->second.readOnlyResources.size(); bind++)
          {
            const ShaderResource &resource = reflectionIt->second.readOnlyResources[bind];
            if(!resource.isTexture && resource.fixedBindNumber == member && resource.name.find(prefix) == 0)
            {
              access.index = (uint16_t)bind;
              access.staticallyUnused = bind >= usageIt->second.readOnlyResources.size() ||
                                        !usageIt->second.readOnlyResources[bind];
              break;
            }
          }
        access.descriptorStore = store;
        access.byteOffset = 0x2000 + (uint32_t)bufferSlot * 32 + (uint32_t)member;
        access.byteSize = 1;
        ret.push_back(access);
      }
    for(size_t member = 0; member < argument.textures.size(); member++)
    {
      if(argument.textures[member] == ResourceId())
        continue;
      DescriptorAccess access;
      access.stage = ShaderStage::Fragment;
      access.type = DescriptorType::Image;
      access.index = DescriptorAccess::NoShaderBinding;
      if(hasReflection)
      {
        for(size_t bind = 0; bind < reflectionIt->second.readOnlyResources.size(); bind++)
          if(reflectionIt->second.readOnlyResources[bind].isTexture &&
             reflectionIt->second.readOnlyResources[bind].fixedBindNumber == member &&
             !prefix.empty() && reflectionIt->second.readOnlyResources[bind].name.find(prefix) == 0)
          {
            access.index = (uint16_t)bind;
            access.staticallyUnused = bind >= usageIt->second.readOnlyResources.size() ||
                                      !usageIt->second.readOnlyResources[bind];
            break;
          }
      }
      access.descriptorStore = store;
      access.byteOffset = ArgumentTextureOffset + (uint32_t)bufferSlot * 32 + (uint32_t)member;
      access.byteSize = 1;
      ret.push_back(access);
    }
    for(size_t member = 0; member < argument.samplers.size(); member++)
    {
      if(argument.samplers[member] == ResourceId())
        continue;
      DescriptorAccess access;
      access.stage = ShaderStage::Fragment;
      access.type = DescriptorType::Sampler;
      access.index = DescriptorAccess::NoShaderBinding;
      if(hasReflection)
      {
        for(size_t bind = 0; bind < reflectionIt->second.samplers.size(); bind++)
          if(reflectionIt->second.samplers[bind].fixedBindNumber == member && !prefix.empty() &&
             reflectionIt->second.samplers[bind].name.find(prefix) == 0)
          {
            access.index = (uint16_t)bind;
            access.staticallyUnused = bind >= usageIt->second.samplers.size() ||
                                      !usageIt->second.samplers[bind];
            break;
          }
      }
      access.descriptorStore = store;
      access.byteOffset = ArgumentSamplerOffset + (uint32_t)bufferSlot * 32 + (uint32_t)member;
      access.byteSize = 1;
      ret.push_back(access);
    }
  }
  const ResourceId computeShader = state->computeShader.resourceId;
  reflectionIt = m_Shaders.find(computeShader);
  usageIt = m_ShaderBindingUsage.find(computeShader);
  const bool hasComputeReflection = reflectionIt != m_Shaders.end() &&
                                    usageIt != m_ShaderBindingUsage.end() &&
                                    usageIt->second.available;
  for(size_t slot = 0; slot < state->computeSamplers.size(); slot++)
  {
    if(state->computeSamplers[slot] == ResourceId())
      continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Compute;
    access.type = DescriptorType::Sampler;
    access.index = DescriptorAccess::NoShaderBinding;
    if(hasComputeReflection)
    {
      const rdcarray<ShaderSampler> &samplers = reflectionIt->second.samplers;
      for(size_t bind = 0; bind < samplers.size(); bind++)
        if(samplers[bind].fixedBindNumber == slot)
        {
          access.index = (uint16_t)bind;
          access.staticallyUnused = bind >= usageIt->second.samplers.size() ||
                                    !usageIt->second.samplers[bind];
          break;
        }
      if(access.index == DescriptorAccess::NoShaderBinding)
        access.staticallyUnused = true;
    }
    else
      access.index = (uint16_t)slot;
    access.descriptorStore = store;
    access.byteOffset = ComputeSamplerOffset + (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  for(size_t slot = 0; slot < state->computeTextures.size(); slot++)
  {
    if(state->computeTextures[slot] == ResourceId())
      continue;
    bool readOnly = true;
    size_t bindingIndex = slot;
    if(hasComputeReflection)
    {
      const ShaderReflection &reflection = reflectionIt->second;
      bool found = false;
      for(size_t bind = 0; bind < reflection.readOnlyResources.size(); bind++)
      {
        if(reflection.readOnlyResources[bind].isTexture &&
           reflection.readOnlyResources[bind].fixedBindNumber == slot)
        {
          bindingIndex = bind;
          found = true;
          break;
        }
      }
      if(!found)
      {
        readOnly = false;
        for(size_t bind = 0; bind < reflection.readWriteResources.size(); bind++)
        {
          if(reflection.readWriteResources[bind].isTexture &&
             reflection.readWriteResources[bind].fixedBindNumber == slot)
          {
            bindingIndex = bind;
            found = true;
            break;
          }
        }
      }
      if(!found)
      {
        bindingIndex = DescriptorAccess::NoShaderBinding;
        readOnly = true;
      }
    }
    DescriptorAccess access;
    access.stage = ShaderStage::Compute;
    access.type = readOnly ? DescriptorType::Image : DescriptorType::ReadWriteImage;
    access.index = (uint16_t)bindingIndex;
    access.staticallyUnused = hasComputeReflection &&
        (bindingIndex == DescriptorAccess::NoShaderBinding ||
         (readOnly ? bindingIndex >= usageIt->second.readOnlyResources.size() ||
                    !usageIt->second.readOnlyResources[bindingIndex]
                  : bindingIndex >= usageIt->second.readWriteResources.size() ||
                    !usageIt->second.readWriteResources[bindingIndex]));
    access.descriptorStore = store;
    access.byteOffset = (readOnly ? ComputeReadOffset : ComputeWriteOffset) + (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  for(size_t slot = 0; slot < state->computeBuffers.size(); slot++)
  {
    if(state->computeBuffers[slot].resourceId == ResourceId())
      continue;
    bool readOnly = true;
    size_t bindingIndex = slot;
    if(hasComputeReflection)
    {
      const ShaderReflection &reflection = reflectionIt->second;
      bool found = false;
      for(size_t bind = 0; bind < reflection.readOnlyResources.size(); bind++)
        if(!reflection.readOnlyResources[bind].isTexture &&
           reflection.readOnlyResources[bind].fixedBindNumber == slot)
        {
          bindingIndex = bind;
          found = true;
          break;
        }
      if(!found)
      {
        readOnly = false;
        for(size_t bind = 0; bind < reflection.readWriteResources.size(); bind++)
          if(!reflection.readWriteResources[bind].isTexture &&
             reflection.readWriteResources[bind].fixedBindNumber == slot)
          {
            bindingIndex = bind;
            found = true;
            break;
          }
      }
      if(!found)
      {
        bindingIndex = DescriptorAccess::NoShaderBinding;
        readOnly = true;
      }
    }
    DescriptorAccess access;
    access.stage = ShaderStage::Compute;
    access.type = readOnly ? DescriptorType::Buffer : DescriptorType::ReadWriteBuffer;
    access.index = (uint16_t)bindingIndex;
    access.staticallyUnused = hasComputeReflection &&
        (bindingIndex == DescriptorAccess::NoShaderBinding ||
         (readOnly ? bindingIndex >= usageIt->second.readOnlyResources.size() ||
                    !usageIt->second.readOnlyResources[bindingIndex]
                  : bindingIndex >= usageIt->second.readWriteResources.size() ||
                    !usageIt->second.readWriteResources[bindingIndex]));
    access.descriptorStore = store;
    access.byteOffset = (readOnly ? ComputeReadBufferOffset : ComputeWriteBufferOffset) + (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  AppendExtendedDescriptorAccess(ret);
  m_UniformInspectionOffset=eventId<m_Events.size()?m_Events[eventId].fileOffset:UINT64_MAX;
  ResolveUniformBindlessAccess();
  for(const auto &binding : m_SelectedBindlessDescriptors)
    ret.push_back(binding.access);
  // Bindings may remain set on an absent Native shader stage. Those valid API
  // bindings are not accesses; keep pipeline-state display separate from usage.
  ret.removeIf([&](const DescriptorAccess &access) {
    switch(access.stage)
    {
      case ShaderStage::Vertex: return state->vertexShader.resourceId == ResourceId();
      case ShaderStage::Fragment: return state->fragmentShader.resourceId == ResourceId();
      case ShaderStage::Compute: return state->computeShader.resourceId == ResourceId();
      case ShaderStage::Task: return state->taskShader.resourceId == ResourceId();
      case ShaderStage::Mesh: return state->meshShader.resourceId == ResourceId();
      default: return false;
    }
  });
  AddBindlessUsage(eventId);
  return ret;
}

void MetalReplay::AppendExtendedDescriptorAccess(rdcarray<DescriptorAccess> &ret) const
{
  if(!m_MetalPipelineState) return;
  const auto &state = *m_MetalPipelineState;
  const ResourceId store = GetResID(m_pDriver);
  auto append = [&](ShaderStage stage, ResourceId shader, uint32_t slot,
                    DescriptorType fallback, uint32_t offset) {
    DescriptorAccess access;
    access.stage = stage; access.type = fallback; access.index = DescriptorAccess::NoShaderBinding;
    access.descriptorStore = store; access.byteOffset = offset; access.byteSize = 1;
    auto reflection = m_Shaders.find(shader); auto usage = m_ShaderBindingUsage.find(shader);
    if(reflection != m_Shaders.end() && usage != m_ShaderBindingUsage.end() && usage->second.available)
    {
      access.staticallyUnused = true;
      if(fallback == DescriptorType::Sampler)
      {
        for(size_t i = 0; i < reflection->second.samplers.size(); i++)
          if(reflection->second.samplers[i].fixedBindNumber == slot)
          { access.index = uint16_t(i); access.staticallyUnused = i >= usage->second.samplers.size() || !usage->second.samplers[i]; break; }
      }
      else
      {
        auto resources = [&](const rdcarray<ShaderResource> &bindings, const rdcarray<bool> &used) {
          for(size_t i = 0; i < bindings.size(); i++)
          {
            const auto &resource = bindings[i];
            const bool matches = fallback == DescriptorType::AccelerationStructure
                ? resource.descriptorType == DescriptorType::AccelerationStructure
                : fallback == DescriptorType::Image ? resource.isTexture
                : !resource.isTexture && resource.descriptorType != DescriptorType::AccelerationStructure;
            if(matches && resource.fixedBindNumber == slot)
            { access.index = uint16_t(i); access.type = resource.descriptorType; access.staticallyUnused = i >= used.size() || !used[i]; return true; }
          }
          return false;
        };
        if(fallback == DescriptorType::Buffer)
          for(size_t i = 0; i < reflection->second.constantBlocks.size(); i++)
            if(reflection->second.constantBlocks[i].fixedBindNumber == slot)
            { access.type = DescriptorType::ConstantBuffer; access.index = uint16_t(i); access.staticallyUnused = i >= usage->second.constantBlocks.size() || !usage->second.constantBlocks[i]; break; }
        if(access.index == DescriptorAccess::NoShaderBinding)
          if(!resources(reflection->second.readOnlyResources, usage->second.readOnlyResources))
            resources(reflection->second.readWriteResources, usage->second.readWriteResources);
      }
    }
    ret.push_back(access);
  };
  for(ShaderStage stage : {ShaderStage::Task, ShaderStage::Mesh})
  {
    const ResourceId shader = stage == ShaderStage::Task ? state.taskShader.resourceId : state.meshShader.resourceId;
    if(shader == ResourceId()) continue;
    const uint32_t base = 0x10000 | uint32_t(stage) << 12;
    const auto &buffers = stage == ShaderStage::Task ? state.taskBuffers : state.meshBuffers;
    const auto &textures = stage == ShaderStage::Task ? state.taskTextures : state.meshTextures;
    const auto &samplers = stage == ShaderStage::Task ? state.taskSamplers : state.meshSamplers;
    for(size_t i = 0; i < buffers.size(); i++)
      if(buffers[i].resourceId != ResourceId()) append(stage, shader, uint32_t(i), DescriptorType::Buffer, base + uint32_t(i));
    for(size_t i = 0; i < textures.size(); i++)
      if(textures[i] != ResourceId()) append(stage, shader, uint32_t(i), DescriptorType::Image, base + 0x100 + uint32_t(i));
    for(size_t i = 0; i < samplers.size(); i++)
      if(samplers[i] != ResourceId()) append(stage, shader, uint32_t(i), DescriptorType::Sampler, base + 0x300 + uint32_t(i));
  }
  for(ShaderStage stage : {ShaderStage::Vertex, ShaderStage::Fragment, ShaderStage::Compute})
  {
    const auto &structures = stage == ShaderStage::Compute ? state.computeAccelerationStructures :
                             stage == ShaderStage::Vertex ? state.vertexAccelerationStructures : state.fragmentAccelerationStructures;
    const ResourceId shader = stage == ShaderStage::Compute ? state.computeShader.resourceId :
                              stage == ShaderStage::Vertex ? state.vertexShader.resourceId : state.fragmentShader.resourceId;
    for(size_t i = 0; i < structures.size(); i++)
      if(structures[i] != ResourceId()) append(stage, shader, uint32_t(i), DescriptorType::AccelerationStructure,
                                              0x20000 | uint32_t(stage) << 8 | uint32_t(i));
  }

  // Direct UAVs and all compute bindings use the same stage/category contract as
  // D3D12/Vulkan. Do not label a writable texture as an unused SRV.
  for(ShaderStage stage : {ShaderStage::Vertex, ShaderStage::Fragment, ShaderStage::Compute})
  {
    const ResourceId shader = stage == ShaderStage::Compute ? state.computeShader.resourceId :
                             stage == ShaderStage::Vertex ? state.vertexShader.resourceId : state.fragmentShader.resourceId;
    auto reflection = m_Shaders.find(shader); auto usage = m_ShaderBindingUsage.find(shader);
    if(reflection == m_Shaders.end() || usage == m_ShaderBindingUsage.end() || !usage->second.available) continue;
    const auto &buffers = stage == ShaderStage::Compute ? state.computeBuffers :
                          stage == ShaderStage::Vertex ? state.vertexStorageBuffers : state.fragmentBuffers;
    const auto &textures = stage == ShaderStage::Compute ? state.computeTextures :
                           stage == ShaderStage::Vertex ? state.vertexTextures : state.fragmentTextures;
    auto resources = [&](const rdcarray<ShaderResource> &bindings, const rdcarray<bool> &used, bool write) {
      if(!write && stage != ShaderStage::Compute) return;
      for(size_t i = 0; i < bindings.size(); i++)
      {
        const auto &binding = bindings[i]; const uint32_t slot = binding.fixedBindNumber;
        if(binding.descriptorType == DescriptorType::AccelerationStructure) continue;
        if(binding.isTexture ? slot >= textures.size() || textures[slot] == ResourceId()
                             : slot >= buffers.size() || buffers[slot].resourceId == ResourceId()) continue;
        DescriptorAccess access; access.stage = stage; access.type = binding.descriptorType;
        access.index = uint16_t(i); access.staticallyUnused = i >= used.size() || !used[i];
        access.descriptorStore = store; access.byteSize = 1;
        access.byteOffset = slot + (stage == ShaderStage::Compute ?
            (binding.isTexture ? (write ? 0x400 : 0x300) : (write ? 0xC00 : 0xB00)) :
            stage == ShaderStage::Vertex ? (binding.isTexture ? 0xD00 : 0xF00) : (binding.isTexture ? 0 : 0x200));
        bool replaced = false;
        for(auto &old : ret)
          if(old.stage == stage && old.descriptorStore == store && old.byteOffset == access.byteOffset)
          { old = access; replaced = true; break; }
        if(!replaced) ret.push_back(access);
      }
    };
    resources(reflection->second.readOnlyResources, usage->second.readOnlyResources, false);
    resources(reflection->second.readWriteResources, usage->second.readWriteResources, true);
  }

}

void MetalReplay::AddBindlessUsage(uint32_t eventId)
{
  auto action = m_EventActionFlags.find(eventId);
  if(action == m_EventActionFlags.end()) return;
  for(const auto &binding : m_SelectedBindlessDescriptors)
  {
    const bool compute = binding.access.stage == ShaderStage::Compute;
    if(!(action->second & (compute ? ActionFlags::Dispatch
                                  : ActionFlags::Drawcall | ActionFlags::MeshDispatch))) continue;
    const bool write = IsReadWriteDescriptor(binding.access.type);
    AddUsage(binding.descriptor.resource,
             write ? RWResUsage(uint32_t(binding.access.stage)) : ResUsage(uint32_t(binding.access.stage)), eventId);
  }
}

void MetalReplay::ResolveSubmissionBindlessUsage(ResourceId commandBuffer)
{
  auto pending = m_SubmissionBindlessEvents.find(commandBuffer);
  if(pending == m_SubmissionBindlessEvents.end()) return;
  // Like baked command-list usage in D3D12/Vulkan, keep the action's bindings, but
  // inspect Shared uniform values only after its submission's CPU snapshot has
  // been restored. UE can fill upload memory after encoding all the dispatches.
  // Inspecting it in AddAction attributes an old descriptor index to a new action.
  MetalPipe::State *selected = m_MetalPipelineState;
  GraphicsInlineData inlineData;
  inlineData.swap(m_SelectedGraphicsInlineData);
  rdcarray<BindlessDescriptor> descriptors;
  descriptors.swap(m_SelectedBindlessDescriptors);
  const uint64_t inspectionOffset=m_UniformInspectionOffset;
  for(uint32_t eventId : pending->second)
  {
    m_MetalPipelineState = &m_EventPipelineStates[eventId];
    m_SelectedGraphicsInlineData = m_EventGraphicsInlineData[eventId];
    m_UniformInspectionOffset = m_Events[eventId].fileOffset;
    ResolveUniformBindlessAccess(true);
    AddBindlessUsage(eventId);
  }
  m_SelectedBindlessDescriptors.swap(descriptors);
  m_UniformInspectionOffset=inspectionOffset;
  m_SelectedGraphicsInlineData.swap(inlineData);
  m_MetalPipelineState = selected;
  m_SubmissionBindlessEvents.erase(pending);
}

void MetalReplay::AddEvent(uint32_t chunkIndex, uint64_t fileOffset)
{
  APIEvent event;
  event.eventId = m_NextEventID++;
  event.chunkIndex = chunkIndex;
  event.fileOffset = fileOffset;
  m_FrameChunkOffsets.insert(fileOffset);
  m_PendingEvents.push_back(event);
  m_Events.resize_for_index(event.eventId);
  m_Events[event.eventId] = event;
  m_EventPipelineStates[event.eventId] = m_CurrentPipelineState;
  m_EventGraphicsInlineData[event.eventId] = m_CurrentGraphicsInlineData;
  m_EventSamplerLOD[event.eventId] = m_CurrentSamplerLOD;
}

void MetalReplay::AddAction(const ActionDescription &in)
{
  ActionDescription action = in;
  rdcarray<uint32_t> &debugGroupPath = m_DebugGroupPaths[m_ActiveEncoderContext];
  // Metal debug groups are encoder scoped. An incomplete group cannot contain the next
  // encoder's pass, and a pass end must sit outside the group for navigation.
  if(action.flags & ActionFlags::EndPass)
    debugGroupPath.clear();
  action.eventId = m_PendingEvents.empty() ? m_NextEventID++ : m_PendingEvents.back().eventId;
  action.actionId = m_NextActionID;
  if(!(action.flags & (ActionFlags::PassBoundary | ActionFlags::PushMarker |
                      ActionFlags::PopMarker | ActionFlags::SetMarker)))
    m_NextActionID++;
  action.events.swap(m_PendingEvents);
  m_LastActionEventID = action.eventId;
  m_ActionEncoderContexts[action.eventId] = m_ActiveEncoderContext;
  m_EventActionFlags[action.eventId] = action.flags;
  m_EventPipelineStates[action.eventId] = m_CurrentPipelineState;
  m_EventGraphicsInlineData[action.eventId] = m_CurrentGraphicsInlineData;
  m_EventSamplerLOD[action.eventId] = m_CurrentSamplerLOD;
  // Vulkan's GetPassEvents returns earlier draws and boundaries in the actual
  // render pass, excluding the selected action. Metal encoders from different
  // command buffers can be interleaved in the capture, so previousAction cannot
  // determine ownership. Parallel children belong to their parent's single pass.
  ResourceId renderPass;
  WrappedMTLRenderCommandEncoder *render = m_pDriver->GetReplayRenderCommandEncoder();
  if(render)
    renderPass = render->GetParallelParent() ? GetResID(render->GetParallelParent()) : GetResID(render);
  else if(m_pDriver->GetReplayParallelRenderCommandEncoder())
    renderPass = GetResID(m_pDriver->GetReplayParallelRenderCommandEncoder());
  if(renderPass != ResourceId())
  {
    m_EventRenderPass[action.eventId] = renderPass;
    if((action.flags & (ActionFlags::Drawcall | ActionFlags::MeshDispatch | ActionFlags::PassBoundary)) ||
       (m_CurrentPipelineState.tileDispatch && (action.flags & ActionFlags::Dispatch)))
      m_RenderPassEvents[renderPass].push_back(action.eventId);
  }
  if(m_CurrentPipelineState.tileDispatch && (action.flags & ActionFlags::Dispatch))
  {
    SetActionOutputs(action);
    for(const auto &target : m_CurrentPipelineState.colorTargets)
      AddUsage(target.resource, ResourceUsage::ColorTarget);
  }
  AppendAction(action);

  if(action.flags & (ActionFlags::Dispatch | ActionFlags::Drawcall | ActionFlags::MeshDispatch))
  {
    // Direct bindings are known while encoding. Shared bindless indices become
    // authoritative at submission, after the captured CPU updates are restored.
    MetalPipe::State *selected = m_MetalPipelineState;
    auto inlineData = m_SelectedGraphicsInlineData;
    m_MetalPipelineState = &m_CurrentPipelineState;
    m_SelectedGraphicsInlineData = m_CurrentGraphicsInlineData;
    rdcarray<DescriptorAccess> extended;
    AppendExtendedDescriptorAccess(extended);
    for(const auto &access : extended)
    {
      if(access.staticallyUnused || access.type == DescriptorType::Sampler) continue;
      if(access.stage == ShaderStage::Compute ? !(action.flags & ActionFlags::Dispatch) :
         !(action.flags & (ActionFlags::Drawcall | ActionFlags::MeshDispatch))) continue;
      DescriptorRange range; range.type = access.type; range.offset = access.byteOffset;
      range.count = range.descriptorSize = 1;
      auto descriptors = GetDescriptors(access.descriptorStore, {range});
      if(!descriptors.empty()) AddUsage(descriptors[0].resource,
          access.type == DescriptorType::ConstantBuffer ? CBUsage(uint32_t(access.stage)) :
          IsReadWriteDescriptor(access.type) ? RWResUsage(uint32_t(access.stage)) : ResUsage(uint32_t(access.stage)), action.eventId);
    }
    m_SubmissionBindlessEvents[m_ActiveEncoderContext].push_back(action.eventId);
    m_SelectedGraphicsInlineData.swap(inlineData);
    m_MetalPipelineState = selected;
  }

  if(action.flags & ActionFlags::Dispatch)
    for(const auto &binding : m_CurrentPipelineState.computeBuffers)
    {
      auto found = m_ArgumentBuffers.find({binding.resourceId, binding.byteOffset});
      if(found != m_ArgumentBuffers.end())
        for(const auto &field : found->second.rayResources)
          AddUsage(field.second, ResourceUsage::CS_Resource);
    }

  if(action.flags & (ActionFlags::Drawcall | ActionFlags::MeshDispatch))
  {
    // Match D3D12CommandData::AddUsage and Vulkan's attachment usage on draws. Use the
    // actual attachment rather than action.outputs, which may refer to the resolve destination.
    for(const Descriptor &target : m_CurrentPipelineState.colorTargets)
      AddUsage(target.resource, ResourceUsage::ColorTarget);
    AddUsage(m_CurrentPipelineState.depthTarget.resource, ResourceUsage::DepthStencilTarget);
    AddUsage(m_CurrentPipelineState.rasterizer.rasterizationRateMap, ResourceUsage::RasterizationRateMap);
    for(uint32_t slot : m_CurrentPipelineState.fragmentShader.framebufferFetch)
      if(slot < m_CurrentPipelineState.colorTargets.size())
        AddUsage(m_CurrentPipelineState.colorTargets[slot].resource, ResourceUsage::InputTarget);
    const ShaderReflection *vertex = m_CurrentPipelineState.vertexShader.reflection;
    for(size_t slot = 0; slot < m_CurrentPipelineState.vertexStorageBuffers.size(); slot++)
    {
      bool write = false;
      if(vertex)
        for(const ShaderResource &resource : vertex->readWriteResources)
          write |= !resource.isTexture && resource.fixedBindNumber == slot;
      AddUsage(m_CurrentPipelineState.vertexStorageBuffers[slot].resourceId,
               write ? ResourceUsage::VS_RWResource : ResourceUsage::VS_Resource);
    }
    for(ShaderStage stage : {ShaderStage::Vertex, ShaderStage::Fragment})
    {
      const auto &textures = stage == ShaderStage::Vertex ? m_CurrentPipelineState.vertexTextures : m_CurrentPipelineState.fragmentTextures;
      const auto *reflection = stage == ShaderStage::Vertex ? m_CurrentPipelineState.vertexShader.reflection : m_CurrentPipelineState.fragmentShader.reflection;
      for(size_t slot = 0; slot < textures.size(); slot++)
      {
        bool write = false;
        if(reflection) for(const auto &resource : reflection->readWriteResources)
          write |= resource.isTexture && resource.fixedBindNumber == slot;
        AddUsage(textures[slot], write ? RWResUsage(uint32_t(stage)) : ResUsage(uint32_t(stage)));
      }
    }
    const ResourceId fragmentShader = m_CurrentPipelineState.fragmentShader.resourceId;
    auto shader = m_Shaders.find(fragmentShader);
    for(size_t slot = 0; slot < m_CurrentPipelineState.fragmentBuffers.size(); slot++)
    {
      const ResourceId buffer = m_CurrentPipelineState.fragmentBuffers[slot].resourceId;
      if(buffer == ResourceId())
        continue;
      bool storage = false;
      if(shader != m_Shaders.end())
        for(const ShaderResource &resource : shader->second.readOnlyResources)
          if(!resource.isTexture && resource.fixedBindNumber == slot &&
             !(slot < m_CurrentPipelineState.fragmentArgumentBuffers.size() &&
               m_CurrentPipelineState.fragmentArgumentBuffers[slot].buffer.resourceId == buffer) &&
             resource.descriptorType == DescriptorType::Buffer)
            storage = true;
      bool write = false;
      if(shader != m_Shaders.end()) for(const auto &resource : shader->second.readWriteResources)
        write |= !resource.isTexture && resource.fixedBindNumber == slot;
      AddUsage(buffer, write ? ResourceUsage::PS_RWResource : storage ? ResourceUsage::PS_Resource : ResourceUsage::PS_Constants);
    }
    for(const MetalPipe::VertexBuffer &buffer : m_CurrentPipelineState.vertexBuffers)
      AddUsage(buffer.resourceId, ResourceUsage::VertexBuffer);
    if(action.flags & ActionFlags::Indexed)
      AddUsage(m_CurrentPipelineState.indexBuffer.resourceId, ResourceUsage::IndexBuffer);
    for(const MetalPipe::ArgumentBuffer &argumentBuffer :
        m_CurrentPipelineState.fragmentArgumentBuffers)
    {
      AddUsage(argumentBuffer.buffer.resourceId, ResourceUsage::PS_Constants);
      for(ResourceId texture : argumentBuffer.textures)
        AddUsage(texture, ResourceUsage::PS_Resource);
      auto packet = m_ArgumentBuffers.find({argumentBuffer.buffer.resourceId, argumentBuffer.buffer.byteOffset});
      if(packet != m_ArgumentBuffers.end())
        for(const MetalPipe::BufferBinding &binding : packet->second.buffers)
        {
          AddUsage(binding.resourceId, ResourceUsage::PS_Resource);
          // A read-only argument-buffer pointer can itself reference a packet of textures.
          // Keep the GPU resource usage reachable even though the public Pipeline State model
          // currently exposes only direct members of the outer argument buffer.
          auto nested = m_ArgumentBuffers.find({binding.resourceId, binding.byteOffset});
          if(nested != m_ArgumentBuffers.end())
            for(ResourceId texture : nested->second.binding.textures)
              AddUsage(texture, ResourceUsage::PS_Resource);
        }
    }
  }
}

void MetalReplay::AppendAction(ActionDescription &action)
{
  rdcarray<ActionDescription> &root = m_FrameRecord.actionList;
  rdcarray<uint32_t> &debugGroupPath = m_DebugGroupPaths[m_ActiveEncoderContext];
  if(action.flags & ActionFlags::EndPass)
    debugGroupPath.clear();
  // Command buffers can be encoded concurrently. Appending into an earlier
  // command's still-open marker would move this action before intervening EIDs
  // in a depth-first traversal, breaking the shared action table and navigation.
  // Keep captured EIDs/stream order and reopen only the interrupted marker path
  // using the common fake-marker representation, rather than moving GPU work.
  rdcarray<ActionDescription> scopes;
  const rdcarray<ActionDescription> *previousActions = &root;
  bool interrupted = false;
  for(uint32_t index : debugGroupPath)
  {
    if(index >= previousActions->size())
      break;
    interrupted |= index + 1 != previousActions->size();
    const ActionDescription &scope = (*previousActions)[index];
    ActionDescription continuation;
    continuation.customName = scope.customName;
    continuation.flags = ActionFlags::PushMarker;
    continuation.eventId = action.eventId;
    continuation.actionId = action.actionId;
    APIEvent event;
    event.eventId = action.eventId;
    event.chunkIndex = APIEvent::NoChunk;
    continuation.events.push_back(event);
    scopes.push_back(continuation);
    previousActions = &scope.children;
  }
  if(interrupted)
  {
    debugGroupPath.clear();
    rdcarray<ActionDescription> *continued = &root;
    for(const ActionDescription &scope : scopes)
    {
      debugGroupPath.push_back((uint32_t)continued->size());
      continued->push_back(scope);
      continued = &continued->back().children;
    }
  }
  rdcarray<ActionDescription> *actions = &root;
  for(uint32_t index : debugGroupPath)
  {
    if(index >= actions->size())
    {
      RDCERR("Invalid Metal debug group action path");
      debugGroupPath.clear();
      actions = &root;
      break;
    }
    actions = &(*actions)[index].children;
  }
  if(m_MultiActionChildrenRemaining)
  {
    ActionDescription &parent = actions->back();
    parent.children.push_back(action);
    if(--m_MultiActionChildrenRemaining == 0)
    {
      m_MultiActionEndEvents[parent.eventId] = action.eventId;
      parent.outputs = action.outputs;
      parent.depthOut = action.depthOut;
      m_EventPipelineStates[parent.eventId] = m_CurrentPipelineState;
      m_EventSamplerLOD[parent.eventId] = m_CurrentSamplerLOD;
    }
  }
  else
  {
    actions->push_back(action);
    // MultiAction owns its fixed number of children through BeginMultiAction. Entering it
    // as a debug group too would make the first child read back() from an empty child list.
    if((action.flags & ActionFlags::PushMarker) && !(action.flags & ActionFlags::MultiAction))
      debugGroupPath.push_back((uint32_t)actions->size() - 1);
    else if(action.flags & ActionFlags::PopMarker)
    {
      if(debugGroupPath.empty())
        RDCWARN("Metal debug group pop without a matching push");
      else
        debugGroupPath.pop_back();
    }
  }

}

void MetalReplay::AddDebugGroup(const NS::String *label, ActionFlags flag)
{
  // Match D3D12/Vulkan's marker actions. The name comes from the captured Metal label,
  // and the existing API event remains the action's final event for seek and inspection.
  ActionDescription action;
  action.flags = flag;
  if(label && label->utf8String())
    action.customName = label->utf8String();
  AddAction(action);
}

void MetalReplay::ClearPendingComputeIndirectActions()
{
  for(const auto &pending : m_PendingComputeIndirectActions)
    if(pending.snapshot) pending.snapshot->release();
  m_PendingComputeIndirectActions.clear();
}

void MetalReplay::ResetComputeIndirectTracking()
{
  // Called only after earlier replay commands have completed. Pending snapshots
  // own their execution arguments, including unretained command buffers.
  m_LoadComputeIndirectOrdinals.clear();
}

bool MetalReplay::RegisterComputeIndirectAction(uint32_t eventId, ResourceId buffer,
                                                uint64_t offset, MTL::ComputeCommandEncoder *encoder,
                                                ResourceId encoderID,
                                                MTL::Buffer **executionArguments,
                                                uint32_t threadsPerGroup)
{
  // Vulkan FetchIndirectData/D3D12 ExecuteIndirect patching save each use's arguments,
  // rather than reading the source at frame end. Metal cannot blit within a compute
  // encoder, so copy three words with a native debug kernel in the same GPU stream.
  auto source = m_pDriver->GetResourceManager()->GetResource(buffer, true);
  auto pipeline = m_pDriver->GetResourceManager()->GetResource(m_CurrentPipelineState.computePipelineResourceId, true);
  if(!encoder || !source || source->m_Type != eResBuffer || !source->m_Real ||
     !pipeline || pipeline->m_Type != eResComputePipelineState || !pipeline->m_Real)
    return false;
  rdcarray<uint32_t> expected;
  if(encoderID != ResourceId() && m_pDriver->m_HasCapturedComputeIndirectArguments)
  {
    const uint32_t ordinal = m_LoadComputeIndirectOrdinals[encoderID]++;
    const auto proof = m_pDriver->m_CapturedComputeIndirectArguments.find(make_rdcpair(encoderID, ordinal));
    if(proof == m_pDriver->m_CapturedComputeIndirectArguments.end() ||
       proof->second.buffer != buffer || proof->second.offset != offset ||
       proof->second.groups.size() != 3) return false;
    expected = proof->second.groups;
  }
  MTL::Device *device = Unwrap(m_pDriver);
  const bool guarded=executionArguments!=NULL;
  const bool frozen=m_pDriver->HasRayQueryHeapPipeline(m_CurrentPipelineState.computePipelineResourceId);
  if(guarded && frozen && expected.size()!=3) return false;
  MTL::ComputePipelineState *copy=NULL;
  if(guarded)
  {
    if(!threadsPerGroup || threadsPerGroup>1024) return false;
    if(!m_IndirectReplayPipeline)
      m_IndirectReplayPipeline=CreateMetalIndirectReplayPipeline(device);
    if(!m_IndirectReplayPipeline) return false;
    copy=m_IndirectReplayPipeline;
  }
  else
  {
    if(!m_IndirectReadbackPipeline)
      m_IndirectReadbackPipeline=CreateMetalIndirectReadbackPipeline(device);
    if(!m_IndirectReadbackPipeline) return false;
    copy=m_IndirectReadbackPipeline;
  }
  MTL::Buffer *snapshot = device->newBuffer(guarded?32:16, MTL::ResourceStorageModeShared);
  if(!snapshot || !snapshot->contents()) { if(snapshot)snapshot->release();return false; }
  memset(snapshot->contents(),0,guarded?32:16);
  encoder->memoryBarrier(MTL::BarrierScope(MTL::BarrierScopeBuffers | MTL::BarrierScopeTextures));
  encoder->setComputePipelineState(copy);
  encoder->setBuffer(Unwrap((WrappedMTLBuffer *)source), offset, 0);
  encoder->setBuffer(snapshot, 0, 1);
  if(guarded)
  {
    const uint32_t limits[6]={threadsPerGroup,0,uint32_t(frozen),
        expected.empty()?0U:expected[0],expected.empty()?0U:expected[1],expected.empty()?0U:expected[2]};
    encoder->setBytes(limits,sizeof(limits),3);
  }
  encoder->dispatchThreadgroups(MTL::Size(1,1,1), MTL::Size(1,1,1));
  encoder->memoryBarrier(MTL::BarrierScope(MTL::BarrierScopeBuffers | MTL::BarrierScopeTextures));
  encoder->setComputePipelineState(Unwrap((WrappedMTLComputePipelineState *)pipeline));
  for(uint32_t slot = 0; slot < (guarded?4U:2U); slot++)
  {
    // BindComputeBytes stores the already relocated Native payload. Restoring
    // the capture-process bytes here would undo actual root pointer recovery.
    auto bytes = m_CurrentComputeInlineData.find(slot);
    if(bytes != m_CurrentComputeInlineData.end())
      encoder->setBytes(bytes->second.data(), bytes->second.size(), slot);
    else
    {
      const auto binding = GetComputeBuffer(slot);
      auto resource = m_pDriver->GetResourceManager()->GetResource(binding.resourceId, true);
      encoder->setBuffer(resource ? Unwrap((WrappedMTLBuffer *)resource) : NULL, binding.byteOffset, slot);
    }
  }
  m_PendingComputeIndirectActions.push_back({eventId, buffer, offset, snapshot});
  auto &pending=m_PendingComputeIndirectActions.back();
  pending.expected=expected; pending.guarded=guarded;
  // This legacy protocol froze its exact invocation footprint as part of the
  // AS recipe. Native sourced dispatches instead restore a conservative closure.
  pending.frozenArguments=m_pDriver->HasRayQueryHeapPipeline(m_CurrentPipelineState.computePipelineResourceId);
  if(guarded) *executionArguments=snapshot;
  return true;
}

bool MetalReplay::HasPendingComputeIndirectActionFor(ResourceId id) const
{
  // Pending actions own independent readback buffers, so source retirement/purge
  // after the dispatch cannot invalidate the saved per-use arguments.
  return false;
}

bool MetalReplay::TraceComputeArgumentProducers(uint32_t eventId, ResourceId command,
                                               MTL::ComputeCommandEncoder *encoder)
{
  if(!getenv("RENDERDOC_METAL_TRACE_COMPUTE_ARGUMENT_PRODUCERS")) return true;
  std::set<ResourceId> traced;
  for(const auto &proof : m_pDriver->m_CapturedComputeIndirectArguments)
  {
    if(proof.second.command != command || !traced.insert(proof.second.buffer).second) continue;
    for(uint64_t offset : {0ULL, 16ULL})
    {
      if(GetBuffer(proof.second.buffer).length < offset + 12) continue;
      if(!RegisterComputeIndirectAction(eventId, proof.second.buffer, offset, encoder)) return false;
      auto &pending = m_PendingComputeIndirectActions.back();
      pending.diagnostic = true;
      pending.pipeline = m_CurrentPipelineState.computePipelineResourceId;
    }
  }
  return true;
}

bool MetalReplay::ResolvePendingComputeIndirectActions()
{
  bool success = true;
  for(const PendingComputeIndirectAction &pending : m_PendingComputeIndirectActions)
  {
    bytebuf bytes;
    if(pending.snapshot && pending.snapshot->contents())
    {
      bytes.resize(sizeof(MTL::DispatchThreadgroupsIndirectArguments));
      memcpy(bytes.data(), pending.snapshot->contents(), bytes.size());
    }
    if(bytes.size() != sizeof(MTL::DispatchThreadgroupsIndirectArguments))
    {
      RDCERR("Couldn't read Metal compute indirect arguments at EID %u", pending.eventId);
      success = false;
      continue;
    }

    uint32_t groups[3] = {};
    memcpy(groups, bytes.data(), sizeof(groups));
    if(pending.diagnostic)
    {
      fprintf(stderr, "Metal compute argument producer: EID=%u pipeline=%s buffer=%s offset=%llu actual=%u,%u,%u\n",
              pending.eventId, ToStr(pending.pipeline).c_str(), ToStr(pending.buffer).c_str(),
              (unsigned long long)pending.offset, groups[0], groups[1], groups[2]);
      continue;
    }
    if(getenv("RENDERDOC_METAL_TRACE_INDIRECT_REPLAY"))
    {
      fprintf(stderr,"Metal compute indirect readback: EID=%u buffer=%s offset=%llu actual=%u,%u,%u\n",
          pending.eventId,ToStr(pending.buffer).c_str(),(unsigned long long)pending.offset,groups[0],groups[1],groups[2]);
      if(!pending.expected.empty())
        fprintf(stderr,"Metal compute indirect expected: EID=%u groups=%u,%u,%u\n",
                pending.eventId,pending.expected[0],pending.expected[1],pending.expected[2]);
    }
    if(pending.frozenArguments && !pending.expected.empty() &&
       memcmp(pending.expected.data(),groups,sizeof(groups)))
    {
      RDCERR("Metal frozen indirect invocation contract differs at EID %u",pending.eventId);
      success=false;
      continue;
    }
    uint32_t validation=0;
    memcpy(&validation,(const byte *)pending.snapshot->contents()+12,4);
    if(validation!=0x52444349U)
    {
      RDCERR("Metal compute indirect extent overflows, invocation differs or snapshot failed at EID %u",pending.eventId);
      success=false;
      continue;
    }
    if(!pending.expected.empty() && memcmp(pending.expected.data(), groups, sizeof(groups)))
    {
      // Numerical evidence is useful for output diagnosis, not a substitute for
      // restored state. VK FetchIndirectData updates actions from replay GPU data;
      // DX12 patches address arguments, leaving ordinary dispatch counts Native.
      RDCWARN("Metal compute indirect replay counts differ from capture at EID %u: <%u,%u,%u> vs <%u,%u,%u>",
              pending.eventId,groups[0],groups[1],groups[2],
              pending.expected[0],pending.expected[1],pending.expected[2]);
    }
    if(!pending.eventId) continue; // Active replay still checks the actual GPU arguments.
    // Debug groups can now contain indirect actions; traverse their children too.
    rdcarray<rdcarray<ActionDescription> *> lists = {&m_FrameRecord.actionList};
    while(!lists.empty())
    {
      rdcarray<ActionDescription> *list = lists.back();
      lists.pop_back();
      for(ActionDescription &action : *list)
      {
        if(!action.IsFakeMarker() && action.eventId == pending.eventId)
        {
          action.dispatchDimension[0] = groups[0];
          action.dispatchDimension[1] = groups[1];
          action.dispatchDimension[2] = groups[2];
          action.customName = StringFormat::Fmt("dispatchThreadgroups(indirect, <%u, %u, %u>)",
                                                 groups[0], groups[1], groups[2]);
        }
        if(!action.children.empty()) lists.push_back(&action.children);
      }
    }
  }
  ClearPendingComputeIndirectActions();
  return success;
}

bool MetalReplay::RegisterRenderIndirectAction(uint32_t eventId,
    WrappedMTLRenderCommandEncoder *encoder, ResourceId buffer, uint64_t offset, uint32_t wordCount,
    uint32_t *arguments)
{
  if(!m_pDriver->m_HasCapturedRenderIndirectArguments)return true;
  const ResourceId id=GetResID(encoder);
  const uint32_t ordinal=m_LoadRenderIndirectOrdinals[id]++;
  const auto proof=m_pDriver->m_CapturedRenderIndirectArguments.find(make_rdcpair(id,ordinal));
  const ResourceId pass=encoder->GetParallelParent()?GetResID(encoder->GetParallelParent()):id;
  auto source=m_pDriver->GetResourceManager()->GetResource(buffer,true);
  if(proof==m_pDriver->m_CapturedRenderIndirectArguments.end() || !source ||
     source->m_Type!=eResBuffer || !source->m_Real || proof->second.pass!=pass ||
     proof->second.buffer!=buffer || proof->second.offset!=offset ||
     proof->second.wordCount!=wordCount || proof->second.arguments.size()!=wordCount)
    return false;
  PendingRenderIndirectAction pending;
  pending.eventId=eventId;pending.pass=pass;pending.offset=offset;pending.wordCount=wordCount;
  pending.expected=proof->second.arguments;
  if(arguments)memcpy(arguments,pending.expected.data(),wordCount*4);
  pending.readback.source=NS::RetainPtr(Unwrap((WrappedMTLBuffer *)source));
  m_PendingRenderIndirectActions.push_back(pending);
  return true;
}

void MetalReplay::NoteRenderIndirectWrite(ResourceId pass, WrappedMTLResource *resource)
{
  if(!m_pDriver->m_HasCapturedRenderIndirectArguments)return;
  auto wrapped=(WrappedMTLObject *)resource;
  m_RenderIndirectWrites[pass].push_back(wrapped && wrapped->m_Real &&
      (wrapped->m_Type==eResBuffer || wrapped->m_Type==eResTexture)?
      GetMetalIndirectWriteFootprint(Unwrap(resource),wrapped->m_Type==eResTexture):
      MetalIndirectWriteFootprint());
}

bool MetalReplay::FlushRenderIndirectActions(ResourceId pass, MTL::CommandBuffer *command)
{
  if(!m_pDriver->m_HasCapturedRenderIndirectArguments)return true;
  auto write=[&](const RDMTL::RenderPassAttachmentDescriptor &attachment) {
    if(attachment.texture)NoteRenderIndirectWrite(pass,(WrappedMTLResource *)attachment.texture);
    if(attachment.resolveTexture)NoteRenderIndirectWrite(pass,(WrappedMTLResource *)attachment.resolveTexture);
  };
  for(const auto &attachment:m_CurrentRenderPassDescriptor.colorAttachments)write(attachment);
  write(m_CurrentRenderPassDescriptor.depthAttachment);write(m_CurrentRenderPassDescriptor.stencilAttachment);
  if(m_CurrentRenderPassDescriptor.visibilityResultBuffer)
    NoteRenderIndirectWrite(pass,(WrappedMTLResource *)m_CurrentRenderPassDescriptor.visibilityResultBuffer);
  const auto &writes=m_RenderIndirectWrites[pass];
  bool success=true;
  for(auto &pending:m_PendingRenderIndirectActions)
    if(pending.pass==pass && !pending.readback.snapshot &&
       !CopyMetalRenderIndirectArguments(Unwrap(m_pDriver),command,pending.readback.source.get(),
           pending.offset,pending.wordCount,writes,pending.readback))success=false;
  m_RenderIndirectWrites.erase(pass);
  return success;
}

bool MetalReplay::ResolvePendingRenderIndirectActions()
{
  bool success=true;
  for(const auto &pending:m_PendingRenderIndirectActions)
  {
    if(getenv("RENDERDOC_METAL_TRACE_INDIRECT_REPLAY"))
    {
      uint32_t actual[5]={};
      if(pending.readback.snapshot && pending.readback.snapshot->contents())
        memcpy(actual,pending.readback.snapshot->contents(),pending.wordCount*4);
      fprintf(stderr,"Metal render indirect readback: EID=%u pass=%s offset=%llu words=%u expected=",
          pending.eventId,ToStr(pending.pass).c_str(),(unsigned long long)pending.offset,pending.wordCount);
      for(uint32_t word:pending.expected)fprintf(stderr,"%u,",word);
      fprintf(stderr," actual=");for(uint32_t i=0;i<pending.wordCount;i++)fprintf(stderr,"%u,",actual[i]);
      fprintf(stderr," snapshot=%d\n",bool(pending.readback.snapshot));
    }
    if(!pending.readback.snapshot || !pending.readback.snapshot->contents() ||
       memcmp(pending.expected.data(),pending.readback.snapshot->contents(),pending.wordCount*4))
    {
      RDCERR("Metal render indirect per-use replay arguments differ at EID %u",pending.eventId);
      success=false;continue;
    }
    const uint32_t *args=(const uint32_t *)pending.readback.snapshot->contents();
    rdcarray<rdcarray<ActionDescription> *> lists={&m_FrameRecord.actionList};
    while(!lists.empty())
    {
      auto list=lists.back();lists.pop_back();
      for(auto &action:*list)
      {
        if(!action.IsFakeMarker() && action.eventId==pending.eventId)
        {
          action.numIndices=args[0];action.numInstances=args[1];
          if(pending.wordCount==5){action.baseVertex=(int32_t)args[3];action.instanceOffset=args[4];}
          else{action.vertexOffset=args[2];action.instanceOffset=args[3];}
          if(args[1]>1 || action.instanceOffset)action.flags|=ActionFlags::Instanced;
          action.customName=StringFormat::Fmt("%s(indirect, %u %s, %u instances)",
              pending.wordCount==5?"drawIndexedPrimitives":"drawPrimitives",args[0],
              pending.wordCount==5?"indices":"vertices",args[1]);
        }
        if(!action.children.empty())lists.push_back(&action.children);
      }
    }
  }
  m_PendingRenderIndirectActions.clear();
  return success;
}

void MetalReplay::BeginMultiAction(uint32_t childCount)
{
  rdcarray<ActionDescription> *actions = &m_FrameRecord.actionList;
  for(uint32_t index : m_DebugGroupPaths[m_ActiveEncoderContext])
  {
    RDCASSERT(index < actions->size());
    if(index >= actions->size()) return;
    actions = &(*actions)[index].children;
  }
  RDCASSERT(m_MultiActionChildrenRemaining == 0 && childCount > 0 && !actions->empty());
  m_MultiActionChildrenRemaining = childCount;
}

uint32_t MetalReplay::GetMultiActionEndEvent(uint32_t eventId) const
{
  auto it = m_MultiActionEndEvents.find(eventId);
  return it == m_MultiActionEndEvents.end() ? eventId : it->second;
}

void MetalReplay::AddUsage(ResourceId id, ResourceUsage usage)
{
  AddUsage(id, usage, m_LastActionEventID);
}

void MetalReplay::AddUsage(ResourceId id, ResourceUsage usage, uint32_t eventId)
{
  if(id == ResourceId() || eventId == 0 || m_FrameRecord.actionList.empty())
    return;

  // Vulkan records an image-view use on its image; D3D12 records an SRV/RTV use on the
  // underlying resource. Metal views have their own ResourceIds, so use their explicit
  // replay parent relationship for the same whole-resource usage semantics. Heap aliases
  // and unrelated allocations are never merged.
  ResourceId parent = m_pDriver->GetReplayTextureViewParent(id);
  if(parent != ResourceId()) id = parent;

  rdcarray<EventUsage> &uses = m_ResourceUses[id];
  const EventUsage entry(eventId, usage);
  if(uses.empty() || uses.back() < entry)
  {
    uses.push_back(entry);
    return;
  }
  // Load adds chronological entries, but inspecting an earlier bindless draw adds entries later.
  // Keep the common GetUsage contract sorted and unique across repeated event selection.
  auto pos = std::lower_bound(uses.begin(), uses.end(), entry);
  if(pos == uses.end() || !(*pos == entry))
    uses.insert(size_t(pos - uses.begin()), entry);
}

void MetalReplay::AddRenderPassLoadUsage(const RDMTL::RenderPassDescriptor &descriptor)
{
  const auto add = [this](const RDMTL::RenderPassAttachmentDescriptor &attachment) {
    if(attachment.texture && attachment.loadAction == MTL::LoadActionClear)
      AddUsage(GetResID(attachment.texture), ResourceUsage::Clear);
    else if(attachment.texture && attachment.loadAction == MTL::LoadActionDontCare)
      AddUsage(GetResID(attachment.texture), ResourceUsage::Discard);
  };
  for(const RDMTL::RenderPassColorAttachmentDescriptor &attachment : descriptor.colorAttachments)
    add(attachment);
  add(descriptor.depthAttachment);
  add(descriptor.stencilAttachment);
}

void MetalReplay::AddRenderPassStoreUsage(const RDMTL::RenderPassDescriptor &descriptor)
{
  const auto add = [this](const RDMTL::RenderPassAttachmentDescriptor &attachment) {
    if(attachment.texture && (attachment.storeAction == MTL::StoreActionDontCare ||
                              attachment.storeAction == MTL::StoreActionMultisampleResolve))
      AddUsage(GetResID(attachment.texture), ResourceUsage::Discard);
    if(attachment.resolveTexture &&
       (attachment.storeAction == MTL::StoreActionMultisampleResolve ||
        attachment.storeAction == MTL::StoreActionStoreAndMultisampleResolve))
    {
      AddUsage(GetResID(attachment.texture), ResourceUsage::ResolveSrc);
      AddUsage(GetResID(attachment.resolveTexture), ResourceUsage::ResolveDst);
    }
  };
  for(const RDMTL::RenderPassColorAttachmentDescriptor &attachment : descriptor.colorAttachments)
    add(attachment);
  add(descriptor.depthAttachment);
  add(descriptor.stencilAttachment);
}

rdcarray<EventUsage> MetalReplay::GetUsage(ResourceId id)
{
  ResourceId parent = m_pDriver->GetReplayTextureViewParent(id);
  if(parent != ResourceId()) id = parent;
  auto it = m_ResourceUses.find(id);
  return it == m_ResourceUses.end() ? rdcarray<EventUsage>() : it->second;
}

rdcarray<uint32_t> MetalReplay::GetPassEvents(uint32_t eventId)
{
  rdcarray<uint32_t> events;
  auto finish = [&]() {
    if(!Process::GetEnvVariable("RENDERDOC_METAL_TRACE_PASS_EVENTS").empty())
    {
      fprintf(stderr, "Metal pass events: EID=%u events=", eventId);
      for(uint32_t earlier : events) fprintf(stderr, "%u,", earlier);
      fprintf(stderr, "\n");
    }
    return events;
  };
  const auto flags = m_EventActionFlags.find(eventId);
  // Like Vulkan, an end boundary is outside the pass. Unknown/API-only events
  // and compute/blit encoders have no render-pass draws either.
  if(flags == m_EventActionFlags.end() || (flags->second & ActionFlags::EndPass))
    return finish();
  const auto owner = m_EventRenderPass.find(eventId);
  if(owner == m_EventRenderPass.end())
    return finish();
  const auto pass = m_RenderPassEvents.find(owner->second);
  if(pass == m_RenderPassEvents.end())
    return finish();
  for(uint32_t earlier : pass->second)
  {
    if(earlier >= eventId)
      break;
    events.push_back(earlier);
  }
  return finish();
}

bool MetalReplay::BakeSubmissionEvents()
{
  // D3D12 ExecuteCommandLists / Vulkan QueueSubmit insert each baked command
  // stream into the public event space at submission. Keep the original chunks
  // and their physical offsets for replay, but expose one coherent event space
  // to the action tree, pipeline state, pass queries and resource usage.
  const auto &owners = m_pDriver->GetReplayChunkOwners();
  const SDFile *file = m_pDriver->GetStructuredFile();
  std::map<ResourceId, uint32_t> commits;
  for(const APIEvent &event : m_Events)
  {
    if(!event.eventId || event.chunkIndex >= file->chunks.size()) continue;
    const SDChunk *chunk = file->chunks[event.chunkIndex];
    if(chunk->metadata.chunkID == (uint32_t)MetalChunk::MTLCommandBuffer_commit)
    {
      auto owner = owners.find(event.fileOffset);
      if(owner != owners.end()) commits[owner->second] = event.eventId;
    }
  }
  if(commits.empty()) return true;
  std::map<uint32_t, ResourceId> eventOwners;
  std::map<ResourceId, rdcarray<uint32_t>> bodies;
  for(const APIEvent &event : m_Events)
  {
    if(!event.eventId) continue;
    auto owner = owners.find(event.fileOffset);
    if(owner != owners.end() && commits.count(owner->second) &&
       event.fileOffset <= m_Events[commits[owner->second]].fileOffset)
    {
      eventOwners[event.eventId] = owner->second;
      if(event.eventId != commits[owner->second]) bodies[owner->second].push_back(event.eventId);
    }
  }
  // Parallel render encoders execute their children in creation order, even
  // when CPU workers encode the child commands in a different order. Bake
  // each child's calls together, just as Vulkan executes a secondary stream.
  std::map<ResourceId, rdcarray<uint32_t>> children;
  std::map<uint32_t, ResourceId> childEvents, childCreates;
  for(const APIEvent &event : m_Events)
  {
    if(!event.eventId || event.chunkIndex >= file->chunks.size()) continue;
    const SDChunk *chunk = file->chunks[event.chunkIndex];
    if(chunk->metadata.chunkID == (uint32_t)MetalChunk::MTLParallelRenderCommandEncoder_renderCommandEncoder)
    {
      const SDObject *child = chunk->FindChild("RenderCommandEncoder");
      if(child) childCreates[event.eventId] = child->AsResourceId();
    }
  }
  std::set<ResourceId> parallelChildren;
  for(const auto &create : childCreates) parallelChildren.insert(create.second);
  for(const APIEvent &event : m_Events)
  {
    if(!event.eventId || event.chunkIndex >= file->chunks.size()) continue;
    const SDChunk *chunk = file->chunks[event.chunkIndex];
    const SDObject *encoder = chunk->FindChild("RenderCommandEncoder");
    if(!encoder) encoder = chunk->FindChild("encoder");
    if(encoder && parallelChildren.count(encoder->AsResourceId()))
    {
      const ResourceId child = encoder->AsResourceId();
      childEvents[event.eventId] = child;
      children[child].push_back(event.eventId);
    }
  }
  for(auto &body : bodies)
  {
    rdcarray<uint32_t> baked;
    for(uint32_t id : body.second)
    {
      const auto create = childCreates.find(id);
      if(create != childCreates.end()) baked.append(children[create->second]);
      else if(!childEvents.count(id)) baked.push_back(id);
    }
    body.second.swap(baked);
  }
  rdcarray<uint32_t> order;
  for(const APIEvent &event : m_Events)
  {
    if(!event.eventId) continue;
    const ResourceId owner = eventOwners[event.eventId];
    if(owner == ResourceId()) order.push_back(event.eventId);
    else if(commits[owner] == event.eventId)
    {
      order.push_back(event.eventId);
      order.append(bodies[owner]);
    }
  }
  std::map<uint32_t, uint32_t> ids;
  uint32_t nextID = 1;
  for(uint32_t old : order)
  {
    if(ids.count(old)) return false;
    ids[old] = nextID++;
  }
  if(ids.size() != m_Events.size() - 1) return false;

  std::map<uint32_t, ActionDescription> flat;
  std::function<void(const rdcarray<ActionDescription> &)> flatten =
      [&](const rdcarray<ActionDescription> &actions) {
        for(const ActionDescription &action : actions)
        {
          if(!action.IsFakeMarker())
          {
            flat[action.eventId] = action;
            if(action.flags & ActionFlags::MultiAction) continue;
            flat[action.eventId].children.clear();
          }
          flatten(action.children);
        }
      };
  flatten(m_FrameRecord.actionList);
  std::set<uint32_t> childAPIEvents;
  std::function<void(const rdcarray<ActionDescription> &)> noteChildEvents =
      [&](const rdcarray<ActionDescription> &actions) {
        for(const ActionDescription &child : actions)
        {
          for(const APIEvent &event : child.events) childAPIEvents.insert(event.eventId);
          noteChildEvents(child.children);
        }
      };
  for(const auto &entry : flat)
    if(entry.second.flags & ActionFlags::MultiAction) noteChildEvents(entry.second.children);
  // API Inspector gets only the baked owner's calls, not interleaved calls from
  // another recording thread. The submission itself remains its own root action.
  std::map<ResourceId, rdcarray<APIEvent>> pending;
  for(uint32_t old : order)
  {
    const ResourceId owner = eventOwners[old];
    if(owner != ResourceId() && commits[owner] == old) continue;
    // Execution slots keep their own API events below the MultiAction. Do not
    // also attach those same events to the following ordinary action.
    if(childAPIEvents.count(old)) continue;
    APIEvent event = m_Events[old];
    event.eventId = ids[old];
    pending[owner].push_back(event);
    auto action = flat.find(old);
    if(action != flat.end())
    {
      action->second.events.swap(pending[owner]);
      pending[owner].clear();
    }
  }
  // Command-buffer signals/waits may follow the last encoder action. Keep
  // those calls on a flat end boundary, as the other explicit APIs do, rather
  // than losing the pending inspector events when the commit moves to the front.
  for(auto &entry : pending)
  {
    if(entry.first == ResourceId() || entry.second.empty()) continue;
    const uint32_t tail = bodies[entry.first].back();
    RDCASSERT(flat.count(tail) == 0);
    ActionDescription end;
    end.eventId = tail;
    end.flags = ActionFlags::CommandBufferBoundary | ActionFlags::PassBoundary | ActionFlags::EndPass;
    end.customName = StringFormat::Fmt("End Metal Command Buffer (%s)", ToStr(entry.first).c_str());
    end.events.swap(entry.second);
    flat[tail] = end;
    m_EventActionFlags[tail] = end.flags;
  }
  std::function<void(ActionDescription &)> remapChildren = [&](ActionDescription &action) {
    action.eventId = ids[action.eventId];
    // MultiAction children are execution slots with real serialized events.
    for(ActionDescription &child : action.children)
    {
      for(APIEvent &event : child.events) event.eventId = ids[event.eventId];
      remapChildren(child);
    }
  };
  rdcarray<APIEvent> events;
  events.resize(m_Events.size());
  for(uint32_t old : order)
  {
    APIEvent event = m_Events[old]; event.eventId = ids[old];
    events[event.eventId] = event;
    if(getenv("RENDERDOC_METAL_TRACE_SUBMISSION_EVENTS"))
      fprintf(stderr, "Metal submission event: old=%u new=%u chunk=%u owner=%s offset=%llu\n",
              old,event.eventId,event.chunkIndex,ToStr(eventOwners[old]).c_str(),
              (unsigned long long)event.fileOffset);
  }
  m_Events.swap(events);
  const auto rekey = [&](auto &table) {
    typename std::decay<decltype(table)>::type mapped;
    for(auto &entry : table) mapped[ids[entry.first]] = std::move(entry.second);
    table.swap(mapped);
  };
  rekey(m_EventPipelineStates);
  rekey(m_EventGraphicsInlineData);
  rekey(m_EventSamplerLOD);
  rekey(m_EventActionFlags);
  rekey(m_EventRenderPass);
  for(auto &entry : m_MultiActionEndEvents) entry.second = ids[entry.second];
  rekey(m_MultiActionEndEvents);
  for(auto &entry : m_RenderPassEvents)
  {
    for(uint32_t &id : entry.second) id = ids[id];
    std::sort(entry.second.begin(), entry.second.end());
  }
  for(auto &entry : m_ResourceUses)
  {
    for(EventUsage &use : entry.second) use.eventId = ids[use.eventId];
    std::sort(entry.second.begin(), entry.second.end());
    entry.second.resize(std::unique(entry.second.begin(), entry.second.end()) - entry.second.begin());
  }
  m_LastActionEventID = ids[m_LastActionEventID];
  m_FrameRecord.actionList.clear();
  m_DebugGroupPaths.clear();
  m_MultiActionChildrenRemaining = 0;
  for(uint32_t old : order)
  {
    const ResourceId owner = eventOwners[old];
    if(owner != ResourceId() && commits[owner] == old)
    {
      ActionDescription submit;
      submit.eventId = ids[old];
      // Match Vulkan's submit boundary so the common fake-pass grouping cannot
      // wrap a whole command buffer (including compute/pass boundaries) as a draw pass.
      submit.flags = ActionFlags::CommandBufferBoundary | ActionFlags::PassBoundary;
      submit.customName = StringFormat::Fmt("commit(%s)", ToStr(owner).c_str());
      submit.events.push_back(m_Events[submit.eventId]);
      m_EventActionFlags[submit.eventId] = submit.flags;
      m_FrameRecord.actionList.push_back(submit);
      m_DebugGroupPaths.clear();
      for(uint32_t body : bodies[owner])
      {
        auto found = flat.find(body);
        if(found == flat.end()) continue;
        ActionDescription action = found->second;
        remapChildren(action);
        if(action.flags & ActionFlags::CommandBufferBoundary)
        {
          m_DebugGroupPaths.clear();
          m_FrameRecord.actionList.push_back(action);
          continue;
        }
        m_ActiveEncoderContext = m_ActionEncoderContexts[body];
        AppendAction(action);
      }
      m_DebugGroupPaths.clear();
    }
    else if(owner == ResourceId() && flat.count(old))
    {
      ActionDescription action = flat[old]; remapChildren(action);
      m_ActiveEncoderContext = m_ActionEncoderContexts[old];
      AppendAction(action);
    }
  }
  rekey(m_ActionEncoderContexts);
  m_NextActionID = 1;
  std::function<void(rdcarray<ActionDescription> &)> numberActions =
      [&](rdcarray<ActionDescription> &actions) {
        for(ActionDescription &action : actions)
        {
          action.actionId = m_NextActionID;
          if(!(action.flags & (ActionFlags::PassBoundary | ActionFlags::PushMarker |
                               ActionFlags::PopMarker | ActionFlags::SetMarker))) ++m_NextActionID;
          numberActions(action.children);
        }
      };
  numberActions(m_FrameRecord.actionList);
  m_PendingEvents.clear();
  return true;
}

RDResult MetalReplay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  if(!rdc)
    return ResultCode::Succeeded;

  m_FrameRecord = {};
  ClearPendingComputeIndirectActions();
  m_LoadComputeIndirectOrdinals.clear();
  m_PendingRenderIndirectActions.clear();
  m_LoadRenderIndirectOrdinals.clear();
  m_RenderIndirectWrites.clear();
  m_PendingEvents.clear();
  m_ActionEncoderContexts.clear();
  m_SubmissionBindlessEvents.clear();
  m_FrameChunkOffsets.clear();
  m_Events.clear();
  m_ResourceUses.clear();
  m_EventPipelineStates.clear();
  m_EventGraphicsInlineData.clear();
  m_SelectedGraphicsInlineData.clear();
  m_SelectedBindlessDescriptors.clear();
  m_EventSamplerLOD.clear();
  m_CurrentSamplerLOD.clear();
  m_SelectedSamplerLOD.clear();
  m_NextEventID = 1;
  m_NextActionID = 1;
  m_LastActionEventID = 0;
  m_MultiActionChildrenRemaining = 0;
  m_MultiActionEndEvents.clear();
  m_EventActionFlags.clear();
  m_EventRenderPass.clear();
  m_RenderPassEvents.clear();
  m_DebugGroupPaths.clear();
  m_ContinuationEvents.clear();

  RDResult ret = m_pDriver->ReadLogInitialisation(rdc, storeStructuredBuffers);
  // As with ReplayController::AddFakeMarkers, synthetic groups get unique IDs
  // outside the captured event range and map to the state before their first
  // child. Never share an ID with a child: the UI indexes nodes by event ID.
  if(ret == ResultCode::Succeeded)
  {
    m_pDriver->CacheReplaySubmissionChunks(m_Events);
    if(!BakeSubmissionEvents())
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal submission event mapping");
    uint32_t syntheticID = m_NextEventID;
    std::function<void(rdcarray<ActionDescription> &)> assign =
        [&](rdcarray<ActionDescription> &actions) {
          for(ActionDescription &action : actions)
          {
            if(action.IsFakeMarker())
            {
              const uint32_t target = action.eventId - 1;
              action.eventId = syntheticID++;
              action.events[0].eventId = action.eventId;
              m_ContinuationEvents[action.eventId] = target;
            }
            assign(action.children);
          }
        };
    assign(m_FrameRecord.actionList);
  }
  if(ret != ResultCode::Succeeded)
    m_FatalError = ret;

  return ret;
}

void MetalReplay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  // Clear Before has already executed the selected draw with original shaders.
  // The common output's final OnlyDraw must not execute it twice or restore
  // away the modified target. Leaving this mode uses its existing full refresh.
  if(replayType == eReplay_OnlyDraw && m_ClearBeforeCompletedEvent == endEventID && endEventID)
  {
    m_ClearBeforeCompletedEvent = 0;
    return;
  }
  m_ClearBeforeCompletedEvent = 0;
  // An overlay submits a prefix and owns its own draw encoder. Restore the
  // original capture through the normal full replay rather than resuming an
  // encoder that was ended during inspection.
  if(replayType == eReplay_OnlyDraw && m_OverlayNeedsFullReplay)
    replayType = eReplay_Full;
  m_OverlayNeedsFullReplay = false;
  auto continuation = m_ContinuationEvents.find(endEventID);
  if(continuation != m_ContinuationEvents.end()) endEventID = continuation->second;
  if(replayType != eReplay_OnlyDraw)
    m_CurrentPipelineState = MetalPipe::State();
  m_CurrentGraphicsInlineData.clear();

  RDResult result = m_pDriver->ReplayLog(endEventID, replayType);
  if(result != ResultCode::Succeeded)
    m_FatalError = result;
}

const APIEvent *MetalReplay::GetEvent(uint32_t eventId) const
{
  if(m_Events.size() <= 1)
    return NULL;

  size_t index = RDCMIN((size_t)eventId, m_Events.size() - 1);
  while(index > 0 && m_Events[index].eventId == 0)
    index--;
  return index > 0 ? &m_Events[index] : NULL;
}

uint64_t MetalReplay::GetNextEventOffset(uint32_t eventId, uint64_t frameSize) const
{
  const APIEvent *event = GetEvent(eventId);
  if(event)
  {
    // Public EIDs are in submission order, not physical file order. A replay
    // chunk boundary must always come from the latter, including ICB children.
    const auto next = m_FrameChunkOffsets.upper_bound(event->fileOffset);
    if(next != m_FrameChunkOffsets.end())
      return *next;
  }
  return frameSize;
}

void MetalReplay::GetSubmissionReplayPrefix(ResourceId owner, uint32_t eventId, bool withoutDraw,
                                            std::set<uint64_t> &chunks, uint64_t &endOffset) const
{
  const APIEvent *selected = GetEvent(eventId);
  if(!selected) return;
  const auto &owners = m_pDriver->GetReplayChunkOwners();
  const SDFile *file = m_pDriver->GetStructuredFile();
  uint64_t commitOffset = UINT64_MAX;
  for(const APIEvent &event : m_Events)
  {
    auto found = owners.find(event.fileOffset);
    if(!event.eventId || found == owners.end() || found->second != owner ||
       event.chunkIndex >= file->chunks.size()) continue;
    if(file->chunks[event.chunkIndex]->metadata.chunkID == (uint32_t)MetalChunk::MTLCommandBuffer_commit)
      commitOffset = event.fileOffset;
  }
  const bool complete = selected->fileOffset >= commitOffset;
  const uint32_t limit = GetMultiActionEndEvent(eventId);
  for(const APIEvent &event : m_Events)
  {
    if(!event.eventId) continue;
    auto found = owners.find(event.fileOffset);
    if(found == owners.end() || found->second != owner) continue;
    if(complete ? event.fileOffset > selected->fileOffset :
                  (event.eventId > limit || event.fileOffset >= commitOffset)) continue;
    if(withoutDraw && event.fileOffset == selected->fileOffset) continue;
    chunks.insert(event.fileOffset);
    endOffset = RDCMAX(endOffset, GetNextEventOffset(event.eventId, endOffset));
  }
}

SDFile *MetalReplay::GetStructuredFile()
{
  return m_pDriver->GetStructuredFile();
}

MTL::Texture *MetalReplay::GetOverlayTexture(const TextureDescription &target)
{
  auto manager = m_pDriver->GetResourceManager();
  auto previous = manager->GetResource(m_OverlayTexture, true);
  MTL::Texture *texture = previous ? Unwrap((WrappedMTLTexture *)previous) : NULL;
  if(texture && (texture->width() != target.width || texture->height() != target.height))
  {
    manager->ReleaseWrappedResource((WrappedMTLTexture *)previous);
    for(size_t i = 0; i < m_Textures.size(); i++)
      if(m_Textures[i].resourceId == m_OverlayTexture) { m_Textures.erase(i); break; }
    m_OverlayTexture = ResourceId();
    texture = NULL;
  }
  if(!texture)
  {
    MTL::TextureDescriptor *desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA16Float, target.width, target.height, false);
    desc->setStorageMode(MTL::StorageModePrivate);
    desc->setUsage(MTL::TextureUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead));
    texture = Unwrap(m_pDriver)->newTexture(desc);
    if(!texture) return NULL;
    WrappedMTLTexture *wrapped = NULL;
    m_OverlayTexture = manager->WrapResource(ResourceId(), texture, wrapped, true);
    AddTexture(m_OverlayTexture, texture, false);
  }
  return texture;
}

ResourceId MetalReplay::RenderClearBefore(ResourceId texid, FloatVector clearCol,
                                         DebugOverlay overlay, uint32_t eventId,
                                         const rdcarray<uint32_t> &passEvents)
{
  auto owner = m_EventRenderPass.find(eventId);
  const APIEvent *event = GetEvent(eventId);
  const TextureDescription target = GetTexture(texid);
  if(!event || owner == m_EventRenderPass.end() || !IsRenderOutput(texid) ||
     target.dimension != 2 || target.arraysize != 1 || target.msSamp > 1 ||
     !target.width || !target.height || uint64_t(target.width)*target.height*8 > 128*1024*1024 ||
     !InitialiseOutputResources()) return ResourceId();
  auto selectPassContext = [&]() {
    auto object = m_pDriver->GetResourceManager()->GetResource(owner->second, true);
    if(!object) return false;
    if(object->m_Type == eResRenderCommandEncoder)
      return m_pDriver->GetReplayRenderCommandEncoder((WrappedMTLRenderCommandEncoder *)object) != NULL;
    if(object->m_Type == eResParallelRenderCommandEncoder)
      return m_pDriver->GetReplayParallelRenderCommandEncoder((WrappedMTLParallelRenderCommandEncoder *)object) != NULL;
    return false;
  };
  if(!selectPassContext()) return ResourceId();
  auto state = SaveEncoderState();
  if(state.renderPass.renderTargetArrayLength > 1 || state.renderPass.rasterizationRateMap)
    return ResourceId();
  // Restarting a memoryless or multisampled attachment needs a different
  // preservation path. Reject these before changing the original resources.
  auto supportedAttachment = [](const auto &attachment) {
    return !attachment.texture ||
        (!attachment.level && !attachment.slice && !attachment.depthPlane &&
         Unwrap(attachment.texture)->sampleCount() == 1 &&
         Unwrap(attachment.texture)->storageMode() != MTL::StorageModeMemoryless);
  };
  for(const auto &attachment : state.renderPass.colorAttachments)
    if(!supportedAttachment(attachment)) return ResourceId();
  if(!supportedAttachment(state.renderPass.depthAttachment) ||
     !supportedAttachment(state.renderPass.stencilAttachment)) return ResourceId();
  MTL::Texture *overlayTexture = GetOverlayTexture(target);
  if(!overlayTexture) return ResourceId();
  auto depthClearFor = [&](const MetalPipe::State &pipeline) {
    if(texid != pipeline.depthTarget.resource) return -1.0;
    auto original = m_DepthStencilStates.find(pipeline.depthStencil.resourceId);
    if(original == m_DepthStencilStates.end()) return -1.0;
    switch(original->second.depthCompareFunction)
    {
      case MTL::CompareFunctionLess: case MTL::CompareFunctionLessEqual: return 1.0;
      case MTL::CompareFunctionGreater: case MTL::CompareFunctionGreaterEqual:
      case MTL::CompareFunctionNever: return 0.0;
      default: return -1.0; // Vulkan leaves Equal/NotEqual/Always depth alone.
    }
  };
  bool success = true;
  if(overlay == DebugOverlay::ClearBeforePass)
  {
    double depthClear = -1.0;
    rdcarray<uint32_t> events = passEvents;
    events.push_back(eventId);
    for(uint32_t first : events)
    {
      auto flags = m_EventActionFlags.find(first);
      auto pipeline = m_EventPipelineStates.find(first);
      if(flags != m_EventActionFlags.end() && pipeline != m_EventPipelineStates.end() &&
         (flags->second & (ActionFlags::Drawcall | ActionFlags::MeshDispatch)))
      { depthClear = depthClearFor(pipeline->second); break; }
    }
    RDResult result = m_pDriver->ReplayClearBeforePass(eventId, owner->second, clearCol, depthClear);
    if(result != ResultCode::Succeeded) { m_FatalError = result; success = false; }
  }
  else
  {
    // Metal has no vkCmdClearAttachments. Finish/store the original prefix,
    // then restart the same attachments with Clear colour and Load depth.
    // All shaders, bindings, discard, blending and depth exports stay original.
    if(!m_pDriver->BeginOverlayReplayPrefix(owner->second)) return ResourceId();
    ReplayLog(eventId, eReplay_WithoutDraw);
    if(!selectPassContext())
    {
      m_pDriver->FinishOverlayReplayPrefix(owner->second);
      m_OverlayNeedsFullReplay = true;
      return ResourceId();
    }
    state = SaveEncoderState();
    m_OverlayNeedsFullReplay = true;
    success = m_pDriver->FinishOverlayReplayPrefix(owner->second);
    RestoreEncoderState(state);
    if(success)
    {
      MTL::RenderPassDescriptor *pass = (MTL::RenderPassDescriptor *)state.renderPass;
      for(unsigned i = 0; i < 8; i++)
      {
        auto color = pass->colorAttachments()->object(i);
        if(!color->texture()) continue;
        color->setLoadAction(MTL::LoadActionClear);
        color->setClearColor(MTL::ClearColor::Make(clearCol.x,clearCol.y,clearCol.z,clearCol.w));
        color->setStoreAction(color->resolveTexture() ? MTL::StoreActionStoreAndMultisampleResolve : MTL::StoreActionStore);
      }
      const double depthClear = depthClearFor(state.pipeline);
      if(pass->depthAttachment()->texture())
      {
        pass->depthAttachment()->setLoadAction(depthClear < 0 ? MTL::LoadActionLoad : MTL::LoadActionClear);
        if(depthClear >= 0) pass->depthAttachment()->setClearDepth(depthClear);
        pass->depthAttachment()->setStoreAction(pass->depthAttachment()->resolveTexture() ?
            MTL::StoreActionStoreAndMultisampleResolve : MTL::StoreActionStore);
      }
      if(pass->stencilAttachment()->texture())
      {
        pass->stencilAttachment()->setLoadAction(depthClear < 0 ? MTL::LoadActionLoad : MTL::LoadActionClear);
        if(depthClear >= 0) pass->stencilAttachment()->setClearStencil(0);
        pass->stencilAttachment()->setStoreAction(pass->stencilAttachment()->resolveTexture() ?
            MTL::StoreActionStoreAndMultisampleResolve : MTL::StoreActionStore);
      }
      MTL::CommandBuffer *command = m_OutputQueue->commandBuffer();
      MTL::RenderCommandEncoder *encoder = command->renderCommandEncoder(pass);
      pass->release();
      success = encoder && m_pDriver->ReplayOverlayDraw(eventId, encoder,
          [](MTL::RenderCommandEncoder *) {}, true);
      if(encoder) encoder->endEncoding();
      command->commit(); command->waitUntilCompleted();
      success &= command->status() == MTL::CommandBufferStatusCompleted;
    }
  }
  // Vulkan returns a transparent overlay and displays the modified ORIGINAL
  // target underneath. Keep the shared viewer's channel/range/gamma behaviour.
  if(success)
  {
    MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    pass->colorAttachments()->object(0)->setTexture(overlayTexture);
    pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0,0,0,0));
    pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    MTL::CommandBuffer *command = m_OutputQueue->commandBuffer();
    MTL::RenderCommandEncoder *encoder = command->renderCommandEncoder(pass);
    if(encoder) encoder->endEncoding(); else success = false;
    command->commit(); command->waitUntilCompleted();
    success &= command->status() == MTL::CommandBufferStatusCompleted;
  }
  RestoreEncoderState(state);
  m_OverlayNeedsFullReplay = !success;
  m_ClearBeforeCompletedEvent = success ? eventId : 0;
  return success ? m_OverlayTexture : ResourceId();
}

ResourceId MetalReplay::RenderDepthExportOverlay(ResourceId texid, uint32_t eventId,
                                                   WrappedMTLRenderCommandEncoder *source)
{
  const TextureDescription target = GetTexture(texid);
  EncoderReplayState state = SaveEncoderState();
  auto info = m_RenderPipelines.find(state.pipeline.pipelineResourceId);
  if(info == m_RenderPipelines.end() || !state.renderPass.depthAttachment.texture)
    return ResourceId();
  if(!Process::GetEnvVariable("RENDERDOC_METAL_TRACE_OVERLAY").empty())
    fprintf(stderr, "Metal original fragment depth mask: EID=%u shader=%s\n", eventId,
            ToStr(info->second.fragmentFunction).c_str());
  MTL::Texture *originalDepth = Unwrap(state.renderPass.depthAttachment.texture);
  auto supported = [&](MTL::Texture *texture) {
    return texture && texture->textureType() == MTL::TextureType2D &&
           texture->sampleCount() == 1 && texture->width() == target.width &&
           texture->height() == target.height && texture->storageMode() != MTL::StorageModeMemoryless;
  };
  if(!supported(originalDepth)) return ResourceId();
  // Budget private MRTs, the depth source copy, D32S8 mask, and returned overlay.
  uint64_t bytes = uint64_t(target.width) * target.height * 16 + originalDepth->allocatedSize();
  for(const auto &attachment : state.renderPass.colorAttachments)
    if(attachment.texture)
    {
      MTL::Texture *texture = Unwrap(attachment.texture);
      if(!supported(texture)) return ResourceId();
      bytes += texture->allocatedSize();
    }
  if(bytes > 128 * 1024 * 1024 || !InitialiseOutputResources()) return ResourceId();
  MTL::Texture *overlay = GetOverlayTexture(target);
  if(!overlay) return ResourceId();
  MTL::Device *device = Unwrap(m_pDriver);
  NS::Error *error = NULL;
  auto library = NS::TransferPtr(device->newLibrary(NS::String::string(
      "#include <metal_stdlib>\nusing namespace metal;\n"
      "vertex float4 full(uint i [[vertex_id]]) {"
      "const float2 p[6]={float2(-1,-1),float2(1,-1),float2(-1,1),"
      "float2(-1,1),float2(1,-1),float2(1,1)};return float4(p[i],0,1);}"
      "struct DepthValue {float depth [[depth(any)]];};"
      "fragment DepthValue copyDepth(float4 p [[position]], depth2d<float,access::read> t [[texture(0)]]) {"
      "return {t.read(uint2(p.xy))};}"
      "fragment float4 red() {return float4(1,0,0,1);}"
      "fragment float4 resolve(float4 p [[position]], texture2d<uint,access::read> t [[texture(0)]]) {"
      "if(t.read(uint2(p.xy)).x==0) discard_fragment();return float4(0,1,0,1);}",
      NS::UTF8StringEncoding), NULL, &error));
  if(!library) return ResourceId();
  auto full = NS::TransferPtr(library->newFunction(NS::String::string("full", NS::UTF8StringEncoding)));
  auto red = NS::TransferPtr(library->newFunction(NS::String::string("red", NS::UTF8StringEncoding)));
  auto copy = NS::TransferPtr(library->newFunction(NS::String::string("copyDepth", NS::UTF8StringEncoding)));
  auto resolve = NS::TransferPtr(library->newFunction(NS::String::string("resolve", NS::UTF8StringEncoding)));
  auto makeTexture = [&](MTL::PixelFormat format, MTL::TextureUsage usage) {
    auto desc = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    desc->setTextureType(MTL::TextureType2D);
    desc->setWidth(target.width); desc->setHeight(target.height);
    desc->setPixelFormat(format); desc->setStorageMode(MTL::StorageModePrivate);
    desc->setUsage(usage);
    return NS::TransferPtr(device->newTexture(desc.get()));
  };
  auto depthCopy = makeTexture(originalDepth->pixelFormat(), MTL::TextureUsageShaderRead);
  auto mask = makeTexture(MTL::PixelFormatDepth32Float_Stencil8,
      MTL::TextureUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead | MTL::TextureUsagePixelFormatView));
  if(!depthCopy || !mask) return ResourceId();
  auto stencilView = NS::TransferPtr(mask->newTextureView(MTL::PixelFormatX32_Stencil8));
  if(!stencilView) return ResourceId();
  rdcarray<NS::SharedPtr<MTL::Texture>> colours;
  for(const auto &attachment : state.renderPass.colorAttachments)
  {
    NS::SharedPtr<MTL::Texture> texture;
    if(attachment.texture)
    {
      texture = makeTexture(Unwrap(attachment.texture)->pixelFormat(), MTL::TextureUsageRenderTarget);
      if(!texture) return ResourceId();
    }
    colours.push_back(texture);
  }
  auto redCaptured = info->second.descriptor;
  redCaptured.binaryArchives.clear(); redCaptured.fragmentPreloadedLibraries.clear();
  redCaptured.fragmentLinkedFunctions = RDMTL::LinkedFunctions(); redCaptured.fragmentBuffers.clear();
  auto redDesc = NS::TransferPtr((MTL::RenderPipelineDescriptor *)redCaptured);
  redDesc->setFragmentFunction(red.get());
  redDesc->setAlphaToCoverageEnabled(false); redDesc->setAlphaToOneEnabled(false);
  redDesc->setDepthAttachmentPixelFormat(MTL::PixelFormatInvalid);
  redDesc->setStencilAttachmentPixelFormat(MTL::PixelFormatInvalid);
  for(unsigned i = 0; i < 8; i++)
  {
    auto a = redDesc->colorAttachments()->object(i);
    a->setPixelFormat(i ? MTL::PixelFormatInvalid : MTL::PixelFormatRGBA16Float);
    a->setBlendingEnabled(false); a->setWriteMask(MTL::ColorWriteMaskAll);
  }
  auto redPSO = NS::TransferPtr(device->newRenderPipelineState(redDesc.get(), &error));
  auto maskCaptured = info->second.descriptor;
  maskCaptured.binaryArchives.clear();
  auto maskDesc = NS::TransferPtr((MTL::RenderPipelineDescriptor *)maskCaptured);
  maskDesc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);
  maskDesc->setStencilAttachmentPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);
  maskDesc->setAlphaToCoverageEnabled(false); maskDesc->setAlphaToOneEnabled(false);
  for(unsigned i = 0; i < 8; i++)
  {
    auto a = maskDesc->colorAttachments()->object(i);
    a->setWriteMask(MTL::ColorWriteMaskNone); a->setBlendingEnabled(false);
  }
  auto maskPSO = NS::TransferPtr(device->newRenderPipelineState(maskDesc.get(), &error));
  auto fullDesc = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
  fullDesc->setVertexFunction(full.get()); fullDesc->setFragmentFunction(copy.get());
  fullDesc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);
  fullDesc->setStencilAttachmentPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);
  auto copyPSO = NS::TransferPtr(device->newRenderPipelineState(fullDesc.get(), &error));
  fullDesc->setDepthAttachmentPixelFormat(MTL::PixelFormatInvalid);
  fullDesc->setStencilAttachmentPixelFormat(MTL::PixelFormatInvalid);
  fullDesc->setFragmentFunction(resolve.get());
  fullDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA16Float);
  auto resolvePSO = NS::TransferPtr(device->newRenderPipelineState(fullDesc.get(), &error));
  if(!redPSO || !maskPSO || !copyPSO || !resolvePSO) return ResourceId();
  RDMTL::DepthStencilDescriptor depth;
  auto original = m_DepthStencilStates.find(state.pipeline.depthStencil.resourceId);
  if(original != m_DepthStencilStates.end()) depth = original->second;
  depth.depthWriteEnabled = false;
  for(auto s : {&depth.frontFaceStencil, &depth.backFaceStencil})
  {
    s->enabled = true; s->stencilCompareFunction = MTL::CompareFunctionAlways;
    s->stencilFailureOperation = s->depthFailureOperation = MTL::StencilOperationKeep;
    s->depthStencilPassOperation = MTL::StencilOperationReplace;
    s->readMask = s->writeMask = 0xff;
  }
  auto depthDesc = NS::TransferPtr((MTL::DepthStencilDescriptor *)depth);
  auto maskDS = NS::TransferPtr(device->newDepthStencilState(depthDesc.get()));
  auto plainDesc = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
  plainDesc->setDepthCompareFunction(MTL::CompareFunctionAlways);
  auto redDS = NS::TransferPtr(device->newDepthStencilState(plainDesc.get()));
  plainDesc->setDepthWriteEnabled(true);
  auto copyDS = NS::TransferPtr(device->newDepthStencilState(plainDesc.get()));
  if(!maskDS || !redDS || !copyDS) return ResourceId();
  const ResourceId preserve = source->GetParallelParent() ? GetResID(source->GetParallelParent()) : GetResID(source);
  bool success = true;
  for(unsigned phase = 0; phase < 2 && success; phase++)
  {
    if(!m_pDriver->BeginOverlayReplayPrefix(preserve)) { success = false; break; }
    ReplayLog(eventId, eReplay_WithoutDraw);
    m_OverlayNeedsFullReplay = true;
    // The submission prefix may last touch another command buffer. Select
    // the captured draw's encoder context, as the other Metal overlays do.
    if(!m_pDriver->GetReplayRenderCommandEncoder(source))
    {
      m_pDriver->FinishOverlayReplayPrefix(preserve);
      success = false;
      break;
    }
    state = SaveEncoderState();
    auto sourceDepth = NS::RetainPtr(Unwrap(state.renderPass.depthAttachment.texture));
    success = m_pDriver->FinishOverlayReplayPrefix(preserve);
    RestoreEncoderState(state);
    if(!success || !supported(sourceDepth.get())) { success = false; break; }
    MTL::CommandBuffer *command = m_OutputQueue->commandBuffer();
    if(phase)
    {
      auto blit = command->blitCommandEncoder();
      blit->copyFromTexture(sourceDepth.get(), 0, 0, MTL::Origin(0,0,0),
          MTL::Size(target.width,target.height,1), depthCopy.get(), 0, 0, MTL::Origin(0,0,0));
      blit->endEncoding();
      auto pass = MTL::RenderPassDescriptor::renderPassDescriptor();
      pass->depthAttachment()->setTexture(mask.get());
      pass->depthAttachment()->setLoadAction(MTL::LoadActionDontCare);
      pass->depthAttachment()->setStoreAction(MTL::StoreActionStore);
      pass->stencilAttachment()->setTexture(mask.get());
      pass->stencilAttachment()->setLoadAction(MTL::LoadActionClear);
      pass->stencilAttachment()->setStoreAction(MTL::StoreActionStore);
      pass->stencilAttachment()->setClearStencil(0);
      auto encoder = command->renderCommandEncoder(pass);
      if(!encoder) success = false;
      else
      {
        encoder->setRenderPipelineState(copyPSO.get()); encoder->setDepthStencilState(copyDS.get());
        encoder->setFragmentTexture(depthCopy.get(),0);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(6)); encoder->endEncoding();
      }
    }
    auto pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    if(!phase)
    {
      pass->colorAttachments()->object(0)->setTexture(overlay);
      pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
      pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0,0,0,0));
      pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    }
    else
    {
      for(unsigned i = 0; i < colours.size(); i++)
        if(colours[i])
        {
          auto a = pass->colorAttachments()->object(i);
          a->setTexture(colours[i].get()); a->setLoadAction(MTL::LoadActionDontCare);
          a->setStoreAction(MTL::StoreActionDontCare);
        }
      pass->depthAttachment()->setTexture(mask.get());
      pass->depthAttachment()->setLoadAction(MTL::LoadActionLoad);
      pass->depthAttachment()->setStoreAction(MTL::StoreActionStore);
      pass->stencilAttachment()->setTexture(mask.get());
      pass->stencilAttachment()->setLoadAction(MTL::LoadActionLoad);
      pass->stencilAttachment()->setStoreAction(MTL::StoreActionStore);
    }
    auto encoder = success ? command->renderCommandEncoder(pass) : NULL;
    success = encoder && m_pDriver->ReplayOverlayDraw(eventId, encoder, [&](MTL::RenderCommandEncoder *e) {
      e->setRenderPipelineState(phase ? maskPSO.get() : redPSO.get());
      e->setDepthStencilState(phase ? maskDS.get() : redDS.get());
      e->setTriangleFillMode(MTL::TriangleFillModeFill);
      if(phase) e->setStencilReferenceValue(1);
      else { e->setCullMode(MTL::CullModeNone); e->setDepthClipMode(MTL::DepthClipModeClamp); }
    });
    if(encoder) encoder->endEncoding();
    if(phase && success)
    {
      auto resolvePass = MTL::RenderPassDescriptor::renderPassDescriptor();
      resolvePass->colorAttachments()->object(0)->setTexture(overlay);
      resolvePass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionLoad);
      resolvePass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
      auto resolveEncoder = command->renderCommandEncoder(resolvePass);
      if(!resolveEncoder) success = false;
      else
      {
        resolveEncoder->setRenderPipelineState(resolvePSO.get());
        resolveEncoder->setFragmentTexture(stencilView.get(),0);
        resolveEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(6)); resolveEncoder->endEncoding();
      }
    }
    command->commit(); command->waitUntilCompleted();
    success &= command->status() == MTL::CommandBufferStatusCompleted;
  }
  RestoreEncoderState(state);
  return success ? m_OverlayTexture : ResourceId();
}

ResourceId MetalReplay::RenderGeometryOverlay(ResourceId texid, DebugOverlay kind, uint32_t eventId,
                                               const rdcarray<uint32_t> &passEvents)
{
  // Vulkan quadwrite counts live lanes in each primitive's 2x2 pixel group.
  // Keep the four live-lane buckets and divide by their lane count at resolve.
  const bool triangle = kind == DebugOverlay::TriangleSizeDraw || kind == DebugOverlay::TriangleSizePass;
  const TextureDescription target = GetTexture(texid);
  const uint32_t qw = (target.width + 1) / 2, qh = (target.height + 1) / 2;
  const uint64_t counterBytes = triangle ? 0 : uint64_t(qw) * qh * 4 * sizeof(uint32_t);
  if(counterBytes + uint64_t(target.width) * target.height * 16 > 128 * 1024 * 1024 ||
     !InitialiseOutputResources()) return ResourceId();
  auto owner = m_EventRenderPass.find(eventId);
  if(owner == m_EventRenderPass.end()) return ResourceId();
  rdcarray<uint32_t> events;
  if(kind == DebugOverlay::QuadOverdrawPass || kind == DebugOverlay::TriangleSizePass)
    for(uint32_t eid : passEvents)
    {
      auto flags = m_EventActionFlags.find(eid);
      if(flags != m_EventActionFlags.end() && (flags->second & ActionFlags::BeginPass)) continue;
      if(eid >= eventId || m_EventRenderPass.find(eid) == m_EventRenderPass.end() ||
         m_EventRenderPass[eid] != owner->second) return ResourceId();
      events.push_back(eid);
    }
  events.push_back(eventId);
  MTL::Device *device = Unwrap(m_pDriver);
  MTL::Texture *overlay = GetOverlayTexture(target);
  if(!overlay) return ResourceId();
  NS::SharedPtr<MTL::Buffer> counters;
  if(!triangle)
  {
    counters = NS::TransferPtr(device->newBuffer(counterBytes, MTL::ResourceStorageModeShared));
    if(!counters) return ResourceId();
    memset(counters->contents(), 0, size_t(counterBytes));
  }
  const uint32_t dimensions[2] = {qw,qh};
  NS::Error *error = NULL;
  auto library = NS::TransferPtr(device->newLibrary(NS::String::string(
      "#include <metal_stdlib>\nusing namespace metal;\n"
      "[[early_fragment_tests]] fragment void count(float4 p [[position]],"
      "uint coverage [[sample_mask,post_depth_coverage]],device atomic_uint *b [[buffer(0)]],"
      "constant uint2 &size [[buffer(1)]]) {uint c=coverage&1u;"
      "uint n=c+quad_shuffle_xor(c,1)+quad_shuffle_xor(c,2)+quad_shuffle_xor(c,3);"
      "uint2 q=uint2(p.xy)/2; if(c && n && all(q<size))"
      "atomic_fetch_add_explicit(b+(n-1)*size.x*size.y+q.y*size.x+q.x,1u,memory_order_relaxed);}"
      "fragment float4 area(float3 b [[barycentric_coord,center_no_perspective]]) {"
      "float2 x=dfdx(b.xy),y=dfdy(b.xy);"
      "float a=max(.5f/abs(x.x*y.y-x.y*y.x),.001f);return float4(a,a,a,1);}"
      "vertex float4 full(uint i [[vertex_id]]) {"
      "const float2 p[3]={float2(-1,1),float2(3,1),float2(-1,-3)};return float4(p[i],0,1);}"
      "fragment float4 resolve(float4 p [[position]],device const uint *b [[buffer(0)]],"
      "constant uint2 &size [[buffer(1)]]) {uint2 q=uint2(p.xy)/2;uint count=0;"
      "for(uint i=0;i<4;i++) count+=b[i*size.x*size.y+q.y*size.x+q.x]/(i+1);"
      "return float4(float(count));}", NS::UTF8StringEncoding), NULL, &error));
  if(!library) return ResourceId();
  auto fragment = NS::TransferPtr(library->newFunction(NS::String::string(triangle ? "area" : "count",NS::UTF8StringEncoding)));
  auto full = NS::TransferPtr(library->newFunction(NS::String::string("full",NS::UTF8StringEncoding)));
  auto resolve = NS::TransferPtr(library->newFunction(NS::String::string("resolve",NS::UTF8StringEncoding)));
  auto makeTexture = [&](MTL::PixelFormat format) {
    auto desc = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    desc->setTextureType(MTL::TextureType2D);desc->setWidth(target.width);desc->setHeight(target.height);
    desc->setPixelFormat(format);desc->setStorageMode(MTL::StorageModePrivate);
    desc->setUsage(MTL::TextureUsageRenderTarget);
    return NS::TransferPtr(device->newTexture(desc.get()));
  };
  auto dummy = makeTexture(MTL::PixelFormatRGBA16Float);
  if(!dummy) return ResourceId();
  if(triangle)
  {
    auto command=m_OutputQueue->commandBuffer();
    auto pass=MTL::RenderPassDescriptor::renderPassDescriptor();
    auto colour=pass->colorAttachments()->object(0);colour->setTexture(overlay);
    colour->setLoadAction(MTL::LoadActionClear);colour->setClearColor(MTL::ClearColor::Make(0,0,0,0));
    colour->setStoreAction(MTL::StoreActionStore);
    auto encoder=command->renderCommandEncoder(pass);
    if(!encoder) return ResourceId();
    encoder->endEncoding();command->commit();command->waitUntilCompleted();
    if(command->status()!=MTL::CommandBufferStatusCompleted) return ResourceId();
  }
  bool success = true;
  EncoderReplayState saved = SaveEncoderState();
  for(uint32_t eid : events)
  {
    auto selected=m_EventPipelineStates.find(eid);
    if(triangle && (selected==m_EventPipelineStates.end() ||
        (selected->second.topology!=Topology::TriangleList && selected->second.topology!=Topology::TriangleStrip)))
    { success=false;break; }
    const APIEvent *event = GetEvent(eid);
    if(!event || event->chunkIndex >= GetStructuredFile()->chunks.size()) { success=false;break; }
    const SDObject *field = GetStructuredFile()->chunks[event->chunkIndex]->FindChild("RenderCommandEncoder");
    if(!field) { success=false;break; }
    auto object = m_pDriver->GetResourceManager()->GetResource(field->AsResourceId(),true);
    if(!object || object->m_Type != eResRenderCommandEncoder) { success=false;break; }
    auto source = (WrappedMTLRenderCommandEncoder *)object;
    const ResourceId preserve = source->GetParallelParent() ? GetResID(source->GetParallelParent()) : GetResID(source);
    if(!m_pDriver->BeginOverlayReplayPrefix(preserve)) { success=false;break; }
    // Independent completed ordinary prefixes reproduce real prior depth writes
    // and vertex/storage side effects, while the private counters accumulate.
    ReplayLog(eid,eReplay_WithoutDraw);
    m_OverlayNeedsFullReplay = true;
    if(!m_pDriver->GetReplayRenderCommandEncoder(source))
    { m_pDriver->FinishOverlayReplayPrefix(preserve);success=false;break; }
    EncoderReplayState state = SaveEncoderState();
    auto sourceDepth = NS::RetainPtr(Unwrap(state.renderPass.depthAttachment.texture));
    auto sourceStencil = NS::RetainPtr(Unwrap(state.renderPass.stencilAttachment.texture));
    success = m_pDriver->FinishOverlayReplayPrefix(preserve);
    RestoreEncoderState(state);
    auto info = m_RenderPipelines.find(state.pipeline.pipelineResourceId);
    if(!success || info==m_RenderPipelines.end() || !info->second.rasterizationEnabled ||
       info->second.sampleCount!=1 || state.renderPass.rasterizationRateMap ||
       state.renderPass.renderTargetArrayLength>1) { success=false;break; }
    uint64_t bytes=counterBytes+uint64_t(target.width)*target.height*16;
    auto supported = [&](MTL::Texture *texture, const RDMTL::RenderPassAttachmentDescriptor &a) {
      if(!texture) return true;
      bytes+=texture->allocatedSize();
      return texture->textureType()==MTL::TextureType2D && texture->sampleCount()==1 &&
          texture->width()==target.width && texture->height()==target.height &&
          texture->storageMode()!=MTL::StorageModeMemoryless && !a.level && !a.slice && !a.depthPlane;
    };
    if(!supported(sourceDepth.get(),state.renderPass.depthAttachment) ||
       !supported(sourceStencil.get(),state.renderPass.stencilAttachment) || bytes>128*1024*1024)
    { success=false;break; }
    NS::SharedPtr<MTL::Texture> depth, stencil;
    if(sourceDepth) depth=makeTexture(sourceDepth->pixelFormat());
    if(sourceStencil) stencil=sourceStencil.get()==sourceDepth.get() ? depth : makeTexture(sourceStencil->pixelFormat());
    if((sourceDepth && !depth) || (sourceStencil && !stencil)) { success=false;break; }
    auto captured = info->second.descriptor;
    captured.binaryArchives.clear();captured.fragmentPreloadedLibraries.clear();
    captured.fragmentLinkedFunctions=RDMTL::LinkedFunctions();captured.fragmentBuffers.clear();
    auto desc = NS::TransferPtr((MTL::RenderPipelineDescriptor *)captured);
    desc->setFragmentFunction(fragment.get());desc->setAlphaToCoverageEnabled(false);desc->setAlphaToOneEnabled(false);
    for(unsigned i=0;i<8;i++)
    {
      auto a=desc->colorAttachments()->object(i);
      a->setPixelFormat(i ? MTL::PixelFormatInvalid : MTL::PixelFormatRGBA16Float);
      a->setWriteMask(triangle ? MTL::ColorWriteMaskAll : MTL::ColorWriteMaskNone);a->setBlendingEnabled(false);
    }
    desc->setDepthAttachmentPixelFormat(depth ? depth->pixelFormat() : MTL::PixelFormatInvalid);
    desc->setStencilAttachmentPixelFormat(stencil ? stencil->pixelFormat() : MTL::PixelFormatInvalid);
    auto pipeline=NS::TransferPtr(device->newRenderPipelineState(desc.get(),&error));
    RDMTL::DepthStencilDescriptor ds;
    auto original=m_DepthStencilStates.find(state.pipeline.depthStencil.resourceId);
    if(original!=m_DepthStencilStates.end()) ds=original->second;
    if(!triangle)
    {
      ds.depthWriteEnabled=false;
      for(auto s : {&ds.frontFaceStencil,&ds.backFaceStencil})
      {
        s->writeMask=0;
        s->stencilFailureOperation=s->depthFailureOperation=s->depthStencilPassOperation=MTL::StencilOperationKeep;
      }
    }
    auto dsDesc=NS::TransferPtr((MTL::DepthStencilDescriptor *)ds);
    auto depthState=NS::TransferPtr(device->newDepthStencilState(dsDesc.get()));
    if(!pipeline || !depthState) { success=false;break; }
    auto command=m_OutputQueue->commandBuffer();
    if(depth || stencil)
    {
      auto blit=command->blitCommandEncoder();
      if(depth) blit->copyFromTexture(sourceDepth.get(),0,0,MTL::Origin(0,0,0),
          MTL::Size(target.width,target.height,1),depth.get(),0,0,MTL::Origin(0,0,0));
      if(stencil && stencil.get()!=depth.get()) blit->copyFromTexture(sourceStencil.get(),0,0,MTL::Origin(0,0,0),
          MTL::Size(target.width,target.height,1),stencil.get(),0,0,MTL::Origin(0,0,0));
      blit->endEncoding();
    }
    auto pass=MTL::RenderPassDescriptor::renderPassDescriptor();
    auto colour=pass->colorAttachments()->object(0);colour->setTexture(triangle ? overlay : dummy.get());
    colour->setLoadAction(triangle ? MTL::LoadActionLoad : MTL::LoadActionDontCare);
    colour->setStoreAction(triangle ? MTL::StoreActionStore : MTL::StoreActionDontCare);
    if(depth) {pass->depthAttachment()->setTexture(depth.get());pass->depthAttachment()->setLoadAction(MTL::LoadActionLoad);pass->depthAttachment()->setStoreAction(MTL::StoreActionDontCare);}
    if(stencil) {pass->stencilAttachment()->setTexture(stencil.get());pass->stencilAttachment()->setLoadAction(MTL::LoadActionLoad);pass->stencilAttachment()->setStoreAction(MTL::StoreActionDontCare);}
    auto encoder=command->renderCommandEncoder(pass);
    success=encoder && m_pDriver->ReplayOverlayDraw(eid,encoder,[&](MTL::RenderCommandEncoder *e) {
      e->setRenderPipelineState(pipeline.get());e->setDepthStencilState(depthState.get());
      if(!triangle) {e->setFragmentBuffer(counters.get(),0,0);e->setFragmentBytes(dimensions,sizeof(dimensions),1);}
    });
    if(encoder) encoder->endEncoding();
    command->commit();command->waitUntilCompleted();
    success &= command->status()==MTL::CommandBufferStatusCompleted;
    if(!success) break;
  }
  if(success && !triangle)
  {
    auto desc=NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
    desc->setVertexFunction(full.get());desc->setFragmentFunction(resolve.get());
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA16Float);
    auto pipeline=NS::TransferPtr(device->newRenderPipelineState(desc.get(),&error));
    if(!pipeline) success=false;
    else
    {
      auto command=m_OutputQueue->commandBuffer();
      auto pass=MTL::RenderPassDescriptor::renderPassDescriptor();
      auto colour=pass->colorAttachments()->object(0);colour->setTexture(overlay);
      colour->setLoadAction(MTL::LoadActionDontCare);colour->setStoreAction(MTL::StoreActionStore);
      auto encoder=command->renderCommandEncoder(pass);
      if(!encoder) success=false;
      else
      {
        encoder->setRenderPipelineState(pipeline.get());encoder->setFragmentBuffer(counters.get(),0,0);
        encoder->setFragmentBytes(dimensions,sizeof(dimensions),1);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));encoder->endEncoding();
      }
      command->commit();command->waitUntilCompleted();success &= command->status()==MTL::CommandBufferStatusCompleted;
    }
  }
  RestoreEncoderState(saved);
  return success ? m_OverlayTexture : ResourceId();
}

ResourceId MetalReplay::RenderOverlay(ResourceId texid, FloatVector clearCol, DebugOverlay overlay,
                                     uint32_t eventId, const rdcarray<uint32_t> &passEvents)
{
  if(overlay == DebugOverlay::ClearBeforeDraw || overlay == DebugOverlay::ClearBeforePass)
    return RenderClearBefore(texid, clearCol, overlay, eventId, passEvents);
  if(overlay != DebugOverlay::Depth && overlay != DebugOverlay::Stencil &&
     overlay != DebugOverlay::Drawcall && overlay != DebugOverlay::BackfaceCull &&
     overlay != DebugOverlay::Wireframe && overlay != DebugOverlay::ViewportScissor &&
     overlay != DebugOverlay::QuadOverdrawDraw && overlay != DebugOverlay::QuadOverdrawPass &&
     overlay != DebugOverlay::TriangleSizeDraw && overlay != DebugOverlay::TriangleSizePass)
    return ResourceId();
  const APIEvent *event = GetEvent(eventId);
  if(!event || event->chunkIndex >= GetStructuredFile()->chunks.size() || !IsRenderOutput(texid))
    return ResourceId();
  const SDChunk *chunk = GetStructuredFile()->chunks[event->chunkIndex];
  const SDObject *field = chunk->FindChild("RenderCommandEncoder");
  if(!field) return ResourceId();
  auto manager = m_pDriver->GetResourceManager();
  auto object = manager->GetResource(field->AsResourceId(), true);
  if(!object || object->m_Type != eResRenderCommandEncoder) return ResourceId();
  auto source = (WrappedMTLRenderCommandEncoder *)object;
  if(!m_pDriver->GetReplayRenderCommandEncoder(source)) return ResourceId();
  EncoderReplayState state = SaveEncoderState();
  if(overlay == DebugOverlay::ViewportScissor &&
     (state.viewportCount != 1 || state.scissorCount != 1))
    return ResourceId();
  auto info = m_RenderPipelines.find(state.pipeline.pipelineResourceId);
  const TextureDescription target = GetTexture(texid);
  if(info == m_RenderPipelines.end() || !info->second.rasterizationEnabled ||
     info->second.sampleCount != 1 || target.msSamp > 1 || target.dimension != 2 ||
     target.arraysize != 1 || !target.width || !target.height ||
     uint64_t(target.width) * target.height * 8 > 128 * 1024 * 1024 ||
     state.renderPass.renderTargetArrayLength > 1 || state.renderPass.rasterizationRateMap)
    return ResourceId();
  for(const auto &a : state.renderPass.colorAttachments)
    if(a.texture && (a.level || a.slice || a.depthPlane)) return ResourceId();
  if(state.renderPass.depthAttachment.level || state.renderPass.depthAttachment.slice ||
     state.renderPass.depthAttachment.depthPlane || state.renderPass.stencilAttachment.level ||
     state.renderPass.stencilAttachment.slice || state.renderPass.stencilAttachment.depthPlane)
    return ResourceId();
  if(overlay == DebugOverlay::QuadOverdrawDraw || overlay == DebugOverlay::QuadOverdrawPass ||
     overlay == DebugOverlay::TriangleSizeDraw || overlay == DebugOverlay::TriangleSizePass)
    return RenderGeometryOverlay(texid,overlay,eventId,passEvents);
  // D3D12/Vulkan preserve fragment depth exports using a stencil mask.
  bool exportedDepth = false;
  if(overlay == DebugOverlay::Depth)
  {
    auto shader = m_Shaders.find(info->second.fragmentFunction);
    if(shader != m_Shaders.end())
      for(const SigParameter &output : shader->second.outputSignature)
        exportedDepth |= output.systemValue == ShaderBuiltin::DepthOutput;
    auto lib = m_ShaderLibraries.find(info->second.fragmentFunction);
    if(lib != m_ShaderLibraries.end())
    {
      auto sourceText = m_LibrarySources.find(lib->second);
      if(sourceText != m_LibrarySources.end() && !sourceText->second.empty())
      {
        rdcstr compact;
        for(char c : sourceText->second)
          if(c != ' ' && c != '\t' && c != '\r' && c != '\n') compact += c;
        exportedDepth |= compact.contains("[[depth(");
      }
      else if(m_LibraryBinaries.count(lib->second) && shader != m_Shaders.end())
      {
        const rdcstr air = DisassembleShader(ResourceId(), &shader->second, "Metal AIR (Apple toolchain)");
        if(!air.contains("define ")) return ResourceId();
        exportedDepth |= air.contains("!\"air.depth\"");
      }
    }
    if(exportedDepth && state.renderPass.depthAttachment.texture)
      return RenderDepthExportOverlay(texid, eventId, source);
  }
  if(!InitialiseOutputResources()) return ResourceId();
  MTL::Device *device = Unwrap(m_pDriver);
  MTL::Texture *texture = GetOverlayTexture(target);
  if(!texture) return ResourceId();
  NS::Error *error = NULL;
  MTL::Library *library = device->newLibrary(NS::String::string(
      "#include <metal_stdlib>\nusing namespace metal;\n"
      "fragment float4 rdoc_overlay_fs(constant float4 &c [[buffer(0)]]) { return c; }",
      NS::UTF8StringEncoding), NULL, &error);
  if(!library) return ResourceId();
  MTL::Function *fragment = library->newFunction(NS::String::string("rdoc_overlay_fs", NS::UTF8StringEncoding));
  RDMTL::RenderPipelineDescriptor captured = info->second.descriptor;
  captured.binaryArchives.clear();
  captured.fragmentPreloadedLibraries.clear();
  captured.fragmentLinkedFunctions = RDMTL::LinkedFunctions();
  captured.fragmentBuffers.clear();
  MTL::RenderPipelineDescriptor *desc = (MTL::RenderPipelineDescriptor *)captured;
  desc->setFragmentFunction(fragment);
  desc->setAlphaToCoverageEnabled(false);
  desc->setAlphaToOneEnabled(false);
  for(unsigned i = 0; i < 8; i++)
  {
    auto a = desc->colorAttachments()->object(i);
    a->setPixelFormat(i == 0 ? MTL::PixelFormatRGBA16Float : MTL::PixelFormatInvalid);
    a->setBlendingEnabled(false);
    a->setWriteMask(MTL::ColorWriteMaskAll);
  }
  const bool testDepth = overlay == DebugOverlay::Depth;
  const bool testStencil = overlay == DebugOverlay::Stencil;
  desc->setDepthAttachmentPixelFormat(testDepth && state.renderPass.depthAttachment.texture ?
      Unwrap(state.renderPass.depthAttachment.texture)->pixelFormat() : MTL::PixelFormatInvalid);
  desc->setStencilAttachmentPixelFormat(testStencil && state.renderPass.stencilAttachment.texture ?
      Unwrap(state.renderPass.stencilAttachment.texture)->pixelFormat() : MTL::PixelFormatInvalid);
  MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(desc, &error);
  desc->release(); fragment->release(); library->release();
  if(!pipeline)
  {
    RDCWARN("Metal overlay pipeline: %s", error ? error->localizedDescription()->utf8String() : "unknown");
    return ResourceId();
  }
  bool success = true;
  const unsigned passes = overlay == DebugOverlay::Drawcall || overlay == DebugOverlay::Wireframe ? 1 : 2;
  for(unsigned p = 0; p < passes && success; p++)
  {
    const ResourceId preserve = source->GetParallelParent() ? GetResID(source->GetParallelParent()) : GetResID(source);
    if(!m_pDriver->BeginOverlayReplayPrefix(preserve)) { success = false; break; }
    // Restore each prefix, and request deferred store at pass creation. A
    // vertex shader can write storage buffers, so red must not feed green.
    ReplayLog(eventId, eReplay_WithoutDraw);
    m_OverlayNeedsFullReplay = true;
    if(!m_pDriver->GetReplayRenderCommandEncoder(source))
    {
      m_pDriver->FinishOverlayReplayPrefix(preserve);
      success = false; break;
    }
    state = SaveEncoderState();
    if(!m_pDriver->FinishOverlayReplayPrefix(preserve)) { success = false; break; }
    RestoreEncoderState(state);
    RDMTL::DepthStencilDescriptor depth;
    if(p && (testDepth || testStencil))
    {
      auto original = m_DepthStencilStates.find(state.pipeline.depthStencil.resourceId);
      if(original != m_DepthStencilStates.end()) depth = original->second;
    }
    depth.depthWriteEnabled = false;
    if(!testDepth || !p) depth.depthCompareFunction = MTL::CompareFunctionAlways;
    for(auto s : {&depth.frontFaceStencil, &depth.backFaceStencil})
    {
      if(!testStencil || !p) s->enabled = false;
      s->writeMask = 0;
      s->stencilFailureOperation = s->depthFailureOperation = s->depthStencilPassOperation = MTL::StencilOperationKeep;
    }
    MTL::DepthStencilDescriptor *nativeDepth = (MTL::DepthStencilDescriptor *)depth;
    MTL::DepthStencilState *ds = device->newDepthStencilState(nativeDepth);
    nativeDepth->release();
    MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    pass->colorAttachments()->object(0)->setTexture(texture);
    pass->colorAttachments()->object(0)->setLoadAction(p ? MTL::LoadActionLoad : MTL::LoadActionClear);
    pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(
        0,0,0,overlay == DebugOverlay::Drawcall ? .5 : 0));
    pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    if(testDepth && state.renderPass.depthAttachment.texture)
    {
      state.renderPass.depthAttachment.CopyTo(pass->depthAttachment());
      pass->depthAttachment()->setLoadAction(MTL::LoadActionLoad);
      pass->depthAttachment()->setStoreAction(MTL::StoreActionStore);
      pass->depthAttachment()->setResolveTexture(NULL);
    }
    if(testStencil && state.renderPass.stencilAttachment.texture)
    {
      state.renderPass.stencilAttachment.CopyTo(pass->stencilAttachment());
      pass->stencilAttachment()->setLoadAction(MTL::LoadActionLoad);
      pass->stencilAttachment()->setStoreAction(MTL::StoreActionStore);
      pass->stencilAttachment()->setResolveTexture(NULL);
    }
    MTL::CommandBuffer *command = m_OutputQueue->commandBuffer();
    MTL::RenderCommandEncoder *encoder = command->renderCommandEncoder(pass);
    if(!encoder || !ds)
      success = false;
    else
      success = m_pDriver->ReplayOverlayDraw(eventId, encoder, [&](MTL::RenderCommandEncoder *e) {
        e->setRenderPipelineState(pipeline);
        e->setDepthStencilState(ds);
        const float red[4] = {1,0,0,1}, green[4] = {0,1,0,1}, purple[4] = {.8f,.1f,.8f,1};
        const float wire[4] = {200.0f/255.0f,1,0,1};
        e->setFragmentBytes(overlay == DebugOverlay::Wireframe ? wire :
            passes == 1 ? purple : p ? green : red, sizeof(red), 0);
        e->setTriangleFillMode(overlay == DebugOverlay::Wireframe ? MTL::TriangleFillModeLines : MTL::TriangleFillModeFill);
        if(!p || overlay == DebugOverlay::ViewportScissor)
        {
          e->setCullMode(MTL::CullModeNone); e->setDepthClipMode(MTL::DepthClipModeClamp);
        }
        // Vulkan draws red with scissoring disabled, then green with the
        // original scissors. Keep the captured vertex shader and actual draw.
        if(overlay == DebugOverlay::ViewportScissor)
        {
          const Scissor &s = state.pipeline.rasterizer.scissor;
          MTL::ScissorRect rect = {0,0,target.width,target.height};
          if(p && s.enabled)
            rect = {(NS::UInteger)s.x, (NS::UInteger)s.y,
                    (NS::UInteger)s.width, (NS::UInteger)s.height};
          e->setScissorRect(rect);
        }
      });
    if(encoder) encoder->endEncoding();
    command->commit(); command->waitUntilCompleted();
    if(command->status() != MTL::CommandBufferStatusCompleted) success = false;
    if(ds) ds->release();
  }
  pipeline->release();
  if(success && overlay == DebugOverlay::ViewportScissor)
  {
    // Same 3px border, 16px checks, blue translucent viewport and transparent
    // scissor interior as Vulkan's CheckerboardFS. These are GPU annotation
    // draws over the real red/green coverage, not a replacement coverage mask.
    MTL::Library *rectLibrary = device->newLibrary(NS::String::string(
        "#include <metal_stdlib>\nusing namespace metal;\n"
        "vertex float4 rdoc_rect_vs(uint i [[vertex_id]]) {"
        "const float2 p[4]={float2(-1,-1),float2(1,-1),float2(-1,1),float2(1,1)};"
        "return float4(p[i],0,1); }\n"
        "fragment float4 rdoc_rect_fs(float4 p [[position]], constant float4 *c [[buffer(0)]]) {"
        "float2 r=p.xy-c[0].xy; float2 sz=c[0].zw;"
        "if(any(r<0.0f)||any(r>=sz)) discard_fragment();"
        "if(all(r>=3.0f)&&all(r<=sz-3.0f)) return c[1];"
        "if(c[2].x==0.0f) return float4(.1f,.1f,.1f,1);"
        "float2 a=fmod(r,32.0f); bool v=(a.x<16&&a.y<16)||(a.x>16&&a.y>16);"
        "return float4(float3(v?1.0f:0.0f),1); }", NS::UTF8StringEncoding), NULL, &error);
    if(!rectLibrary) return ResourceId();
    MTL::Function *vs = rectLibrary->newFunction(NS::String::string("rdoc_rect_vs", NS::UTF8StringEncoding));
    MTL::Function *fs = rectLibrary->newFunction(NS::String::string("rdoc_rect_fs", NS::UTF8StringEncoding));
    MTL::RenderPipelineDescriptor *rectDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    rectDesc->setVertexFunction(vs); rectDesc->setFragmentFunction(fs);
    auto attachment = rectDesc->colorAttachments()->object(0);
    attachment->setPixelFormat(MTL::PixelFormatRGBA16Float);
    attachment->setBlendingEnabled(true);
    attachment->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    attachment->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment->setSourceAlphaBlendFactor(MTL::BlendFactorSourceAlpha);
    attachment->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    MTL::RenderPipelineState *rectPipeline = device->newRenderPipelineState(rectDesc, &error);
    rectDesc->release(); vs->release(); fs->release(); rectLibrary->release();
    if(!rectPipeline) return ResourceId();
    MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    pass->colorAttachments()->object(0)->setTexture(texture);
    pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionLoad);
    pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    MTL::CommandBuffer *command = m_OutputQueue->commandBuffer();
    MTL::RenderCommandEncoder *encoder = command->renderCommandEncoder(pass);
    if(encoder)
    {
      encoder->setRenderPipelineState(rectPipeline);
      encoder->setViewport({0,0,(double)target.width,(double)target.height,0,1});
      const Viewport &v = state.pipeline.rasterizer.viewport;
      const Scissor &s = state.pipeline.rasterizer.scissor;
      float config[3][4] = {{0,0,(float)target.width,(float)target.height},
                           {.2f,.2f,.9f,.4f}, {0,0,0,0}};
      if(v.enabled) { config[0][0]=v.x; config[0][1]=v.y; config[0][2]=v.width; config[0][3]=v.height; }
      encoder->setFragmentBytes(config, sizeof(config), 0);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, (NS::UInteger)0, (NS::UInteger)4);
      config[0][0]=s.enabled ? (float)s.x : 0; config[0][1]=s.enabled ? (float)s.y : 0;
      config[0][2]=s.enabled ? (float)s.width : (float)target.width;
      config[0][3]=s.enabled ? (float)s.height : (float)target.height;
      config[1][0]=config[1][1]=config[1][2]=config[1][3]=0;
      config[2][0]=1;
      encoder->setFragmentBytes(config, sizeof(config), 0);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, (NS::UInteger)0, (NS::UInteger)4);
      encoder->endEncoding();
    }
    else success = false;
    command->commit(); command->waitUntilCompleted();
    success &= command->status() == MTL::CommandBufferStatusCompleted;
    rectPipeline->release();
  }
  return success ? m_OverlayTexture : ResourceId();
}

void MetalReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData)
{
  retData.clear();
  // Reject a texture or stale ID before casting the wrapped object to a buffer.
  if(GetBuffer(buff).resourceId == ResourceId())
    return;
  WrappedMTLObject *resource = m_pDriver->GetResourceManager()->GetResource(buff, true);
  WrappedMTLBuffer *buffer = (WrappedMTLBuffer *)resource;
  MTL::Buffer *real = buffer ? Unwrap(buffer) : NULL;
  if(!real || offset >= real->length())
    return;

  uint64_t available = real->length() - offset;
  if(len == 0 || len > available)
    len = available;
  if(real->storageMode() == MTL::StorageModePrivate)
  {
    if(!InitialiseOutputResources())
      return;
    MTL::Buffer *staging = Unwrap(m_pDriver)->newBuffer(len, MTL::ResourceStorageModeShared);
    if(!staging)
      return;
    MTL::CommandBuffer *command = m_OutputQueue->commandBuffer();
    MTL::BlitCommandEncoder *blit = command ? command->blitCommandEncoder() : NULL;
    if(!blit)
    {
      staging->release();
      return;
    }
    blit->copyFromBuffer(real, offset, staging, 0, len);
    blit->endEncoding();
    command->commit();
    command->waitUntilCompleted();
    if(command->status() != MTL::CommandBufferStatusError)
      retData.assign((const byte *)staging->contents(), (size_t)len);
    else
      RDCERR("Metal private buffer readback failed");
    staging->release();
    return;
  }
  if(real->storageMode() == MTL::StorageModeManaged)
  {
    if(!InitialiseOutputResources())
      return;
    MTL::CommandBuffer *command = m_OutputQueue->commandBuffer();
    MTL::BlitCommandEncoder *blit = command ? command->blitCommandEncoder() : NULL;
    if(!blit)
      return;
    blit->synchronizeResource(real);
    blit->endEncoding();
    command->commit();
    command->waitUntilCompleted();
    if(command->status() == MTL::CommandBufferStatusError)
      return;
  }
  if(!real->contents())
    return;
  retData.assign((byte *)real->contents() + offset, len);
}

void MetalReplay::GetTextureData(ResourceId tex, const Subresource &sub,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  data.clear();

  if(params.remap != RemapTexture::NoRemap || params.resolve)
  {
    RDCERR("Metal texture readback does not yet support remapping or multisample resolve");
    return;
  }

  if(GetTexture(tex).resourceId == ResourceId())
  {
    RDCERR("Metal texture readback requested for non-texture resource %s", ToStr(tex).c_str());
    return;
  }

  WrappedMTLObject *resource = m_pDriver->GetResourceManager()->GetResource(tex, true);
  WrappedMTLTexture *texture = (WrappedMTLTexture *)resource;
  MTL::Texture *real = texture ? Unwrap(texture) : NULL;
  ReadTextureSubresource(real, sub, data, tex);
}

bool MetalReplay::IsRenderOutput(ResourceId id)
{
  if(id == ResourceId()) return false;
  const MetalPipe::State &state = m_MetalPipelineState ? *m_MetalPipelineState : m_CurrentPipelineState;
  for(const Descriptor &target : state.colorTargets)
    if(target.resource == id) return true;
  return state.depthTarget.resource == id;
}

uint64_t MetalReplay::MakeOutputWindow(WindowingData window, bool depth)
{
  if(window.system != WindowingSystem::MacOS && window.system != WindowingSystem::Headless)
  {
    RDCERR("Metal replay requires a macOS CAMetalLayer or headless output window");
    return 0;
  }

  OutputWindow output;
  int32_t width = 0;
  int32_t height = 0;
  if(window.system == WindowingSystem::Headless)
  {
    width = window.headless.width;
    height = window.headless.height;
  }
  else
  {
    CA::MetalLayer *layer = (CA::MetalLayer *)window.macOS.layer;
    if(layer == NULL)
    {
      RDCERR("Metal replay macOS output window has no CAMetalLayer");
      return 0;
    }
    layer->retain();
    layer->setDevice(Unwrap(m_pDriver));
    layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer->setFramebufferOnly(true);
    output.layer = layer;

    CGRect bounds = layer->bounds();
    CGFloat scale = layer->contentsScale();
    width = (int32_t)(bounds.size.width * scale);
    height = (int32_t)(bounds.size.height * scale);
    if(width <= 0 || height <= 0)
    {
      CGSize drawableSize = layer->drawableSize();
      width = (int32_t)drawableSize.width;
      height = (int32_t)drawableSize.height;
    }
    else
    {
      layer->setDrawableSize(CGSizeMake(width, height));
    }
  }

  if(!ResizeOutputWindow(output, RDCMAX(1, width), RDCMAX(1, height)))
  {
    if(output.layer)
      output.layer->release();
    return 0;
  }

  const uint64_t id = m_NextOutputWindowID++;
  m_OutputWindows[id] = output;
  return id;
}

void MetalReplay::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end())
    return;

  if(m_ActiveOutputWindowID == id)
    m_ActiveOutputWindowID = 0;
  if(it->second.texture)
    it->second.texture->release();
  if(it->second.layer)
    it->second.layer->release();
  m_OutputWindows.erase(it);
}

bool MetalReplay::CheckResizeOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end() || it->second.layer == NULL)
    return false;

  CGRect bounds = it->second.layer->bounds();
  CGFloat scale = it->second.layer->contentsScale();
  int32_t width = (int32_t)(bounds.size.width * scale);
  int32_t height = (int32_t)(bounds.size.height * scale);
  if(width <= 0 || height <= 0)
  {
    CGSize drawableSize = it->second.layer->drawableSize();
    width = (int32_t)drawableSize.width;
    height = (int32_t)drawableSize.height;
  }
  else
  {
    it->second.layer->setDrawableSize(CGSizeMake(width, height));
  }

  width = RDCMAX(1, width);
  height = RDCMAX(1, height);
  if(width == it->second.width && height == it->second.height)
    return false;

  return ResizeOutputWindow(it->second, width, height);
}

void MetalReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end())
  {
    w = h = 0;
    return;
  }

  w = it->second.width;
  h = it->second.height;
}

void MetalReplay::GetOutputWindowData(uint64_t id, bytebuf &retData)
{
  retData.clear();

  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end() || it->second.texture == NULL || !InitialiseOutputResources())
    return;

  MTL::Device *device = Unwrap(m_pDriver);
  const uint64_t tightRowPitch = uint64_t(it->second.width) * 4;
  const uint64_t alignment = RDCMAX<uint64_t>(
      1, device->minimumLinearTextureAlignmentForPixelFormat(MTL::PixelFormatBGRA8Unorm));
  const uint64_t rowPitch = AlignUp(tightRowPitch, alignment);
  const uint64_t bufferSize = rowPitch * uint64_t(it->second.height);
  MTL::Buffer *readback = device->newBuffer((NS::UInteger)bufferSize, MTL::ResourceStorageModeShared);
  if(readback == NULL)
  {
    RDCERR("Failed to allocate Metal replay output readback buffer");
    return;
  }

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::BlitCommandEncoder *blit = commandBuffer->blitCommandEncoder();
  blit->copyFromTexture(
      it->second.texture, 0, 0, MTL::Origin::Make(0, 0, 0),
      MTL::Size::Make((NS::UInteger)it->second.width, (NS::UInteger)it->second.height, 1), readback,
      0, (NS::UInteger)rowPitch, (NS::UInteger)bufferSize);
  blit->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();

  retData.resize(size_t(it->second.width) * size_t(it->second.height) * 3);
  const byte *srcBase = (const byte *)readback->contents();
  for(int32_t y = 0; y < it->second.height; ++y)
  {
    const byte *src = srcBase + uint64_t(y) * rowPitch;
    byte *dst = retData.data() + size_t(y) * size_t(it->second.width) * 3;
    for(int32_t x = 0; x < it->second.width; ++x)
    {
      dst[x * 3 + 0] = src[x * 4 + 2];
      dst[x * 3 + 1] = src[x * 4 + 1];
      dst[x * 3 + 2] = src[x * 4 + 0];
    }
  }

  readback->release();
}

bool MetalReplay::ReadTextureSubresource(MTL::Texture *texture, const Subresource &sub,
                                         bytebuf &data, ResourceId id)
{
  data.clear();

  if(texture == NULL)
  {
    RDCERR("Cannot read back a NULL Metal texture");
    return false;
  }

  const MTL::TextureType textureType = texture->textureType();
  if(texture->pixelFormat() == MTL::PixelFormatX32_Stencil8)
  {
    // Aspect views share their base image, as in Vulkan image views. Read the
    // validated D32S8 base representation and expose its one-byte stencil plane.
    const ResourceId parent = m_pDriver->GetReplayTextureViewParent(id);
    auto object = m_pDriver->GetResourceManager()->GetResource(parent, true);
    MTL::Texture *base = object && object->m_Type == eResTexture && object->m_Real ?
        Unwrap((WrappedMTLTexture *)object) : NULL;
    if(!base || base == texture || texture->parentTexture() != base ||
       base->pixelFormat() != MTL::PixelFormatDepth32Float_Stencil8 ||
       sub.mip >= texture->mipmapLevelCount()) return false;
    const uint64_t viewSlices=textureType==MTL::TextureType2DArray?texture->arrayLength():
        textureType==MTL::TextureTypeCube || textureType==MTL::TextureTypeCubeArray?6*texture->arrayLength():1;
    if(sub.slice>=viewSlices) return false;
    Subresource parentSub=sub;
    parentSub.mip+=uint32_t(texture->parentRelativeLevel());
    parentSub.slice+=uint32_t(texture->parentRelativeSlice());
    bytebuf merged;
    if(!ReadTextureSubresource(base, parentSub, merged, parent) || merged.size() % 8) return false;
    data.resize(merged.size() / 8);
    for(size_t pixel = 0; pixel < data.size(); pixel++) data[pixel] = merged[pixel * 8 + 4];
    return true;
  }
  if(textureType == MTL::TextureTypeTextureBuffer)
  {
    // D3D12/Vulkan buffer views alias the base resource. Read the exact view range
    // through the existing buffer readback path instead of issuing a texture blit.
    ResourceId parent = GetBufferTextureSource(id);
    auto object = m_pDriver->GetResourceManager()->GetResource(parent, true);
    MTL::Buffer *buffer = object && object->m_Type == eResBuffer && object->m_Real ?
        Unwrap((WrappedMTLBuffer *)object) : NULL;
    uint32_t bw = 0, bh = 0, bytes = 0;
    if(sub.mip || sub.slice || sub.sample || !buffer || texture->buffer() != buffer ||
       texture->sampleCount() != 1 || texture->height() != 1 || texture->depth() != 1 ||
       !GetTextureDataBlockShape(texture->pixelFormat(), bw, bh, bytes) || bw != 1 || bh != 1 ||
       !texture->width() || texture->width() > (128ULL << 20) / bytes)
      return false;
    const uint64_t length = texture->width() * bytes, offset = texture->bufferOffset();
    if(offset > buffer->length() || length > buffer->length() - offset) return false;
    GetBufferData(parent, offset, length, data);
    return data.size() == length;
  }
  const bool volume = textureType == MTL::TextureType3D;
  const bool supportedType = volume || textureType == MTL::TextureType2D ||
                             textureType == MTL::TextureType2DArray ||
                             textureType == MTL::TextureTypeCube ||
                             textureType == MTL::TextureTypeCubeArray;
  const uint32_t sliceCount = (uint32_t)texture->arrayLength() *
      ((textureType == MTL::TextureTypeCube || textureType == MTL::TextureTypeCubeArray) ? 6U : 1U);
  if(!supportedType || texture->sampleCount() != 1 || sub.sample != 0 ||
     (!volume && sub.slice >= sliceCount) || sub.mip >= texture->mipmapLevelCount())
  {
    RDCERR("Metal texture readback invalid type, sample, slice, or mip");
    return false;
  }
  uint32_t blockWidth = 1, blockHeight = 1, blockBytes = 4;
  const bool depthStencil = texture->pixelFormat() == MTL::PixelFormatDepth32Float_Stencil8;
  if(!depthStencil && !GetTextureDataBlockShape(texture->pixelFormat(), blockWidth, blockHeight, blockBytes))
  {
    RDCERR("Metal texture readback does not support format %s", ToStr(texture->pixelFormat()).c_str());
    return false;
  }
  if(!InitialiseOutputResources())
    return false;

  MTL::Device *device = Unwrap(m_pDriver);
  const uint64_t width = RDCMAX(1ULL, uint64_t(texture->width()) >> sub.mip);
  const uint64_t height = RDCMAX(1ULL, uint64_t(texture->height()) >> sub.mip);
  const uint64_t depth = volume ? RDCMAX(1ULL, uint64_t(texture->depth()) >> sub.mip) : 1;
  const bool compressed = blockWidth > 1 || blockHeight > 1;
  const bool depthFormat = depthStencil || texture->pixelFormat() == MTL::PixelFormatDepth16Unorm ||
                           texture->pixelFormat() == MTL::PixelFormatDepth32Float;
  const uint64_t rows = (height + blockHeight - 1) / blockHeight;
  const uint64_t tightRowPitch = ((width + blockWidth - 1) / blockWidth) * blockBytes;
  const uint64_t alignment = depthFormat ? 256 : compressed ? 1 : RDCMAX<uint64_t>(
      1, device->minimumLinearTextureAlignmentForPixelFormat(texture->pixelFormat()));
  const uint64_t rowPitch = AlignUp(tightRowPitch, alignment);
  const uint64_t depthSize = rowPitch * rows * depth;
  const uint64_t stencilRowPitch = depthStencil ? AlignUp(width, alignment) : 0;
  const uint64_t bufferSize = depthSize + stencilRowPitch * height;
  if(width > 8192 || height > 8192 || depth > 256 || bufferSize > 128ULL * 1024 * 1024)
  {
    RDCERR("Metal texture readback exceeds the validated subresource budget");
    return false;
  }

  MTL::Buffer *readback =
      device->newBuffer((NS::UInteger)bufferSize, MTL::ResourceStorageModeShared);
  if(readback == NULL)
  {
    RDCERR("Failed to allocate %llu bytes for Metal texture readback",
           (unsigned long long)bufferSize);
    return false;
  }

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::BlitCommandEncoder *blit = commandBuffer ? commandBuffer->blitCommandEncoder() : NULL;
  if(commandBuffer == NULL || blit == NULL)
  {
    RDCERR("Failed to create Metal texture readback command buffer");
    readback->release();
    return false;
  }

  blit->copyFromTexture(texture, volume ? 0 : sub.slice, sub.mip, MTL::Origin::Make(0, 0, 0),
                        MTL::Size::Make((NS::UInteger)width, (NS::UInteger)height, depth), readback, 0,
                        (NS::UInteger)rowPitch, (NS::UInteger)(rowPitch * rows),
                        depthStencil ? MTL::BlitOptionDepthFromDepthStencil : MTL::BlitOptionNone);
  if(depthStencil)
    blit->copyFromTexture(texture, sub.slice, sub.mip, MTL::Origin::Make(0, 0, 0),
                          MTL::Size::Make((NS::UInteger)width, (NS::UInteger)height, 1), readback,
                          (NS::UInteger)depthSize, (NS::UInteger)stencilRowPitch,
                          (NS::UInteger)(stencilRowPitch * height),
                          MTL::BlitOptionStencilFromDepthStencil);
  blit->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();

  if(commandBuffer->status() == MTL::CommandBufferStatusError)
  {
    NS::Error *error = commandBuffer->error();
    RDCERR("Metal texture readback failed: %s",
           error ? error->localizedDescription()->utf8String() : "unknown error");
    readback->release();
    return false;
  }

  // RenderDoc's D32S8 representation is eight bytes: float depth, uint8 stencil, three padding.
  const uint64_t outputRowPitch = depthStencil ? width * 8 : tightRowPitch;
  data.resize(size_t(outputRowPitch * rows * depth));
  const byte *src = (const byte *)readback->contents();
  for(uint64_t z = 0; z < depth; z++)
  for(uint64_t y = 0; y < rows; y++)
  {
    if(depthStencil)
    {
      byte *dst = data.data() + size_t(y * outputRowPitch);
      memset(dst, 0, size_t(outputRowPitch));
      for(uint64_t x = 0; x < width; x++)
      {
        memcpy(dst + x * 8, src + y * rowPitch + x * 4, 4);
        dst[x * 8 + 4] = src[depthSize + y * stencilRowPitch + x];
      }
    }
    else
      memcpy(data.data() + size_t((z * rows + y) * tightRowPitch),
             src + (z * rows + y) * rowPitch, size_t(tightRowPitch));
  }

  readback->release();
  return true;
}

void MetalReplay::ClearOutputWindowColor(uint64_t id, FloatVector col)
{
  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end() || it->second.texture == NULL || !InitialiseOutputResources())
    return;

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
  colour->setTexture(it->second.texture);
  colour->setLoadAction(MTL::LoadActionClear);
  colour->setStoreAction(MTL::StoreActionStore);
  // Output bytes are UNorm while the common output API supplies linear colours.
  const FloatVector encoded(ConvertLinearToSRGB(col.x), ConvertLinearToSRGB(col.y),
                            ConvertLinearToSRGB(col.z), col.w);
  colour->setClearColor(MTL::ClearColor::Make(encoded.x, encoded.y, encoded.z, col.w));

  MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
  encoder->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();
}

void MetalReplay::BindOutputWindow(uint64_t id, bool depth)
{
  m_ActiveOutputWindowID = m_OutputWindows.find(id) != m_OutputWindows.end() ? id : 0;
}

bool MetalReplay::IsOutputWindowVisible(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  return it != m_OutputWindows.end() && it->second.layer != NULL && it->second.width > 0 &&
         it->second.height > 0;
}

void MetalReplay::FlipOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end() || it->second.layer == NULL || it->second.texture == NULL)
    return;

  CA::MetalDrawable *drawable = it->second.layer->nextDrawable();
  if(drawable == NULL)
    return;

  TextureDisplay display;
  display.scale = -1.0f;
  display.red = display.green = display.blue = display.alpha = true;
  display.rangeMin = 0.0f;
  display.rangeMax = 1.0f;
  display.rawOutput = true;

  if(!RenderTextureInternal(it->second.texture, drawable->texture(), display, MTL::LoadActionClear,
                            MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0)))
    return;

  MTL::CommandBuffer *present = m_OutputQueue->commandBuffer();
  present->presentDrawable(drawable);
  present->commit();
  present->waitUntilCompleted();
}

bool MetalReplay::InitialiseOutputResources()
{
  if(m_OutputPipeline && m_RawOutputPipeline && m_BackgroundPipeline && m_MeshPipeline && m_OutputQueue)
    return true;

  MTL::Device *device = Unwrap(m_pDriver);
  if(device == NULL)
    return false;

  if(m_OutputQueue == NULL)
    m_OutputQueue = device->newCommandQueue();
  if(m_OutputQueue == NULL)
  {
    RDCERR("Failed to create Metal replay output command queue");
    return false;
  }

  static const char *source = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

struct DisplayParams
{
  float2 outputSize;
  float2 textureSize;
  float2 offset;
  float scale;
  uint mip;
  uint channelMask;
  uint flipY;
  uint rawOutput;
  float rangeMin;
  float inverseRange;
  uint slice;
  uint textureType;
  uint textureDepth;
  uint overlay;
  uint encodeSRGB;
  float hdrMultiplier;
  uint decodeYUV;
};

struct VSOut
{
  float4 position [[position]];
  float2 uv;
};

vertex VSOut rdoc_display_vs(uint vertexID [[vertex_id]], constant DisplayParams &params [[buffer(0)]])
{
  const float2 corners[4] = {float2(0.0, 0.0), float2(0.0, 1.0),
                             float2(1.0, 0.0), float2(1.0, 1.0)};
  float2 corner = corners[vertexID];
  float2 pixel = params.offset + corner * params.textureSize * params.scale;

  VSOut output;
  output.position = float4(pixel.x * 2.0 / params.outputSize.x - 1.0,
                           1.0 - pixel.y * 2.0 / params.outputSize.y, 0.0, 1.0);
  output.uv = corner;
  if(params.flipY != 0)
    output.uv.y = 1.0 - output.uv.y;
  return output;
}

fragment float4 rdoc_display_fs(VSOut input [[stage_in]], texture2d<float> source2d [[texture(0)]],
                                texture2d_array<float> sourceArray [[texture(1)]],
                                texturecube<float> sourceCube [[texture(2)]],
                                texture3d<float> sourceVolume [[texture(3)]],
                                texturecube_array<float> sourceCubeArray [[texture(4)]],
                                constant DisplayParams &params [[buffer(0)]],
                                constant float4 *heatmapRamp [[buffer(1)]])
{
  constexpr sampler nearestSampler(coord::normalized, address::clamp_to_edge, filter::nearest,
                                   mip_filter::nearest);
  float4 value;
  if(params.textureType == 1)
  {
    value = sourceArray.sample(nearestSampler, input.uv, params.slice, level(params.mip));
  }
  else if(params.textureType == 2 || params.textureType == 4)
  {
    float2 p = input.uv * 2.0 - 1.0;
    const float3 directions[6] = {
        float3(1.0, -p.y, -p.x), float3(-1.0, -p.y, p.x),
        float3(p.x, 1.0, p.y), float3(p.x, -1.0, -p.y),
        float3(p.x, -p.y, 1.0), float3(-p.x, -p.y, -1.0),
    };
    value = params.textureType == 2 ?
        sourceCube.sample(nearestSampler, directions[params.slice % 6], level(params.mip)) :
        sourceCubeArray.sample(nearestSampler, directions[params.slice % 6], params.slice / 6,
                               level(params.mip));
  }
  else if(params.textureType == 3)
  {
    value = sourceVolume.sample(nearestSampler,
        float3(input.uv, (float(params.slice) + 0.5) / float(params.textureDepth)), level(params.mip));
  }
  else
  {
    value = source2d.sample(nearestSampler, input.uv, level(params.mip));
  }
  if(params.rawOutput != 0)
    return value;

  // Vulkan's linear heatmap buckets: raw output above keeps actual counts.
  if(params.overlay == 3)
  {
    int bucket = clamp(int(floor(value.x + 0.25f)), 0, 21);
    if(bucket == 0) discard_fragment();
    float4 colour = heatmapRamp[bucket];
    // Preserve shared ramp UI colours through the sRGB attachment.
    colour.rgb = select(colour.rgb / 12.92,
        pow((max(colour.rgb, 0.0) + 0.055) / 1.055, float3(2.4)), colour.rgb > 0.04045);
    return colour;
  }

  if(params.overlay == 4)
  {
    if(value.a < 0.5f) discard_fragment();
    float area = max(value.x, 0.001f);
    int bucket = clamp(2 + int(floor(20.0f - 20.1f * (1.0f - exp(-0.4f * area)))), 0, 21);
    float4 colour = heatmapRamp[bucket];
    colour.rgb = select(colour.rgb / 12.92,
        pow((max(colour.rgb, 0.0) + 0.055) / 1.055, float3(2.4)), colour.rgb > 0.04045);
    return colour;
  }

  // Port the shared HLSL/GLSL texture-display order and overlay colours. Do not
  // saturate before clipping tests, or out-of-range values become invisible.
  if(params.decodeYUV != 0)
  {
    float L = value.g, Pb = value.b - 0.5, Pr = value.r - 0.5;
    value.b = L + Pb * 2.0 * (1.0 - 0.0722);
    value.r = L + Pr * 2.0 * (1.0 - 0.2126);
    value.g = (L - 0.2126 * value.r - 0.0722 * value.b) / (1.0 - 0.2126 - 0.0722);
  }
  if(params.hdrMultiplier > 0.0)
    value = float4(value.rgb * value.a * params.hdrMultiplier, 1.0);
  float4 original = value;
  value = (value - params.rangeMin) * params.inverseRange;

  bool r = (params.channelMask & 1) != 0;
  bool g = (params.channelMask & 2) != 0;
  bool b = (params.channelMask & 4) != 0;
  bool a = (params.channelMask & 8) != 0;
  uint rgbCount = uint(r) + uint(g) + uint(b);
  original.rgb = select(float3(0.0), original.rgb, bool3(r, g, b));
  original.a = a ? original.a : 1.0;
  value.rgb = select(float3(0.0), value.rgb, bool3(r, g, b));
  value.a = a ? value.a : 1.0;
  if(params.overlay == 1)
  {
    if(any(isnan(original))) return float4(1, 0, 0, 1);
    if(any(isinf(original))) return float4(0, 1, 0, 1);
    if(any(original < 0.0)) return float4(0, 0, 1, 1);
    value = float4(float3(dot(value.rgb, float3(0.2126, 0.7152, 0.0722))), 1.0);
  }
  else if(params.overlay == 2)
  {
    if(any(value < 0.0)) return float4(1, 0, 0, 1);
    if(any(value > (1.0 + 1.192092896e-7))) return float4(0, 1, 0, 1);
    value = float4(float3(dot(value.rgb, float3(0.2126, 0.7152, 0.0722))), 1.0);
  }
  else if(rgbCount == 0 && a)
    value = float4(value.aaa, 1.0);
  else if(rgbCount == 1 && !a)
  {
    float selected = r ? value.r : (g ? value.g : value.b);
    value = float4(selected, selected, selected, 1.0);
  }
  // Display through an sRGB target view, matching D3D12/Vulkan: blending is
  // linear and the attachment encodes the result. Already-gamma input is first
  // decoded so the display preserves its stored values.
  if(params.encodeSRGB == 0)
    value.rgb = select(value.rgb / 12.92,
                       pow((max(value.rgb, 0.0) + 0.055) / 1.055, float3(2.4)),
                       value.rgb > 0.04045);
  return value;
}

struct BackgroundParams
{
  float4 dark;
  float4 light;
  float2 topLeft;
  float size;
  uint highlight;
};

vertex float4 rdoc_background_vs(uint id [[vertex_id]])
{
  const float2 p[3] = {float2(-1, -1), float2(-1, 3), float2(3, -1)};
  return float4(p[id], 0, 1);
}

fragment float4 rdoc_background_fs(float4 position [[position]],
                                   constant BackgroundParams &params [[buffer(0)]])
{
  if(params.highlight == 0)
  {
    uint2 tile = uint2(position.xy) / 64;
    return ((tile.x + tile.y) & 1) ? params.light : params.dark;
  }
  int2 p = int2(position.xy) - int2(params.topLeft);
  int size = int(params.size);
  if(p.x < -1 || p.y < -1 || p.x > size + 1 || p.y > size + 1) discard_fragment();
  if(p.x == -1 || p.y == -1 || p.x == size + 1 || p.y == size + 1)
    return float4(0, 0, 0, 1);
  if(p.x == 0 || p.y == 0 || p.x == size || p.y == size)
    return float4(1, 1, 1, 1);
  discard_fragment();
  return float4(0);
}

struct MeshParams
{
  float4x4 mvp;
  uint stride;
  uint componentCount;
  uint2 padding;
  float4 color;
};

struct MeshVSOut
{
  float4 position [[position]];
  float pointSize [[point_size]];
};

vertex MeshVSOut rdoc_mesh_vs(device const uchar *vertices [[buffer(0)]],
                          constant MeshParams &params [[buffer(1)]],
                          uint vertexID [[vertex_id]])
{
  device const float *position =
      reinterpret_cast<device const float *>(vertices + ulong(vertexID) * params.stride);
  float4 value = float4(position[0], position[1], 0.0, 1.0);
  if(params.componentCount >= 3)
    value.z = position[2];
  if(params.componentCount >= 4)
    value.w = position[3];

  MeshVSOut output;
  output.position = params.mvp * value;
  output.pointSize = 9.0;
  return output;
}

fragment float4 rdoc_mesh_fs(MeshVSOut input [[stage_in]],
                             constant MeshParams &params [[buffer(1)]])
{
  return params.color;
}
)EOSHADER";

  NS::Error *error = NULL;
  MTL::CompileOptions *displayOptions = MTL::CompileOptions::alloc()->init();
  // NaN/Inf inspection requires IEEE values to survive compiler optimisation.
  displayOptions->setFastMathEnabled(false);
  MTL::Library *library =
      device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), displayOptions, &error);
  displayOptions->release();
  if(library == NULL)
  {
    RDCERR("Failed to compile Metal replay output shaders: %s",
           error ? error->localizedDescription()->utf8String() : "unknown error");
    return false;
  }

  MTL::Function *vertex =
      library->newFunction(NS::String::string("rdoc_display_vs", NS::UTF8StringEncoding));
  MTL::Function *fragment =
      library->newFunction(NS::String::string("rdoc_display_fs", NS::UTF8StringEncoding));
  MTL::RenderPipelineDescriptor *descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
  descriptor->setVertexFunction(vertex);
  descriptor->setFragmentFunction(fragment);
  MTL::RenderPipelineColorAttachmentDescriptor *colour = descriptor->colorAttachments()->object(0);
  colour->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  colour->setBlendingEnabled(true);
  colour->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
  colour->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
  colour->setRgbBlendOperation(MTL::BlendOperationAdd);
  colour->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
  colour->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
  colour->setAlphaBlendOperation(MTL::BlendOperationAdd);

  if(m_RawOutputPipeline == NULL)
    m_RawOutputPipeline = device->newRenderPipelineState(descriptor, &error);
  colour->setPixelFormat(MTL::PixelFormatBGRA8Unorm_sRGB);
  if(m_OutputPipeline == NULL)
    m_OutputPipeline = device->newRenderPipelineState(descriptor, &error);
  descriptor->release();

  MTL::Function *backgroundVertex =
      library->newFunction(NS::String::string("rdoc_background_vs", NS::UTF8StringEncoding));
  MTL::Function *backgroundFragment =
      library->newFunction(NS::String::string("rdoc_background_fs", NS::UTF8StringEncoding));
  MTL::RenderPipelineDescriptor *backgroundDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
  backgroundDescriptor->setVertexFunction(backgroundVertex);
  backgroundDescriptor->setFragmentFunction(backgroundFragment);
  backgroundDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  if(m_BackgroundPipeline == NULL)
    m_BackgroundPipeline = device->newRenderPipelineState(backgroundDescriptor, &error);
  backgroundDescriptor->release();
  if(backgroundVertex) backgroundVertex->release();
  if(backgroundFragment) backgroundFragment->release();

  MTL::Function *meshVertex =
      library->newFunction(NS::String::string("rdoc_mesh_vs", NS::UTF8StringEncoding));
  MTL::Function *meshFragment =
      library->newFunction(NS::String::string("rdoc_mesh_fs", NS::UTF8StringEncoding));
  MTL::RenderPipelineDescriptor *meshDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
  meshDescriptor->setVertexFunction(meshVertex);
  meshDescriptor->setFragmentFunction(meshFragment);
  meshDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  if(m_MeshPipeline == NULL)
    m_MeshPipeline = device->newRenderPipelineState(meshDescriptor, &error);
  meshDescriptor->release();
  if(meshFragment)
    meshFragment->release();
  if(meshVertex)
    meshVertex->release();
  if(fragment)
    fragment->release();
  if(vertex)
    vertex->release();
  library->release();

  if(m_OutputPipeline == NULL || m_RawOutputPipeline == NULL)
  {
    RDCERR("Failed to create Metal replay output pipeline: %s",
           error ? error->localizedDescription()->utf8String() : "unknown error");
    return false;
  }
  if(m_BackgroundPipeline == NULL)
  {
    RDCERR("Failed to create Metal replay background pipeline");
    return false;
  }
  if(m_MeshPipeline == NULL)
  {
    RDCERR("Failed to create Metal replay mesh pipeline: %s",
           error ? error->localizedDescription()->utf8String() : "unknown error");
    return false;
  }

  return true;
}

bool MetalReplay::ResizeOutputWindow(OutputWindow &output, int32_t width, int32_t height)
{
  MTL::Device *device = Unwrap(m_pDriver);
  if(device == NULL || width <= 0 || height <= 0)
    return false;

  MTL::TextureDescriptor *descriptor = MTL::TextureDescriptor::texture2DDescriptor(
      MTL::PixelFormatBGRA8Unorm, (NS::UInteger)width, (NS::UInteger)height, false);
  descriptor->setStorageMode(MTL::StorageModePrivate);
  descriptor->setUsage(
      (MTL::TextureUsage)(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead |
                         MTL::TextureUsagePixelFormatView));
  MTL::Texture *texture = device->newTexture(descriptor);
  if(texture == NULL)
  {
    RDCERR("Failed to create %dx%d Metal replay output texture", width, height);
    return false;
  }

  if(output.texture)
    output.texture->release();
  output.texture = texture;
  output.width = width;
  output.height = height;
  return true;
}

bool MetalReplay::RenderTextureInternal(MTL::Texture *source, MTL::Texture *target,
                                        TextureDisplay cfg, MTL::LoadAction loadAction,
                                        MTL::ClearColor clearColor, uint32_t displayWidth,
                                        uint32_t displayHeight)
{
  if(source == NULL || target == NULL || !InitialiseOutputResources())
    return false;

  const MTL::TextureType textureType = source->textureType();
  const bool supportedType = textureType == MTL::TextureType2D ||
                             textureType == MTL::TextureType2DArray ||
                             textureType == MTL::TextureTypeCube || textureType == MTL::TextureType3D ||
                             textureType == MTL::TextureTypeCubeArray;
  if(cfg.subresource.mip >= source->mipmapLevelCount()) return false;
  const uint32_t sliceCount = textureType == MTL::TextureTypeCube || textureType == MTL::TextureTypeCubeArray
                                  ? uint32_t(source->arrayLength()) * 6U
                                  : textureType == MTL::TextureType3D ?
                                    uint32_t(RDCMAX(1ULL,source->depth() >> cfg.subresource.mip)) :
                                    (uint32_t)source->arrayLength();
  if(!supportedType || source->sampleCount() != 1 ||
     (cfg.subresource.sample != 0 && cfg.subresource.sample != ~0U) ||
     cfg.subresource.slice >= sliceCount ||
     cfg.subresource.mip >= source->mipmapLevelCount())
  {
    RDCERR("Metal texture display does not support type %s or subresource mip %u slice %u",
           ToStr(textureType).c_str(), cfg.subresource.mip, cfg.subresource.slice);
    return false;
  }

  struct DisplayParams
  {
    float outputSize[2];
    float textureSize[2];
    float offset[2];
    float scale;
    uint32_t mip;
    uint32_t channelMask;
    uint32_t flipY;
    uint32_t rawOutput;
    float rangeMin;
    float inverseRange;
    uint32_t slice;
    uint32_t textureType;
    // Match the MSL struct's 8-byte alignment, including display controls.
    uint32_t textureDepth;
    uint32_t overlay;
    uint32_t encodeSRGB;
    float hdrMultiplier;
    uint32_t decodeYUV;
  } params = {};
  RDCCOMPILE_ASSERT(sizeof(DisplayParams) == 80, "Metal display uniform layout must match MSL");

  const uint32_t mip = cfg.subresource.mip;
  // Shared Texture Viewer scale/pan and pixel context use mip0 dimensions.
  // Like D3D12/Vulkan, selecting a mip changes sampling, not the display extent.
  const float textureWidth = displayWidth ? float(displayWidth) : float(source->width());
  const float textureHeight = displayHeight ? float(displayHeight) : float(source->height());
  params.outputSize[0] = (float)target->width();
  params.outputSize[1] = (float)target->height();
  params.textureSize[0] = textureWidth;
  params.textureSize[1] = textureHeight;
  params.scale = cfg.scale;
  params.offset[0] = cfg.xOffset;
  params.offset[1] = cfg.yOffset;
  if(params.scale <= 0.0f)
  {
    params.scale = RDCMIN(params.outputSize[0] / textureWidth, params.outputSize[1] / textureHeight);
    params.offset[0] = (params.outputSize[0] - textureWidth * params.scale) * 0.5f;
    params.offset[1] = (params.outputSize[1] - textureHeight * params.scale) * 0.5f;
  }
  params.mip = mip;
  params.channelMask =
      (cfg.red ? 1U : 0U) | (cfg.green ? 2U : 0U) | (cfg.blue ? 4U : 0U) | (cfg.alpha ? 8U : 0U);
  params.flipY = cfg.flipY ? 1U : 0U;
  params.rawOutput = cfg.rawOutput ? 1U : 0U;
  params.rangeMin = cfg.rangeMin;
  params.inverseRange = 1.0f / (cfg.rangeMax - cfg.rangeMin);
  if(!std::isfinite(params.inverseRange)) params.inverseRange = FLT_MAX;
  params.slice = cfg.subresource.slice;
  params.textureType = textureType == MTL::TextureType2DArray ? 1U
                       : textureType == MTL::TextureTypeCube   ? 2U
                       : textureType == MTL::TextureType3D     ? 3U
                       : textureType == MTL::TextureTypeCubeArray ? 4U
                                                              : 0U;
  params.textureDepth = textureType == MTL::TextureType3D ? sliceCount : 1U;
  params.overlay = cfg.overlay == DebugOverlay::NaN ? 1U :
                   cfg.overlay == DebugOverlay::Clipping ? 2U :
                   (cfg.overlay == DebugOverlay::QuadOverdrawDraw ||
                    cfg.overlay == DebugOverlay::QuadOverdrawPass) ? 3U :
                   (cfg.overlay == DebugOverlay::TriangleSizeDraw ||
                    cfg.overlay == DebugOverlay::TriangleSizePass) ? 4U : 0U;
  params.encodeSRGB = MakeResourceFormat(source->pixelFormat()).SRGBCorrected() ||
                      cfg.typeCast == CompType::UNormSRGB || !cfg.linearDisplayAsGamma;
  params.hdrMultiplier = cfg.hdrMultiplier;
  params.decodeYUV = cfg.decodeYUV ? 1U : 0U;

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
  // Keep the stored/output bytes and raw presentation path UNorm. Only the
  // Texture Viewer uses an sRGB view for correct linear alpha compositing.
  MTL::Texture *displayTarget = cfg.rawOutput ? NULL :
      target->newTextureView(MTL::PixelFormatBGRA8Unorm_sRGB);
  if(!cfg.rawOutput && !displayTarget) return false;
  colour->setTexture(displayTarget ? displayTarget : target);
  colour->setLoadAction(loadAction);
  colour->setStoreAction(MTL::StoreActionStore);
  colour->setClearColor(clearColor);

  MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
  encoder->setRenderPipelineState(cfg.rawOutput ? m_RawOutputPipeline : m_OutputPipeline);
  encoder->setVertexBytes(&params, sizeof(params), 0);
  encoder->setFragmentBytes(&params, sizeof(params), 0);
  encoder->setFragmentBytes(colorRamp, sizeof(colorRamp), 1);
  encoder->setFragmentTexture(source, params.textureType);
  encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
  encoder->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();
  if(displayTarget) displayTarget->release();
  return commandBuffer->status() == MTL::CommandBufferStatusCompleted;
}

bool MetalReplay::RenderTexture(TextureDisplay cfg)
{
  auto output = m_OutputWindows.find(m_ActiveOutputWindowID);
  if(output == m_OutputWindows.end() || output->second.texture == NULL)
    return false;

  TextureDescription desc = GetTexture(cfg.resourceId);
  if(desc.resourceId == ResourceId())
    return false;

  WrappedMTLObject *resource = m_pDriver->GetResourceManager()->GetResource(cfg.resourceId, true);
  WrappedMTLTexture *wrappedTexture = (WrappedMTLTexture *)resource;
  MTL::Texture *texture = wrappedTexture ? Unwrap(wrappedTexture) : NULL;
  if(!texture) return false;
  ResourceFormat format = MakeResourceFormat(texture->pixelFormat());
  // Integer/depth/buffer views cannot be sampled through texture2d<float> on
  // Metal. Decode their exact Native readback using the same shared conversion
  // as pixel picking, then display a temporary float image. Capture resources
  // and their bytes remain untouched; the image lives only until GPU completion.
  if(format.compType == CompType::UInt || format.compType == CompType::SInt ||
     format.compType == CompType::Depth || format.type == ResourceFormatType::D32S8 ||
     texture->textureType() == MTL::TextureTypeTextureBuffer ||
     (cfg.typeCast != CompType::Typeless && cfg.typeCast != format.compType))
  {
    bytebuf data;
    uint32_t stride;
    size_t offset, count;
    if(!ReadTextureInspection(cfg.resourceId, cfg.subresource, cfg.typeCast, data, format,
                              stride, offset, count) || count > (128U << 20) / sizeof(FloatVector))
      return false;
    rdcarray<FloatVector> pixels;
    pixels.resize(count);
    for(size_t i = 0; i < count; i++)
    {
      bool decoded = false;
      pixels[i] = DecodeFormattedComponents(format, data.data() + offset + i * stride, &decoded);
      if(!decoded) return false;
    }
    const NS::UInteger width = RDCMAX(1ULL, texture->width() >> cfg.subresource.mip);
    const NS::UInteger height = RDCMAX(1ULL, texture->height() >> cfg.subresource.mip);
    auto descriptor = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA32Float,
                                                                 width, height, false);
    descriptor->setStorageMode(MTL::StorageModeShared);
    descriptor->setUsage(MTL::TextureUsageShaderRead);
    auto image = Unwrap(m_pDriver)->newTexture(descriptor);
    if(!image) return false;
    image->replaceRegion(MTL::Region::Make2D(0, 0, width, height), 0, pixels.data(),
                          width * sizeof(FloatVector));
    cfg.subresource = {0, 0, 0};
    const bool result = RenderTextureInternal(image, output->second.texture, cfg, MTL::LoadActionLoad,
                                               MTL::ClearColor::Make(0, 0, 0, 0),
                                               uint32_t(texture->width()), uint32_t(texture->height()));
    image->release();
    return result;
  }
  return RenderTextureInternal(texture, output->second.texture, cfg, MTL::LoadActionLoad,
                               MTL::ClearColor::Make(0.0, 0.0, 0.0, 0.0));
}

void MetalReplay::RenderMesh(uint32_t eventId, const rdcarray<MeshFormat> &secondaryDraws,
                             const MeshDisplay &cfg)
{
  auto output = m_OutputWindows.find(m_ActiveOutputWindowID);
  if(output == m_OutputWindows.end() || output->second.texture == NULL ||
     !InitialiseOutputResources())
    return;

  const MeshFormat &position = cfg.position;
  if(cfg.type != MeshDataStage::VSIn || position.vertexResourceId == ResourceId() ||
     position.numIndices == 0)
    return;
  if(position.format.type != ResourceFormatType::Regular ||
     position.format.compType != CompType::Float || position.format.compByteWidth != 4 ||
     position.format.compCount < 2 || position.format.compCount > 4)
  {
    RDCERR("Metal mesh preview currently supports only Float2/Float3/Float4 VS input positions");
    return;
  }

  MTL::PrimitiveType primitive = MTL::PrimitiveTypeTriangle;
  switch(position.topology)
  {
    case Topology::PointList: primitive = MTL::PrimitiveTypePoint; break;
    case Topology::LineList: primitive = MTL::PrimitiveTypeLine; break;
    case Topology::LineStrip: primitive = MTL::PrimitiveTypeLineStrip; break;
    case Topology::TriangleList: primitive = MTL::PrimitiveTypeTriangle; break;
    case Topology::TriangleStrip: primitive = MTL::PrimitiveTypeTriangleStrip; break;
    default:
      RDCERR("Metal mesh preview does not support topology %s", ToStr(position.topology).c_str());
      return;
  }

  if(GetBuffer(position.vertexResourceId).resourceId == ResourceId())
    return;
  WrappedMTLObject *vertexResource =
      m_pDriver->GetResourceManager()->GetResource(position.vertexResourceId, true);
  MTL::Buffer *vertexBuffer = vertexResource ? Unwrap((WrappedMTLBuffer *)vertexResource) : NULL;
  if(vertexBuffer == NULL || position.vertexByteOffset >= vertexBuffer->length())
    return;

  MTL::Buffer *indexBuffer = NULL;
  if(position.indexResourceId != ResourceId())
  {
    if(GetBuffer(position.indexResourceId).resourceId == ResourceId())
      return;
    WrappedMTLObject *indexResource =
        m_pDriver->GetResourceManager()->GetResource(position.indexResourceId, true);
    indexBuffer = indexResource ? Unwrap((WrappedMTLBuffer *)indexResource) : NULL;
    if(indexBuffer == NULL || position.indexByteOffset >= indexBuffer->length() ||
       (position.indexByteStride != 2 && position.indexByteStride != 4))
      return;
  }

  Matrix4f mvp = Matrix4f::Identity();
  if(cfg.cam)
  {
    Camera *camera = (Camera *)cfg.cam;
    const float aspect = output->second.height > 0
                             ? float(output->second.width) / float(output->second.height)
                             : 1.0f;
    mvp = Matrix4f::Perspective(cfg.fov, camera->GetNear(), camera->GetFar(), aspect)
              .Mul(camera->GetMatrix());
    if(!position.unproject)
      mvp = mvp.Mul(Matrix4f(cfg.axisMapping));
  }

  struct MeshParams
  {
    float mvp[16];
    uint32_t stride;
    uint32_t componentCount;
    uint32_t padding[2];
    float color[4];
  } params = {};
  memcpy(params.mvp, mvp.Data(), sizeof(params.mvp));
  params.stride = position.vertexByteStride ? position.vertexByteStride
                                             : position.format.compByteWidth *
                                                   position.format.compCount;
  params.componentCount = position.format.compCount;
  params.color[0] = position.meshColor.x;
  params.color[1] = position.meshColor.y;
  params.color[2] = position.meshColor.z;
  params.color[3] = position.meshColor.w > 0.0f ? position.meshColor.w : 1.0f;
  if(params.color[0] == 0.0f && params.color[1] == 0.0f && params.color[2] == 0.0f)
  {
    params.color[0] = 0.95f;
    params.color[1] = 0.75f;
    params.color[2] = 0.15f;
  }

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
  colour->setTexture(output->second.texture);
  colour->setLoadAction(MTL::LoadActionLoad);
  colour->setStoreAction(MTL::StoreActionStore);

  MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
  encoder->setRenderPipelineState(m_MeshPipeline);
  encoder->setViewport(MTL::Viewport{0.0, 0.0, (double)output->second.width,
                                     (double)output->second.height, 0.0, 1.0});
  encoder->setCullMode(MTL::CullModeNone);
  if(cfg.wireframeDraw &&
     (primitive == MTL::PrimitiveTypeTriangle || primitive == MTL::PrimitiveTypeTriangleStrip))
    encoder->setTriangleFillMode(MTL::TriangleFillModeLines);
  encoder->setVertexBuffer(vertexBuffer, (NS::UInteger)position.vertexByteOffset, 0);
  encoder->setVertexBytes(&params, sizeof(params), 1);
  encoder->setFragmentBytes(&params, sizeof(params), 1);

  if(indexBuffer)
  {
    const MTL::IndexType indexType = position.indexByteStride == 2 ? MTL::IndexTypeUInt16
                                                                   : MTL::IndexTypeUInt32;
    encoder->drawIndexedPrimitives(primitive, position.numIndices, indexType, indexBuffer,
                                   (NS::UInteger)position.indexByteOffset, 1,
                                   (NS::Integer)position.baseVertex, 0);
  }
  else
  {
    encoder->drawPrimitives(primitive, NS::UInteger(0), NS::UInteger(position.numIndices));
  }

  encoder->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();
}

void MetalReplay::RenderOutputBackground(FloatVector dark, FloatVector light, float w, float h,
                                         float scale)
{
  auto output = m_OutputWindows.find(m_ActiveOutputWindowID);
  if(output == m_OutputWindows.end() || !output->second.texture || !InitialiseOutputResources())
    return;
  struct BackgroundParams
  {
    float dark[4], light[4], topLeft[2], size;
    uint32_t highlight;
  } params = {};
  RDCCOMPILE_ASSERT(sizeof(BackgroundParams) == 48, "Metal background uniform layout");
  memcpy(params.dark, &dark.x, sizeof(params.dark));
  memcpy(params.light, &light.x, sizeof(params.light));
  params.topLeft[0] = std::floor(w * 0.5f + 0.5f);
  params.topLeft[1] = std::floor(h * 0.5f + 0.5f);
  params.size = std::floor(scale);
  params.highlight = scale > 0.0f ? 1U : 0U;
  MTL::CommandBuffer *command = m_OutputQueue->commandBuffer();
  if(!command) return;
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  auto colour = pass->colorAttachments()->object(0);
  colour->setTexture(output->second.texture);
  colour->setLoadAction(params.highlight ? MTL::LoadActionLoad : MTL::LoadActionDontCare);
  colour->setStoreAction(MTL::StoreActionStore);
  MTL::RenderCommandEncoder *encoder = command->renderCommandEncoder(pass);
  if(!encoder) return;
  encoder->setRenderPipelineState(m_BackgroundPipeline);
  encoder->setFragmentBytes(&params, sizeof(params), 0);
  encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
  encoder->endEncoding();
  command->commit();
  command->waitUntilCompleted();
}

void MetalReplay::RenderCheckerboard(FloatVector dark, FloatVector light)
{
  RenderOutputBackground(dark, light, 0.0f, 0.0f, 0.0f);
}

void MetalReplay::RenderHighlightBox(float w, float h, float scale)
{
  if(std::isfinite(w) && std::isfinite(h) && std::isfinite(scale) && scale >= 1.0f)
    RenderOutputBackground(FloatVector(), FloatVector(), w, h, scale);
}

bool MetalReplay::ReadTextureInspection(ResourceId id, const Subresource &sub, CompType typeCast,
                                        bytebuf &data, ResourceFormat &format, uint32_t &stride,
                                        size_t &offset, size_t &count)
{
  data.clear();
  offset = count = 0;
  stride = 0;
  if(GetTexture(id).resourceId == ResourceId()) return false;
  auto object = m_pDriver->GetResourceManager()->GetResource(id, true);
  if(!object || object->m_Type != eResTexture || !object->m_Real) return false;
  auto texture = Unwrap((WrappedMTLTexture *)object);
  if(!texture || sub.mip >= texture->mipmapLevelCount()) return false;
  uint32_t bw = 1, bh = 1;
  if(texture->pixelFormat() == MTL::PixelFormatDepth32Float_Stencil8) stride = 8;
  else if(texture->pixelFormat() == MTL::PixelFormatX32_Stencil8) stride = 1;
  else if(!GetTextureDataBlockShape(texture->pixelFormat(), bw, bh, stride) || bw != 1 || bh != 1)
    return false;
  const size_t width = RDCMAX(1ULL, texture->width() >> sub.mip);
  const size_t height = RDCMAX(1ULL, texture->height() >> sub.mip);
  count = width * height;
  if(texture->textureType() == MTL::TextureType3D)
  {
    const size_t depth = RDCMAX(1ULL, texture->depth() >> sub.mip);
    if(sub.slice >= depth) return false;
    offset = count * sub.slice * stride;
  }
  Subresource readSub = sub;
  // ResolveSamples has the same meaning as sample0 on single-sample textures.
  if(texture->sampleCount() == 1 && readSub.sample == ~0U) readSub.sample = 0;
  if(!ReadTextureSubresource(texture, readSub, data, id) ||
     offset > data.size() || count > (data.size() - offset) / stride)
    return false;
  format = MakeResourceFormat(texture->pixelFormat());
  if(typeCast != CompType::Typeless) format.compType = typeCast;
  return true;
}

bool MetalReplay::GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast,
                            float *minval, float *maxval)
{
  // Use the existing bounded Native readback and RenderDoc's shared decoder.
  // Like the D3D12/Vulkan reduction, results are per-channel and integer results
  // preserve PixelValue union bits. Display range does not affect the reduction.
  bytebuf data;
  ResourceFormat format;
  uint32_t stride;
  size_t offset, count;
  if(!ReadTextureInspection(texid, sub, typeCast, data, format, stride, offset, count)) return false;
  PixelValue lo = {}, hi = {};
  bool seen[4] = {};
  for(size_t i = 0; i < count; i++)
  {
    PixelValue value;
    bool decoded = false;
    DecodePixelData(format, data.data() + offset + i * stride, value, &decoded);
    if(!decoded) return false;
    for(size_t c = 0; c < 4; c++)
    {
      if(format.compType == CompType::UInt)
      {
        lo.uintValue[c] = seen[c] ? RDCMIN(lo.uintValue[c], value.uintValue[c]) : value.uintValue[c];
        hi.uintValue[c] = seen[c] ? RDCMAX(hi.uintValue[c], value.uintValue[c]) : value.uintValue[c];
      }
      else if(format.compType == CompType::SInt)
      {
        lo.intValue[c] = seen[c] ? RDCMIN(lo.intValue[c], value.intValue[c]) : value.intValue[c];
        hi.intValue[c] = seen[c] ? RDCMAX(hi.intValue[c], value.intValue[c]) : value.intValue[c];
      }
      else
      {
        if(std::isnan(value.floatValue[c])) continue;
        lo.floatValue[c] = seen[c] ? RDCMIN(lo.floatValue[c], value.floatValue[c]) : value.floatValue[c];
        hi.floatValue[c] = seen[c] ? RDCMAX(hi.floatValue[c], value.floatValue[c]) : value.floatValue[c];
      }
      seen[c] = true;
    }
  }
  memcpy(minval, lo.floatValue.data(), sizeof(float) * 4);
  memcpy(maxval, hi.floatValue.data(), sizeof(float) * 4);
  return true;
}

bool MetalReplay::GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast,
                               float minval, float maxval, const rdcfixedarray<bool, 4> &channels,
                               rdcarray<uint32_t> &histogram)
{
  histogram.clear();
  if(!std::isfinite(minval) || !std::isfinite(maxval) || minval >= maxval) return false;
  bytebuf data;
  ResourceFormat format;
  uint32_t stride;
  size_t offset, count;
  if(!ReadTextureInspection(texid, sub, typeCast, data, format, stride, offset, count)) return false;
  // Match the shared histogram's 256 buckets, combined selected channels and
  // inclusive maximum. Out-of-range and non-finite values do not contribute.
  histogram.resize(256);
  for(uint32_t &bucket : histogram) bucket = 0;
  const double inverseRange = 1.0 / (double(maxval) - double(minval));
  for(size_t i = 0; i < count; i++)
  {
    bool decoded = false;
    FloatVector value = DecodeFormattedComponents(format, data.data() + offset + i * stride, &decoded);
    if(!decoded) { histogram.clear(); return false; }
    const float values[4] = {value.x, value.y, value.z, value.w};
    for(size_t c = 0; c < 4; c++)
    {
      if(!channels[c] || !std::isfinite(values[c]) || values[c] < minval || values[c] > maxval) continue;
      uint32_t bucket = uint32_t((double(values[c]) - minval) * inverseRange * 256.0);
      histogram[RDCMIN(bucket, 255U)]++;
    }
  }
  return true;
}

void MetalReplay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                            CompType typeCast, float pixel[4])
{
  pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0.0f;

  if(GetTexture(texture).resourceId == ResourceId())
  {
    RDCERR("Metal pixel picking requested for non-texture resource %s", ToStr(texture).c_str());
    return;
  }

  WrappedMTLObject *resource = m_pDriver->GetResourceManager()->GetResource(texture, true);
  WrappedMTLTexture *wrappedTexture = (WrappedMTLTexture *)resource;
  MTL::Texture *real = wrappedTexture ? Unwrap(wrappedTexture) : NULL;
  if(real == NULL || sub.mip >= real->mipmapLevelCount())
    return;

  uint32_t blockWidth = 1, blockHeight = 1, blockBytes = 8;
  const bool depthStencil = real->pixelFormat() == MTL::PixelFormatDepth32Float_Stencil8;
  const bool stencilView = real->pixelFormat() == MTL::PixelFormatX32_Stencil8;
  if(stencilView) blockBytes = 1;
  if(!depthStencil && !stencilView && (!GetTextureDataBlockShape(real->pixelFormat(), blockWidth, blockHeight, blockBytes) ||
                      blockWidth != 1 || blockHeight != 1))
  {
    RDCERR("Metal pixel picking requires an uncompressed supported pixel format");
    return;
  }
  const uint32_t width = RDCMAX(1U, (uint32_t)real->width() >> sub.mip);
  const uint32_t height = RDCMAX(1U, (uint32_t)real->height() >> sub.mip);
  const bool volume = real->textureType() == MTL::TextureType3D;
  const uint32_t depth = volume ? RDCMAX(1U, (uint32_t)real->depth() >> sub.mip) : 1;
  if(x >= width || y >= height || (volume && sub.slice >= depth)) return;
  bytebuf data;
  if(!ReadTextureSubresource(real, sub, data, texture)) return;
  const size_t offset = ((size_t(volume ? sub.slice : 0) * height + y) * width + x) * blockBytes;
  if(offset > data.size() || blockBytes > data.size() - offset) return;
  ResourceFormat format = MakeResourceFormat(real->pixelFormat());
  if(typeCast != CompType::Typeless) format.compType = typeCast;
  PixelValue value = {};
  bool decoded = false;
  // Use the shared decoder, preserving UInt/SInt union bits for IReplayDriver's float array.
  DecodePixelData(format, data.data() + offset, value, &decoded);
  if(decoded) memcpy(pixel, value.floatValue.data(), sizeof(float) * 4);

}

void MetalReplay::BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  id = ResourceId();
  errors = "Metal custom shaders are not implemented.";
}

RDResult Metal_CreateReplayDevice(RDCFile *rdc, const ReplayOptions &opts, IReplayDriver **driver)
{
  if(!driver)
    return ResultCode::InvalidParameter;

  *driver = NULL;

  MetalInitParams initParams;
  uint64_t version = MetalInitParams::CurrentVersion;

  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);
    if(sectionIdx < 0)
      RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

    version = rdc->GetSectionProperties(sectionIdx).version;
    if(version < 0x1 || version > MetalInitParams::CurrentVersion)
      RETURN_ERROR_RESULT(ResultCode::APIIncompatibleVersion,
                          "Metal capture version %llu is not supported; latest is %llu", version,
                          MetalInitParams::CurrentVersion);

    StreamReader *reader = rdc->ReadSection(sectionIdx);
    ReadSerialiser ser(reader, Ownership::Stream);
    ser.SetVersion(version);

    SystemChunk chunk = ser.ReadChunk<SystemChunk>();
    if(chunk != SystemChunk::DriverInit)
      RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "Expected a DriverInit chunk, instead got %u",
                          chunk);

    SERIALISE_ELEMENT(initParams);
    if(ser.IsErrored())
      return ser.GetError();
  }

  MTL::Device *realDevice = MTL::CreateSystemDefaultDevice();
  if(!realDevice)
    RETURN_ERROR_RESULT(ResultCode::APIInitFailed, "Failed to create the default Metal device");

  ResourceId deviceId = initParams.DeviceID;
  if(deviceId == ResourceId())
    deviceId = ResourceIDGen::GetNewUniqueID();

  WrappedMTLDevice *wrappedDevice = new WrappedMTLDevice(realDevice, deviceId);
  wrappedDevice->SetReplayVersion(version);
  wrappedDevice->SetReplayOptions(opts);
  MetalReplay *replay = wrappedDevice->GetReplay();
  replay->SetProxy(rdc == NULL);
  *driver = replay;
  return ResultCode::Succeeded;
}

static DriverRegistration MetalDriverRegistration(RDCDriver::Metal, &Metal_CreateReplayDevice);

RDResult Metal_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  WrappedMTLDevice device(NULL, ResourceIDGen::GetNewUniqueID());

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);
  if(sectionIdx < 0)
    RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

  device.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  RDResult status = device.ReadLogInitialisation(rdc, true);

  if(status == ResultCode::Succeeded)
    device.GetStructuredFile()->Swap(output);

  return status;
}

static StructuredProcessRegistration MetalProcessRegistration(RDCDriver::Metal,
                                                              &Metal_ProcessStructured);
