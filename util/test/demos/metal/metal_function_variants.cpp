// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"
#include <atomic>

RD_TEST(Metal_Function_Variants, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Function constants by index/range/name, copies/reset and sync/async descriptor snapshots.";
  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
using namespace metal::raytracing;
constant uint Bias [[function_constant(0)]];
constant uint Step [[function_constant(1)]];
constant bool Enabled [[function_constant(2)]];
constant float Scale [[function_constant(3)]];
kernel void cs_function_variant(device uint *output [[buffer(0)]], uint id [[thread_position_in_grid]])
{ output[id] = Enabled ? Bias + Step * id + uint(Scale) : 999; }
vertex float4 vs_function_variant(uint id [[vertex_id]])
{ const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)}; return float4(p[id],0,1); }
fragment float4 fs_function_variant(const device uint *output [[buffer(0)]])
{ return float4(output[0],output[32],output[64],255)/255.0; }
[[intersection(triangle, triangle_data)]]
bool is_function_variant() { return true; }
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!lib)
    {
      TEST_WARN("T55 Metal library: %s", error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    NS::AutoreleasePool *setup = NS::AutoreleasePool::alloc()->init();
    MTL::Function *functions[4] = {};
    dispatch_group_t group = dispatch_group_create();
    std::atomic<bool> failed{false};
    std::atomic<uint32_t> completions{0};
    auto completed = [&](uint32_t index, MTL::Function *function, NS::Error *failure) {
      completions++;
      if(!function || failure || function->device() != device) failed = true;
      if(function) functions[index] = function->retain();
      dispatch_group_leave(group);
    };
    const uint32_t starts[] = {17,43,79,127};
    MTL::FunctionConstantValues *values = MTL::FunctionConstantValues::alloc()->init();
    const uint32_t initial[] = {15,3};
    const bool enabled = true;
    const float scale = 2.0f;
    values->setConstantValues(initial, MTL::DataTypeUInt, NS::Range::Make(0,2));
    values->setConstantValue(&enabled, MTL::DataTypeBool, NS::UInteger(2));
    values->setConstantValue(&scale, MTL::DataTypeFloat, NS::UInteger(3));
    functions[0] = lib->newFunction(MTLSTR("cs_function_variant"), values, &error);
    MTL::FunctionConstantValues *copy = values->copy();
    values->reset();
    values->release();
    for(uint32_t i = 1; i < 4; i++)
    {
      uint32_t bias = starts[i] - 2;
      if(i == 1)
        copy->setConstantValue(&bias, MTL::DataTypeUInt, NS::UInteger(0));
      else
      {
        // Metal resolves an index assignment before a name assignment for the same constant.
        // Reset before switching addressing modes so this genuinely tests the named value.
        copy->reset();
        copy->setConstantValue(&initial[1], MTL::DataTypeUInt, NS::UInteger(1));
        copy->setConstantValue(&enabled, MTL::DataTypeBool, NS::UInteger(2));
        copy->setConstantValue(&scale, MTL::DataTypeFloat, NS::UInteger(3));
        copy->setConstantValue(&bias, MTL::DataTypeUInt, MTLSTR("Bias"));
      }
      MTL::FunctionDescriptor *desc = MTL::FunctionDescriptor::alloc()->init();
      desc->setName(MTLSTR("cs_function_variant")); desc->setConstantValues(copy);
      if(i == 1)
      {
        dispatch_group_enter(group);
        lib->newFunction(MTLSTR("cs_function_variant"), copy,
            [&, i](MTL::Function *f, NS::Error *e) { completed(i,f,e); });
      }
      else if(i == 2)
      {
        desc->setSpecializedName(MTLSTR("cs_function_named"));
        functions[i] = lib->newFunction(desc, &error);
      }
      else
      {
        dispatch_group_enter(group);
        lib->newFunction(desc, [&, i](MTL::Function *f, NS::Error *e) { completed(i,f,e); });
      }
      desc->setName(MTLSTR("invalid_after_call")); desc->release();
    }
    dispatch_group_enter(group);
    lib->newFunction(MTLSTR("not_a_function"), copy, [&](MTL::Function *f, NS::Error *e) {
      completions++;
      if(f || !e) failed = true;
      dispatch_group_leave(group);
    });
    copy->reset(); copy->release();
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_function_variant"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_function_variant"));
    // The replay device deliberately does not advertise full ray tracing yet. Function-pointer
    // support is enough for this linked-function creation test on the Metal test host.
    const bool raytracing = device->supportsFunctionPointers();
    MTL::Function *intersection[2] = {};
    if(raytracing)
    {
      MTL::IntersectionFunctionDescriptor *desc =
          MTL::IntersectionFunctionDescriptor::alloc()->init();
      desc->setName(MTLSTR("is_function_variant"));
      intersection[0] = lib->newIntersectionFunction(desc, &error);
      if(!intersection[0]) TEST_WARN("T55 intersection sync: %s", error ? error->localizedDescription()->utf8String() : "nil");
      dispatch_group_enter(group);
      lib->newIntersectionFunction(desc, [&](MTL::Function *f, NS::Error *e) {
        completions++;
        if(!f) TEST_WARN("T55 intersection async: %s", e ? e->localizedDescription()->utf8String() : "nil");
        if(!f || e || f->functionType() != MTL::FunctionTypeIntersection) failed = true;
        if(f) intersection[1] = f->retain();
        dispatch_group_leave(group);
      });
      desc->setName(MTLSTR("invalid_after_call")); desc->release();
    }
    lib->release();
    if(dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW,20*NSEC_PER_SEC))) return 4;
    if(completions != (raytracing ? 4U : 3U)) failed = true;
    if(raytracing && (!intersection[0] || !intersection[1] ||
                      intersection[0]->functionType() != MTL::FunctionTypeIntersection))
    {
      TEST_WARN("T55 intersection missing or wrong type sync=%p async=%p", intersection[0], intersection[1]);
      return 4;
    }
    MTL::ComputePipelineState *pipelines[4] = {};
    for(uint32_t i = 0; i < 4; i++)
    {
      if(!functions[i]) return 4;
      if(i < 2 && raytracing)
      {
        MTL::ComputePipelineDescriptor *desc = MTL::ComputePipelineDescriptor::alloc()->init();
        desc->setComputeFunction(functions[i]);
        MTL::LinkedFunctions *links = MTL::LinkedFunctions::alloc()->init();
        links->setFunctions(NS::Array::array(intersection[i]));
        desc->setLinkedFunctions(links);
        pipelines[i] = device->newComputePipelineState(desc, MTL::PipelineOptionNone, NULL, &error);
        links->release(); desc->release();
      }
      else
        pipelines[i] = device->newComputePipelineState(functions[i], &error);
      functions[i]->release();
      if(!pipelines[i])
      {
        TEST_WARN("T55 compute pipeline %u: %s", i,
                  error ? error->localizedDescription()->utf8String() : "nil");
        return 4;
      }
    }
    for(MTL::Function *f : intersection) if(f) f->release();
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *renderPSO = device->newRenderPipelineState(pd, &error);
    pd->release(); vs->release(); fs->release(); setup->drain();
    if(!renderPSO) return 4;
    MTL::Buffer *output = device->newBuffer(548, MTL::ResourceStorageModeShared);
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    uint32_t frames = 0;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
      blit->fillBuffer(output, NS::Range::Make(0,548), 0); blit->endEncoding();
      MTL::ComputeCommandEncoder *compute = cb->computeCommandEncoder();
      for(uint32_t i = 0; i < 4; i++)
      {
        compute->setComputePipelineState(pipelines[i]); compute->setBuffer(output, i*128, 0);
        compute->dispatchThreadgroups(MTL::Size::Make(1,1,1), MTL::Size::Make(32,1,1));
      }
      compute->endEncoding();
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0,0,0,1)));
      render->setRenderPipelineState(renderPSO); render->setFragmentBuffer(output,0,0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      EndCaptureFrame();
      if(cb->error()) failed = true;
      const uint32_t *data = (const uint32_t *)output->contents();
      if(frames == 0)
        TEST_LOG("T55 starts %u/%u/%u/%u; completions %u; callback failure %u", data[0],
                 data[32],data[64],data[96],completions.load(),failed.load() ? 1U : 0U);
      for(uint32_t i = 0; i < 137; i++)
        if(data[i] != (i < 128 ? starts[i/32] + 3*(i%32) : 0)) failed = true;
      if(native)
      {
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(200,150,1,1),0);
        const byte expected[] = {79,43,17,255};
        if(memcmp(pixel,expected,4)) failed = true;
      }
      pool->drain(); frames++;
    }
    for(auto *p : pipelines) p->release();
    output->release(); renderPSO->release(); dispatch_release(group);
    if(failed) TEST_WARN("T55 function constants/copy/reset/async snapshot failed");
    else TEST_LOG("T55 passed four specializations, %u completions, intersection functions %u, %u frames",
                  completions.load(), raytracing ? 2U : 0U, frames);
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
