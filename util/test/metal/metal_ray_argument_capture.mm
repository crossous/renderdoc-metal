// SPDX-License-Identifier: MIT
// Device-created pointer argument packets used by the official ray tracing sample.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  if(argc != 2) return 1;
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  @autoreleasepool
  {
    RENDERDOC_API_1_6_0 *api = nullptr;
    auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(getAPI) getAPI(eRENDERDOC_API_Version_1_6_0, (void **)&api);
    NSError *error = nil;
    NSString *source = @"#include <metal_stdlib>\nusing namespace metal;\n"
      "struct Packet {array<device const uint *,2> values [[id(0)]];};\n"
      "kernel void sum(constant Packet &packet [[buffer(0)]], device uint *out [[buffer(1)]])"
      "{out[0]=packet.values[0][0]+packet.values[1][0];}\n";
    id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
    if(!library) {fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:
      [library newFunctionWithName:@"sum"] error:&error];
    if(!pipeline) {fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    MTLArgumentDescriptor *descriptor = [MTLArgumentDescriptor argumentDescriptor];
    descriptor.index = 0; descriptor.dataType = MTLDataTypePointer;
    descriptor.arrayLength = 2; descriptor.access = MTLBindingAccessReadOnly;
    id<MTLArgumentEncoder> encoder = [device newArgumentEncoderWithArguments:@[descriptor]];
    id<MTLBuffer> packet = [device newBufferWithLength:encoder.encodedLength
      options:MTLResourceStorageModeManaged];
    const unsigned a[] = {90,3}, b[] = {4};
    id<MTLBuffer> first = [device newBufferWithBytes:a length:sizeof(a) options:MTLResourceStorageModeManaged];
    id<MTLBuffer> second = [device newBufferWithBytes:b length:sizeof(b) options:MTLResourceStorageModeShared];
    [encoder setArgumentBuffer:packet offset:0];
    [encoder setBuffer:first offset:4 atIndex:0];
    [encoder setBuffer:second offset:0 atIndex:1];
    [packet didModifyRange:NSMakeRange(0,packet.length)];
    id<MTLBuffer> output = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
    *(unsigned *)output.contents = 99; output.label = @"Ray device argument output";
    packet.label = @"Ray device argument packet";
    first.label = @"Ray device argument first"; second.label = @"Ray device argument second";
    CAMetalLayer *layer = [CAMetalLayer layer]; layer.device=device;
    layer.pixelFormat=MTLPixelFormatRGBA16Float; layer.drawableSize=CGSizeMake(16,16);
    layer.framebufferOnly=NO;
    id<CAMetalDrawable> drawable = [layer nextDrawable]; if(!drawable) return 3;
    id<MTLCommandQueue> queue=[device newCommandQueue];
    if(api) {api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
    id<MTLCommandBuffer> command=[queue commandBuffer];
    id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];
    [compute setComputePipelineState:pipeline];
    [compute setBuffer:packet offset:0 atIndex:0]; [compute setBuffer:output offset:0 atIndex:1];
    [compute useResource:first usage:MTLResourceUsageRead];
    [compute useResource:second usage:MTLResourceUsageRead];
    [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
    [compute endEncoding]; [command presentDrawable:drawable];
    [command commit]; [command waitUntilCompleted];
    if(command.error || *(unsigned *)output.contents!=7) return 4;
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 5;
    puts("PASS device pointer array, Managed packet, offset member: 7");
  }
  return 0;
}
