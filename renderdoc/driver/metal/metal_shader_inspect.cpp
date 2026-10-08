// SPDX-License-Identifier: MIT
#include "metal_replay.h"
#include "metal_device.h"
#include "metal_buffer.h"
#include "metal_texture.h"
#include "metal_sampler_state.h"
#include "metal_acceleration_structure.h"
#include "metal_function.h"
#include "metal_air_access.h"
#include "metal_descriptor_types.h"
#include "metal_shader_features.h"
#include <unistd.h>

void MetalReplay::AddShaderBinary(ResourceId library, const bytebuf &data)
{
  m_LibraryBinaries[library] = data;
  m_LibraryDebugSources[library] = ExtractMetalDebugSources(data);
}

rdcstr MetalReplay::GetComputeAIR(ResourceId pipeline, rdcstr &entry)
{
  const auto function=m_ComputePipelines.find(pipeline);
  if(function==m_ComputePipelines.end())return {};
  const auto reflection=m_Shaders.find(function->second);
  if(reflection==m_Shaders.end())return {};
  entry=reflection->second.entryPoint;
  // The public disassembler can return captured MSL or an explanatory string.
  // Neither is an AIR module. Source-compiled Native pipelines still replay
  // through the original API; unavailable optional AIR is a display state.
  const auto library=m_ShaderLibraries.find(reflection->second.resourceId);
  if(library==m_ShaderLibraries.end() || !m_LibraryBinaries.count(library->second))return {};
  return DisassembleShader(pipeline,&reflection->second,"Metal AIR (Apple toolchain)");
}

bool MetalReplay::GetGraphicsAIR(ResourceId pipeline, uint32_t stage, rdcstr &entry,
    rdcstr &air, std::map<rdcstr, rdcpair<rdcstr, rdcstr>> &linked)
{
  entry.clear();air.clear();linked.clear();
  const auto info=m_RenderPipelines.find(pipeline);
  if(info==m_RenderPipelines.end() || (stage!=1 && stage!=2) || IsMeshPipeline(pipeline) ||
     IsTilePipeline(pipeline))return false;
  const auto function=stage==1?info->second.vertexFunction:info->second.fragmentFunction;
  if(function==ResourceId())return stage==2;
  auto read=[&](ResourceId id,rdcstr &name,rdcstr &module) {
    const auto reflection=m_Shaders.find(id);
    if(reflection==m_Shaders.end())return false;
    name=reflection->second.entryPoint;
    module=DisassembleShader(pipeline,&reflection->second,"Metal AIR (Apple toolchain)");
    return !name.empty() && !module.empty() && module.size()<=4*1024*1024;
  };
  if(!read(function,entry,air))return false;
  const auto &functions=stage==1?info->second.descriptor.vertexLinkedFunctions:
      info->second.descriptor.fragmentLinkedFunctions;
  rdcarray<WrappedMTLFunction *> attached=functions.functions;
  attached.append(functions.binaryFunctions);attached.append(functions.privateFunctions);
  for(const auto &group:functions.groups)attached.append(group.functions);
  if(attached.size()>32)return false;
  for(auto fn:attached)
  {
    rdcstr name,module;
    if(!read(GetResID(fn),name,module))return false;
    auto existing=linked.find(name);
    if(existing!=linked.end() && existing->second.second!=module)return false;
    linked[name]=make_rdcpair(name,module);
  }
  return true;
}

rdcarray<rdcstr> MetalReplay::GetDisassemblyTargets(bool withPipeline)
{
  return {"Metal AIR (Apple toolchain)", "Captured MSL", "Metal AIR (editable)"};
}

