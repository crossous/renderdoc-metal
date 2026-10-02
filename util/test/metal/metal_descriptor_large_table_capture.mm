// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    constexpr NSUInteger count = 786432, samplerCount = 4096;
    constexpr NSUInteger bufferOffset = (count - 2) * 24, textureOffset = (count - 1) * 24;
    constexpr NSUInteger samplerOffset = (samplerCount - 1) * 24;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice(); NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct BufferEntry { const device uint *value [[id(0)]]; ulong z [[id(1)]]; ulong metadata [[id(2)]]; };
struct TextureEntry { ulong z [[id(0)]]; texture2d<float> image [[id(1)]]; ulong metadata [[id(2)]]; };
struct SamplerEntry { sampler point [[id(0)]]; ulong bias [[id(1)]]; ulong metadata [[id(2)]]; };
kernel void large_tables(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]])
{
  const device BufferEntry *buffer = reinterpret_cast<const device BufferEntry *>(root[0]);
  const device TextureEntry *texture = reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling = reinterpret_cast<const device SamplerEntry *>(root[4]);
  output[0] = buffer->value[0] + uint(texture->image.sample(sampling->point, float2(0.5)).r * 255.0 + 0.5) + 17;
  output[1] = buffer->metadata == 0x1111111111111111ul && texture->metadata == 0x2222222222222222ul &&
    sampling->bias == 0x123456789abcdef0ul && sampling->metadata == 0x3333333333333333ul &&
    root[1] == 0xabcdef0123456789ul && root[3] == root[1] && root[5] == root[1] ? 0xdeadbeef : 0xbad;
}
)MSL" options:nil error:&error];
    id<MTLComputePipelineState> pipeline = library ? [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"large_tables"] error:&error] : nil;
    if(!pipeline) { fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); return 2; }
    const uint32_t values[] = {41, 80};
    id<MTLBuffer> input = [device newBufferWithBytes:values length:8 options:MTLResourceStorageModeShared];
    id<MTLBuffer> table = [device newBufferWithLength:count * 24 options:MTLResourceStorageModeShared];
    id<MTLBuffer> samplerTable = [device newBufferWithLength:samplerCount * 24 options:MTLResourceStorageModeShared];
    id<MTLBuffer> output = [device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    if(!input || !table || !samplerTable || !output) return 3;
    memset(table.contents, 0, table.length); memset(samplerTable.contents, 0, samplerTable.length);
    // Empty slots may contain ordinary metadata; only declared identity fields are addresses.
    for(NSUInteger i = 0; i < count - 2; i++) ((uint64_t *)table.contents)[i * 3 + 2] = 0xababababababababULL;
    for(NSUInteger i = 0; i < samplerCount - 1; i++) { ((uint64_t *)samplerTable.contents)[i * 3 + 1] = 0xefefefefefefefefULL; ((uint64_t *)samplerTable.contents)[i * 3 + 2] = 0xcdcdcdcdcdcdcdcdULL; }
    auto descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared; descriptor.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> image = [device newTextureWithDescriptor:descriptor]; const uint8_t pixel[] = {64,128,192,255};
    [image replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:pixel bytesPerRow:4];
    auto sd = [MTLSamplerDescriptor new]; sd.supportArgumentBuffers = YES;
    id<MTLSamplerState> point = [device newSamplerStateWithDescriptor:sd];
    sd.minFilter = MTLSamplerMinMagFilterLinear; sd.magFilter = MTLSamplerMinMagFilterLinear;
    id<MTLSamplerState> linear = [device newSamplerStateWithDescriptor:sd];
    const uint64_t textureBytes[] = {0, image.gpuResourceID._impl, 0x2222222222222222ULL};
    memcpy((uint8_t *)table.contents + textureOffset, textureBytes, 24);
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0, (void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path); RENDERDOC_AnnotationValue v = {}; v.uint32 = 41;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device, "metal.descriptorCoverage", eRENDERDOC_UInt32, 0, &v)) return 5;
    }
    auto annotate = [&](id object, const char *key, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
      if(!api) return uint32_t(0); RENDERDOC_AnnotationValue v = {};
      v.vector.uint64[0] = a; v.vector.uint64[1] = b; v.vector.uint64[2] = c; v.vector.uint64[3] = d;
      return api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)object, key, eRENDERDOC_UInt64, 4, &v);
    };
    auto update = [&](unsigned phase, uint64_t generation, bool allocate) {
      const uint64_t bufferBytes[] = {input.gpuAddress + phase * 4, 0, 0x1111111111111111ULL};
      id<MTLSamplerState> sampling = phase ? linear : point;
      const uint64_t samplerBytes[] = {sampling.gpuResourceID._impl, 0x123456789abcdef0ULL, 0x3333333333333333ULL};
      memcpy((uint8_t *)table.contents + bufferOffset, bufferBytes, 24);
      memcpy((uint8_t *)samplerTable.contents + samplerOffset, samplerBytes, 24);
      return (!allocate || (!annotate(table, "metal.descriptorSlotEvent", bufferOffset, generation, 0, 0) &&
              !annotate(samplerTable, "metal.descriptorSlotEvent", samplerOffset, generation, 0, 7))) &&
          !annotate(table, "metal.descriptorSlotEvent", bufferOffset, generation, 2, 0) &&
          !annotate(table, "metal.descriptorSlotBinding", bufferOffset, 0, (uint64_t)(__bridge void *)input, phase * 4) &&
          !annotate(samplerTable, "metal.descriptorSlotEvent", samplerOffset, generation, 2, 7) &&
          !annotate(samplerTable, "metal.descriptorSlotBinding", samplerOffset, 2, (uint64_t)(__bridge void *)sampling, 0);
    };
    if(annotate(table, "metal.descriptorTable", 1, 0, count, 24) ||
       annotate(samplerTable, "metal.descriptorTable", 2, 0, samplerCount, 24) ||
       annotate(table, "metal.descriptorSlotEvent", textureOffset, 1, 0, 4) ||
       annotate(table, "metal.descriptorSlotEvent", textureOffset, 1, 2, 4) ||
       annotate(table, "metal.descriptorSlotBinding", textureOffset, 1, (uint64_t)(__bridge void *)image, 0) || !update(0, 1, true)) return 6;
    CAMetalLayer *layer = [CAMetalLayer layer]; layer.device = device; layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO; layer.drawableSize = CGSizeMake(2,2);
    id<MTLCommandQueue> queue = [device newCommandQueue];
    const uint64_t root[] = {table.gpuAddress + bufferOffset, 0xabcdef0123456789ULL,
      table.gpuAddress + textureOffset, 0xabcdef0123456789ULL, samplerTable.gpuAddress + samplerOffset, 0xabcdef0123456789ULL};
    for(unsigned capture = 0; capture < (api ? 2u : 1u); capture++)
    {
      const uint64_t generation = capture * 2 + 1;
      if(capture && !update(0, generation, false)) return 7;
      if(api) api->StartFrameCapture(nullptr, nullptr);
      for(unsigned phase = 0; phase < 2; phase++)
      {
        if(phase && (annotate(table, "metal.descriptorSlotEvent", bufferOffset, generation, 1, 0) ||
                     annotate(samplerTable, "metal.descriptorSlotEvent", samplerOffset, generation, 1, 7) ||
                     !update(1, generation + 1, true))) return 8;
        id<MTLCommandBuffer> command = [queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder]; [encoder setComputePipelineState:pipeline];
        if(annotate(encoder, "metal.descriptorInlineLayout", 0, 0, 3, 16) ||
           annotate(encoder, "metal.descriptorInlineBinding", 0, 0, (uint64_t)(__bridge void *)table, bufferOffset) ||
           annotate(encoder, "metal.descriptorInlineBinding", 0, 1, (uint64_t)(__bridge void *)table, textureOffset) ||
           annotate(encoder, "metal.descriptorInlineBinding", 0, 2, (uint64_t)(__bridge void *)samplerTable, samplerOffset)) return 9;
        [encoder setBytes:root length:sizeof(root) atIndex:0]; [encoder setBuffer:output offset:0 atIndex:1];
        [encoder useResource:table usage:MTLResourceUsageRead]; [encoder useResource:samplerTable usage:MTLResourceUsageRead];
        [encoder useResource:input usage:MTLResourceUsageRead]; [encoder useResource:image usage:MTLResourceUsageRead];
        [encoder dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)]; [encoder endEncoding];
        if(phase)
        {
          id<CAMetalDrawable> drawable = [layer nextDrawable]; auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
          pass.colorAttachments[0].texture = drawable.texture; pass.colorAttachments[0].loadAction = MTLLoadActionClear;
          pass.colorAttachments[0].storeAction = MTLStoreActionStore;
          [[command renderCommandEncoderWithDescriptor:pass] endEncoding]; [command presentDrawable:drawable];
        }
        [command commit]; [command waitUntilCompleted];
        const auto words = (const uint32_t *)output.contents;
        if(command.status != MTLCommandBufferStatusCompleted || words[0] != (phase ? 161u : 122u) || words[1] != 0xdeadbeef) return 10;
        printf("Native large table PASS capture=%u phase=%u GPU=%u\n", capture, phase, words[0]);
      }
      if(api && !api->EndFrameCapture(nullptr, nullptr)) return 11;
      if(annotate(table, "metal.descriptorSlotEvent", bufferOffset, generation + 1, 1, 0) ||
         annotate(samplerTable, "metal.descriptorSlotEvent", samplerOffset, generation + 1, 1, 7) ||
         !update(0, generation + 2, true)) return 12;
    }
    printf("CAPTURED_VA=%llu captures=%u\n", (unsigned long long)input.gpuAddress, api ? 2u : 1u);
  }
  return 0;
}
