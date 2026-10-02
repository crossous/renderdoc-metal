// SPDX-License-Identifier: MIT
// Native proof of typed resource relocation across processes, not a RenderDoc replay feature.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

int main(int argc, char **argv)
{
  if(argc != 3 || (strcmp(argv[1], "write") && strcmp(argv[1], "read"))) return 2;
  const bool read = strcmp(argv[1], "read") == 0;
  @autoreleasepool
  {
    uint64_t captured[4] = {};
    if(read)
    {
      FILE *file = fopen(argv[2], "rb");
      if(!file) return 3;
      const bool valid = fread(captured, sizeof(captured), 1, file) == 1 && fgetc(file) == EOF;
      fclose(file);
      if(!valid) return 3;
    }
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device || device.argumentBuffersSupport != MTLArgumentBuffersTier2) return 4;
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Packet
{
  device const uint *input [[id(0)]];
  texture2d<float> image [[id(1)]];
  sampler point [[id(2)]];
  uint bias [[id(3)]];
};
kernel void relocate_probe(device Packet &packet [[buffer(0)]], device uint *output [[buffer(1)]])
{
  output[0] = packet.input[0] + uint(packet.image.sample(packet.point, float2(0.5)).r * 255.0 + 0.5)
            + packet.bias;
}
)MSL" options:nil error:&error];
    if(!library) { fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); return 5; }
    id<MTLFunction> function = [library newFunctionWithName:@"relocate_probe"];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
    id<MTLArgumentEncoder> encoder = [function newArgumentEncoderWithBufferIndex:0];
    if(!pipeline || encoder.encodedLength != sizeof(captured)) return 6;

    // Deliberately change the native allocation order in the importing process.
    id<MTLBuffer> padding = read ? [device newBufferWithLength:1048576 options:MTLResourceStorageModeShared] : nil;
    const uint32_t values[] = {0xbadU, 41U};
    id<MTLBuffer> input = [device newBufferWithBytes:values length:sizeof(values) options:MTLResourceStorageModeShared];
    if(read && captured[0] == input.gpuAddress + 4) return 7;
    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
        MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    desc.storageMode = MTLStorageModeShared;
    desc.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> texturePadding = read ? [device newTextureWithDescriptor:desc] : nil;
    id<MTLTexture> image = [device newTextureWithDescriptor:desc];
    const unsigned char pixel[] = {64, 128, 192, 255};
    [image replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:pixel bytesPerRow:4];
    MTLSamplerDescriptor *sampling = [MTLSamplerDescriptor new];
    sampling.supportArgumentBuffers = YES;
    id<MTLSamplerState> point = [device newSamplerStateWithDescriptor:sampling];
    if(!input || !image || !point || (read && (!padding || !texturePadding))) return 8;

    const uint32_t bias = 17, sentinel = 0xdeadbeefU;
    const uint64_t original[] = {input.gpuAddress + 4, image.gpuResourceID._impl,
                               point.gpuResourceID._impl, (uint64_t(sentinel) << 32) | bias};
    id<MTLBuffer> table = [device newBufferWithBytes:read ? captured : original
        length:sizeof(captured) options:MTLResourceStorageModeShared];
    if(!table) return 9;
    if(read)
    {
      // Schema and resource+offset associations are explicit in this fixture.
      // Only resource fields are re-encoded. Never submit the old pointer packet.
      [encoder setArgumentBuffer:table offset:0];
      if((char *)[encoder constantDataAtIndex:3] - (char *)table.contents != 24) return 10;
      [encoder setBuffer:input offset:4 atIndex:0];
      [encoder setTexture:image atIndex:1];
      [encoder setSamplerState:point atIndex:2];
      if(memcmp((char *)table.contents + 24, (char *)captured + 24, 8)) return 11;
      if(memcmp(table.contents, original, 24)) return 12;
    }
    else
    {
      FILE *file = fopen(argv[2], "wb");
      if(!file) return 13;
      bool written = fwrite(table.contents, sizeof(captured), 1, file) == 1;
      written = fclose(file) == 0 && written;
      if(!written) return 13;
    }
    id<MTLBuffer> output = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLCommandBuffer> command = [queue commandBuffer];
    id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
    [compute setComputePipelineState:pipeline];
    [compute setBuffer:table offset:0 atIndex:0];
    [compute setBuffer:output offset:0 atIndex:1];
    [compute useResource:input usage:MTLResourceUsageRead];
    [compute useResource:image usage:MTLResourceUsageRead];
    [compute dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
    [compute endEncoding];
    [command commit];
    [command waitUntilCompleted];
    if(command.error || command.status != MTLCommandBufferStatusCompleted ||
       *(uint32_t *)output.contents != 122) return 14;
    printf("typed relocation %s PASS result=122 oldVA=%llu newVA=%llu offset=4 constants=unchanged\n",
           read ? "read" : "write", (unsigned long long)(read ? captured[0] : original[0]),
           (unsigned long long)input.gpuAddress + 4);
  }
  return 0;
}
