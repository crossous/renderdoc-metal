#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstdio>
#include <cstdint>

// Native capability probe only. The real UE Shader Converter descriptor also carries a
// texture ID and metadata; this deliberately checks just a GPU VA read through an MSL pointer.
int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device) return 2;
    NSError *error = nil;
    NSString *source = @R"METAL(
#include <metal_stdlib>
using namespace metal;
kernel void probe(device const ulong *table [[buffer(0)]],
                  device uint *output [[buffer(1)]])
{
  device const uint *input = reinterpret_cast<device const uint *>(table[0]);
  output[0] = input[0] + 1;
}
)METAL";
    id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
    if(!library)
    {
      fprintf(stderr, "MSL compile failed: %s\n", error.localizedDescription.UTF8String);
      return 3;
    }
    id<MTLFunction> function = [library newFunctionWithName:@"probe"];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function
                                                                               error:&error];
    if(!pipeline) return 4;
    uint32_t inputValue = 41;
    id<MTLBuffer> input = [device newBufferWithBytes:&inputValue length:sizeof(inputValue)
                                            options:MTLResourceStorageModeShared];
    uint64_t gpuVA = input.gpuAddress;
    id<MTLBuffer> table = [device newBufferWithBytes:&gpuVA length:sizeof(gpuVA)
                                            options:MTLResourceStorageModeShared];
    id<MTLBuffer> output = [device newBufferWithLength:sizeof(uint32_t)
                                              options:MTLResourceStorageModeShared];
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLCommandBuffer> command = [queue commandBuffer];
    id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
    [compute setComputePipelineState:pipeline];
    [compute setBuffer:table offset:0 atIndex:0];
    [compute setBuffer:output offset:0 atIndex:1];
    [compute dispatchThreadgroups:MTLSizeMake(1, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
    [compute endEncoding];
    [command commit];
    [command waitUntilCompleted];
    if(command.error)
    {
      fprintf(stderr, "GPU failed: %s\n", command.error.localizedDescription.UTF8String);
      return 5;
    }
    const uint32_t result = *(const uint32_t *)output.contents;
    printf("native GPU VA probe: result=%u\n", result);
    return result == 42 ? 0 : 6;
  }
}
