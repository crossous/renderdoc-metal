// SPDX-License-Identifier: MIT
// Device query paths must work through the injected bridge without a frame/capture chunk.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <objc/runtime.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device) return 2;
    const bool expectBridge = std::getenv("RENDERDOC_METAL_EXPECT_BRIDGE") != NULL;
    const bool bridged = std::strstr(object_getClassName(device), "ObjCBridgeMTLDevice") != NULL;
    if(expectBridge != bridged)
    {
      std::fprintf(stderr,"Device bridge mismatch: %s\n",object_getClassName(device));
      return 3;
    }
    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
        MTLPixelFormatRGBA8Unorm width:64 height:32 mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    MTLSizeAndAlign texture = [device heapTextureSizeAndAlignWithDescriptor:descriptor];
    MTLSizeAndAlign buffer = [device heapBufferSizeAndAlignWithLength:4096
                                                              options:MTLResourceStorageModeShared];
    if(!texture.size || !texture.align || !buffer.size || !buffer.align)
      return 4;
    MTLSamplePosition positions[4] = {};
    [device getDefaultSamplePositions:positions count:4];
    for(const auto &position : positions)
      if(!std::isfinite(position.x) || !std::isfinite(position.y) ||
         position.x < 0 || position.x > 1 || position.y < 0 || position.y > 1)
        return 5;

    MTLSize tile = [device sparseTileSizeWithTextureType:MTLTextureType2D
                                             pixelFormat:MTLPixelFormatRGBA8Unorm sampleCount:1];
    if(tile.width && tile.height && tile.depth)
    {
      MTLRegion pixels = MTLRegionMake2D(1,2,17,19), tiles = {};
      [device convertSparsePixelRegions:&pixels toTileRegions:&tiles withTileSize:tile
                          alignmentMode:MTLSparseTextureRegionAlignmentModeOutward numRegions:1];
      MTLRegion roundtrip = {};
      [device convertSparseTileRegions:&tiles toPixelRegions:&roundtrip
                          withTileSize:tile numRegions:1];
      if(roundtrip.origin.x > pixels.origin.x || roundtrip.origin.y > pixels.origin.y ||
         roundtrip.size.width < pixels.size.width || roundtrip.size.height < pixels.size.height)
        return 6;
    }
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
    if(@available(macOS 13.0, *))
    {
      const NSUInteger pageBytes = [device sparseTileSizeInBytesForSparsePageSize:MTLSparsePageSize16];
      MTLSize pageTile = [device sparseTileSizeWithTextureType:MTLTextureType2D
                                                  pixelFormat:MTLPixelFormatRGBA8Unorm sampleCount:1
                                               sparsePageSize:MTLSparsePageSize16];
      if((pageBytes == 0) != (pageTile.width == 0)) return 7;
    }
#endif
    MTLTimestamp cpu = 0, gpu = 0;
    [device sampleTimestamps:&cpu gpuTimestamp:&gpu];
    if(!cpu) return 8;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_3
    if(@available(macOS 13.3, *))
    {
      const BOOL originalCompilation = device.shouldMaximizeConcurrentCompilation;
      device.shouldMaximizeConcurrentCompilation = !originalCompilation;
      if(device.shouldMaximizeConcurrentCompilation == originalCompilation) return 9;
      device.shouldMaximizeConcurrentCompilation = originalCompilation;
    }
#endif
    const bool raytracing = device.supportsRaytracing;
    const bool testRayQuery = raytracing || std::getenv("RENDERDOC_METAL_FORCE_RAY_QUERY") != NULL;
    if(testRayQuery)
    {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
      if(@available(macOS 13.0, *))
      {
        MTLSizeAndAlign bySize =
            [device heapAccelerationStructureSizeAndAlignWithSize:4096];
        if(!bySize.size || !bySize.align) return 10;
      }
#endif
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    id<MTLCommandQueue> queue = [device newCommandQueue];
    [queue insertDebugCaptureBoundary];
    [queue release];
#pragma clang diagnostic pop
    std::printf("device-query bridge=%d raytracing=%d rayQuery=%d heap=%llu/%llu buffer=%llu/%llu sample=(%g,%g) "
                "sparse=(%llu,%llu,%llu) timestamps=%llu/%llu\n",bridged,raytracing,testRayQuery,
                (unsigned long long)texture.size,(unsigned long long)texture.align,
                (unsigned long long)buffer.size,(unsigned long long)buffer.align,
                positions[0].x,positions[0].y,
                (unsigned long long)tile.width,(unsigned long long)tile.height,
                (unsigned long long)tile.depth,(unsigned long long)cpu,(unsigned long long)gpu);
    [device release];
    return 0;
  }
}
