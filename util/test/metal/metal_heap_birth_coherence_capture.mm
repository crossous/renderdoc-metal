// SPDX-License-Identifier: MIT
// A heap allocation is not an upload of an observed GPU result. Both Native
// kernels keep their complete result invariant across address-derived values
// and unordered atomic allocation; the overlapping buffer is only declared.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

int main(int argc, char **argv)
{
  if(argc != 3 && argc != 4) return 1;
  const bool bufferCarrier = argc == 4 && strcmp(argv[3], "buffer") == 0;
  if(argc == 4 && !bufferCarrier) return 1;
  @autoreleasepool
  {
    auto device = MTLCreateSystemDefaultDevice();
    auto queue = [device newCommandQueue];
    NSError *error = nil;
    NSString *source = @"#include <metal_stdlib>\nusing namespace metal;\n"
      "kernel void make_data(device atomic_uint *counter [[buffer(0)]], device uint *map [[buffer(1)]], texture2d<uint,access::write> image [[texture(0)]], uint i [[thread_position_in_grid]]) { if(i>=4096) return; uint rank=atomic_fetch_add_explicit(counter,1u,memory_order_relaxed); uint seed=uint(reinterpret_cast<ulong>(map)>>12); map[i]=rank; image.write(uint4(i^seed,0,0,0),uint2(rank%64,rank/64)); }\n"
      "kernel void check_data(device const uint *map [[buffer(0)]], device uint *result [[buffer(1)]], texture2d<uint,access::read> image [[texture(0)]], uint i [[thread_position_in_grid]]) { if(i>=4096) return; uint rank=map[i]; uint seed=uint(reinterpret_cast<ulong>(map)>>12); result[i]=(image.read(uint2(rank%64,rank/64)).x!=(i^seed)); }\n"
      "kernel void make_buffer(device atomic_uint *counter [[buffer(0)]], device uint *map [[buffer(1)]], device uint *image [[buffer(2)]], uint i [[thread_position_in_grid]]) { if(i>=4096) return; uint rank=atomic_fetch_add_explicit(counter,1u,memory_order_relaxed); uint seed=uint(reinterpret_cast<ulong>(map)>>12); map[i]=rank; image[rank]=i^seed; }\n"
      "kernel void check_buffer(device const uint *map [[buffer(0)]], device uint *result [[buffer(1)]], device const uint *image [[buffer(2)]], uint i [[thread_position_in_grid]]) { if(i>=4096) return; uint rank=map[i]; uint seed=uint(reinterpret_cast<ulong>(map)>>12); result[i]=(image[rank]!=(i^seed)); }\n";
    auto library = [device newLibraryWithSource:source options:nil error:&error];
    if(!library) { fprintf(stderr, "%s\n", error.description.UTF8String); return 2; }
    auto make = [device newComputePipelineStateWithFunction:[library newFunctionWithName:bufferCarrier ? @"make_buffer" : @"make_data"] error:&error];
    auto check = [device newComputePipelineStateWithFunction:[library newFunctionWithName:bufferCarrier ? @"check_buffer" : @"check_data"] error:&error];
    if(!make || !check) { fprintf(stderr, "%s\n", error.description.UTF8String); return 2; }
    auto descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Uint width:64 height:64 mipmapped:NO];
    descriptor.storageMode = MTLStorageModePrivate;
    descriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;
    descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    const auto layout = bufferCarrier ? [device heapBufferSizeAndAlignWithLength:16384 options:MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeTracked] : [device heapTextureSizeAndAlignWithDescriptor:descriptor];
    auto hd = [MTLHeapDescriptor new]; hd.type = MTLHeapTypePlacement;
    hd.size = layout.size + layout.align;
    hd.storageMode = MTLStorageModePrivate; hd.hazardTrackingMode = MTLHazardTrackingModeTracked;
    auto heap = [device newHeapWithDescriptor:hd];
    id<MTLTexture> image = bufferCarrier ? nil : [heap newTextureWithDescriptor:descriptor offset:0];
    id<MTLBuffer> bufferImage = bufferCarrier ? [heap newBufferWithLength:16384 options:MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeTracked offset:0] : nil;
    auto counter = [device newBufferWithLength:4 options:MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeTracked];
    auto map = [device newBufferWithLength:16384 options:MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeTracked];
    auto result = [device newBufferWithLength:16448 options:MTLResourceStorageModeShared];
    auto table = [device newBufferWithLength:256 options:MTLResourceStorageModeShared];
    auto zero = [device newBufferWithLength:16384 options:MTLResourceStorageModeShared];
    if((bufferCarrier ? !bufferImage : !image) || !counter || !map || !result || !table || !zero || !queue) return 3;
    memset(result.contents, 0xa7, result.length); memset(zero.contents, 0, zero.length);
    memset(table.contents, 0, table.length);
    const uint64_t packet[] = {bufferCarrier ? bufferImage.gpuAddress : 0, bufferCarrier ? 0 : image.gpuResourceID._impl, bufferCarrier ? bufferImage.length : 0};
    memcpy(table.contents, packet, sizeof(packet));
    image.label = @"Coherent live GPU texture";
    map.label = @"Coherent GPU permutation";
    result.label = @"Heap creation coherence output";
    auto init = [queue commandBuffer]; auto b = [init blitCommandEncoder];
    [b fillBuffer:counter range:NSMakeRange(0,4) value:0];
    [b fillBuffer:map range:NSMakeRange(0,map.length) value:0];
    if(bufferCarrier) [b fillBuffer:bufferImage range:NSMakeRange(0,16384) value:0];
    else [b copyFromBuffer:zero sourceOffset:0 sourceBytesPerRow:256 sourceBytesPerImage:16384 sourceSize:MTLSizeMake(64,64,1)
      toTexture:image destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
    [b endEncoding]; [init commit]; [init waitUntilCompleted]; if(init.error) return 4;
    RENDERDOC_API_1_7_0 *api = nullptr;
    auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(get && !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 5;
    auto annotation = [&](const char *key, uint64_t a,uint64_t c,uint64_t d,uint64_t e) {
      if(!api) return 0u;
      RENDERDOC_AnnotationValue v = {}; v.vector.uint64[0]=a;v.vector.uint64[1]=c;v.vector.uint64[2]=d;v.vector.uint64[3]=e;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)table,key,eRENDERDOC_UInt64,4,&v);
    };
    if(api)
    {
      RENDERDOC_AnnotationValue coverage = {}; coverage.uint32 = 65;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage) ||
         annotation("metal.descriptorTable",1,0,1,24) || annotation("metal.descriptorSlotEvent",0,7,0,bufferCarrier ? 2 : 5) ||
         annotation("metal.descriptorSlotEvent",0,7,2,bufferCarrier ? 2 : 5) ||
         annotation("metal.descriptorSlotBinding",0,bufferCarrier ? 0 : 1,bufferCarrier ? (uint64_t)(__bridge void *)bufferImage : (uint64_t)(__bridge void *)image,0)) return 5;
      api->SetCaptureFilePathTemplate(argv[1]); api->StartFrameCapture(nullptr,nullptr);
    }
    auto producer = [queue commandBuffer]; auto p = [producer computeCommandEncoder];
    [p setComputePipelineState:make]; [p setBuffer:counter offset:0 atIndex:0]; [p setBuffer:map offset:0 atIndex:1];
    if(bufferCarrier) [p setBuffer:bufferImage offset:0 atIndex:2]; else [p setTexture:image atIndex:0];
    [p dispatchThreadgroups:MTLSizeMake(64,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
    [p endEncoding]; [producer commit]; [producer waitUntilCompleted]; if(producer.error) return 6;
    // No API writes this carrier. Its allocation shares the current image's
    // physical range and must not upload a recorded observation over that image.
    auto carrier = [heap newBufferWithLength:4096 options:MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeTracked offset:0];
    if(!carrier) return 7;
    auto consumer = [queue commandBuffer]; auto c = [consumer computeCommandEncoder];
    [c setComputePipelineState:check]; [c setBuffer:map offset:0 atIndex:0]; [c setBuffer:result offset:0 atIndex:1];
    if(bufferCarrier) [c setBuffer:bufferImage offset:0 atIndex:2]; else [c setTexture:image atIndex:0];
    [c useResource:carrier usage:MTLResourceUsageRead];
    [c dispatchThreadgroups:MTLSizeMake(64,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
    [c endEncoding]; [consumer commit]; [consumer waitUntilCompleted]; if(consumer.error) return 8;
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 9;
    unsigned errors = 0; for(unsigned i=0;i<4096;i++) errors += ((uint32_t *)result.contents)[i] != 0;
    for(unsigned i=16384;i<16448;i++) if(((unsigned char *)result.contents)[i] != 0xa7) return 10;
    FILE *file = fopen(argv[2],"wbx"); if(!file) return 11;
    const bool saved = fwrite(result.contents,1,result.length,file) == result.length; fclose(file);
    printf("Native GPU COMPLETED; coherence errors=%u complete=%lu; map-address=%llu heap=%lu texture-allocation=%lu\n",
      errors,result.length,(unsigned long long)map.gpuAddress,heap.size,layout.size);
    return saved && !errors ? 0 : 12;
  }
}
