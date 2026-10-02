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
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void sourced(const device ulong *root [[buffer(0)]], device ulong *output [[buffer(1)]])
{
  const device ulong *table = reinterpret_cast<const device ulong *>(root[0]);
  const device uint *input = reinterpret_cast<const device uint *>(table[0]);
  const device uint *direct = reinterpret_cast<const device uint *>(root[2]);
  const bool valid = root[1] == 0xabcdef0123456789ul && root[3] == 0xfeedfacec001d00dul &&
                     table[1] == 0 && table[2] == 0x123456789abcdef0ul && table[3] == 0;
  output[0] = ulong(valid && input[0] == direct[0] ? input[0] : 0xbadU) | (0xdeadbeeful << 32);
  output[1] = table[0]; output[2] = root[0]; output[3] = root[2];
}
)MSL" options:nil error:&error];
    id<MTLFunction> function = [library newFunctionWithName:@"sourced"];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
    MTLHeapDescriptor *hd = [MTLHeapDescriptor new];
    hd.type = MTLHeapTypePlacement; hd.storageMode = MTLStorageModeShared;
    hd.hazardTrackingMode = MTLHazardTrackingModeTracked; hd.size = 65536;
    id<MTLHeap> heap = [device newHeapWithDescriptor:hd];
    id<MTLBuffer> a = [heap newBufferWithLength:12 options:MTLResourceStorageModeShared offset:0];
    const MTLSizeAndAlign layout = [device heapBufferSizeAndAlignWithLength:12 options:MTLResourceStorageModeShared];
    const NSUInteger secondOffset = (layout.size + layout.align - 1) / layout.align * layout.align;
    id<MTLBuffer> b = [heap newBufferWithLength:12 options:MTLResourceStorageModeShared offset:secondOffset];
    id<MTLBuffer> table = [device newBufferWithLength:48 options:MTLResourceStorageModeShared];
    id<MTLBuffer> output = [device newBufferWithLength:32 options:MTLResourceStorageModeShared];
    if(!pipeline || !a || !b || !table || !output) return 2;
    const uint32_t values[] = {0xbadU, 41U, 80U};
    memcpy(a.contents, values, sizeof(values));
    memcpy(b.contents, values, sizeof(values));
    const uint64_t vaA = a.gpuAddress + 4, vaB = b.gpuAddress + 8, vaTable = table.gpuAddress;
    if(a.gpuAddress == b.gpuAddress || !vaA || !vaB || !vaTable) return 3;
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0, (void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue coverage = {}; coverage.uint32 = 4;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
          "metal.descriptorCoverage", eRENDERDOC_UInt32, 0, &coverage)) return 5;
    }
    auto annotate = [&](id object, const char *key, uint64_t x, uint64_t y, uint64_t z, uint64_t w)
    {
      if(!api) return uint32_t(0);
      RENDERDOC_AnnotationValue value = {};
      value.vector.uint64[0]=x; value.vector.uint64[1]=y; value.vector.uint64[2]=z; value.vector.uint64[3]=w;
      return api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)object,
          key, eRENDERDOC_UInt64, 4, &value);
    };
    if(annotate(table, "metal.descriptorTable", 1, 0, 2, 24)) return 6;
    auto write = [&](uint64_t offset, uint64_t generation, uint64_t address, id source, uint64_t member)
    {
      const uint64_t entry[] = {address, 0, 0x123456789abcdef0ULL};
      memcpy((char *)table.contents + offset, entry, 24);
      const uint32_t first = annotate(table, "metal.descriptorSlotEvent", offset, generation, 2, 5);
      const uint32_t second = annotate(table, "metal.descriptorSlotBinding", offset, 0, (uint64_t)(__bridge void *)source, member);
      return first | second;
    };
    if(annotate(table, "metal.descriptorSlotEvent", 0, 1, 0, 5) || write(0, 1, vaA, a, 4) ||
       annotate(table, "metal.descriptorSlotEvent", 24, 1, 0, 5) || write(24, 1, vaA, a, 4) ||
       annotate(table, "metal.descriptorSlotEvent", 24, 1, 1, 5)) return 7;
    // A retired entry retains ordinary bytes but has no live pointer in replay.
    ((uint64_t *)table.contents)[3] = 0;
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device=device; layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO; layer.drawableSize=CGSizeMake(2, 2);
    id<MTLCommandQueue> queue = [device newCommandQueue];
    for(int capture=0; capture < (api ? 2 : 1); capture++)
    {
      if(capture && (annotate(table, "metal.descriptorSlotEvent", 0, 2, 1, 5) ||
         annotate(table, "metal.descriptorSlotEvent", 0, 3, 0, 5) || write(0, 3, vaA, a, 4))) return 8;
      if(api) api->StartFrameCapture(nullptr, nullptr);
      const uint64_t oldGen = capture ? 3 : 1, newGen = capture ? 4 : 2;
      if(annotate(table, "metal.descriptorSlotEvent", 0, oldGen, 1, 5) ||
         annotate(table, "metal.descriptorSlotEvent", 0, newGen, 0, 5) || write(0, newGen, vaB, b, 8)) return 9;
      const uint64_t root[] = {vaTable, 0xabcdef0123456789ULL, vaB, 0xfeedfacec001d00dULL};
      id<MTLCommandBuffer> command = [queue commandBuffer];
      [command enqueue];
      id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
      [compute setComputePipelineState:pipeline];
      if(annotate(compute, "metal.descriptorInlineLayout", 0, 0, 2, 16) ||
         annotate(compute, "metal.descriptorInlineBinding", 0, 0, (uint64_t)(__bridge void *)table, 0) ||
         annotate(compute, "metal.descriptorInlineBinding", 0, 1, (uint64_t)(__bridge void *)b, 8)) return 10;
      [compute setBytes:root length:sizeof(root) atIndex:0];
      [compute setBuffer:output offset:0 atIndex:1];
      [compute useResource:table usage:MTLResourceUsageRead];
      [compute useResource:b usage:MTLResourceUsageRead];
      [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      [compute endEncoding];
      id<CAMetalDrawable> drawable = [layer nextDrawable];
      MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=drawable.texture;
      pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=MTLStoreActionStore;
      pass.colorAttachments[0].clearColor=MTLClearColorMake(0.125,0.25,0.5,1);
      [[command renderCommandEncoderWithDescriptor:pass] endEncoding];
      [command presentDrawable:drawable]; [command commit]; [command waitUntilCompleted];
      const uint32_t *result = (const uint32_t *)output.contents;
      if(command.error || result[0] != 80 || result[1] != 0xdeadbeefU) return 11;
      if(api && !api->EndFrameCapture(nullptr,nullptr)) return 12;
    }
    printf("sourced shadow native PASS result=80 VA_A=%llu VA_B=%llu VA_TABLE=%llu captures=%d\n",
        (unsigned long long)vaA,(unsigned long long)vaB,(unsigned long long)vaTable,api?2:0);
  }
  return 0;
}
