// SPDX-License-Identifier: MIT
// Diagnostic slot history only. The non-zero payload is never consumed by a GPU shader.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <initializer_list>
#include <atomic>
#include <thread>
#include <chrono>
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLBuffer> table = [device newBufferWithLength:48 options:MTLResourceStorageModeShared];
    id<MTLBuffer> inputA = [device newBufferWithLength:16 options:MTLResourceStorageModeShared];
    id<MTLBuffer> inputB = [device newBufferWithLength:24 options:MTLResourceStorageModeShared];
    auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(!device || !table || !getAPI || !getAPI(eRENDERDOC_API_Version_1_7_0, (void **)&api)) return 2;
    if(!inputA.gpuAddress || !inputB.gpuAddress) return 2;
    auto annotate = [&](const char *key, uint64_t a, uint64_t b, uint64_t c, uint64_t d)
    {
      RENDERDOC_AnnotationValue value = {};
      value.vector.uint64[0] = a; value.vector.uint64[1] = b;
      value.vector.uint64[2] = c; value.vector.uint64[3] = d;
      return api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)table, key,
          eRENDERDOC_UInt64, 4, &value);
    };
    if(annotate("metal.descriptorSlotEvent", 40, 1, 0, 0) != 2 ||
       annotate("metal.descriptorSlotEvent", 0, 0, 0, 0) != 2 ||
       annotate("metal.descriptorSlotEvent", 0, 1, 3, 0) != 2 ||
       annotate("metal.descriptorSlotEvent", 0, 1, 0, 256) != 2 ||
       annotate("metal.descriptorSlotBinding", 24, 1, (uint64_t)(__bridge void *)inputA, 0) != 2 ||
       annotate("metal.descriptorSlotBinding", 24, 0, (uint64_t)(__bridge void *)inputA, 16) != 2 ||
       annotate("metal.descriptorSlotBinding", 24, 0, 1, 0) != 2) return 3;
    if(annotate("metal.descriptorTable", 1, 0, 2, 24) ||
       annotate("metal.descriptorSlotEvent", 24, 1, 0, 5)) return 4;
    const uint64_t first[] = {0x1010101010101010ULL, 0x2020202020202020ULL, 0x3030303030303030ULL};
    memcpy((char *)table.contents + 24, first, 24);
    if(annotate("metal.descriptorSlotEvent", 24, 1, 2, 5)) return 5;
    if(annotate("metal.descriptorSlotBinding", 24, 0, (uint64_t)(__bridge void *)inputA, 4)) return 5;
    api->SetCaptureFilePathTemplate(getenv("RENDERDOC_METAL_CAPTURE_PATH"));
    api->StartFrameCapture(nullptr, nullptr);
    if(annotate("metal.descriptorSlotEvent", 24, 1, 1, 5) ||
       annotate("metal.descriptorSlotEvent", 24, 2, 0, 4)) return 6;
    const uint64_t second[] = {0x4141414141414141ULL, 0x5252525252525252ULL, 0x6363636363636363ULL};
    memcpy((char *)table.contents + 24, second, 24);
    if(annotate("metal.descriptorSlotEvent", 24, 2, 2, 4) ||
       annotate("metal.descriptorSlotBinding", 24, 0, (uint64_t)(__bridge void *)inputB, 8) ||
       annotate("metal.descriptorSlotGPUValue", 24, 11, 22, 33) ||
       annotate("metal.descriptorSlotBinding", 24, 0, (uint64_t)(__bridge void *)inputB, 12)) return 7;
    // Presentation supplies a backbuffer for capture closure; 2x2 clear only.
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device; layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.drawableSize = CGSizeMake(2, 2);
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    id<MTLCommandBuffer> command = [[device newCommandQueue] commandBuffer];
    auto inlineAnnotation = [&](id encoder, const char *key, uint64_t a, uint64_t b, uint64_t c, uint64_t d)
    {
      RENDERDOC_AnnotationValue value = {};
      value.vector.uint64[0] = a; value.vector.uint64[1] = b;
      value.vector.uint64[2] = c; value.vector.uint64[3] = d;
      return api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)encoder,
          key, eRENDERDOC_UInt64, 4, &value);
    };
    id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
    if(inlineAnnotation(compute, "metal.descriptorInlineLayout", 1, 2, 1, 8) != 2 ||
       inlineAnnotation(compute, "metal.descriptorInlineLayout", 0, 31, 1, 8) != 2 ||
       inlineAnnotation(compute, "metal.descriptorInlineLayout", 0, 2, 0, 8) != 2 ||
       inlineAnnotation(compute, "metal.descriptorInlineLayout", 0, 2, 513, 8) != 2 ||
       inlineAnnotation(compute, "metal.descriptorInlineBinding", 2, 0, 1, 0) != 2 ||
       inlineAnnotation(compute, "metal.descriptorInlineBinding", 2, 0, (uint64_t)(__bridge void *)inputA, 16) != 2)
      return 9;
    const uint64_t roots[] = {inputA.gpuAddress + 4, inputB.gpuAddress + 8};
    for(uint64_t count : {1ULL, 2ULL})
    {
      if(inlineAnnotation(compute, "metal.descriptorInlineLayout", 0, 2, count, 8)) return 10;
      for(uint64_t entry = 0; entry < count; entry++)
        if(inlineAnnotation(compute, "metal.descriptorInlineBinding", 2, entry,
            (uint64_t)(__bridge void *)(entry ? inputB : inputA), entry ? 8 : 4)) return 10;
      [compute setBytes:roots length:count * 8 atIndex:2];
    }
    [compute endEncoding];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> render = [command renderCommandEncoderWithDescriptor:pass];
    struct Vertex { uint64_t address; uint32_t length, stride; } vertices[] = {
      {roots[0], 12, 4}, {roots[1], 16, 8}};
    if(inlineAnnotation(render, "metal.descriptorInlineLayout", 0, 2, 1, 8) != 2 ||
       inlineAnnotation(render, "metal.descriptorInlineLayout", 1, 6, 2, 16)) return 11;
    for(uint64_t entry = 0; entry < 2; entry++)
      if(inlineAnnotation(render, "metal.descriptorInlineBinding", (1ULL << 32) | 6, entry,
          (uint64_t)(__bridge void *)(entry ? inputB : inputA), entry ? 8 : 4)) return 11;
    [render setVertexBytes:vertices length:sizeof(vertices) atIndex:6];
    if(inlineAnnotation(render, "metal.descriptorInlineLayout", 2, 2, 1, 8) ||
       inlineAnnotation(render, "metal.descriptorInlineBinding", (2ULL << 32) | 2, 0,
          (uint64_t)(__bridge void *)inputA, 4)) return 12;
    [render setFragmentBytes:roots length:8 atIndex:2];
    [render endEncoding];
    [command presentDrawable:drawable]; [command commit]; [command waitUntilCompleted];
    // Reproduce retirement racing EndFrameCapture's file write. Post-end facts
    // must appear in the next start snapshot, never ahead of this frame's scope.
    std::atomic<int> retired{0};
    std::thread retirement([&]()
    {
      while(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
          "metal.capturePresented", eRENDERDOC_Empty, 0, nullptr) == 0)
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      int result = annotate("metal.descriptorSlotEvent", 24, 2, 1, 4);
      result |= annotate("metal.descriptorSlotEvent", 24, 3, 0, 5);
      memcpy((char *)table.contents + 24, first, 24);
      result |= annotate("metal.descriptorSlotEvent", 24, 3, 2, 5);
      result |= annotate("metal.descriptorSlotBinding", 24, 0, (uint64_t)(__bridge void *)inputA, 4);
      retired.store(result ? -1 : 1);
    });
    const bool ended = api->EndFrameCapture(nullptr, nullptr);
    retirement.join();
    if(command.error || !ended || retired.load() != 1)
    {
      fprintf(stderr,"slot first capture: status=%lu error=%s ended=%d retired=%d\n",(unsigned long)command.status,
          command.error ? command.error.localizedDescription.UTF8String : "none",ended,retired.load());
      return 8;
    }
    api->StartFrameCapture(nullptr, nullptr);
    fprintf(stderr,"slot second capture started=%u\n",api->IsFrameCapturing());
    drawable = [layer nextDrawable];
    command = [[device newCommandQueue] commandBuffer];
    pass.colorAttachments[0].texture = drawable.texture;
    [[command renderCommandEncoderWithDescriptor:pass] endEncoding];
    [command presentDrawable:drawable]; [command commit]; [command waitUntilCompleted];
    const bool secondEnded=api->EndFrameCapture(nullptr, nullptr);
    if(command.error || !secondEnded)
    {
      fprintf(stderr,"slot second capture: status=%lu error=%s ended=%d\n",(unsigned long)command.status,
          command.error ? command.error.localizedDescription.UTF8String : "none",secondEnded);
      return 13;
    }
    puts("PASS slot capture: generation 1 allocate/write/free, generation 2 allocate/write, GPU expected payload");
  }
  return 0;
}
