// SPDX-License-Identifier: MIT
// CPU-only DXC -> Apple IR conversion. External headers/libraries come from UE.
#include <dxc/dxcapi.h>
#include <metal_irconverter/metal_irconverter.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static bool Save(const std::string &path, IRMetalLibBinary *binary)
{
  const size_t size = IRMetalLibGetBytecodeSize(binary);
  if(!size) return false;
  std::vector<uint8_t> bytes(size);
  if(IRMetalLibGetBytecode(binary, bytes.data()) != size) return false;
  std::ofstream file(path, std::ios::binary);
  file.write((const char *)bytes.data(), bytes.size());
  printf("metallib %s %zu bytes\n", path.c_str(), size);
  return file.good();
}

int main(int argc, char **argv)
{
  if(argc < 3 || argc > 4) return 1;
  const bool local = argc == 4 && strstr(argv[3],"local-root");
  const bool six = local && strstr(argv[3],"six-samplers");
  const bool global = argc == 4 && strstr(argv[3],"global-");
  const bool heaps = argc == 4 && strstr(argv[3],"descriptor-heaps");
  const bool heapAS = argc == 4 && strstr(argv[3],"heap-as");
  const bool indirect = argc == 4 && strstr(argv[3],"indirect-tlas");
  const bool multi = argc == 4 && strstr(argv[3],"multi-geometry");
  const bool heapOnly = argc == 4 && strstr(argv[3],"heap-as-only");
  const bool queryMixed=argc==4 && strstr(argv[3],"mixed-static");
  const unsigned queryCBVCount=argc==4 && strstr(argv[3],"cbv6")?6:argc==4 && strstr(argv[3],"cbv5")?5:0;
  const bool query = argc == 4 && !strncmp(argv[3],"inline-query",12);
  std::ifstream file(argv[1]);
  std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  if(source.empty()) return 2;
  IDxcCompiler3 *dxc = nullptr;
  HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc));
  if(FAILED(hr) || !dxc) return 3;
  std::vector<const wchar_t *> arguments = {L"-T",query?(heaps?L"cs_6_6":L"cs_6_5"):(heaps||multi)?L"lib_6_6":L"lib_6_3",L"-HV",L"2021",L"-O3"};
  if(queryCBVCount) {arguments.push_back(L"-D");arguments.push_back(queryMixed?(queryCBVCount==6?L"CBV_ROOT_COUNT=5":L"CBV_ROOT_COUNT=4"):(queryCBVCount==6?L"CBV_ROOT_COUNT=6":L"CBV_ROOT_COUNT=5"));}
  if(argc==4 && strstr(argv[3],"texture-uav")) {arguments.push_back(L"-D");arguments.push_back(strstr(argv[3],"texture-uav-bufferuint")?L"TEXTURE_UAV=3":strstr(argv[3],"texture-uav-float4")?L"TEXTURE_UAV=2":L"TEXTURE_UAV=1");}
  if(argc==4 && strstr(argv[3],"dynamic-uav")) {arguments.push_back(L"-D");arguments.push_back(L"MULTI_UAV");}
  if(argc==4 && strstr(argv[3],"texture-dimensions")) {arguments.push_back(L"-D");arguments.push_back(L"TEXTURE_DIMENSIONS");}
  if(query) {arguments.push_back(L"-E");arguments.push_back(L"main");}
  if(multi) {arguments.push_back(L"-D");arguments.push_back(L"MULTI_GEOMETRY");}
  if(local) {arguments.push_back(L"-D");arguments.push_back(L"LOCAL_ROOT");}
  if(six) {arguments.push_back(L"-D");arguments.push_back(L"STATIC_SIX");}
  if(global) {arguments.push_back(L"-D");arguments.push_back(L"GLOBAL_ROOT");}
  if(indirect) {arguments.push_back(L"-D");arguments.push_back(L"INDIRECT_AS");}
  if(heaps) {arguments.push_back(L"-D");arguments.push_back(L"DIRECT_HEAPS");}
  if(heapAS) {arguments.push_back(L"-D");arguments.push_back(L"HEAP_AS");}
  DxcBuffer input = {source.data(), source.size(), DXC_CP_UTF8};
  IDxcResult *result = nullptr;
  hr = dxc->Compile(&input, arguments.data(), arguments.size(), nullptr,
                    IID_PPV_ARGS(&result));
  if(FAILED(hr) || !result) return 4;
  IDxcBlobEncoding *messages = nullptr;
  result->GetErrorBuffer(&messages);
  if(messages && messages->GetBufferSize())
    fwrite(messages->GetBufferPointer(), 1, messages->GetBufferSize(), stderr);
  if(messages) messages->Release();
  HRESULT status = E_FAIL;
  result->GetStatus(&status);
  if(FAILED(status)) return 5;
  IDxcBlob *dxil = nullptr;
  result->GetResult(&dxil);
  if(!dxil || !dxil->GetBufferSize()) return 6;
  const std::string output = argv[2];
  std::ofstream dxilFile(output + "/pipeline.dxil", std::ios::binary);
  dxilFile.write((const char *)dxil->GetBufferPointer(), dxil->GetBufferSize());
  dxilFile.close();
  auto object = IRObjectCreateFromDXIL((const uint8_t *)dxil->GetBufferPointer(),
                                     dxil->GetBufferSize(), IRBytecodeOwnershipNone);
  if(!object) return 7;
  auto compiler = IRCompilerCreate();
  IRRootParameter1 parameters[6] = {};
  IRDescriptorRange1 globalTextureRange = {};
  IRStaticSamplerDescriptor globalSamplers[6] = {};
  parameters[0].ParameterType = IRRootParameterTypeSRV;
  parameters[0].Descriptor.ShaderRegister = 0;
  parameters[0].ShaderVisibility = IRShaderVisibilityAll;
  parameters[1].ParameterType = IRRootParameterTypeUAV;
  parameters[1].Descriptor.ShaderRegister = 0;
  parameters[1].ShaderVisibility = IRShaderVisibilityAll;
  IRVersionedRootSignatureDescriptor description = {};
  description.version = IRRootSignatureVersion_1_1;
  description.desc_1_1.NumParameters = 2;
  description.desc_1_1.pParameters = parameters;
  if(heaps) description.desc_1_1.Flags=IRRootSignatureFlags(IRRootSignatureFlagCBVSRVUAVHeapDirectlyIndexed | IRRootSignatureFlagSamplerHeapDirectlyIndexed);
  if(global)
  {
    parameters[2].ParameterType=IRRootParameterTypeCBV;parameters[2].Descriptor.ShaderRegister=0;parameters[2].Descriptor.RegisterSpace=2;
    parameters[3].ParameterType=IRRootParameterType32BitConstants;parameters[3].Constants.ShaderRegister=1;
    parameters[3].Constants.RegisterSpace=2;parameters[3].Constants.Num32BitValues=4;
    parameters[4].ParameterType=IRRootParameterTypeSRV;parameters[4].Descriptor.ShaderRegister=1;parameters[4].Descriptor.RegisterSpace=2;
    globalTextureRange.RangeType=IRDescriptorRangeTypeSRV;globalTextureRange.NumDescriptors=1;
    globalTextureRange.BaseShaderRegister=2;globalTextureRange.RegisterSpace=2;
    parameters[5].ParameterType=IRRootParameterTypeDescriptorTable;parameters[5].DescriptorTable.NumDescriptorRanges=1;
    parameters[5].DescriptorTable.pDescriptorRanges=&globalTextureRange;
    for(unsigned i=0;i<6;i++)
    {
      auto &sampler=globalSamplers[i];sampler.ShaderRegister=i;sampler.RegisterSpace=2;
      sampler.Filter=i<2?IRFilterMinMagMipPoint:i<4?IRFilterMinMagLinearMipPoint:IRFilterMinMagMipLinear;
      sampler.AddressU=sampler.AddressV=sampler.AddressW=i%2?IRTextureAddressModeClamp:IRTextureAddressModeWrap;
      sampler.MaxAnisotropy=1;sampler.MaxLOD=1000;sampler.ComparisonFunc=IRComparisonFunctionNever;
    }
    description.desc_1_1.NumParameters=6;description.desc_1_1.NumStaticSamplers=6;
    description.desc_1_1.pStaticSamplers=globalSamplers;
  }
  if(heapOnly)
  {
    parameters[0].ParameterType=IRRootParameterTypeUAV;parameters[0].Descriptor.ShaderRegister=0;
    parameters[1]={};parameters[1].ParameterType=IRRootParameterType32BitConstants;
    parameters[1].Constants.ShaderRegister=7;parameters[1].Constants.RegisterSpace=3;parameters[1].Constants.Num32BitValues=2;
  }
  if(queryCBVCount)
  {
    const unsigned count=queryCBVCount-(queryMixed?1:0);
    description.desc_1_1.NumParameters=count;
    for(unsigned i=0;i<count;i++)
    {
      parameters[i]={};parameters[i].ParameterType=IRRootParameterTypeCBV;
      parameters[i].Descriptor.ShaderRegister=i;parameters[i].ShaderVisibility=IRShaderVisibilityAll;
    }
  }
  if(queryMixed)
  {
    for(unsigned i=0;i<6;i++)
    {
      auto &sampler=globalSamplers[i];sampler.ShaderRegister=i;
      sampler.Filter=i<2?IRFilterMinMagMipPoint:i<4?IRFilterMinMagLinearMipPoint:IRFilterMinMagMipLinear;
      sampler.AddressU=sampler.AddressV=sampler.AddressW=i%2?IRTextureAddressModeClamp:IRTextureAddressModeWrap;
      sampler.MaxAnisotropy=1;sampler.MaxLOD=1000;sampler.ComparisonFunc=IRComparisonFunctionNever;
    }
    description.desc_1_1.NumStaticSamplers=6;description.desc_1_1.pStaticSamplers=globalSamplers;
  }
  IRError *error = nullptr;
  auto root = IRRootSignatureCreateFromDescriptor(&description, &error);
  if(!root) {fprintf(stderr, "root error %u\n", error ? IRErrorGetCode(error) : 0); return 8;}
  IRCompilerSetGlobalRootSignature(compiler, root);
  const char *globalJSON=IRVersionedRootSignatureDescriptorCopyJSONString(&description);
  if(!globalJSON) return 14;
  {std::ofstream json(output+"/global-root.json");json<<globalJSON;}
  IRVersionedRootSignatureDescriptorReleaseString(globalJSON);
  if(query)
  {
    auto converted = IRCompilerAllocCompileAndLink(compiler, "main", object, &error);
    if(!converted) {fprintf(stderr,"query IR error %u: %s\n",error?IRErrorGetCode(error):0,
        error?(const char *)IRErrorGetPayload(error):"no diagnostics");return 17;}
    auto binary = IRMetalLibBinaryCreate(); auto reflection = IRShaderReflectionCreate();
    if(!IRObjectGetReflection(converted,IRShaderStageCompute,reflection)) return 18;
    const char *json = IRShaderReflectionCopyJSONString(reflection);
    if(!json) return 19;
    {std::ofstream out(output+"/query.reflection.json");out<<json;}
    IRShaderReflectionReleaseString(json); IRShaderReflectionDestroy(reflection);
    if(!IRObjectGetMetalLibBinary(converted,IRShaderStageCompute,binary) ||
       !Save(output+"/query.metallib",binary)) return 20;
    IRMetalLibBinaryDestroy(binary); IRObjectDestroy(converted); IRRootSignatureDestroy(root);
    IRCompilerDestroy(compiler); IRObjectDestroy(object); dxil->Release(); result->Release(); dxc->Release();
    puts(heaps?"PASS converted compute RayQuery cs_6_6 descriptor heap; no SBT or function tables":"PASS converted compute RayQuery cs_6_5; no SBT or function tables"); return 0;
  }
  auto config = IRRayTracingPipelineConfigurationCreate();
  IRRayTracingPipelineConfigurationSetIntrinsicMasks(config, IRIntrinsicMaskClosestHitAll,
      IRIntrinsicMaskMissShaderAll, IRIntrinsicMaskAnyHitShaderAll, IRIntrinsicMaskCallableShaderAll);
  IRRayTracingPipelineConfigurationSetMaxRecursiveDepth(config, 1);
  IRRayTracingPipelineConfigurationSetMaxAttributeSizeInBytes(config, 8);
  IRRayTracingPipelineConfigurationSetRayGenerationCompilationMode(config, IRRayGenerationCompilationVisibleFunction);
  IRRayTracingPipelineConfigurationSetIntersectionFunctionCompilationMode(config, IRIntersectionFunctionCompilationVisibleFunction);
  IRCompilerSetRayTracingPipelineConfiguration(compiler, config);
  IRCompilerSetHitgroupType(compiler, IRHitGroupTypeTriangles);
  IRRootSignature *localSignature = nullptr;
  IRRootParameter1 locals[4] = {};
  IRDescriptorRange1 textureRange = {};
  IRStaticSamplerDescriptor samplers[6] = {};
  auto &sampler = samplers[0];
  if(local)
  {
    locals[0].ParameterType = IRRootParameterTypeCBV;
    locals[0].Descriptor.ShaderRegister = 0; locals[0].Descriptor.RegisterSpace = 1;
    locals[1].ParameterType = IRRootParameterType32BitConstants;
    locals[1].Constants.ShaderRegister = 1; locals[1].Constants.RegisterSpace = 1;
    locals[1].Constants.Num32BitValues = 4;
    locals[2].ParameterType = IRRootParameterTypeSRV;
    locals[2].Descriptor.ShaderRegister = 1; locals[2].Descriptor.RegisterSpace = 1;
    textureRange.RangeType = IRDescriptorRangeTypeSRV; textureRange.NumDescriptors = 1;
    textureRange.BaseShaderRegister = 2; textureRange.RegisterSpace = 1;
    locals[3].ParameterType = IRRootParameterTypeDescriptorTable;
    locals[3].DescriptorTable.NumDescriptorRanges = 1; locals[3].DescriptorTable.pDescriptorRanges = &textureRange;
    sampler.Filter = IRFilterMinMagLinearMipPoint;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = IRTextureAddressModeClamp;
    sampler.MaxAnisotropy = 1; sampler.ComparisonFunc = IRComparisonFunctionNever;
    sampler.MaxLOD = 1000; sampler.ShaderRegister = 0; sampler.RegisterSpace = 1;
    if(six) for(unsigned i=0;i<6;i++)
    {
      samplers[i]=sampler; samplers[i].ShaderRegister=i;
      samplers[i].Filter = i<2 ? IRFilterMinMagMipPoint : i<4 ? IRFilterMinMagLinearMipPoint : IRFilterMinMagMipLinear;
      samplers[i].AddressU=samplers[i].AddressV=samplers[i].AddressW=
          i%2 ? IRTextureAddressModeClamp : IRTextureAddressModeWrap;
    }
    IRVersionedRootSignatureDescriptor localDescription = {};
    localDescription.version = IRRootSignatureVersion_1_1;
    localDescription.desc_1_1.NumParameters = 4; localDescription.desc_1_1.pParameters = locals;
    localDescription.desc_1_1.NumStaticSamplers = six ? 6 : 1; localDescription.desc_1_1.pStaticSamplers = samplers;
    localDescription.desc_1_1.Flags = IRRootSignatureFlagLocalRootSignature;
    localSignature = IRRootSignatureCreateFromDescriptor(&localDescription, &error);
    if(!localSignature) {fprintf(stderr,"local root error %u\n",error ? IRErrorGetCode(error) : 0);return 13;}
  }
  const char *entries[] = {"raygen", "closest_hit", "miss", "any_hit"};
  const IRShaderStage stages[] = {IRShaderStageRayGeneration, IRShaderStageClosestHit, IRShaderStageMiss, IRShaderStageAnyHit};
  for(unsigned i = 0; i < 4; i++)
  {
    error = nullptr;
    IRCompilerSetLocalRootSignature(compiler, local ? localSignature : nullptr);
    auto converted = IRCompilerAllocCompileAndLink(compiler, entries[i], object, &error);
    if(!converted)
    {
      fprintf(stderr, "%s IR error %u: %s\n", entries[i], error ? IRErrorGetCode(error) : 0,
              error ? (const char *)IRErrorGetPayload(error) : "no diagnostics");
      return 9;
    }
    auto binary = IRMetalLibBinaryCreate();
    auto reflection=IRShaderReflectionCreate();
    if(!IRObjectGetReflection(converted,stages[i],reflection)) return 15;
    const char *reflectionJSON=IRShaderReflectionCopyJSONString(reflection);
    if(!reflectionJSON) return 16;
    {std::ofstream json(output+"/"+entries[i]+".reflection.json");json<<reflectionJSON;}
    IRShaderReflectionReleaseString(reflectionJSON);IRShaderReflectionDestroy(reflection);
    if(!IRObjectGetMetalLibBinary(converted, stages[i], binary) ||
       !Save(output + "/" + entries[i] + ".metallib", binary)) return 10;
    IRMetalLibBinaryDestroy(binary); IRObjectDestroy(converted);
  }
  IRCompilerSetLocalRootSignature(compiler, nullptr);
  auto binary = IRMetalLibBinaryCreate();
  if(!IRMetalLibSynthesizeIndirectRayDispatchFunction(compiler, binary) ||
     !Save(output + "/dispatch.metallib", binary)) return 11;
  IRMetalLibBinaryDestroy(binary); binary = IRMetalLibBinaryCreate();
  if(!IRMetalLibSynthesizeIndirectIntersectionFunction(compiler, binary) ||
     !Save(output + "/intersection.metallib", binary)) return 12;
  IRMetalLibBinaryDestroy(binary);
  if(localSignature) IRRootSignatureDestroy(localSignature);
  IRRayTracingPipelineConfigurationDestroy(config); IRRootSignatureDestroy(root);
  IRCompilerDestroy(compiler); IRObjectDestroy(object);
  dxil->Release(); result->Release(); dxc->Release();
  printf("PASS converted DXR raygen/closest-hit/miss and IR runtime indirection\n");
  return 0;
}
