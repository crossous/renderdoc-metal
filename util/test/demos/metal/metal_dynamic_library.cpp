// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"
#include <atomic>
#include <string>
#include <unistd.h>

RD_TEST(Metal_Dynamic_Library, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Compile, serialize and link a Metal dynamic library into a compute pipeline.";

  int main()
  {
    if(!Init()) return 3;
    if(!device->supportsDynamicLibraries()) return 6;
    const bool mixedURL = !GetEnvVar("RENDERDOC_METAL_T217_MIXED_URL_DYNAMIC").empty();
    const bool multiple = mixedURL ||
        !GetEnvVar("RENDERDOC_METAL_T105_MULTIPLE_DYNAMIC").empty();
    const bool preloaded = !GetEnvVar("RENDERDOC_METAL_T106_PRELOADED_DYNAMIC").empty();
    const bool renderPreloaded = !GetEnvVar("RENDERDOC_METAL_T107_RENDER_PRELOADED").empty();
    const bool vertexPreloaded = !GetEnvVar("RENDERDOC_METAL_T108_VERTEX_PRELOADED").empty();
    const bool renderOptions = !GetEnvVar("RENDERDOC_METAL_T109_RENDER_OPTIONS").empty();
    const bool asyncDynamic = !GetEnvVar("RENDERDOC_METAL_T110_ASYNC_DYNAMIC").empty();
    const bool asyncExecutable = !GetEnvVar("RENDERDOC_METAL_T111_ASYNC_EXECUTABLE").empty();
    const bool asyncRender = !GetEnvVar("RENDERDOC_METAL_T112_ASYNC_RENDER").empty();
    const bool asyncRenderOptions = !GetEnvVar("RENDERDOC_METAL_T113_ASYNC_RENDER_OPTIONS").empty();
    const bool asyncCompute = !GetEnvVar("RENDERDOC_METAL_T114_ASYNC_COMPUTE").empty();
    const bool asyncVertex = !GetEnvVar("RENDERDOC_METAL_T115_ASYNC_VERTEX").empty();
    const bool urlDynamic = mixedURL ||
        !GetEnvVar("RENDERDOC_METAL_T216_URL_DYNAMIC").empty();
    if((renderPreloaded || vertexPreloaded || renderOptions || asyncRender ||
        asyncRenderOptions || asyncVertex) &&
       !device->supportsRenderDynamicLibraries()) return 6;
    char directory[] = "/tmp/renderdoc-metal-test-dynamic.XXXXXX";
    if(!mkdtemp(directory)) return 4;
    std::string installPath = std::string(directory) + "/library.metallib";
    NS::Error *error = NULL;
    MTL::CompileOptions *dynamicOptions = MTL::CompileOptions::alloc()->init();
    dynamicOptions->setLibraryType(MTL::LibraryTypeDynamic);
    dynamicOptions->setInstallName(NS::String::string(installPath.c_str(),NS::UTF8StringEncoding));
    const char *dynamicSource = R"(
#include <metal_stdlib>
using namespace metal;
extern "C" [[visible]] float metal_dynamic_add(float x) { return x+1.0; }
)";
    MTL::Library *dynamicSourceLibrary = NULL;
    if(asyncDynamic)
    {
      dispatch_group_t group = dispatch_group_create();
      std::atomic<bool> failed{false};
      dispatch_group_enter(group);
      device->newLibrary(NS::String::string(dynamicSource,NS::UTF8StringEncoding),dynamicOptions,
          [&](MTL::Library *result, NS::Error *callbackError) {
            if(!result || callbackError || result->device() != device) failed = true;
            if(result) dynamicSourceLibrary = result->retain();
            dispatch_group_leave(group);
          });
      if(dispatch_group_wait(group,dispatch_time(DISPATCH_TIME_NOW,20*NSEC_PER_SEC)) || failed)
        return 4;
    }
    else
      dynamicSourceLibrary = device->newLibrary(
          NS::String::string(dynamicSource,NS::UTF8StringEncoding),dynamicOptions,&error);
    dynamicOptions->release();
    if(!dynamicSourceLibrary)
    {
      TEST_WARN("T104 dynamic source: %s",error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    MTL::DynamicLibrary *dynamic = device->newDynamicLibrary(dynamicSourceLibrary,&error);
    if(!dynamic)
    {
      TEST_WARN("T104 dynamic library: %s",error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    if(!dynamic->serializeToURL(NS::URL::fileURLWithPath(
        NS::String::string(installPath.c_str(),NS::UTF8StringEncoding)),&error))
    {
      TEST_WARN("T104 serialize: %s",error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    if(urlDynamic)
    {
      MTL::DynamicLibrary *loaded = device->newDynamicLibrary(NS::URL::fileURLWithPath(
          NS::String::string(installPath.c_str(),NS::UTF8StringEncoding)), &error);
      if(!loaded)
      {
        TEST_WARN("T216 URL dynamic library: %s",
                  error ? error->localizedDescription()->utf8String() : "nil");
        return 4;
      }
      dynamic->release();
      dynamic = loaded;
    }
    std::string secondPath = std::string(directory) + "/second.metallib";
    MTL::Library *secondSourceLibrary = NULL;
    MTL::DynamicLibrary *secondDynamic = NULL;
    if(multiple)
    {
      MTL::CompileOptions *secondOptions = MTL::CompileOptions::alloc()->init();
      secondOptions->setLibraryType(MTL::LibraryTypeDynamic);
      secondOptions->setInstallName(NS::String::string(secondPath.c_str(),NS::UTF8StringEncoding));
      secondSourceLibrary = device->newLibrary(MTLSTR(
          "#include <metal_stdlib>\nusing namespace metal;\n"
          "extern \"C\" [[visible]] float metal_dynamic_double(float x) { return x*2.0; }\n"),
          secondOptions,&error);
      secondOptions->release();
      if(!secondSourceLibrary) return 4;
      secondDynamic = device->newDynamicLibrary(secondSourceLibrary,&error);
      if(!secondDynamic || !secondDynamic->serializeToURL(NS::URL::fileURLWithPath(
          NS::String::string(secondPath.c_str(),NS::UTF8StringEncoding)),&error))
      {
        TEST_WARN("T105 second dynamic library: %s",
                  error ? error->localizedDescription()->utf8String() : "nil");
        return 4;
      }
    }
    const char *executableSource = (vertexPreloaded || asyncVertex) ? R"(
#include <metal_stdlib>
using namespace metal;
extern "C" float metal_dynamic_add(float);
kernel void metal_dynamic_compute(device float *output [[buffer(0)]])
{ output[0] = metal_dynamic_add(2.0); }
vertex float4 metal_dynamic_vs(uint id [[vertex_id]])
{ const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(metal_dynamic_add(p[id].x-1.0),p[id].y,0,1); }
fragment float4 metal_dynamic_fs(const device float *output [[buffer(0)]])
{ return float4(output[0]/3.0,0,0,1); }
)" : (renderPreloaded || renderOptions || asyncRender || asyncRenderOptions) ? R"(
#include <metal_stdlib>
using namespace metal;
extern "C" float metal_dynamic_add(float);
kernel void metal_dynamic_compute(device float *output [[buffer(0)]])
{ output[0] = metal_dynamic_add(2.0); }
vertex float4 metal_dynamic_vs(uint id [[vertex_id]])
{ const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)}; return float4(p[id],0,1); }
fragment float4 metal_dynamic_fs(const device float *output [[buffer(0)]])
{ return float4(metal_dynamic_add(output[0]-3.0),0,0,1); }
)" : multiple ? R"(
#include <metal_stdlib>
using namespace metal;
extern "C" float metal_dynamic_add(float);
extern "C" float metal_dynamic_double(float);
kernel void metal_dynamic_compute(device float *output [[buffer(0)]])
{ output[0] = metal_dynamic_double(metal_dynamic_add(2.0)); }
vertex float4 metal_dynamic_vs(uint id [[vertex_id]])
{ const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)}; return float4(p[id],0,1); }
fragment float4 metal_dynamic_fs(const device float *output [[buffer(0)]])
{ return float4(output[0]/6.0,0,0,1); }
)" : R"(
#include <metal_stdlib>
using namespace metal;
extern "C" float metal_dynamic_add(float);
kernel void metal_dynamic_compute(device float *output [[buffer(0)]])
{ output[0] = metal_dynamic_add(2.0); }
vertex float4 metal_dynamic_vs(uint id [[vertex_id]])
{ const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)}; return float4(p[id],0,1); }
fragment float4 metal_dynamic_fs(const device float *output [[buffer(0)]])
{ return float4(output[0]/3.0,0,0,1); }
)";
    MTL::CompileOptions *executableOptions = MTL::CompileOptions::alloc()->init();
    MTL::DynamicLibrary *dependencies[] = {dynamic,secondDynamic};
    executableOptions->setLibraries(NS::Array::array(
        (const NS::Object *const *)dependencies,multiple ? 2 : 1));
    MTL::Library *executable = NULL;
    if(asyncExecutable)
    {
      dispatch_group_t group = dispatch_group_create();
      std::atomic<bool> failed{false};
      dispatch_group_enter(group);
      device->newLibrary(NS::String::string(executableSource,NS::UTF8StringEncoding),
          executableOptions,[&](MTL::Library *result,NS::Error *callbackError) {
            if(!result || callbackError || result->device() != device) failed = true;
            if(result) executable = result->retain();
            dispatch_group_leave(group);
          });
      if(dispatch_group_wait(group,dispatch_time(DISPATCH_TIME_NOW,20*NSEC_PER_SEC)) || failed)
        return 4;
    }
    else
      executable = device->newLibrary(
          NS::String::string(executableSource,NS::UTF8StringEncoding),executableOptions,&error);
    executableOptions->release();
    if(!executable)
    {
      TEST_WARN("T104 executable source: %s",error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    MTL::Function *computeFunction = executable->newFunction(MTLSTR("metal_dynamic_compute"));
    MTL::Function *vertexFunction = executable->newFunction(MTLSTR("metal_dynamic_vs"));
    MTL::Function *fragmentFunction = executable->newFunction(MTLSTR("metal_dynamic_fs"));
    MTL::ComputePipelineState *computePipeline = NULL;
    if(preloaded || asyncCompute)
    {
      MTL::ComputePipelineDescriptor *computeDescriptor =
          MTL::ComputePipelineDescriptor::alloc()->init();
      computeDescriptor->setComputeFunction(computeFunction);
      computeDescriptor->setPreloadedLibraries(NS::Array::array(dynamic));
      if(asyncCompute)
      {
        dispatch_group_t group = dispatch_group_create();
        std::atomic<bool> failed{false};
        dispatch_group_enter(group);
        device->newComputePipelineState(computeDescriptor,MTL::PipelineOptionArgumentInfo,
            [&](MTL::ComputePipelineState *result,MTL::ComputePipelineReflection *reflection,
                NS::Error *callbackError) {
              if(!result || !reflection || callbackError || result->device() != device)
                failed = true;
              if(result) computePipeline = result->retain();
              dispatch_group_leave(group);
            });
        if(dispatch_group_wait(group,dispatch_time(DISPATCH_TIME_NOW,20*NSEC_PER_SEC)) || failed)
          return 4;
      }
      else
        computePipeline = device->newComputePipelineState(
            computeDescriptor,MTL::PipelineOptionNone,NULL,&error);
      computeDescriptor->release();
    }
    else
      computePipeline = device->newComputePipelineState(computeFunction,&error);
    MTL::RenderPipelineDescriptor *renderDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    renderDescriptor->setVertexFunction(vertexFunction);
    renderDescriptor->setFragmentFunction(fragmentFunction);
    if(renderPreloaded || renderOptions || asyncRender || asyncRenderOptions)
      renderDescriptor->setFragmentPreloadedLibraries(NS::Array::array(dynamic));
    if(vertexPreloaded || asyncVertex)
      renderDescriptor->setVertexPreloadedLibraries(NS::Array::array(dynamic));
    renderDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *renderPipeline = NULL;
    if(asyncRender || asyncRenderOptions || asyncVertex)
    {
      dispatch_group_t group = dispatch_group_create();
      std::atomic<bool> failed{false};
      dispatch_group_enter(group);
      if(asyncRenderOptions)
        device->newRenderPipelineState(renderDescriptor,MTL::PipelineOptionArgumentInfo,
            [&](MTL::RenderPipelineState *result,MTL::RenderPipelineReflection *reflection,
                NS::Error *callbackError) {
              if(!result || !reflection || callbackError || result->device() != device)
                failed = true;
              if(result) renderPipeline = result->retain();
              dispatch_group_leave(group);
            });
      else
        device->newRenderPipelineState(renderDescriptor,
            [&](MTL::RenderPipelineState *result,NS::Error *callbackError) {
              if(!result || callbackError || result->device() != device) failed = true;
              if(result) renderPipeline = result->retain();
              dispatch_group_leave(group);
            });
      if(dispatch_group_wait(group,dispatch_time(DISPATCH_TIME_NOW,20*NSEC_PER_SEC)) || failed)
        return 4;
    }
    else
      renderPipeline = renderOptions ?
          device->newRenderPipelineState(renderDescriptor,MTL::PipelineOptionArgumentInfo,NULL,&error) :
          device->newRenderPipelineState(renderDescriptor,&error);
    renderDescriptor->release();
    if(!computePipeline || !renderPipeline)
    {
      TEST_WARN("T104 pipeline: %s",error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    MTL::Buffer *output = device->newBuffer(4,MTL::ResourceStorageModeShared);
    bool failed = false;
    uint32_t frames = 0;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      *(float *)output->contents() = 0.0f;
      MTL::CommandBuffer *command = queue->commandBuffer();
      MTL::ComputeCommandEncoder *compute = command->computeCommandEncoder();
      compute->setComputePipelineState(computePipeline);
      compute->setBuffer(output,0,0);
      compute->dispatchThreadgroups(MTL::Size::Make(1,1,1),MTL::Size::Make(1,1,1));
      compute->endEncoding();
      MTL::RenderCommandEncoder *render = command->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
      render->setRenderPipelineState(renderPipeline);
      render->setFragmentBuffer(output,0,0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
      render->endEncoding();
      command->presentDrawable(drawable);
      command->commit(); command->waitUntilCompleted();
      if(command->error()) failed = true;
      if(native)
      {
        if(*(float *)output->contents() != (multiple ? 6.0f : 3.0f)) failed = true;
        uint8_t pixel[4] = {};
        drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(200,150,1,1),0);
        if(pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 255 || pixel[3] != 255)
          failed = true;
      }
      EndCaptureFrame(); pool->drain(); frames++;
    }
    output->release(); renderPipeline->release(); computePipeline->release();
    fragmentFunction->release(); vertexFunction->release(); computeFunction->release();
    executable->release(); dynamic->release(); dynamicSourceLibrary->release();
    if(secondDynamic) secondDynamic->release();
    if(secondSourceLibrary) secondSourceLibrary->release();
    unlink(installPath.c_str());
    if(multiple) unlink(secondPath.c_str());
    rmdir(directory);
    TEST_LOG("T%d %s: %u linked dynamic-library frames",
             asyncVertex ? 115 : asyncCompute ? 114 : asyncRenderOptions ? 113 : asyncRender ? 112 :
             mixedURL ? 217 : urlDynamic ? 216 : asyncExecutable ? 111 :
             asyncDynamic ? 110 : renderOptions ? 109 :
             vertexPreloaded ? 108 : renderPreloaded ? 107 :
             preloaded ? 106 : multiple ? 105 : 104,
             failed ? "FAILED" : "passed",frames);
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
