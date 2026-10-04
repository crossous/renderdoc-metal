// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  if(argc != 2) return 2;
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice(); NSError *error = nil;
    auto library = [device newLibraryWithFile:[NSString stringWithUTF8String:argv[1]] error:&error];
    auto pipeline = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"modify"] error:&error];
    if(!pipeline) { fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); return 3; }
    auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:1 height:1 mipmapped:NO];
    desc.storageMode = MTLStorageModeShared;
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    id<MTLTexture> images[3];
    const float zero[4] = {};
    for(unsigned i = 0; i < 3; i++) {
      images[i] = [device newTextureWithDescriptor:desc];
      images[i].label = [NSString stringWithFormat:@"Bindless output %u", i];
      [images[i] replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:zero bytesPerRow:16];
    }
    const bool lateUniformWrite = getenv("RENDERDOC_METAL_TEST_LATE_UNIFORM_WRITE") != nullptr;
    // The encoded inline pointers stay unchanged. Only the submission-time CPU
    // contents identify the accessed images; loading must not use these old indices.
    uint32_t indices[2] = {lateUniformWrite ? 1U : 0U, lateUniformWrite ? 0U : 1U};
    auto uniform = [device newBufferWithBytes:indices length:sizeof(indices) options:MTLResourceStorageModeShared];
    uint64_t entries[6] = {0, images[0].gpuResourceID._impl, 0, 0, images[1].gpuResourceID._impl, 0};
    auto table = [device newBufferWithBytes:entries length:sizeof(entries) options:MTLResourceStorageModeShared];
    auto queue = [device newCommandQueue];
    auto layer = [CAMetalLayer layer]; layer.device = device; layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.drawableSize = CGSizeMake(2,2); layer.framebufferOnly = NO;
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH")) {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0, (void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue version = {}; version.uint32 = 43;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
          "metal.descriptorCoverage", eRENDERDOC_UInt32, 0, &version)) return 5;
    }
    auto annotation = [&](id object, const char *key, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
      if(!api) return uint32_t(0);
      RENDERDOC_AnnotationValue value = {};
      value.vector.uint64[0]=a; value.vector.uint64[1]=b; value.vector.uint64[2]=c; value.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)object, key, eRENDERDOC_UInt64, 4, &value);
    };
    if(annotation(table, "metal.descriptorTable", 1, 0, 2, 24)) return 6;
    for(unsigned i = 0; i < 2; i++)
      if(annotation(table, "metal.descriptorSlotEvent", i*24, 1, 0, 5) ||
         annotation(table, "metal.descriptorSlotEvent", i*24, 1, 2, 5) ||
         annotation(table, "metal.descriptorSlotBinding", i*24, 1, (uint64_t)(__bridge void *)images[i], 0)) return 7;
    if(api) api->StartFrameCapture(nullptr, nullptr);
    auto command = [queue commandBuffer];
    for(unsigned i = 0; i < 2; i++) {
      auto encoder = [command computeCommandEncoder];
      [encoder setComputePipelineState:pipeline];
      [encoder setBuffer:table offset:0 atIndex:0];
      if(annotation(encoder, "metal.descriptorInlineLayout", 0, 2, 1, 8) ||
         annotation(encoder, "metal.descriptorInlineBinding", 2, 0, (uint64_t)(__bridge void *)uniform, i*4)) return 8;
      uint64_t pointer = uniform.gpuAddress + i*4;
      [encoder setBytes:&pointer length:8 atIndex:2];
      [encoder useResource:uniform usage:MTLResourceUsageRead];
      // All three are resident. Only the indexed table resource is accessed.
      for(auto image : images) [encoder useResource:image usage:MTLResourceUsageRead | MTLResourceUsageWrite];
      [encoder dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      [encoder endEncoding];
    }
    if(lateUniformWrite) {
      const uint32_t submissionIndices[2] = {0, 1};
      memcpy(uniform.contents, submissionIndices, sizeof(submissionIndices));
    }
    auto drawable = [layer nextDrawable];
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    [[command renderCommandEncoderWithDescriptor:pass] endEncoding];
    [command presentDrawable:drawable]; [command commit]; [command waitUntilCompleted];
    if(command.status != MTLCommandBufferStatusCompleted) return 9;
    for(unsigned i = 0; i < 3; i++) {
      float data[4]; [images[i] getBytes:data bytesPerRow:16 fromRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0];
      const float expected[4] = {0.25f, 0.5f, 0.75f, 1.0f};
      if(memcmp(data, i < 2 ? expected : zero, 16)) return 10;
    }
    if(api && !api->EndFrameCapture(nullptr, nullptr)) return 11;
    puts("PASS native indexed read/modify/write, changed inline root, unrelated resident image untouched");
  }
  return 0;
}