rdcstr MetalReplay::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                     const rdcstr &target)
{
  if(!refl) return "; Invalid Metal shader.";
  auto library = m_ShaderLibraries.find(refl->resourceId);
  if(library == m_ShaderLibraries.end()) return "; Captured shader library is unavailable.";
  auto source = m_LibrarySources.find(library->second);
  if(target == "Captured MSL" && !refl->debugInfo.files.empty())
    return refl->debugInfo.files[RDCMAX(0, refl->debugInfo.editBaseFile)].contents;
  if(target == "Captured MSL" || (source != m_LibrarySources.end() && !source->second.empty()))
    return source == m_LibrarySources.end() || source->second.empty()
        ? "; This capture contains a compiled metallib, not MSL source. Select Metal AIR."
        : source->second;
  if(target == "Metal AIR (editable)")
  {
    const rdcstr text = DisassembleShader(pipeline, refl, "Metal AIR (Apple toolchain)");
    const rdcstr entry = refl->debugInfo.entrySourceName.empty() ? refl->entryPoint
                                                              : refl->debugInfo.entrySourceName;
    // Recent metal-objdump omits ModuleID and separates modules with section headers.
    // source_filename is LLVM syntax; the surrounding container headers are not.
    int32_t begin = text.find("source_filename =");
    while(begin >= 0)
    {
      int32_t next = text.find("source_filename =", begin + 1);
      int32_t header = text.find("\n0x", begin);
      int32_t end = next;
      if(header >= 0 && (end < 0 || header < end)) end = header;
      rdcstr module = text.substr(size_t(begin), end < 0 ? text.size() - size_t(begin)
                                                       : size_t(end - begin));
      if(module.contains("@" + entry + "(") ||
         module.contains("@\"" + entry + "\"(")) return module;
      begin = next;
    }
    return "; Could not locate an editable AIR module for this entry.\n" + text;
  }
  auto cached = m_LibraryDisassembly.find(library->second);
  if(cached != m_LibraryDisassembly.end()) return cached->second;
  auto binary = m_LibraryBinaries.find(library->second);
  if(binary == m_LibraryBinaries.end()) return "; No captured metallib bytes available.";

  // Same external-tool model as AMD ISA support; invoke Apple's installed tool directly,
  // never a shell or a path supplied by the capture. Unique files are removed after use.
  char path[] = "/tmp/renderdoc-metal-air-XXXXXX";
  int fd = mkstemp(path);
  if(fd < 0) return "; Could not create AIR inspection input.";
  close(fd);
  FileIO::WriteAll(path, binary->second);
  Process::ProcessResult result = {};
  Process::LaunchProcess("/usr/bin/xcrun", "",
      rdcstr("metal-objdump --metallib --disassemble ") + path, true, &result);
  unlink(path);
  rdcstr text;
  if(result.retCode != 0 || result.strStdout.find("define ") < 0)
    text = "; AIR disassembly unavailable. Requires Xcode's Metal toolchain (metal-objdump).\n; " +
           result.strStderror;
  else
    text = "; Captured Metal AIR intermediate code, not Apple GPU machine ISA or original HLSL.\n" +
           result.strStdout;
  m_LibraryDisassembly[library->second] = text;
  return text;
}

