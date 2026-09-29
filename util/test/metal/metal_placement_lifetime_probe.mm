// SPDX-License-Identifier: MIT
// Probe the native placement heap lifetime rules without submitting GPU work.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstdio>

int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device) return 2;

    const MTLResourceOptions options = MTLResourceStorageModePrivate |
                                       MTLResourceHazardTrackingModeTracked;
    const MTLSizeAndAlign layout = [device heapBufferSizeAndAlignWithLength:66560
                                                                   options:options];
    if(!layout.size || !layout.align) return 3;

    MTLHeapDescriptor *descriptor = [[MTLHeapDescriptor alloc] init];
    descriptor.size = layout.size * 4;
    descriptor.storageMode = MTLStorageModePrivate;
    descriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;
    descriptor.type = MTLHeapTypePlacement;
    id<MTLHeap> heap = [device newHeapWithDescriptor:descriptor];
    [descriptor release];
    if(!heap) return 4;

    id<MTLBuffer> first = [heap newBufferWithLength:66560 options:options offset:0];
    if(!first || first.heapOffset != 0) return 5;
    fprintf(stderr, "first aliasable before overlap: %d\n", first.isAliasable);
    id<MTLBuffer> overlapWhileLive =
        [heap newBufferWithLength:66560 options:options offset:0];
    fprintf(stderr, "live overlap: %s; first aliasable: %d\n",
            overlapWhileLive ? "created" : "rejected", first.isAliasable);
    [overlapWhileLive release];
    [first release];

    id<MTLBuffer> afterRelease =
        [heap newBufferWithLength:66560 options:options offset:0];
    fprintf(stderr, "release then reuse: %s\n", afterRelease ? "created" : "rejected");
    [afterRelease release];
    [heap release];
    return afterRelease ? 0 : 6;
  }
}
