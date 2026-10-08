// SPDX-License-Identifier: MIT
// Independent logical rows/mips/array initial state and a parent-backed view.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  if(argc != 8) return 2;
  const unsigned width = atoi(argv[1]), height = atoi(argv[2]), layers = atoi(argv[3]);
  const unsigned bits = atoi(argv[4]), selectedSlice = atoi(argv[5]), selectedMip = atoi(argv[6]);
  const unsigned offset = atoi(argv[7]), mipCount = 2;
  if(width < 8193 || width > 16384 || !height || height > 16 || layers < 2 || layers > 4 ||
     (bits != 8 && bits != 16) || selectedSlice >= layers || selectedMip >= mipCount ||
     offset > 256 || offset % 4) return 2;
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue = [device newCommandQueue];
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void wide_initial(texture2d<uint, access::read> image [[texture(0)]],
    device uint *out [[buffer(0)]], constant uint2 &extent [[buffer(1)]],
    uint2 pixel [[thread_position_in_grid]]) {
  if(pixel.x < extent.x && pixel.y < extent.y)
    out[pixel.y * extent.x + pixel.x] = image.read(pixel).r;
})MSL" options:nil error:&error];
    if(!library) return 3;
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:
        [library newFunctionWithName:@"wide_initial"] error:&error];
    if(!pipeline) return 3;
    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.textureType = MTLTextureType2DArray;
    desc.pixelFormat = bits == 8 ? MTLPixelFormatR8Uint : MTLPixelFormatR16Uint;
    desc.width = width; desc.height = height; desc.arrayLength = layers;
    desc.mipmapLevelCount = mipCount;
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView;
    id<MTLTexture> image = [device newTextureWithDescriptor:desc];
    if(!image) return 4;
    image.label = @"WideInitialParent";
    auto value = [&](unsigned x, unsigned y, unsigned mip, unsigned slice) {
      return (x * 13 + y * 29 + mip * 71 + slice * 107 + 17) & ((1U << bits) - 1);
    };
    // All background data is explicitly defined by an ordinary native blit.
    // Capture starts only after this producer completes, so replay needs initial state.
    for(unsigned slice = 0; slice < layers; ++slice)
      for(unsigned mip = 0; mip < mipCount; ++mip)
      {
        const unsigned w = width >> mip, h = std::max(1U, height >> mip);
        const size_t pitch = (size_t(w) * (bits / 8) + 255) & ~size_t(255);
        std::vector<uint8_t> seed(pitch * h, 0);
        for(unsigned y = 0; y < h; ++y)
          for(unsigned x = 0; x < w; ++x)
          {
            const unsigned v = value(x, y, mip, slice);
            seed[y * pitch + x * (bits / 8)] = uint8_t(v);
            if(bits == 16) seed[y * pitch + x * 2 + 1] = uint8_t(v >> 8);
          }
        id<MTLBuffer> upload = [device newBufferWithBytes:seed.data() length:seed.size()
                                                options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> command = [queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
        [blit copyFromBuffer:upload sourceOffset:0 sourceBytesPerRow:pitch sourceBytesPerImage:pitch * h
                 sourceSize:MTLSizeMake(w, h, 1) toTexture:image destinationSlice:slice
           destinationLevel:mip destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blit endEncoding]; [command commit]; [command waitUntilCompleted];
        if(command.error) return 5;
      }
    id<MTLTexture> view = [image newTextureViewWithPixelFormat:desc.pixelFormat
        textureType:MTLTextureType2D levels:NSMakeRange(selectedMip, 1)
             slices:NSMakeRange(selectedSlice, 1)];
    if(!view) return 6;
    const unsigned w = width >> selectedMip, h = std::max(1U, height >> selectedMip);
    std::vector<uint32_t> seed(offset / 4 + size_t(w) * h + 32, 0x6ad13b77U);
    id<MTLBuffer> output = [device newBufferWithBytes:seed.data() length:seed.size() * 4
                                           options:MTLResourceStorageModeShared];
    // Reuse the full-byte comparison helper, including untouched prefix/tail.
    output.label = @"LargeDispatchOutput";
    RENDERDOC_API_1_7_0 *api = nullptr;
    auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(get && get(eRENDERDOC_API_Version_1_7_0, (void **)&api) != 1) return 7;
    if(api) api->StartFrameCapture(nullptr, nullptr);
    id<MTLCommandBuffer> command = [queue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
    [encoder setComputePipelineState:pipeline];
    [encoder setTexture:view atIndex:0]; [encoder setBuffer:output offset:offset atIndex:0];
    const uint32_t extent[2] = {w, h};
    [encoder setBytes:extent length:sizeof(extent) atIndex:1];
    [encoder dispatchThreadgroups:MTLSizeMake((w + 31) / 32, (h + 3) / 4, 1)
             threadsPerThreadgroup:MTLSizeMake(32, 4, 1)];
    [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
    if(command.error || (api && !api->EndFrameCapture(nullptr, nullptr))) return 8;
    const uint32_t *actual = (const uint32_t *)output.contents;
    for(size_t i = 0; i < seed.size(); ++i)
    {
      const bool active = i >= offset / 4 && i < offset / 4 + size_t(w) * h;
      const size_t p = i - offset / 4;
      const uint32_t expected = active ? value(p % w, p / w, selectedMip, selectedSlice) : seed[i];
      if(actual[i] != expected) return 9;
    }
    if(const char *path = getenv("METAL_LARGE_DISPATCH_NATIVE_OUTPUT"))
    {
      FILE *f = fopen(path, "wb"); if(!f) return 10;
      const bool saved = fwrite(actual, 4, seed.size(), f) == seed.size(); fclose(f);
      if(!saved) return 10;
    }
    printf("PASS Native wide initial %ux%u array=%u R%uUint view slice=%u mip=%u offset=%u\n",
           width, height, layers, bits, selectedSlice, selectedMip, offset);
  }
  return 0;
}