void MetalReplay::ResolveUniformBindlessAccess(bool cpuOnly)
{
  m_SelectedBindlessDescriptors.clear();
  if(!m_MetalPipelineState) return;
  for(uint32_t stage : {0U, 1U, 2U, 3U, 4U})
  {
    const ResourceId shader = stage == 0 ? m_MetalPipelineState->computeShader.resourceId :
                             stage == 3 ? m_MetalPipelineState->taskShader.resourceId :
                             stage == 4 ? m_MetalPipelineState->meshShader.resourceId :
                             stage == 1 ? m_MetalPipelineState->vertexShader.resourceId
                                       : m_MetalPipelineState->fragmentShader.resourceId;
    auto reflection = m_Shaders.find(shader);
    auto library = m_ShaderLibraries.find(shader);
    if(reflection == m_Shaders.end() || library == m_ShaderLibraries.end() ||
       !m_LibraryBinaries.count(library->second)) continue;
    const auto &buffers = stage == 0 ? m_MetalPipelineState->computeBuffers :
                          stage == 3 ? m_MetalPipelineState->taskBuffers :
                          stage == 4 ? m_MetalPipelineState->meshBuffers :
                          stage == 1 ? m_MetalPipelineState->vertexStorageBuffers
                                    : m_MetalPipelineState->fragmentBuffers;
    std::map<unsigned, MetalAIR::Value> bindings;
    for(size_t slot = 0; slot < buffers.size(); slot++)
    {
      MetalAIR::Value value; value.kind = MetalAIR::Value::Pointer;
      if(buffers[slot].resourceId != ResourceId())
      {
        memcpy(&value.object, &buffers[slot].resourceId, sizeof(value.object));
        value.offset = buffers[slot].byteOffset;
        value.metadata=m_pDriver->IsIRDescriptorMetadataAddress(buffers[slot].resourceId,value.offset);
      }
      else if(m_SelectedGraphicsInlineData.count({stage, uint32_t(slot)})) {value.slot = int(slot);value.metadata=true;}
      else continue;
      bindings[uint32_t(slot)] = value;
    }
    if(bindings.empty()) continue;
    std::map<std::tuple<ResourceId,uint64_t,unsigned>,bytebuf> readCache;
    auto readBytes = [&](ResourceId buffer, uint64_t offset, unsigned bytes) {
      const auto key=std::make_tuple(buffer,offset,bytes);
      const auto cached=readCache.find(key);
      if(cached!=readCache.end())return cached->second;
      bytebuf data;
      if(!cpuOnly) GetBufferData(buffer, offset, bytes, data);
      else
      {
        if(m_pDriver->ReadProvenIRComputeUniformBytes(m_UniformInspectionOffset,buffer,offset,bytes,data))
          return data;
        // Called after this submission's captured CPU writes are restored, before commit.
        // Never submit a readback or infer GPU-written values from stale CPU contents.
        auto slot = m_pDriver->m_DescriptorSlotShadow.find({buffer, offset - offset % 24});
        if(slot != m_pDriver->m_DescriptorSlotShadow.end() && slot->second.live &&
           !slot->second.gpuExpected && slot->second.type == 5 &&
           offset % 24 + bytes <= slot->second.data.size())
          data.assign(slot->second.data.data() + offset % 24, bytes);
        else if(!m_pDriver->IsDescriptorGPUWritten(buffer))
        {
          auto *object = m_pDriver->GetResourceManager()->GetResource(buffer, true);
          MTL::Buffer *real = object && object->m_Type == eResBuffer
                                 ? Unwrap((WrappedMTLBuffer *)object) : NULL;
          bool gpuWritten = false;
          auto uses = m_ResourceUses.find(buffer);
          if(uses != m_ResourceUses.end())
            for(const auto &use : uses->second)
              gpuWritten |= (use.usage >= ResourceUsage::VS_RWResource &&
                             use.usage <= ResourceUsage::All_RWResource) ||
                            use.usage == ResourceUsage::CopyDst || use.usage == ResourceUsage::Copy ||
                            use.usage == ResourceUsage::ResolveDst;
          if(!gpuWritten && real && real->storageMode() == MTL::StorageModeShared && real->contents() &&
             offset <= real->length() && bytes <= real->length() - offset)
            data.assign((const byte *)real->contents() + offset, bytes);
        }
      }
      readCache[key]=data;return data;
    };
    auto load = [&](const MetalAIR::Value &address, unsigned bytes, MetalAIR::Value::Kind kind) {
      MetalAIR::Value result;
      ResourceId buffer; memcpy(&buffer, &address.object, sizeof(buffer));
      if(address.ranged)
      {
        if(kind!=MetalAIR::Value::Integer || !address.offsetKnown || address.slot>=0 ||
           !bytes || bytes>8 || address.High()<address.offset)return result;
        const uint64_t span=address.High()-address.offset+bytes;
        const auto description=GetBuffer(buffer);
        if(span<bytes || span>64*1024 || address.offset%bytes || span%bytes ||
           address.High()>description.length || bytes>description.length-address.High())return result;
        const auto data=readBytes(buffer,address.offset,unsigned(span));
        if(data.size()!=span)return result;
        uint64_t low=UINT64_MAX,high=0;
        for(uint64_t i=0;i<span;i+=bytes)
        {uint64_t scalar=0;memcpy(&scalar,data.data()+i,bytes);low=RDCMIN(low,scalar);high=RDCMAX(high,scalar);}
        return MetalAIR::Value::Range(low,high);
      }
      if(address.slot >= 0)
      {
        auto in = m_SelectedGraphicsInlineData.find({stage, uint32_t(address.slot)});
        if(in == m_SelectedGraphicsInlineData.end()) return result;
        if(kind == MetalAIR::Value::Pointer)
        {
          auto pointer = in->second.pointers.find(address.offset);
          if(pointer == in->second.pointers.end()) return result;
          result.kind = kind;
          memcpy(&result.object, &pointer->second.first, sizeof(result.object));
          result.offset = pointer->second.second;
          result.metadata=m_pDriver->IsIRDescriptorMetadataAddress(pointer->second.first,result.offset);
        }
        else if(kind == MetalAIR::Value::Integer && address.offset <= in->second.data.size() &&
                bytes <= in->second.data.size() - address.offset)
        {
          result.kind = kind;
          memcpy(&result.offset, in->second.data.data() + address.offset, bytes);
        }
        return result;
      }
      auto description = GetBuffer(buffer);
      if(description.resourceId == ResourceId() || address.offset > description.length ||
         bytes > description.length - address.offset) return result;
      if(kind == MetalAIR::Value::Integer)
      {
        bytebuf data = readBytes(buffer, address.offset, bytes);
        if(data.size() == bytes)
        { result.kind = kind; memcpy(&result.offset, data.data(), bytes); }
      }
      else if(kind == MetalAIR::Value::AccelerationStructure)
      {
        // Only the public runtime Header's typed AS field is inspected. The
        // acceleration structure's internal allocation and shader remain opaque.
        const auto header=m_pDriver->m_RayASHeaderCurrent.find({buffer,address.offset});
        if(header==m_pDriver->m_RayASHeaderCurrent.end() ||
           !m_pDriver->ValidateRayASHeader(header->second)) return result;
        auto object=m_pDriver->GetResourceManager()->GetResource(header->second.structure,true);
        if(!object || object->m_Type!=eResAccelerationStructure || !object->m_Real)return result;
        if(!cpuOnly)
        {
          const auto data=readBytes(buffer,address.offset,8);uint64_t handle=0;
          if(data.size()==8)memcpy(&handle,data.data(),8);
          if(!handle || handle!=Unwrap((WrappedMTLAccelerationStructure *)object)->gpuResourceID()._impl)
            return result;
        }
        result=address;result.kind=kind;
      }
      else if(kind == MetalAIR::Value::Sampler)
      {
        const auto slot=m_pDriver->m_DescriptorSlotShadow.find({buffer,address.offset});
        if(slot==m_pDriver->m_DescriptorSlotShadow.end() || !slot->second.live ||
           slot->second.type!=7 || slot->second.gpuExpected)return result;
        const auto source=slot->second.sources.find(2);
        if(source==slot->second.sources.end() || source->second.offset)return result;
        auto *wrapped=m_pDriver->GetResourceManager()->GetResource(source->second.resource,true);
        if(!wrapped || wrapped->m_Type!=eResSamplerState || !wrapped->m_Real)return result;
        if(!cpuOnly)
        {
          const auto data=readBytes(buffer,address.offset,8);uint64_t handle=0;
          if(data.size()==8)memcpy(&handle,data.data(),8);
          if(!handle || handle!=Unwrap((WrappedMTLSamplerState *)wrapped)->gpuResourceID()._impl)return result;
        }
        result=address;result.kind=kind;
      }
      else if(kind == MetalAIR::Value::Texture)
      {
        // Verify the current table's authoritative source AND its actual native handle.
        // No heap-wide enumeration or inference from render targets is involved.
        if(address.offset < 8 || (address.offset - 8) % 24) return result;
        const uint64_t entry = address.offset - 8;
        auto slot = m_pDriver->m_DescriptorSlotShadow.find({buffer, entry});
        if(slot == m_pDriver->m_DescriptorSlotShadow.end() || !slot->second.live) return result;
        auto source = slot->second.sources.find(1);
        if(source == slot->second.sources.end()) return result;
        ResourceId texture = source->second.resource;
        auto *wrapped = m_pDriver->GetResourceManager()->GetResource(texture, true);
        if(!wrapped || wrapped->m_Type != eResTexture || !wrapped->m_Real) return result;
        if(!cpuOnly)
        {
          bytebuf data = readBytes(buffer, address.offset, 8);
          uint64_t handle = 0; if(data.size() == 8) memcpy(&handle, data.data(), 8);
          if(!handle || handle != Unwrap((WrappedMTLTexture *)wrapped)->gpuResourceID()._impl) return result;
        }
        result = address; result.kind = kind;
      }
      else if(kind == MetalAIR::Value::Pointer && !(address.offset % 8))
      {
        auto slot = m_pDriver->m_DescriptorSlotShadow.find({buffer, address.offset - address.offset % 24});
        if(slot == m_pDriver->m_DescriptorSlotShadow.end() || !slot->second.live) return result;
        auto source = slot->second.sources.find(uint32_t(address.offset % 24 / 8));
        bool headerSource=false;
        if(source==slot->second.sources.end() && !(address.offset%24) &&
           slot->second.type==4 && slot->second.sources.size()==1 && slot->second.sources.count(3))
        {source=slot->second.sources.find(3);headerSource=true;}
        if(source == slot->second.sources.end() ||
           GetBuffer(source->second.resource).resourceId == ResourceId()) return result;
        if(!cpuOnly)
        {
          auto *object = m_pDriver->GetResourceManager()->GetResource(source->second.resource, true);
          bytebuf data = readBytes(buffer, address.offset, 8);
          uint64_t pointer = 0; if(data.size() == 8) memcpy(&pointer, data.data(), 8);
          if(!object || !object->m_Real || !pointer ||
             pointer != Unwrap((WrappedMTLBuffer *)object)->gpuAddress() + source->second.offset)
            return result;
        }
        result.kind = kind;
        memcpy(&result.object, &source->second.resource, sizeof(result.object));
        result.offset = source->second.offset;
        memcpy(&result.descriptorObject,&buffer,sizeof(result.descriptorObject));
        result.descriptorOffset=address.offset;
        result.metadata=headerSource;
      }
      return result;
    };
    const rdcstr air = DisassembleShader(ResourceId(), &reflection->second, "Metal AIR (Apple toolchain)");
    const ResourceId selectedPipeline=stage==0?m_MetalPipelineState->computePipelineResourceId:
        m_MetalPipelineState->pipelineResourceId;
    const auto proved=m_pDriver->m_IRRuntimeDescriptorAccesses.find({m_UniformInspectionOffset,stage});
    bool useProved=(stage<=2) && proved!=m_pDriver->m_IRRuntimeDescriptorAccesses.end() &&
        m_ShaderReplacements.empty();
    if(useProved)
      for(const auto &access:proved->second)
        useProved &= access.pipeline==selectedPipeline;
    const auto completeness=m_pDriver->m_IRRuntimeDescriptorAccessComplete.find({m_UniformInspectionOffset,stage});
    if(useProved && completeness!=m_pDriver->m_IRRuntimeDescriptorAccessComplete.end() &&
       !completeness->second && Process::GetEnvVariable("RENDERDOC_METAL_TRACE_UNIFORM_PROOFS")=="1")
      RDCLOG("MetalUniformInspection chunk=%llu stage=%u accessDisplay=partial; Native binding restoration independent",
          (unsigned long long)m_UniformInspectionOffset,stage);
    const auto report=useProved?MetalAIR::UniformAccessReport():
        MetalAIR::UniformResourceAccess(air.c_str(), reflection->second.entryPoint.c_str(), bindings, load,{},true);
    if(cpuOnly && Process::GetEnvVariable("RENDERDOC_METAL_TRACE_UNIFORM_PROOFS")=="1")
      RDCLOG("MetalUniformInspection chunk=%llu stage=%u valid=%u AS=%u unknownAS=%u texture=%u unknownTexture=%u sampler=%u unknownSampler=%u bufferRead=%u bufferWrite=%u unknownBuffer=%u",
          (unsigned long long)m_UniformInspectionOffset,stage,uint32_t(report.validModule),
          report.queryResets,report.unresolvedStructures,report.textureCalls,report.unresolvedTextures,
          report.samplerCalls,report.unresolvedSamplers,report.bufferReads,report.bufferWrites,report.unresolvedBuffers);
    auto reads = report.accesses;
    for(const auto &buffer:report.buffers)
    {auto access=buffer.address;access.kind=MetalAIR::Value::Buffer;access.write=buffer.write;reads.push_back(access);}
    if(useProved)
    {
      reads.clear();
      for(const auto &access:proved->second)
      {
        if(access.pipeline!=selectedPipeline)continue;
        MetalAIR::Value value;value.kind=MetalAIR::Value::Kind(access.kind);
        value.object=access.object;value.offset=access.offset;
        value.descriptorObject=access.descriptorObject;value.descriptorOffset=access.descriptorOffset;
        value.write=access.write;value.resourceOnly=access.resourceOnly;reads.push_back(value);
      }
    }
    for(const auto &read : reads)
    {
      const bool structure=read.kind==MetalAIR::Value::AccelerationStructure;
      const bool sampler=read.kind==MetalAIR::Value::Sampler;
      const bool buffer=read.kind==MetalAIR::Value::Buffer;
      if((structure || buffer) && !read.descriptorObject)continue;
      ResourceId table;const uint64_t object=structure || buffer?read.descriptorObject:read.object;
      memcpy(&table, &object, sizeof(table));
      const uint64_t entry = structure || buffer?read.descriptorOffset:sampler?read.offset:read.offset - 8;
      if(entry > UINT32_MAX) continue;
      auto slot = m_pDriver->m_DescriptorSlotShadow.find({table, entry});
      if(slot == m_pDriver->m_DescriptorSlotShadow.end()) continue;
      auto source = slot->second.sources.find(structure?3:sampler?2:buffer?0:1);
      if(source == slot->second.sources.end()) continue;
      BindlessDescriptor binding;
      binding.access.stage = stage == 0 ? ShaderStage::Compute :
                             stage == 3 ? ShaderStage::Task : stage == 4 ? ShaderStage::Mesh :
                             stage == 1 ? ShaderStage::Vertex : ShaderStage::Fragment;
      const bool write=read.write || (read.resourceOnly && MetalDescriptor::Writable(slot->second.type));
      binding.access.type = structure?DescriptorType::AccelerationStructure:sampler?DescriptorType::Sampler:
                           buffer?(write?DescriptorType::ReadWriteBuffer:
                               MetalDescriptor::ConstantBuffer(slot->second.type)?DescriptorType::ConstantBuffer:DescriptorType::Buffer):
                           write ? DescriptorType::ReadWriteImage : DescriptorType::Image;
      binding.access.index = DescriptorAccess::NoShaderBinding;
      binding.access.descriptorStore = table;
      binding.access.byteOffset = uint32_t(entry);
      binding.access.arrayElement = uint32_t(entry / 24);
      binding.access.byteSize = 24;
      binding.descriptor.type = binding.access.type;
      if(structure)
      {
        ResourceId headerBuffer;memcpy(&headerBuffer,&read.object,sizeof(headerBuffer));
        const auto header=m_pDriver->m_RayASHeaderCurrent.find({headerBuffer,read.offset});
        if(header==m_pDriver->m_RayASHeaderCurrent.end() || source->second.resource!=headerBuffer ||
           source->second.offset!=read.offset)continue;
        binding.descriptor.resource=header->second.structure;
      }
      else if(buffer)
      {
        ResourceId actual;memcpy(&actual,&read.object,sizeof(actual));
        if(actual!=source->second.resource)continue;
        const auto description=GetBuffer(actual);
        if(description.resourceId==ResourceId() || source->second.offset>description.length)continue;
        binding.descriptor.resource=actual;binding.descriptor.byteOffset=source->second.offset;
        binding.descriptor.byteSize=description.length-source->second.offset;
        // IR buffer-view metadata encodes byte length in its low 32 bits;
        // higher bits describe typed views. An unsized legacy entry still
        // exposes the remaining native allocation, as before.
        if(slot->second.data.size()>=24)
        {
          uint32_t length=0;memcpy(&length,slot->second.data.data()+16,4);
          if(length)
          {if(length>binding.descriptor.byteSize)continue;binding.descriptor.byteSize=length;}
        }
      }
      else if(sampler)
        binding.descriptor.resource=source->second.resource;
      else
      {
        binding.descriptor.resource = source->second.resource;
        const auto texture = GetTexture(binding.descriptor.resource);
        binding.descriptor.textureType = texture.type;
        binding.descriptor.format = texture.format;
        binding.descriptor.numMips = uint8_t(texture.mips);
        binding.descriptor.numSlices = uint16_t(texture.arraysize);
        if(texture.type==TextureType::Buffer)
        {
          const ResourceId parent=GetBufferTextureSource(source->second.resource);
          auto viewObject=m_pDriver->GetResourceManager()->GetResource(source->second.resource,true);
          auto real=viewObject && viewObject->m_Type==eResTexture && viewObject->m_Real?
              Unwrap((WrappedMTLTexture *)viewObject):NULL;
          uint32_t bw=0,bh=0,bytes=0;
          if(parent==ResourceId() || !real || !GetTextureDataBlockShape(real->pixelFormat(),bw,bh,bytes) ||
             bw!=1 || bh!=1 || !bytes)continue;
          binding.access.type=binding.descriptor.type=write?DescriptorType::ReadWriteTypedBuffer:DescriptorType::TypedBuffer;
          binding.descriptor.resource=parent;binding.descriptor.view=source->second.resource;
          binding.descriptor.byteOffset=real->bufferOffset();binding.descriptor.byteSize=real->width()*bytes;
        }
      }
      bool duplicate = false;
      for(auto &other : m_SelectedBindlessDescriptors)
      {
        duplicate |= other.access.stage == binding.access.stage &&
                     other.access.descriptorStore == table && other.access.byteOffset == entry;
        if(other.access.stage == binding.access.stage && other.access.descriptorStore == table &&
           other.access.byteOffset == entry && write)
          other.access.type = other.descriptor.type = binding.access.type;
      }
      if(!duplicate) m_SelectedBindlessDescriptors.push_back(binding);
    }
  }
}

