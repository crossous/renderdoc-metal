// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  if(argc != 7) return 2;
  const unsigned width = atoi(argv[1]), height = atoi(argv[2]), count = atoi(argv[3]);
  const unsigned tx = atoi(argv[4]), ty = atoi(argv[5]), offset = atoi(argv[6]);
  if(!width || !height || width > 2048 || height > 2048 || !count || count > 16 ||
     !tx || !ty || tx * ty > 256 || offset > 1024 || offset % 4) return 2;
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice(); NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void image_words(device uint *out [[buffer(0)]], constant uint3 &params [[buffer(1)]],
                        uint2 pixel [[thread_position_in_grid]]) {
  if(pixel.x < params.x && pixel.y < params.y) {
    uint index = pixel.y * params.x + pixel.x;
    out[index] = index * 1664525u + params.z;
  }
})MSL" options:nil error:&error];
    if(!library) { fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); return 3; }
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:
        [library newFunctionWithName:@"image_words"] error:&error];
    if(!pipeline) return 4;
    const size_t words = width * height, bytes = offset + words * 4 + 128;
    std::vector<uint32_t> seed(bytes / 4, 0x6ad13b77U);
    id<MTLBuffer> output = [device newBufferWithBytes:seed.data() length:bytes
                                               options:MTLResourceStorageModeShared];
    output.label = @"LargeDispatchOutput";
    const bool indirect = getenv("METAL_LARGE_DISPATCH_INDIRECT") != nullptr;
    const uint32_t grid[3] = {(width + tx - 1) / tx, (height + ty - 1) / ty, 1};
    uint32_t arguments[16] = {}; memcpy(arguments + 7, grid, sizeof(grid));
    id<MTLBuffer> upload = indirect ? [device newBufferWithBytes:arguments length:sizeof(arguments)
                                  options:MTLResourceStorageModeShared] : nil;
    id<MTLBuffer> args = indirect ? [device newBufferWithLength:sizeof(arguments)
                                  options:MTLResourceStorageModePrivate] : nil;
    id<MTLCommandQueue> queue = [device newCommandQueue];
    RENDERDOC_API_1_7_0 *api = nullptr;
    auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(get && get(eRENDERDOC_API_Version_1_7_0, (void **)&api) != 1) return 5;
    if(api) api->StartFrameCapture(nullptr, nullptr);
    id<MTLCommandBuffer> command = [queue commandBuffer];
    if(indirect)
    {
      auto blit = [command blitCommandEncoder];
      [blit copyFromBuffer:upload sourceOffset:0 toBuffer:args destinationOffset:0 size:sizeof(arguments)];
      [blit endEncoding];
    }
    id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
    [encoder setComputePipelineState:pipeline];
    [encoder setBuffer:output offset:offset atIndex:0];
    for(unsigned pass = 0; pass < count; ++pass)
    {
      const uint32_t params[4] = {width, height, 0x12340000U + pass, 0};
      [encoder setBytes:params length:sizeof(params) atIndex:1];
      if(indirect) [encoder dispatchThreadgroupsWithIndirectBuffer:args indirectBufferOffset:28
                                    threadsPerThreadgroup:MTLSizeMake(tx, ty, 1)];
      else [encoder dispatchThreadgroups:MTLSizeMake(grid[0], grid[1], grid[2])
                   threadsPerThreadgroup:MTLSizeMake(tx, ty, 1)];
    }
    [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
    if(command.error) return 6;
    if(api && !api->EndFrameCapture(nullptr, nullptr)) return 7;
    const auto *actual = (const uint32_t *)output.contents;
    for(size_t i = 0; i < bytes / 4; ++i)
    {
      const bool active = i >= offset / 4 && i < offset / 4 + words;
      const uint32_t expected = active ? uint32_t(i - offset / 4) * 1664525U +
          0x12340000U + count - 1 : 0x6ad13b77U;
      if(actual[i] != expected) return 8;
    }
    if(const char *path = getenv("METAL_LARGE_DISPATCH_NATIVE_OUTPUT"))
    {
      FILE *f = fopen(path, "wb");
      if(!f) return 9;
      const bool saved = fwrite(actual, 1, bytes, f) == bytes; fclose(f);
      if(!saved) return 9;
    }
    printf("PASS Native %ux%u passes=%u threads=%ux%u offset=%u all=%zu bytes capture=%d\n",
           width, height, count, tx, ty, offset, bytes, api != nullptr);
  }
  return 0;
}
