// SPDX-License-Identifier: MIT
#include "metal_replay.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_render_pipeline_state.h"
#include "metal_compute_pipeline_state.h"
#include <unistd.h>

static id MetalGet(id object, const char *selector)
{
  return ((id (*)(id, SEL))objc_msgSend)(object, sel_registerName(selector));
}
static void MetalSet(id object, const char *selector, id value)
{
  ((void (*)(id, SEL, id))objc_msgSend)(object, sel_registerName(selector), value);
}

void MetalReplay::CacheShaderPipeline(ResourceId pipeline, NS::Object *descriptor, uint32_t kind)
{
  auto &cached = m_ShaderPipelineTemplates[pipeline];
  // Frame-born pipelines can be recreated while a shader mapping is active. Keep
  // the first captured template, otherwise a later RemoveReplacement loses the original.
  if(cached.descriptor) return;
  cached.descriptor = (NS::Object *)MetalGet((id)descriptor, "copy");
  cached.kind = kind;
}

void MetalReplay::SetShaderSpecialization(ResourceId shader, const MetalFunctionSnapshot &snapshot)
{
  m_FunctionSpecializations[shader] = snapshot;
  m_Shaders[shader].debugInfo.entrySourceName = snapshot.name;
}

void MetalReplay::BuildTargetShader(ShaderEncoding encoding, const bytebuf &source,
    const rdcstr &entry, const ShaderCompileFlags &flags, ShaderStage stage,
    ResourceId &shader, rdcstr &errors)
{
  shader = ResourceId();
  errors.clear();
  if(source.empty() || entry.empty()) { errors = "Empty Metal source or entry point."; return; }
  if(stage != ShaderStage::Vertex && stage != ShaderStage::Fragment &&
     stage != ShaderStage::Compute && stage != ShaderStage::Task && stage != ShaderStage::Mesh)
  { errors = "This shader stage has no Metal replacement compiler."; return; }
  ResourceId original;
  for(const auto &flag : flags.flags)
    if(flag.name == "@metal_shader")
      for(const auto &s : m_Shaders)
        if(ToStr(s.first) == flag.value) original = s.first;

  MTL::Device *device = Unwrap(m_pDriver);
  NS::Error *error = NULL;
  MTL::Library *library = NULL;
  bytebuf binary = source;
  if(encoding == ShaderEncoding::MetalAIRAsm)
  {
    // A private, fixed-name temporary directory and direct process launch: shader text and
    // capture names never become command-line code. Apple's compiler verifies the AIR module.
    char path[] = "/tmp/renderdoc-metal-edit-XXXXXX";
    if(!mkdtemp(path)) { errors = "Could not create Metal compiler directory."; return; }
    rdcstr dir = path, input = dir + "/edited.ll", air = dir + "/edited.air";
    rdcstr output = dir + "/edited.metallib";
    if(!FileIO::WriteAll(input, source)) errors = "Could not write AIR compiler input.";
    rdcstr language;
    const rdcstr text((const char *)source.data(), source.size());
    // AIR version is tied to its recorded Metal language version. The host default can
    // be newer than the captured module (e.g. MSL3.2/AIR2.7 versus MSL4.0/AIR2.8).
    int32_t marker = text.find("!air.language_version = !{!");
    unsigned metadata = 0, major = 0, minor = 0, patch = 0;
    if(marker >= 0 && sscanf(text.c_str() + marker, "!air.language_version = !{!%u", &metadata) == 1)
    {
      rdcstr key = StringFormat::Fmt("\n!%u = !{!\"Metal\", i32 ", metadata);
      int32_t value = text.find(key);
      if(value >= 0 && sscanf(text.c_str() + value + key.size(), "%u, i32 %u, i32 %u",
                             &major, &minor, &patch) == 3 && major >= 1 && major <= 4 && minor <= 3)
        language = StringFormat::Fmt(" -std=metal%u.%u", major, minor);
    }
    int32_t airVersion = text.find("!air.version = !{!");
    unsigned airMeta = 0, airMajor = 0, airMinor = 0;
    if(airVersion >= 0 && sscanf(text.c_str() + airVersion, "!air.version = !{!%u", &airMeta) == 1)
    {
      const rdcstr key = StringFormat::Fmt("\n!%u = !{i32 ", airMeta);
      const int32_t value = text.find(key);
      if(value >= 0 && sscanf(text.c_str() + value + key.size(), "%u, i32 %u",
                             &airMajor, &airMinor) == 2 && airMajor == 2 && airMinor <= 9)
      {
        // The linker validates AIR metadata against the versioned target triple.
        // LLVM input must retain that version, rather than the host driver default.
        language += StringFormat::Fmt(" -target air64_v%u%u-apple-macosx", airMajor, airMinor);
        unsigned osMajor = 0, osMinor = 0, osPatch = 0;
        const int32_t target = text.find("-apple-macosx");
        if(target >= 0 && sscanf(text.c_str()+target+13, "%u.%u.%u", &osMajor, &osMinor, &osPatch) >= 1 && osMajor >= 10 && osMajor <= 99)
          language += StringFormat::Fmt("%u.%u.%u",osMajor,osMinor,osPatch);
        else language += "15.0.0";
      }
    }
    Process::ProcessResult result = {};
    if(errors.empty())
    {
      Process::LaunchProcess("/usr/bin/xcrun", "", "metal -c" + language + " " + input + " -o " + air,
                             true, &result);
      if(result.retCode != 0) errors = result.strStderror + result.strStdout;
      else
      {
        Process::LaunchProcess("/usr/bin/xcrun", "", "metallib " + air + " -o " + output,
                               true, &result);
        if(result.retCode != 0) errors = result.strStderror + result.strStdout;
        else if(!FileIO::ReadAll(output, binary) || binary.empty()) errors = "No metallib output.";
      }
    }
    unlink(input.c_str()); unlink(air.c_str()); unlink(output.c_str()); rmdir(path);
    if(!errors.empty()) return;
  }
  if(encoding == ShaderEncoding::MSL)
  {
    rdcstr text((const char *)source.data(), source.size());
    MTL::CompileOptions *options = MTL::CompileOptions::alloc()->init();
    library = device->newLibrary(NS::String::string(text.c_str(), NS::UTF8StringEncoding),
                                 options, &error);
    options->release();
  }
  else if(encoding == ShaderEncoding::MetalLib || encoding == ShaderEncoding::MetalAIRAsm)
  {
    if(binary.size() < 4 || memcmp(binary.data(), "MTLB", 4))
    { errors = "Expected a compiled MTLB Metal library."; return; }
    dispatch_data_t data = dispatch_data_create(binary.data(), binary.size(),
        dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    library = device->newLibrary(data, &error);
    dispatch_release(data);
  }
  else { errors = "Unsupported Metal shader encoding."; return; }
  if(!library)
  { errors = error ? error->localizedDescription()->utf8String() : "Metal library compilation failed."; return; }

  MTL::Function *function = NULL;
  auto specialization = m_FunctionSpecializations.find(original);
  if(specialization != m_FunctionSpecializations.end())
  {
    const auto &snapshot = specialization->second;
    auto values = MTL::FunctionConstantValues::alloc()->init();
    for(size_t i = 0; i < snapshot.constantValues.size(); i++)
      if(snapshot.constantNames[i].empty())
        values->setConstantValue(snapshot.constantValues[i].data(),
            MTL::DataType(snapshot.constantTypes[i]), snapshot.constantIndices[i]);
      else
        values->setConstantValue(snapshot.constantValues[i].data(),
            MTL::DataType(snapshot.constantTypes[i]),
            NS::String::string(snapshot.constantNames[i].c_str(), NS::UTF8StringEncoding));
    auto descriptor = MTL::FunctionDescriptor::alloc()->init();
    descriptor->setName(NS::String::string(entry.c_str(), NS::UTF8StringEncoding));
    descriptor->setConstantValues(values);
    if(!snapshot.specializedName.empty())
      descriptor->setSpecializedName(NS::String::string(snapshot.specializedName.c_str(), NS::UTF8StringEncoding));
    function = library->newFunction(descriptor, &error);
    descriptor->release(); values->release();
  }
  else function = library->newFunction(NS::String::string(entry.c_str(), NS::UTF8StringEncoding));
  MTL::FunctionType expected = stage == ShaderStage::Vertex ? MTL::FunctionTypeVertex :
      stage == ShaderStage::Fragment ? MTL::FunctionTypeFragment :
      stage == ShaderStage::Task ? MTL::FunctionTypeObject :
      stage == ShaderStage::Mesh ? MTL::FunctionTypeMesh : MTL::FunctionTypeKernel;
  if(!function || function->functionType() != expected)
  {
    errors = error ? error->localizedDescription()->utf8String() :
                    "Entry point was not found or its Metal function type does not match the selected stage.";
    if(function) function->release(); library->release(); return;
  }
  WrappedMTLFunction *wrapped = NULL;
  shader = m_pDriver->GetResourceManager()->WrapResource(ResourceId(), function, wrapped, true);
  m_TargetShaderLibraries[shader] = library;
  if(encoding == ShaderEncoding::MSL)
    AddShaderLibrary(shader, rdcstr((const char *)source.data(), source.size()));
  else AddShaderBinary(shader, binary);
  AddShader(shader, shader, function, entry);
  if(original != ResourceId())
    m_Shaders[shader].debugInfo.compileFlags = m_Shaders[original].debugInfo.compileFlags;
  if(specialization != m_FunctionSpecializations.end())
  {
    auto snapshot = specialization->second; snapshot.name = entry;
    SetShaderSpecialization(shader, snapshot);
  }
  if(original != ResourceId())
  {
    const auto previous = m_ShaderReplacements;
    m_ShaderReplacements[original] = shader;
    const bool valid = RefreshShaderReplacements(errors, false);
    m_ShaderReplacements = previous;
    if(!valid) { FreeTargetResource(shader); shader = ResourceId(); }
  }
}

void MetalReplay::ReleaseShaderPipeline(ResourceId pipeline)
{
  auto *manager = m_pDriver->GetResourceManager();
  auto *wrapped = manager->GetResource(pipeline, true);
  if(wrapped) manager->ReleaseReplayResource(wrapped);
  m_RenderPipelines.erase(pipeline); m_ComputePipelines.erase(pipeline);
  m_MeshPipelines.erase(pipeline); m_ObjectPipelines.erase(pipeline); m_TilePipelines.erase(pipeline);
  m_ComputeReadOnlyBuffers.erase(pipeline); m_ComputeBufferMinimums.erase(pipeline);
  m_ComputeRequiredTextures.erase(pipeline); m_ComputeRequiredSamplers.erase(pipeline);
  m_ComputeThreadgroupMinimums.erase(pipeline); m_ComputeThreadgroupLimits.erase(pipeline);
  m_ComputeThreadExecutionMultiples.erase(pipeline);
}

bool MetalReplay::RefreshShaderReplacements(rdcstr &errors, bool publish)
{
  auto *manager = m_pDriver->GetResourceManager();
  struct Pending
  {
    ResourceId original;
    NS::Object *descriptor = NULL, *pipeline = NULL, *reflection = NULL;
    uint32_t kind = 0;
  };
  rdcarray<Pending> pending;
  auto releasePending = [&]() {
    for(auto &p : pending)
    { p.descriptor->release(); p.pipeline->release(); if(p.reflection) p.reflection->release(); }
  };
  // Like D3D12/Vulkan RefreshDerivedReplacements, compile all dependent pipelines first,
  // then publish their resource-manager mappings as one transaction.
  for(const auto &entry : m_ShaderPipelineTemplates)
  {
    const uint32_t kind = entry.second.kind;
    NS::Object *descriptor = (NS::Object *)MetalGet((id)entry.second.descriptor, "copy");
    bool changed = false;
    const char *getters[3] = {kind == 1 ? "computeFunction" : kind == 2 ? "meshFunction" :
                             kind == 3 ? "tileFunction" : "vertexFunction",
                             kind == 1 || kind == 3 ? NULL : "fragmentFunction",
                             kind == 2 ? "objectFunction" : NULL};
    const char *setters[3] = {kind == 1 ? "setComputeFunction:" : kind == 2 ? "setMeshFunction:" :
                             kind == 3 ? "setTileFunction:" : "setVertexFunction:",
                             "setFragmentFunction:", "setObjectFunction:"};
    for(int i = 0; i < 3; i++) if(getters[i])
    {
      id original = MetalGet((id)descriptor, getters[i]);
      for(const auto &replacement : m_ShaderReplacements)
        if((id)m_FunctionObjects[replacement.first] == original)
        {
          auto *target = manager->GetResource(replacement.second, true);
          if(target && target->m_Type == eResFunction)
          { MetalSet((id)descriptor, setters[i], (id)target->m_Real); changed = true; }
        }
    }
    if(!changed) { descriptor->release(); continue; }
    // Function tables contain pipeline-specific handles. Do not submit an edited pipeline
    // against handles from another pipeline until its table dependencies can be rebuilt.
    bool linked = false;
    for(const char *name : {kind == 1 ? "linkedFunctions" : kind == 0 ? "vertexLinkedFunctions" : "linkedFunctions",
                           kind == 0 ? "fragmentLinkedFunctions" : "linkedFunctions"})
      if(((bool (*)(id, SEL, SEL))objc_msgSend)((id)descriptor,
             sel_registerName("respondsToSelector:"), sel_registerName(name)))
      {
        id links = MetalGet((id)descriptor, name);
        id functions = links ? MetalGet(links, "functions") : nil;
        linked |= functions && ((NS::Array *)functions)->count() != 0;
      }
    if(linked)
    { errors = "Editing Metal pipelines with linked function tables is not implemented."; descriptor->release(); releasePending(); return false; }
    NS::Error *error = NULL;
    Pending p; p.original = entry.first; p.descriptor = descriptor; p.kind = kind;
    const auto options = MTL::PipelineOption(MTL::PipelineOptionArgumentInfo | MTL::PipelineOptionBufferTypeInfo);
    if(kind == 1)
    {
      MTL::AutoreleasedComputePipelineReflection reflection = NULL;
      p.pipeline = Unwrap(m_pDriver)->newComputePipelineState((MTL::ComputePipelineDescriptor *)descriptor,
          options, &reflection, &error); p.reflection = reflection;
    }
    else
    {
      MTL::AutoreleasedRenderPipelineReflection reflection = NULL;
      if(kind == 0) p.pipeline = Unwrap(m_pDriver)->newRenderPipelineState((MTL::RenderPipelineDescriptor *)descriptor, options, &reflection, &error);
      else if(kind == 2) p.pipeline = Unwrap(m_pDriver)->newRenderPipelineState((MTL::MeshRenderPipelineDescriptor *)descriptor, options, &reflection, &error);
      else p.pipeline = Unwrap(m_pDriver)->newRenderPipelineState((MTL::TileRenderPipelineDescriptor *)descriptor, options, &reflection, &error);
      p.reflection = reflection;
    }
    if(!p.pipeline)
    { errors = error ? error->localizedDescription()->utf8String() : "Metal edited pipeline creation failed."; descriptor->release(); releasePending(); return false; }
    if(p.reflection) p.reflection->retain();
    pending.push_back(p);
  }
  if(!publish) { releasePending(); return true; }
  if(!m_pDriver->WaitForShaderReplacement())
  { errors = "Could not complete GPU work before changing shader pipelines."; releasePending(); return false; }
  for(const auto &previous : m_DerivedShaderPipelines)
  { manager->RemoveReplacement(previous.first); ReleaseShaderPipeline(previous.second); }
  m_DerivedShaderPipelines.clear();
  auto functionId = [&](const char *selector, NS::Object *descriptor) {
    MTL::Function *function = (MTL::Function *)MetalGet((id)descriptor, selector);
    for(const auto &f : m_FunctionObjects) if(f.second == function) return f.first;
    return ResourceId();
  };
  for(auto &p : pending)
  {
    ResourceId live;
    if(p.kind == 1)
    {
      WrappedMTLComputePipelineState *wrapped = NULL;
      live = manager->WrapResource(ResourceId(), (MTL::ComputePipelineState *)p.pipeline, wrapped, true);
      auto *descriptor = (MTL::ComputePipelineDescriptor *)p.descriptor;
      AddComputePipeline(live, functionId("computeFunction", p.descriptor),
          (MTL::ComputePipelineReflection *)p.reflection, (MTL::ComputePipelineState *)p.pipeline,
          descriptor->threadGroupSizeIsMultipleOfThreadExecutionWidth());
    }
    else
    {
      WrappedMTLRenderPipelineState *wrapped = NULL;
      live = manager->WrapResource(ResourceId(), (MTL::RenderPipelineState *)p.pipeline, wrapped, true);
      if(p.kind == 0)
      {
        auto desc = m_RenderPipelines[p.original].descriptor;
        const ResourceId vs = functionId("vertexFunction", p.descriptor);
        const ResourceId fs = functionId("fragmentFunction", p.descriptor);
        if(vs != GetResID(desc.vertexFunction))
          desc.vertexFunction = (WrappedMTLFunction *)manager->GetResource(vs, true);
        if(fs != GetResID(desc.fragmentFunction))
          desc.fragmentFunction = (WrappedMTLFunction *)manager->GetResource(fs, true);
        AddRenderPipeline(live, desc, (MTL::RenderPipelineReflection *)p.reflection);
      }
      else if(p.kind == 2)
      {
        auto info = m_RenderPipelines[p.original]; rdcarray<uint32_t> formats;
        for(const auto &c : info.colorAttachments) formats.push_back((uint32_t)c.pixelFormat);
        AddMeshPipeline(live, functionId("meshFunction", p.descriptor),
            functionId("fragmentFunction", p.descriptor), info.sampleCount,
            (MTL::RenderPipelineReflection *)p.reflection, formats,
            functionId("objectFunction", p.descriptor));
      }
      else AddTilePipeline(live, functionId("tileFunction", p.descriptor),
                            (MTL::RenderPipelineReflection *)p.reflection);
    }
    manager->ReplaceResource(p.original, live);
    m_DerivedShaderPipelines[p.original] = live;
    p.descriptor->release(); if(p.reflection) p.reflection->release();
  }
  m_LibraryDisassembly.clear(); m_SelectedBindlessDescriptors.clear();
  return true;
}

void MetalReplay::ReplaceResource(ResourceId from, ResourceId to)
{
  auto *manager = m_pDriver->GetResourceManager();
  auto *target = manager->GetResource(to, true);
  if(!m_FunctionObjects.count(from) || !target || target->m_Type != eResFunction)
  { manager->ReplaceResource(from, to); return; }
  const auto previous = m_ShaderReplacements;
  m_ShaderReplacements[from] = to;
  rdcstr errors;
  if(!RefreshShaderReplacements(errors))
  { m_ShaderReplacements = previous; RDCERR("Metal shader edit rejected: %s", errors.c_str()); return; }
  manager->RemoveReplacement(from);
  manager->ReplaceResource(from, to);
}

void MetalReplay::RemoveReplacement(ResourceId shader)
{
  auto *manager = m_pDriver->GetResourceManager();
  if(!m_ShaderReplacements.count(shader)) { manager->RemoveReplacement(shader); return; }
  const auto previous = m_ShaderReplacements;
  m_ShaderReplacements.erase(shader);
  rdcstr errors;
  if(!RefreshShaderReplacements(errors))
  { m_ShaderReplacements = previous; RDCERR("Metal shader edit removal failed: %s", errors.c_str()); return; }
  manager->RemoveReplacement(shader);
}

void MetalReplay::FreeTargetResource(ResourceId shader)
{
  auto owned = m_TargetShaderLibraries.find(shader);
  if(owned == m_TargetShaderLibraries.end()) return;
  rdcarray<ResourceId> originals;
  for(const auto &replacement : m_ShaderReplacements)
    if(replacement.second == shader) originals.push_back(replacement.first);
  for(auto original : originals) RemoveReplacement(original);
  for(const auto &replacement : m_ShaderReplacements)
    if(replacement.second == shader) return;
  if(!m_pDriver->WaitForShaderReplacement()) return;
  auto *manager = m_pDriver->GetResourceManager();
  auto *wrapped = manager->GetResource(shader, true);
  if(wrapped) manager->ReleaseReplayResource(wrapped);
  owned->second->release(); m_TargetShaderLibraries.erase(owned);
  m_FunctionObjects.erase(shader); m_Shaders.erase(shader); m_ShaderBindingUsage.erase(shader);
  m_ShaderLibraries.erase(shader); m_LibrarySources.erase(shader); m_LibraryBinaries.erase(shader);
  m_LibraryDisassembly.erase(shader); m_LibraryDebugSources.erase(shader); m_FunctionSpecializations.erase(shader);
  m_ShaderBufferMinimums.erase(shader);
  m_ShaderIndirectReadOnly.erase(shader); m_ShaderArgumentSlots.erase(shader);
}