rdcarray<DescriptorLogicalLocation> MetalReplay::GetDescriptorLocations(
    ResourceId descriptorStore, const rdcarray<DescriptorRange> &ranges)
{
  rdcarray<DescriptorLogicalLocation> locations;
  for(const auto &range : ranges)
    for(uint32_t i = 0; i < range.count; i++)
    {
      DescriptorLogicalLocation location;
      const uint32_t offset = range.offset + i * range.descriptorSize;
      if(descriptorStore == GetResID(m_pDriver))
      {
        if(offset >= 0x50000 && offset < 0x50006)
        {
          location.category = offset == 0x50001 ? DescriptorCategory::ReadWriteResource
                                               : DescriptorCategory::ReadOnlyResource;
          const char *names[] = {"MetalFX Color", "MetalFX Output", "MetalFX Depth",
                                 "MetalFX Motion Vectors", "MetalFX Exposure", "MetalFX Reactive Mask"};
          location.logicalBindName = (!m_MetalPipelineState || m_MetalPipelineState->metalFXTemporal.empty()) && offset == 0x50000 ?
                                    "MetalFX Input" : names[offset-0x50000];
        }
        else if(offset >= 0x40000 && offset < 0x40008)
        {
          location.category = DescriptorCategory::ReadOnlyResource;
          location.logicalBindName = StringFormat::Fmt("Input Attachment: color[%u]", offset - 0x40000);
        }
      }
      for(const auto &binding : m_SelectedBindlessDescriptors)
        if(binding.access.descriptorStore == descriptorStore && binding.access.byteOffset == offset)
        {
          location.category = (binding.access.type == DescriptorType::ReadWriteImage ||
                               binding.access.type == DescriptorType::ReadWriteBuffer ||
                               binding.access.type == DescriptorType::ReadWriteTypedBuffer)
                                  ? DescriptorCategory::ReadWriteResource
                                  : DescriptorCategory::ReadOnlyResource;
          location.fixedBindNumber = offset / 24;
          location.logicalBindName = StringFormat::Fmt("Heap[%u] (uniform AIR)", offset / 24);
        }
      locations.push_back(location);
    }
  return locations;
}

