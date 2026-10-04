// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    const bool interleaved = getenv("RENDERDOC_METAL_INTERLEAVED_SUBMISSIONS") != nullptr;
    const bool splitBinding = getenv("RENDERDOC_METAL_SPLIT_DESCRIPTOR_BINDING") != nullptr;
    if(splitBinding && interleaved) return 2;
    const bool interleavedTail = interleaved && strcmp(getenv("RENDERDOC_METAL_INTERLEAVED_SUBMISSIONS"), "tail") == 0;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice(); NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Entry { device const uint *value [[id(0)]]; ulong zero [[id(1)]]; ulong marker [[id(2)]]; };
kernel void read_entry(const device ulong *root [[buffer(0)]], device uint *out [[buffer(1)]])
{ auto entry = reinterpret_cast<const device Entry *>(root[0]);
  out[0] = entry->value[0]; out[1] = entry->marker == 0xabcdefUL ? 0xdeadbeefU : 0xbadU; }
kernel void read_entry_add(const device ulong *root [[buffer(0)]], device uint *out [[buffer(1)]])
{ auto entry = reinterpret_cast<const device Entry *>(root[0]);
  out[0] = entry->value[0] + 17; out[1] = entry->marker == 0xabcdefUL ? 0xdeadbeefU : 0xbadU; }
)MSL" options:nil error:&error];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"read_entry"] error:&error];
    id<MTLComputePipelineState> latePipeline = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"read_entry_add"] error:&error];
    if(!pipeline || !latePipeline) return 2;
    uint32_t values[] = {17, 34, 51};
    id<MTLBuffer> sources[3];
    for(unsigned i = 0; i < 3; i++) sources[i] = [device newBufferWithBytes:&values[i] length:4 options:MTLResourceStorageModeShared];
    id<MTLBuffer> output = [device newBufferWithLength:24 options:MTLResourceStorageModeShared];
    output.label = @"Future descriptor snapshot result"; memset(output.contents, 0, 24);
    MTLHeapDescriptor *heapDesc = [MTLHeapDescriptor new]; heapDesc.type = MTLHeapTypePlacement;
    heapDesc.storageMode = MTLStorageModeShared; heapDesc.hazardTrackingMode = MTLHazardTrackingModeTracked; heapDesc.size = 65536;
    id<MTLHeap> heap = [device newHeapWithDescriptor:heapDesc];
    MTLResourceOptions options = MTLResourceStorageModeShared | MTLResourceHazardTrackingModeTracked;
    id<MTLBuffer> old = [heap newBufferWithLength:splitBinding ? 48 : 24 options:options offset:0];
    memset(old.contents, 0, old.length);
    id<MTLBuffer> other = [device newBufferWithLength:24 options:MTLResourceStorageModeShared];
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0, (void **)&api)) return 3;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue coverage = {}; coverage.uint32 = 65;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
          "metal.descriptorCoverage", eRENDERDOC_UInt32, 0, &coverage)) return 4;
    }
    auto annotate = [&](id object, const char *key, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
      if(!api) return uint32_t(0);
      RENDERDOC_AnnotationValue value = {}; value.vector.uint64[0] = a; value.vector.uint64[1] = b;
      value.vector.uint64[2] = c; value.vector.uint64[3] = d;
      return api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)object, key, eRENDERDOC_UInt64, 4, &value);
    };
    auto table = [&](id<MTLBuffer> buffer, unsigned source) {
      id<MTLBuffer> input = interleaved && source == 1 ? output : sources[source];
      const uint64_t bytes[] = {input.gpuAddress, 0, 0xabcdef}; memcpy(buffer.contents, bytes, 24);
      return !annotate(buffer, "metal.descriptorTable", 1, 0, splitBinding && buffer == old ? 2 : 1, 24) &&
             !annotate(buffer, "metal.descriptorSlotEvent", 0, 1, 0, 0) &&
             !annotate(buffer, "metal.descriptorSlotEvent", 0, 1, 2, 0) &&
             !annotate(buffer, "metal.descriptorSlotBinding", 0, 0, (uint64_t)(__bridge void *)input, 0);
    };
    if(!table(old, splitBinding ? 1 : 0) || !table(other, 1)) return 5;
    if(splitBinding)
    {
      const uint64_t bytes[] = {sources[1].gpuAddress, 0, 0xabcdef};
      memcpy((uint8_t *)old.contents + 24, bytes, 24);
      if(annotate(old,"metal.descriptorSlotEvent",24,1,0,0) ||
         annotate(old,"metal.descriptorSlotEvent",24,1,2,0) ||
         annotate(old,"metal.descriptorSlotBinding",24,0,(uint64_t)(__bridge void *)sources[1],0) ||
         annotate(old,"metal.descriptorSlotEvent",24,1,1,0)) return 5;
    }
    id<MTLCommandQueue> queue = [device newCommandQueue];
    CAMetalLayer *layer = [CAMetalLayer layer]; layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm; layer.framebufferOnly = NO;
    layer.drawableSize = CGSizeMake(2, 2);
    MTLCommandBufferDescriptor *commandDesc = [MTLCommandBufferDescriptor new];
    commandDesc.retainedReferences = getenv("RENDERDOC_METAL_UNRETAINED_SUBMISSIONS") == nullptr;
    commandDesc.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    auto encode = [&](id<MTLCommandBuffer> command, id<MTLBuffer> input, unsigned index, uint64_t delta = 0) {
      id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
      [encoder setComputePipelineState:delta ? latePipeline : pipeline];
      if(annotate(encoder, "metal.descriptorInlineLayout", 0, 0, 1, 8) ||
         annotate(encoder, "metal.descriptorInlineBinding", 0, 0, (uint64_t)(__bridge void *)input, 0)) return false;
      const uint64_t root = input.gpuAddress; [encoder setBytes:&root length:8 atIndex:0];
      [encoder setBuffer:output offset:index * 8 atIndex:1];
      [encoder useResource:input usage:MTLResourceUsageRead];
      [encoder useResource:interleaved && index == 1 ? output : sources[index] usage:MTLResourceUsageRead];
      [encoder dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      [encoder endEncoding]; return true;
    };
    if(api) api->StartFrameCapture(nullptr, nullptr);
    if(splitBinding)
    {
      const uint64_t bytes[] = {sources[0].gpuAddress, 0, 0xabcdef};
      memcpy(old.contents, bytes, 24);
      if(annotate(old,"metal.descriptorSlotEvent",0,1,1,0) ||
         annotate(old,"metal.descriptorSlotEvent",0,2,0,0) ||
         annotate(old,"metal.descriptorSlotEvent",0,2,2,0) ||
         annotate(old,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)sources[0],0)) return 5;
    }
    id<MTLCommandBuffer> first = [queue commandBufferWithDescriptor:commandDesc];
    id<MTLCommandBuffer> middle = nil;
    if(interleavedTail && !encode(first, old, 0)) return 6;
    if(interleaved)
    {
      middle = [queue commandBufferWithDescriptor:commandDesc];
      if(!encode(middle, other, 1)) return 9;
    }
    if(!encode(first, old, 0, interleavedTail ? 17 : 0)) return 6;
    if(splitBinding && (annotate(old,"metal.descriptorSlotEvent",24,2,0,0) ||
        annotate(old,"metal.descriptorSlotEvent",24,2,2,0))) return 6;
    [first commit]; [first waitUntilCompleted];
    if(first.error || ((uint32_t *)output.contents)[0] != (interleavedTail ? 34U : 17U)) return 7;
    if(splitBinding && annotate(old,"metal.descriptorSlotBinding",24,0,(uint64_t)(__bridge void *)sources[1],0)) return 8;
    if(annotate(old, "metal.descriptorSlotEvent", 0, splitBinding ? 2 : 1, 1, 0)) return 8;
    if(!interleaved)
    {
      middle = [queue commandBufferWithDescriptor:commandDesc];
      if(!encode(middle, other, 1)) return 9;
    }
    // The selected middle dispatch precedes this birth in the encoding stream.
    // Its submission snapshot includes these typed bytes, but its GPU work
    // never reads the retired old table or its physical replacement.
    const bool freshRange = splitBinding || interleaved || getenv("RENDERDOC_METAL_NONOVERLAPPING_FUTURE_TABLE") != nullptr;
    id<MTLBuffer> future = [heap newBufferWithLength:24 options:options offset:freshRange ? 4096 : 0];
    if((future.gpuAddress == old.gpuAddress) == freshRange || !table(future, 2)) return 10;
    [middle commit]; [middle waitUntilCompleted]; if(middle.error) return 11;
    id<MTLCommandBuffer> last = [queue commandBufferWithDescriptor:commandDesc];
    if(!encode(last, future, 2)) return 12; [last commit]; [last waitUntilCompleted];
    const auto *words = (const uint32_t *)output.contents;
    for(unsigned i = 0; i < 3; i++) if(words[i * 2] != (interleavedTail && i < 2 ? 34U : interleaved && i == 1 ? 17U : values[i]) || words[i * 2 + 1] != 0xdeadbeefU) return 13;
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.1, 0.2, 0.3, 1);
    id<MTLCommandBuffer> present = [queue commandBufferWithDescriptor:commandDesc];
    id<MTLRenderCommandEncoder> clear = [present renderCommandEncoderWithDescriptor:pass];
    [clear endEncoding]; [present presentDrawable:drawable]; [present commit]; [present waitUntilCompleted];
    if(last.error || (api && !api->EndFrameCapture(nullptr, nullptr))) return 14;
    printf("Interleaved submission test=%d\n", interleaved);
    printf("PASS Native descriptor snapshots: %u/%u/%u; markers deadbeef\n", words[0], words[2], words[4]);
    return 0;
  }
}
