// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  if(argc != 3 && (argc != 4 || strcmp(argv[3], "drop-owner"))) return 1;
  const NSUInteger size = strtoull(argv[2], nullptr, 10);
  const NSUInteger backing = 1024 * 1024, offset = 128 * 1024, prefix = 64 * 1024;
  if(size != 229376 && size != backing) return 1;
  @autoreleasepool
  {
    __attribute__((objc_precise_lifetime)) id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue = [device newCommandQueue];
    auto descriptor = [MTLHeapDescriptor new];
    descriptor.type = MTLHeapTypePlacement; descriptor.size = 2 * backing;
    descriptor.storageMode = MTLStorageModePrivate;
    descriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;
    id<MTLHeap> heap = [device newHeapWithDescriptor:descriptor];
    const auto options = MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeTracked;
    id<MTLBuffer> original = [heap newBufferWithLength:backing options:options offset:0];
    id<MTLBuffer> upload = [device newBufferWithLength:backing options:MTLResourceStorageModeShared];
    id<MTLBuffer> prefixUpload = [device newBufferWithLength:prefix options:MTLResourceStorageModeShared];
    id<MTLBuffer> aliasUpload = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
    id<MTLBuffer> readOriginal = [device newBufferWithLength:backing options:MTLResourceStorageModeShared];
    id<MTLBuffer> readAlias = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
    if(!heap || !original || !upload || !prefixUpload || !aliasUpload || !readOriginal || !readAlias) return 2;
    original.label = @"Placement alias original";
    readOriginal.label = @"Placement alias original readback";
    readAlias.label = @"Placement alias replacement readback";
    memset(upload.contents, 0x11, backing);
    memset(prefixUpload.contents, 0x22, prefix); memset(aliasUpload.contents, 0x5a, size);
    auto initial = [queue commandBuffer]; auto initialBlit = [initial blitCommandEncoder];
    [initialBlit copyFromBuffer:upload sourceOffset:0 toBuffer:original destinationOffset:0 size:backing];
    [initialBlit endEncoding]; [initial commit]; [initial waitUntilCompleted];
    if(initial.status != MTLCommandBufferStatusCompleted || initial.error) return 3;
    RENDERDOC_API_1_7_0 *api = nullptr;
    auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(get && get(eRENDERDOC_API_Version_1_7_0, (void **)&api) != 1) return 4;
    if(api)
    {
      RENDERDOC_AnnotationValue coverage = {}; coverage.uint32 = 65;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
          "metal.descriptorCoverage", eRENDERDOC_UInt32, 0, &coverage)) return 4;
      api->SetCaptureFilePathTemplate(argv[1]);
    }
    if(argc == 4)
    {
      device = nil;
      if(!original.device || original.device != queue.device || original.device != heap.device) return 4;
    }
    if(api) api->StartFrameCapture(nullptr, nullptr);
    // Allocation while the prior encoder is still open changes no memory contents.
    // Both resources stay alive; tracked-heap submission order owns later writes.
    auto first = [queue commandBuffer]; auto firstBlit = [first blitCommandEncoder];
    [firstBlit copyFromBuffer:prefixUpload sourceOffset:0 toBuffer:original destinationOffset:0 size:prefix];
    id<MTLBuffer> alias = [heap newBufferWithLength:size options:options offset:offset];
    if(!alias) return 5;
    alias.label = @"Placement alias replacement";
    [firstBlit endEncoding]; [first commit];
    auto second = [queue commandBuffer]; auto secondBlit = [second blitCommandEncoder];
    [secondBlit copyFromBuffer:aliasUpload sourceOffset:0 toBuffer:alias destinationOffset:0 size:size];
    [secondBlit endEncoding]; [second commit];
    auto third = [queue commandBuffer]; auto thirdBlit = [third blitCommandEncoder];
    [thirdBlit copyFromBuffer:original sourceOffset:0 toBuffer:readOriginal destinationOffset:0 size:backing];
    [thirdBlit copyFromBuffer:alias sourceOffset:0 toBuffer:readAlias destinationOffset:0 size:size];
    [thirdBlit endEncoding]; [third commit]; [third waitUntilCompleted];
    if(third.status != MTLCommandBufferStatusCompleted || third.error) return 6;
    if(api && !api->EndFrameCapture(nullptr, nullptr)) return 7;
    const auto a = (const unsigned char *)readOriginal.contents;
    const auto b = (const unsigned char *)readAlias.contents;
    for(NSUInteger i = 0; i < backing; i++)
    {
      const unsigned char expected = i < prefix ? 0x22 : i >= offset && i - offset < size ? 0x5a : 0x11;
      if(a[i] != expected) { fprintf(stderr, "Native original mismatch %lu\n", (unsigned long)i); return 8; }
    }
    for(NSUInteger i = 0; i < size; i++)
      if(b[i] != 0x5a) { fprintf(stderr, "Native alias mismatch %lu\n", (unsigned long)i); return 8; }
    printf("PASS native placement alias: original=%lu alias=%lu offset=%lu, full overlap/padding bytes, open encoder allocation, ordered GPU writes/readbacks, dropped device owner=%d\n",
           (unsigned long)backing, (unsigned long)size, (unsigned long)offset, argc == 4);
    return 0;
  }
}
