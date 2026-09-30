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
#include "maths/matrix.h"
#include "replay/dummy_driver.h"
#include "serialise/rdcfile.h"
#include "metal_buffer.h"
#include "metal_argument_encoder.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_texture.h"
#include "metal_sampler_state.h"

MetalReplay::MetalReplay(WrappedMTLDevice *wrappedMTLDevice)
{
  m_pDriver = wrappedMTLDevice;
  m_DriverInfo.vendor = GPUVendor::Unknown;
  memset(m_DriverInfo.version, 0, sizeof(m_DriverInfo.version));
  snprintf(m_DriverInfo.version, sizeof(m_DriverInfo.version), "Apple Metal");
}

MetalReplay::~MetalReplay()
{
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
  return NULL;
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

void MetalReplay::AddTexture(ResourceId id, MTL::Texture *texture, bool swapBuffer)
{
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
    if(!entry.second.sharedData.empty() || entry.second.privateCopy)
      continue;
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
      descriptor->setUsage(MTL::TextureUsage(MTL::TextureUsageShaderRead |
                                             MTL::TextureUsageShaderWrite |
                                             MTL::TextureUsageRenderTarget |
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
  reflection.resourceId = id;
  reflection.entryPoint = entryPoint;
  reflection.stage = MakeShaderStage(function->functionType());
  reflection.debugInfo.entrySourceName = entryPoint;
  reflection.debugInfo.debuggable = false;
  reflection.debugInfo.debugStatus = "Metal shader debugging is not implemented.";

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
  if(shaderIt == m_Shaders.end() || arguments == NULL)
    return;

  ShaderReflection &reflection = shaderIt->second;
  reflection.constantBlocks.clear();
  reflection.samplers.clear();
  reflection.readOnlyResources.clear();
  reflection.readWriteResources.clear();
  m_ShaderArgumentSlots.erase(shader);

  ShaderBindingUsage &usage = m_ShaderBindingUsage[shader];
  usage = ShaderBindingUsage();
  usage.available = true;

  for(NS::UInteger i = 0; i < arguments->count(); i++)
  {
    MTL::Argument *argument = arguments->object<MTL::Argument>(i);
    if(argument == NULL)
      continue;

    const rdcstr name = argument->name() ? argument->name()->utf8String() : "";
    const uint32_t bind = (uint32_t)argument->index();
    const uint32_t arraySize = RDCMAX(1U, (uint32_t)argument->arrayLength());

    if(argument->type() == MTL::ArgumentTypeBuffer)
    {
      // Metal reports a device float4 pointer as Float4 rather than DataTypePointer here.
      // Struct-backed constant/argument buffers continue through the constant-block path.
      if(argument->bufferDataType() != MTL::DataTypeStruct)
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
          usage.readOnlyResources.push_back(argument->active());
        }
        else
        {
          reflection.readWriteResources.push_back(resource);
          usage.readWriteResources.push_back(argument->active());
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
      usage.constantBlocks.push_back(argument->active());

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
        if(argument->active() &&
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
              usage.readOnlyResources.push_back(argument->active());
            }
            else
            {
              reflection.readWriteResources.push_back(resource);
              usage.readWriteResources.push_back(argument->active());
            }
          }
          else if(memberType == MTL::DataTypeSampler)
          {
            ShaderSampler sampler;
            sampler.name = qualifiedName;
            sampler.fixedBindNumber = argumentId;
            sampler.bindArraySize = 1;
            reflection.samplers.push_back(sampler);
            usage.samplers.push_back(argument->active());
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
            usage.readOnlyResources.push_back(argument->active());
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
      usage.samplers.push_back(argument->active());
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
        usage.readOnlyResources.push_back(argument->active());
      }
      else
      {
        reflection.readWriteResources.push_back(resource);
        usage.readWriteResources.push_back(argument->active());
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
  auto it = m_Shaders.find(shader);
  if(it == m_Shaders.end())
    return NULL;

  if(!entry.name.empty() &&
     (entry.name != it->second.entryPoint || entry.stage != it->second.stage))
    return NULL;

  return &it->second;
}

void MetalReplay::AddRenderPipeline(ResourceId id,
                                    const RDMTL::RenderPipelineDescriptor &descriptor,
                                    MTL::RenderPipelineReflection *reflection)
{
  RenderPipelineInfo &pipeline = m_RenderPipelines[id];
  pipeline.vertexFunction = GetResID(descriptor.vertexFunction);
  pipeline.fragmentFunction = GetResID(descriptor.fragmentFunction);
  pipeline.vertexDescriptor = descriptor.vertexDescriptor;
  pipeline.sampleCount = (uint32_t)descriptor.rasterSampleCount;
  pipeline.alphaToCoverageEnabled = descriptor.alphaToCoverageEnabled;
  pipeline.alphaToOneEnabled = descriptor.alphaToOneEnabled;
  pipeline.colorAttachments = descriptor.colorAttachments;

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

void MetalReplay::AddMeshPipeline(ResourceId id, ResourceId meshFunction,
                                  ResourceId fragmentFunction, uint32_t sampleCount,
                                  MTL::RenderPipelineReflection *reflection)
{
  // MetalPipe has no mesh/object shader fields yet; keep the native mesh identity distinct
  // while retaining the fragment shader for ordinary output inspection.
  RenderPipelineInfo &pipeline = m_RenderPipelines[id];
  pipeline.fragmentFunction = fragmentFunction;
  pipeline.sampleCount = sampleCount;
  m_MeshPipelines[id] = meshFunction;
  if(reflection && fragmentFunction != ResourceId())
    AddShaderBindings(fragmentFunction, reflection->fragmentArguments());
}

void MetalReplay::AddComputePipeline(ResourceId id, ResourceId function,
                                     MTL::ComputePipelineReflection *reflection,
                                     MTL::ComputePipelineState *pipeline, bool threadExecutionMultiple)
{
  m_ComputePipelines[id] = function;
  // Pipelines created inside a captured frame can be recreated on event replay.
  m_ComputeBufferMinimums.erase(id);
  m_ComputeRequiredTextures.erase(id);
  m_ComputeRequiredSamplers.erase(id);
  m_ComputeThreadgroupMinimums.erase(id);
  m_ComputeThreadgroupLimits[id] = {(uint64_t)pipeline->staticThreadgroupMemoryLength(),
                                   (uint64_t)pipeline->maxTotalThreadsPerThreadgroup()};
  m_ComputeThreadExecutionMultiples[id] =
      threadExecutionMultiple ? (uint64_t)pipeline->threadExecutionWidth() : 1;
  if(reflection)
  {
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
      else if(argument->type() == MTL::ArgumentTypeThreadgroupMemory)
        m_ComputeThreadgroupMinimums[id][(uint32_t)argument->index()] =
            RDCMAX(1ULL, (uint64_t)argument->threadgroupMemoryDataSize());
    }
  }
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

bool MetalReplay::ValidateComputeThreadgroup(const MTL::Size &threads, const MTL::Size *grid) const
{
  auto limits = m_ComputeThreadgroupLimits.find(m_CurrentPipelineState.computePipelineResourceId);
  if(limits == m_ComputeThreadgroupLimits.end())
    return false;
  const MTL::Size maximum = Unwrap(m_pDriver)->maxThreadsPerThreadgroup();
  if(!threads.width || !threads.height || !threads.depth || threads.width > maximum.width ||
     threads.height > maximum.height || threads.depth > maximum.depth ||
     threads.width > limits->second.second / threads.height ||
     threads.width * threads.height > limits->second.second / threads.depth)
    return false;
  const uint64_t maximumMemory = Unwrap(m_pDriver)->maxThreadgroupMemoryLength();
  auto multiple =
      m_ComputeThreadExecutionMultiples.find(m_CurrentPipelineState.computePipelineResourceId);
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
  for(const auto &binding : m_CurrentComputeThreadgroupMemory)
  {
    if(total > maximumMemory || binding.second > maximumMemory - total)
      return false;
    total += binding.second;
  }
  if(total > maximumMemory)
    return false;
  auto required = m_ComputeThreadgroupMinimums.find(m_CurrentPipelineState.computePipelineResourceId);
  if(required != m_ComputeThreadgroupMinimums.end())
    for(const auto &minimum : required->second)
    {
      auto binding = m_CurrentComputeThreadgroupMemory.find(minimum.first);
      if(binding == m_CurrentComputeThreadgroupMemory.end() || binding->second < minimum.second)
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
  m_CurrentSamplerLOD.clear();
  m_CurrentComputeInlineBytes.clear();
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

void MetalReplay::SetSamplerLOD(ShaderStage stage, uint32_t index, float minimum, float maximum)
{
  const uint32_t base = stage == ShaderStage::Vertex ? 0xE00 :
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
  }
  return GetComputeTexture(write ? 1 : 0);
}

void MetalReplay::BindComputeBuffer(uint32_t index, ResourceId id, uint64_t offset)
{
  m_CurrentComputeInlineBytes.erase(index);
  m_CurrentPipelineState.computeBuffers.resize_for_index(index);
  MetalPipe::BufferBinding &binding = m_CurrentPipelineState.computeBuffers[index];
  binding.resourceId = id;
  binding.byteOffset = offset;
  const BufferDescription buffer = GetBuffer(id);
  binding.byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                         ? buffer.length - offset
                         : 0;
}

void MetalReplay::BindComputeBytes(uint32_t index, uint64_t length)
{
  BindComputeBuffer(index, ResourceId(), 0);
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
  state.computeThreadgroupMemory = m_CurrentComputeThreadgroupMemory;
  state.samplerLOD = m_CurrentSamplerLOD;
  state.vertexAttributeStrides = m_CurrentVertexAttributeStrides;
  return state;
}

void MetalReplay::RestoreEncoderState(const EncoderReplayState &state)
{
  m_CurrentPipelineState = state.pipeline;
  m_CurrentRenderPassDescriptor = state.renderPass;
  m_CurrentComputeInlineBytes = state.computeInlineBytes;
  m_CurrentComputeThreadgroupMemory = state.computeThreadgroupMemory;
  m_CurrentSamplerLOD = state.samplerLOD;
  m_CurrentVertexAttributeStrides = state.vertexAttributeStrides;
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
  m_CurrentSamplerLOD.clear();
  m_CurrentVertexAttributeStrides.clear();
  m_CurrentRenderPassDescriptor = descriptor;
  m_CurrentPipelineState = MetalPipe::State();

  m_CurrentPipelineState.colorTargets.resize(descriptor.colorAttachments.size());
  m_CurrentPipelineState.resolveTargets.resize(descriptor.colorAttachments.size());
  for(size_t i = 0; i < descriptor.colorAttachments.size(); i++)
  {
    m_CurrentPipelineState.colorTargets[i] =
        MakeRenderTargetDescriptor(this, descriptor.colorAttachments[i]);
    m_CurrentPipelineState.resolveTargets[i] =
        MakeResolveTargetDescriptor(this, descriptor.colorAttachments[i]);
  }

  m_CurrentPipelineState.depthTarget = MakeRenderTargetDescriptor(this, descriptor.depthAttachment);
  if(m_CurrentPipelineState.depthTarget.resource == ResourceId())
    m_CurrentPipelineState.depthTarget =
        MakeRenderTargetDescriptor(this, descriptor.stencilAttachment);
}

void MetalReplay::EndRenderPass()
{
  m_CurrentSamplerLOD.clear();
  m_CurrentVertexAttributeStrides.clear();
  m_CurrentPipelineState = MetalPipe::State();
}

void MetalReplay::SetRenderPassStoreAction(uint32_t attachment, MTL::StoreAction action)
{
  if(attachment < m_CurrentRenderPassDescriptor.colorAttachments.size())
    m_CurrentRenderPassDescriptor.colorAttachments[attachment].storeAction = action;
  else if(attachment == 8)
    m_CurrentRenderPassDescriptor.depthAttachment.storeAction = action;
  else if(attachment == 9)
    m_CurrentRenderPassDescriptor.stencilAttachment.storeAction = action;
}

void MetalReplay::SetRenderPassStoreOptions(uint32_t attachment, MTL::StoreActionOptions options)
{
  if(attachment < m_CurrentRenderPassDescriptor.colorAttachments.size())
    m_CurrentRenderPassDescriptor.colorAttachments[attachment].storeActionOptions = options;
  else if(attachment == 8)
    m_CurrentRenderPassDescriptor.depthAttachment.storeActionOptions = options;
  else if(attachment == 9)
    m_CurrentRenderPassDescriptor.stencilAttachment.storeActionOptions = options;
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
  m_CurrentPipelineState.vertexShader = MetalPipe::Shader();
  m_CurrentPipelineState.fragmentShader = MetalPipe::Shader();
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

    return ret;
  };

  m_CurrentPipelineState.vertexShader =
      bindShader(pipeline->second.vertexFunction, ShaderStage::Vertex);
  m_CurrentPipelineState.fragmentShader =
      bindShader(pipeline->second.fragmentFunction, ShaderStage::Fragment);
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
      else if(type != MTL::DataTypeTexture && type != MTL::DataTypeSampler && type != MTL::DataTypePointer)
        continue;
      object = resource == ResourceId() ? NULL : rm->GetResource(resource, true);
      if(resource != ResourceId() && (!object || !object->m_Real))
        return false;
      if(type == MTL::DataTypeTexture)
        encoder->setTexture(Unwrap((WrappedMTLTexture *)object), i);
      else if(type == MTL::DataTypeSampler)
        encoder->setSamplerState(Unwrap((WrappedMTLSamplerState *)object), i);
      else
        encoder->setBuffer(Unwrap((WrappedMTLBuffer *)object), object ? packet.buffers[i].byteOffset : 0, i);
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
      RDCERR("Missing Metal argument packet at shader buffer slot %u", slot.first);
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

void MetalReplay::SetViewport(const MTL::Viewport &viewport)
{
  m_CurrentPipelineState.rasterizer.viewport =
      Viewport((float)viewport.originX, (float)viewport.originY, (float)viewport.width,
               (float)viewport.height, (float)viewport.znear, (float)viewport.zfar, true);
}

void MetalReplay::SetScissor(const MTL::ScissorRect &scissor)
{
  m_CurrentPipelineState.rasterizer.scissor =
      Scissor((int32_t)scissor.x, (int32_t)scissor.y, (int32_t)scissor.width,
              (int32_t)scissor.height, true);
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

void MetalReplay::SetPrimitiveTopology(MTL::PrimitiveType primitiveType)
{
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
  if(descriptorStore != GetResID(m_pDriver) || m_MetalPipelineState == NULL)
    return {};

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
      if(range.type == DescriptorType::Buffer && offset >= ArgumentBufferOffset &&
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
      else if(range.type == DescriptorType::Image && offset >= VertexTextureOffset)
      {
        const uint32_t slot = offset - VertexTextureOffset;
        if(slot < m_MetalPipelineState->vertexTextures.size())
        {
          descriptor.type = DescriptorType::Image;
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
      else if(range.type == DescriptorType::Image && offset < SamplerOffset &&
         offset < m_MetalPipelineState->fragmentTextures.size())
      {
        descriptor.type = DescriptorType::Image;
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
  if(descriptorStore != GetResID(m_pDriver) || m_MetalPipelineState == NULL)
    return {};

  size_t count = 0;
  for(const DescriptorRange &range : ranges)
    count += range.count;
  rdcarray<SamplerDescriptor> ret;
  ret.resize(count);

  size_t dst = 0;
  for(const DescriptorRange &range : ranges)
  {
    for(uint32_t i = 0; i < range.count; i++, dst++)
    {
      const uint32_t offset = range.offset + i * range.descriptorSize;
      if(range.type != DescriptorType::Sampler || offset < SamplerOffset)
        continue;

      ResourceId samplerId;
      if(offset >= ComputeSamplerOffset)
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
      if(overrideLOD != m_SelectedSamplerLOD.end())
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
  return ret;
}

void MetalReplay::AddEvent(uint32_t chunkIndex, uint64_t fileOffset)
{
  APIEvent event;
  event.eventId = m_NextEventID++;
  event.chunkIndex = chunkIndex;
  event.fileOffset = fileOffset;
  m_PendingEvents.push_back(event);
  m_Events.resize_for_index(event.eventId);
  m_Events[event.eventId] = event;
  m_EventPipelineStates[event.eventId] = m_CurrentPipelineState;
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
  m_EventPipelineStates[action.eventId] = m_CurrentPipelineState;
  m_EventSamplerLOD[action.eventId] = m_CurrentSamplerLOD;
  rdcarray<ActionDescription> *actions = &m_FrameRecord.actionList;
  for(uint32_t index : debugGroupPath)
  {
    if(index >= actions->size())
    {
      RDCERR("Invalid Metal debug group action path");
      debugGroupPath.clear();
      actions = &m_FrameRecord.actionList;
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
    if(action.flags & ActionFlags::PushMarker)
      debugGroupPath.push_back((uint32_t)actions->size() - 1);
    else if(action.flags & ActionFlags::PopMarker)
    {
      if(debugGroupPath.empty())
        RDCWARN("Metal debug group pop without a matching push");
      else
        debugGroupPath.pop_back();
    }
  }

  if(action.flags & ActionFlags::Drawcall)
  {
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
    for(ResourceId texture : m_CurrentPipelineState.vertexTextures)
      AddUsage(texture, ResourceUsage::VS_Resource);
    for(ResourceId texture : m_CurrentPipelineState.fragmentTextures)
      AddUsage(texture, ResourceUsage::PS_Resource);
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
      AddUsage(buffer, storage ? ResourceUsage::PS_Resource : ResourceUsage::PS_Constants);
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

void MetalReplay::RegisterComputeIndirectAction(uint32_t eventId, ResourceId buffer,
                                                uint64_t offset)
{
  m_PendingComputeIndirectActions.push_back({eventId, buffer, offset});
}

bool MetalReplay::HasPendingComputeIndirectActionFor(ResourceId id) const
{
  for(const PendingComputeIndirectAction &pending : m_PendingComputeIndirectActions)
    if(pending.buffer == id)
      return true;
  return false;
}

void MetalReplay::ResolvePendingComputeIndirectActions()
{
  for(const PendingComputeIndirectAction &pending : m_PendingComputeIndirectActions)
  {
    bytebuf bytes;
    GetBufferData(pending.buffer, pending.offset,
                  sizeof(MTL::DispatchThreadgroupsIndirectArguments), bytes);
    if(bytes.size() != sizeof(MTL::DispatchThreadgroupsIndirectArguments))
    {
      RDCERR("Couldn't read Metal compute indirect arguments at EID %u", pending.eventId);
      continue;
    }

    uint32_t groups[3] = {};
    memcpy(groups, bytes.data(), sizeof(groups));
    // Debug groups can now contain indirect actions; traverse their children too.
    rdcarray<rdcarray<ActionDescription> *> lists = {&m_FrameRecord.actionList};
    while(!lists.empty())
    {
      rdcarray<ActionDescription> *list = lists.back();
      lists.pop_back();
      for(ActionDescription &action : *list)
      {
        if(action.eventId == pending.eventId)
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
  m_PendingComputeIndirectActions.clear();
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
  if(id == ResourceId() || m_FrameRecord.actionList.empty())
    return;

  rdcarray<EventUsage> &uses = m_ResourceUses[id];
  if(!uses.empty() && uses.back().eventId == m_LastActionEventID && uses.back().usage == usage)
    return;
  uses.push_back(EventUsage(m_LastActionEventID, usage));
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
    if(attachment.texture && attachment.storeAction == MTL::StoreActionDontCare)
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
  auto it = m_ResourceUses.find(id);
  return it == m_ResourceUses.end() ? rdcarray<EventUsage>() : it->second;
}

RDResult MetalReplay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  if(!rdc)
    return ResultCode::Succeeded;

  m_FrameRecord = {};
  m_PendingComputeIndirectActions.clear();
  m_PendingEvents.clear();
  m_Events.clear();
  m_ResourceUses.clear();
  m_EventPipelineStates.clear();
  m_EventSamplerLOD.clear();
  m_CurrentSamplerLOD.clear();
  m_SelectedSamplerLOD.clear();
  m_NextEventID = 1;
  m_NextActionID = 1;
  m_LastActionEventID = 0;
  m_MultiActionChildrenRemaining = 0;
  m_MultiActionEndEvents.clear();
  m_DebugGroupPaths.clear();

  RDResult ret = m_pDriver->ReadLogInitialisation(rdc, storeStructuredBuffers);
  if(ret != ResultCode::Succeeded)
    m_FatalError = ret;

  return ret;
}

void MetalReplay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  if(replayType != eReplay_OnlyDraw)
    m_CurrentPipelineState = MetalPipe::State();

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
  for(size_t i = (size_t)eventId + 1; i < m_Events.size(); i++)
  {
    if(m_Events[i].eventId != 0)
      return m_Events[i].fileOffset;
  }
  return frameSize;
}

SDFile *MetalReplay::GetStructuredFile()
{
  return m_pDriver->GetStructuredFile();
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
  ReadTextureSubresource(real, sub, data);
}

void MetalReplay::BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  id = ResourceId();
  errors = "Metal target shader compilation is not implemented.";
}

void MetalReplay::ReplaceResource(ResourceId from, ResourceId to)
{
  m_pDriver->GetResourceManager()->ReplaceResource(from, to);
}

void MetalReplay::RemoveReplacement(ResourceId id)
{
  m_pDriver->GetResourceManager()->RemoveReplacement(id);
}

bool MetalReplay::IsRenderOutput(ResourceId id)
{
  return id != ResourceId() && id == m_pDriver->GetLastPresentedImage();
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
                                         bytebuf &data)
{
  data.clear();

  if(texture == NULL)
  {
    RDCERR("Cannot read back a NULL Metal texture");
    return false;
  }

  const MTL::TextureType textureType = texture->textureType();
  const bool supportedType = textureType == MTL::TextureType2D ||
                             textureType == MTL::TextureType2DArray ||
                             textureType == MTL::TextureTypeCube ||
                             textureType == MTL::TextureTypeCubeArray;
  const uint32_t sliceCount =
      (uint32_t)texture->arrayLength() *
      ((textureType == MTL::TextureTypeCube || textureType == MTL::TextureTypeCubeArray) ? 6U : 1U);
  if(!supportedType || texture->sampleCount() != 1 || sub.sample != 0 || sub.slice >= sliceCount)
  {
    RDCERR("Metal texture readback does not support type %s, sample %u, or slice %u/%u",
           ToStr(textureType).c_str(), sub.sample, sub.slice, sliceCount);
    return false;
  }

  if(sub.mip >= texture->mipmapLevelCount())
  {
    RDCERR("Metal texture readback mip %u is out of range for a texture with %llu mips", sub.mip,
           (unsigned long long)texture->mipmapLevelCount());
    return false;
  }

  switch(texture->pixelFormat())
  {
    case MTL::PixelFormatBC1_RGBA:
    case MTL::PixelFormatBC1_RGBA_sRGB:
    case MTL::PixelFormatBC5_RGUnorm:
    case MTL::PixelFormatRGBA8Unorm:
    case MTL::PixelFormatRGBA8Unorm_sRGB:
    case MTL::PixelFormatBGRA8Unorm:
    case MTL::PixelFormatBGRA8Unorm_sRGB:
    case MTL::PixelFormatDepth32Float_Stencil8: break;
    default:
      RDCERR("Metal texture readback does not yet support format %s",
             ToStr(texture->pixelFormat()).c_str());
      return false;
  }

  if(!InitialiseOutputResources())
    return false;

  MTL::Device *device = Unwrap(m_pDriver);
  const uint64_t width = RDCMAX(1ULL, uint64_t(texture->width()) >> sub.mip);
  const uint64_t height = RDCMAX(1ULL, uint64_t(texture->height()) >> sub.mip);
  const bool depthStencil = texture->pixelFormat() == MTL::PixelFormatDepth32Float_Stencil8;
  const bool bc1 = texture->pixelFormat() == MTL::PixelFormatBC1_RGBA ||
                   texture->pixelFormat() == MTL::PixelFormatBC1_RGBA_sRGB;
  const bool bc5 = texture->pixelFormat() == MTL::PixelFormatBC5_RGUnorm;
  const uint64_t rows = bc1 || bc5 ? (height + 3) / 4 : height;
  const uint64_t tightRowPitch = bc1 ? ((width + 3) / 4) * 8 :
                                 bc5 ? ((width + 3) / 4) * 16 : width * 4;
  const uint64_t alignment = depthStencil ? 256 : bc1 || bc5 ? 1 : RDCMAX<uint64_t>(
      1, device->minimumLinearTextureAlignmentForPixelFormat(texture->pixelFormat()));
  const uint64_t rowPitch = AlignUp(tightRowPitch, alignment);
  const uint64_t depthSize = rowPitch * rows;
  const uint64_t stencilRowPitch = depthStencil ? AlignUp(width, alignment) : 0;
  const uint64_t bufferSize = depthSize + stencilRowPitch * height;
  if((bc1 || bc5) &&
     (textureType != MTL::TextureType2D || width > 4096 || height > 4096 ||
      bufferSize > 32ULL * 1024 * 1024))
  {
    RDCERR("Metal BC texture readback exceeds the validated 2D block layout");
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

  blit->copyFromTexture(texture, sub.slice, sub.mip, MTL::Origin::Make(0, 0, 0),
                        MTL::Size::Make((NS::UInteger)width, (NS::UInteger)height, 1), readback, 0,
                        (NS::UInteger)rowPitch, (NS::UInteger)depthSize,
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
  data.resize(size_t(outputRowPitch * rows));
  const byte *src = (const byte *)readback->contents();
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
      memcpy(data.data() + size_t(y * tightRowPitch), src + y * rowPitch, size_t(tightRowPitch));
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
  colour->setClearColor(MTL::ClearColor::Make(col.x, col.y, col.z, col.w));

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
  if(m_OutputPipeline && m_MeshPipeline && m_OutputQueue)
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
  uint padding;
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
                                constant DisplayParams &params [[buffer(0)]])
{
  constexpr sampler nearestSampler(coord::normalized, address::clamp_to_edge, filter::nearest,
                                   mip_filter::nearest);
  float4 value;
  if(params.textureType == 1)
  {
    value = sourceArray.sample(nearestSampler, input.uv, params.slice, level(params.mip));
  }
  else if(params.textureType == 2)
  {
    const float3 directions[6] = {
        float3(1.0, 0.0, 0.0), float3(-1.0, 0.0, 0.0),
        float3(0.0, 1.0, 0.0), float3(0.0, -1.0, 0.0),
        float3(0.0, 0.0, 1.0), float3(0.0, 0.0, -1.0),
    };
    value = sourceCube.sample(nearestSampler, directions[params.slice % 6], level(params.mip));
  }
  else
  {
    value = source2d.sample(nearestSampler, input.uv, level(params.mip));
  }
  if(params.rawOutput == 0)
    value = saturate((value - params.rangeMin) * params.inverseRange);

  bool r = (params.channelMask & 1) != 0;
  bool g = (params.channelMask & 2) != 0;
  bool b = (params.channelMask & 4) != 0;
  bool a = (params.channelMask & 8) != 0;
  uint rgbCount = uint(r) + uint(g) + uint(b);
  if(rgbCount == 0 && a)
    return float4(value.aaa, 1.0);
  if(rgbCount == 1 && !a)
  {
    float selected = r ? value.r : (g ? value.g : value.b);
    return float4(selected, selected, selected, 1.0);
  }

  value.rgb *= float3(r, g, b);
  value.a = a ? value.a : 1.0;
  return value;
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
  MTL::Library *library =
      device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
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

  if(m_OutputPipeline == NULL)
    m_OutputPipeline = device->newRenderPipelineState(descriptor, &error);
  descriptor->release();

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

  if(m_OutputPipeline == NULL)
  {
    RDCERR("Failed to create Metal replay output pipeline: %s",
           error ? error->localizedDescription()->utf8String() : "unknown error");
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
      (MTL::TextureUsage)(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead));
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
                                        MTL::ClearColor clearColor)
{
  if(source == NULL || target == NULL || !InitialiseOutputResources())
    return false;

  const MTL::TextureType textureType = source->textureType();
  const bool supportedType = textureType == MTL::TextureType2D ||
                             textureType == MTL::TextureType2DArray ||
                             textureType == MTL::TextureTypeCube;
  const uint32_t sliceCount = textureType == MTL::TextureTypeCube
                                  ? 6U
                                  : (uint32_t)source->arrayLength();
  if(!supportedType || source->sampleCount() != 1 ||
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
    // MSL float2 gives the struct 8-byte alignment, rounding its size up to 64.
    uint32_t padding;
  } params = {};
  RDCCOMPILE_ASSERT(sizeof(DisplayParams) == 64, "Metal display uniform layout must match MSL");

  const uint32_t mip = cfg.subresource.mip;
  const float textureWidth = (float)RDCMAX(1ULL, source->width() >> mip);
  const float textureHeight = (float)RDCMAX(1ULL, source->height() >> mip);
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
  params.inverseRange = cfg.rangeMax > cfg.rangeMin ? 1.0f / (cfg.rangeMax - cfg.rangeMin) : 1.0f;
  params.slice = cfg.subresource.slice;
  params.textureType = textureType == MTL::TextureType2DArray ? 1U
                       : textureType == MTL::TextureTypeCube   ? 2U
                                                              : 0U;

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
  colour->setTexture(target);
  colour->setLoadAction(loadAction);
  colour->setStoreAction(MTL::StoreActionStore);
  colour->setClearColor(clearColor);

  MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
  encoder->setRenderPipelineState(m_OutputPipeline);
  encoder->setVertexBytes(&params, sizeof(params), 0);
  encoder->setFragmentBytes(&params, sizeof(params), 0);
  encoder->setFragmentTexture(source, params.textureType);
  encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
  encoder->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();
  return true;
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

void MetalReplay::RenderCheckerboard(FloatVector dark, FloatVector light)
{
  // The first output implementation uses a solid dark tile until the checkerboard shader is
  // added. Clearing here is important: pixel-context and empty thumbnail windows otherwise expose
  // undefined private-texture contents.
  ClearOutputWindowColor(m_ActiveOutputWindowID, dark);
}

bool MetalReplay::GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast,
                            float *minval, float *maxval)
{
  *minval = 0.0f;
  *maxval = 1.0f;
  return false;
}

bool MetalReplay::GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast,
                               float minval, float maxval, const rdcfixedarray<bool, 4> &channels,
                               rdcarray<uint32_t> &histogram)
{
  histogram.clear();
  return false;
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

  if(real->pixelFormat() == MTL::PixelFormatBC1_RGBA ||
     real->pixelFormat() == MTL::PixelFormatBC1_RGBA_sRGB ||
     real->pixelFormat() == MTL::PixelFormatBC5_RGUnorm)
  {
    RDCERR("Metal BC pixel picking requires block decompression");
    return;
  }

  const uint32_t width = RDCMAX(1U, (uint32_t)real->width() >> sub.mip);
  const uint32_t height = RDCMAX(1U, (uint32_t)real->height() >> sub.mip);
  if(x >= width || y >= height)
    return;

  bytebuf data;
  if(!ReadTextureSubresource(real, sub, data))
    return;

  const bool depthStencil = real->pixelFormat() == MTL::PixelFormatDepth32Float_Stencil8;
  const byte *value = data.data() + (size_t(y) * width + x) * (depthStencil ? 8 : 4);
  switch(real->pixelFormat())
  {
    case MTL::PixelFormatRGBA8Unorm:
    case MTL::PixelFormatRGBA8Unorm_sRGB:
      pixel[0] = float(value[0]) / 255.0f;
      pixel[1] = float(value[1]) / 255.0f;
      pixel[2] = float(value[2]) / 255.0f;
      pixel[3] = float(value[3]) / 255.0f;
      break;
    case MTL::PixelFormatBGRA8Unorm:
    case MTL::PixelFormatBGRA8Unorm_sRGB:
      pixel[0] = float(value[2]) / 255.0f;
      pixel[1] = float(value[1]) / 255.0f;
      pixel[2] = float(value[0]) / 255.0f;
      pixel[3] = float(value[3]) / 255.0f;
      break;
    case MTL::PixelFormatDepth32Float_Stencil8:
      memcpy(&pixel[0], value, sizeof(float));
      pixel[1] = float(value[4]) / 255.0f;
      pixel[3] = 1.0f;
      break;
    default:
      RDCERR("Metal pixel picking does not yet support format %s",
             ToStr(real->pixelFormat()).c_str());
      break;
  }
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