void MetalReplay::PopulateShaderFeatures(MetalPipe::Shader &shader)
{
  if(shader.resourceId == ResourceId() || !shader.reflection) return;
  ResourceId live = shader.reflection->resourceId;
  auto cached = m_ShaderFeatures.find(live);
  if(cached == m_ShaderFeatures.end())
  {
    MetalPipe::Shader features;
    MetalFeatures::Features parsed;
    auto library = m_ShaderLibraries.find(live);
    rdcstr entry = shader.reflection->debugInfo.entrySourceName.empty() ? shader.entryPoint
                                            : shader.reflection->debugInfo.entrySourceName;
    if(library != m_ShaderLibraries.end())
    {
      auto source = m_LibrarySources.find(library->second);
      if(source != m_LibrarySources.end() && !source->second.empty())
      {
        parsed = MetalFeatures::MSL(source->second.c_str(), entry.c_str());
        if(parsed.known) features.metadataSource = "Captured MSL declaration";
      }
      else if(m_LibraryBinaries.count(library->second))
      {
        parsed = MetalFeatures::AIR(DisassembleShader(ResourceId(), shader.reflection,
                                   "Metal AIR (Apple toolchain)").c_str(), entry.c_str());
        if(parsed.known) features.metadataSource = "Compiled AIR entry metadata";
      }
    }
    if(shader.stage == ShaderStage::Fragment)
      for(unsigned slot : parsed.fetch) features.framebufferFetch.push_back(slot);
    for(unsigned group : parsed.groups) features.rasterOrderGroups.push_back(group);
    features.usesImageblock = parsed.imageblock;
    cached = m_ShaderFeatures.insert({live, features}).first;
  }
  shader.metadataSource = cached->second.metadataSource;
  shader.framebufferFetch = cached->second.framebufferFetch;
  shader.rasterOrderGroups = cached->second.rasterOrderGroups;
  shader.usesImageblock = cached->second.usesImageblock;
}
