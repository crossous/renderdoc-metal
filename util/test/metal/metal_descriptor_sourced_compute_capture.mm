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
    const bool interleavedProducer = getenv("RENDERDOC_METAL_INTERLEAVED_PRODUCER") != nullptr;
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
kernel void copy_sourced(const device ulong *root [[buffer(0)]], device uint *marker [[buffer(1)]])
{
  const device ulong *source = reinterpret_cast<const device ulong *>(root[0]);
  device ulong *destination = reinterpret_cast<device ulong *>(root[2]);
  if(root[1] == 0xabcdef0123456789ul && root[3] == 0xfeedfacec001d00dul)
    for(uint index = 0; index < 3; ++index) destination[index] = source[index];
  marker[1] = 0xdeadbeefU;
}
)MSL" options:nil error:&error];
    id<MTLFunction> function = [library newFunctionWithName:@"sourced"];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
    id<MTLComputePipelineState> producerPipeline = [device newComputePipelineStateWithFunction:
        [library newFunctionWithName:@"copy_sourced"] error:&error];
    MTLHeapDescriptor *hd = [MTLHeapDescriptor new];
    hd.type = MTLHeapTypePlacement; hd.storageMode = MTLStorageModeShared;
    hd.hazardTrackingMode = MTLHazardTrackingModeTracked; hd.size = 65536;
    id<MTLHeap> heap = [device newHeapWithDescriptor:hd];
    id<MTLBuffer> a = [heap newBufferWithLength:12 options:MTLResourceStorageModeShared offset:0];
    const MTLSizeAndAlign layout = [device heapBufferSizeAndAlignWithLength:12 options:MTLResourceStorageModeShared];
    const NSUInteger secondOffset = (layout.size + layout.align - 1) / layout.align * layout.align;
    id<MTLBuffer> b = [heap newBufferWithLength:12 options:MTLResourceStorageModeShared offset:secondOffset];
    id<MTLBuffer> table = [device newBufferWithLength:48 options:MTLResourceStorageModeShared];
    id<MTLBuffer> output = [device newBufferWithLength:64 options:MTLResourceStorageModeShared];
    id<MTLBuffer> payload = [device newBufferWithLength:24 options:MTLResourceStorageModeShared];
    if(!pipeline || !producerPipeline || !a || !b || !table || !output || !payload) return 2;
    const uint32_t values[] = {0xbadU, 41U, 80U};
    memcpy(a.contents, values, sizeof(values));
    memcpy(b.contents, values, sizeof(values));
    const uint64_t vaA = a.gpuAddress + 4, vaB = b.gpuAddress + 8, vaTable = table.gpuAddress, vaPayload = payload.gpuAddress;
    if(a.gpuAddress == b.gpuAddress || !vaA || !vaB || !vaTable) return 3;
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0, (void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue coverage = {}; coverage.uint32 = interleavedProducer ? 65 : 6;
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
    if(annotate(table, "metal.descriptorTable", 1, 0, 2, 24) ||
       annotate(payload, "metal.descriptorTable", 1, 0, 1, 24)) return 6;
    if(api)
    {
      RENDERDOC_AnnotationValue writes = {}; writes.uint32 = 1;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)table,
          "metal.descriptorGPUWrites", eRENDERDOC_UInt32, 0, &writes)) return 13;
    }
    auto write = [&](id<MTLBuffer> target, uint64_t address, id source, uint64_t member)
    {
      const uint64_t entry[] = {address, 0, 0x123456789abcdef0ULL};
      memcpy(target.contents, entry, 24);
      const uint32_t first = annotate(target, "metal.descriptorSlotEvent", 0, 1, 2, 5);
      const uint32_t second = annotate(target, "metal.descriptorSlotBinding", 0, 0, (uint64_t)(__bridge void *)source, member);
      return first | second;
    };
    memset(table.contents, 0, 48);
    if(annotate(table, "metal.descriptorSlotEvent", 0, 1, 0, 5) || write(table, vaA, a, 4) ||
       annotate(payload, "metal.descriptorSlotEvent", 0, 1, 0, 5) || write(payload, vaB, b, 8)) return 7;
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device=device; layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO; layer.drawableSize=CGSizeMake(2, 2);
    id<MTLCommandQueue> queue = [device newCommandQueue];
    MTLCommandBufferDescriptor *commandDesc = [MTLCommandBufferDescriptor new];
    commandDesc.retainedReferences = getenv("RENDERDOC_METAL_UNRETAINED_SUBMISSIONS") == nullptr;
    MTLTextureDescriptor *independentDesc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:1 height:1 mipmapped:NO];
    independentDesc.storageMode = MTLStorageModeShared;
    independentDesc.usage = MTLTextureUsageRenderTarget;
    id<MTLTexture> independentTarget = interleavedProducer ? [device newTextureWithDescriptor:independentDesc] : nil;
    const uint8_t seed[] = {0, 0, 0, 255};
    if(independentTarget) [independentTarget replaceRegion:MTLRegionMake2D(0,0,1,1)
        mipmapLevel:0 withBytes:seed bytesPerRow:4];
    for(int capture=0; capture < (api ? 2 : 1); capture++)
    {
      if(capture && write(payload, vaA, a, 4)) return 8;
      if(api) api->StartFrameCapture(nullptr, nullptr);
      id<MTLCommandBuffer> independent = nil;
      if(interleavedProducer)
      {
        independent = [queue commandBufferWithDescriptor:commandDesc]; [independent enqueue];
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = independentTarget;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.75,0.25,0.5,1);
        [[independent renderCommandEncoderWithDescriptor:pass] endEncoding];
      }
      id<MTLCommandBuffer> command = [queue commandBufferWithDescriptor:commandDesc]; [command enqueue];
      auto consume = [&](id<MTLBuffer> direct, uint64_t address, uint64_t member, NSUInteger offset)
      {
        const uint64_t root[] = {vaTable, 0xabcdef0123456789ULL, address, 0xfeedfacec001d00dULL};
        id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
        [compute setComputePipelineState:pipeline];
        if(annotate(compute, "metal.descriptorInlineLayout", 0, 0, 2, 16) ||
           annotate(compute, "metal.descriptorInlineBinding", 0, 0, (uint64_t)(__bridge void *)table, 0) ||
           annotate(compute, "metal.descriptorInlineBinding", 0, 1, (uint64_t)(__bridge void *)direct, member)) return false;
        [compute setBytes:root length:sizeof(root) atIndex:0];
        [compute setBuffer:output offset:offset atIndex:1];
        [compute useResource:table usage:MTLResourceUsageRead];
        [compute useResource:direct usage:MTLResourceUsageRead];
        [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
        [compute endEncoding];
        return true;
      };
      if(!consume(capture ? b : a, capture ? vaB : vaA, capture ? 8 : 4, 0)) return 10;
      const uint64_t producerRoot[] = {vaPayload, 0xabcdef0123456789ULL,
                                       vaTable, 0xfeedfacec001d00dULL};
      id<MTLComputeCommandEncoder> producer = [command computeCommandEncoder];
      [producer setComputePipelineState:producerPipeline];
      if(annotate(producer, "metal.descriptorInlineLayout", 0, 0, 2, 16) ||
         annotate(producer, "metal.descriptorInlineBinding", 0, 0, (uint64_t)(__bridge void *)payload, 0) ||
         annotate(producer, "metal.descriptorInlineBinding", 0, 1, (uint64_t)(__bridge void *)table, 0)) return 15;
      [producer setBytes:producerRoot length:sizeof(producerRoot) atIndex:0];
      [producer setBuffer:output offset:0 atIndex:1];
      [producer useResource:payload usage:MTLResourceUsageRead];
      [producer useResource:table usage:MTLResourceUsageWrite];
      [producer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      if(annotate(table, "metal.descriptorSlotProducer", 0, (uint64_t)(__bridge void *)producer,
                   (uint64_t)(__bridge void *)payload, 0)) return 16;

      // A different thread can submit unrelated Native work between these two
      // annotations while this producer encoder is still recording.
      if(independent) [independent commit];

      id<MTLBuffer> next = capture ? a : b;
      const uint64_t nextVA = capture ? vaA : vaB, nextMember = capture ? 4 : 8;
      if(annotate(table, "metal.descriptorSlotGPUValue", 0, nextVA, 0, 0x123456789abcdef0ULL) ||
         annotate(table, "metal.descriptorSlotBinding", 0, 0, (uint64_t)(__bridge void *)next, nextMember)) return 14;
      [producer endEncoding];
      if(!consume(next, nextVA, nextMember, 32)) return 14;
      id<CAMetalDrawable> drawable = [layer nextDrawable];
      MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=drawable.texture;
      pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=MTLStoreActionStore;
      pass.colorAttachments[0].clearColor=MTLClearColorMake(0.125,0.25,0.5,1);
      [[command renderCommandEncoderWithDescriptor:pass] endEncoding];
      [command presentDrawable:drawable]; [command commit]; [command waitUntilCompleted];
      const uint32_t *result = (const uint32_t *)output.contents;
      if(command.error || result[0] != (capture ? 80U : 41U) || result[1] != 0xdeadbeefU ||
         result[8] != (capture ? 41U : 80U) || result[9] != 0xdeadbeefU) return 11;
      if(independent)
      {
        uint8_t pixel[4] = {};
        [independentTarget getBytes:pixel bytesPerRow:4 fromRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0];
        const uint8_t expected[] = {128,64,191,255};
        if(independent.error || memcmp(pixel, expected, 4)) return 17;
      }
      if(api && !api->EndFrameCapture(nullptr,nullptr)) return 12;
    }
    printf("sourced GPU compute native PASS result=41->80 VA_A=%llu VA_B=%llu VA_TABLE=%llu captures=%d\n",
        (unsigned long long)vaA,(unsigned long long)vaB,(unsigned long long)vaTable,api?2:0);
  }
  return 0;
}
