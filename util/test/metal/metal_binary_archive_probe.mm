// SPDX-License-Identifier: MIT
// Native-only capability probe. This is not a RenderDoc capture/replay test.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "../demos/metal/metal_binary_archive_source.h"

int main(int argc, const char **argv)
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device) return 2;
    NSError *error = nil;
    NSString *source = [NSString stringWithUTF8String:kMetalBinaryArchiveSource];
    id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
    id<MTLFunction> function = [library newFunctionWithName:@"archive_probe"];
    if(!function)
    {
      NSLog(@"Binary archive probe: library/function failed: %@", error);
      return 3;
    }

    MTLComputePipelineDescriptor *pipeline = [MTLComputePipelineDescriptor new];
    pipeline.computeFunction = function;
    MTLRenderPipelineDescriptor *renderPipeline = [MTLRenderPipelineDescriptor new];
    renderPipeline.vertexFunction = [library newFunctionWithName:@"archive_vs"];
    renderPipeline.fragmentFunction = [library newFunctionWithName:@"archive_fs"];
    renderPipeline.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    MTLBinaryArchiveDescriptor *descriptor = [MTLBinaryArchiveDescriptor new];
    id<MTLBinaryArchive> archive = [device newBinaryArchiveWithDescriptor:descriptor error:&error];
    if(!archive || ![archive addComputePipelineFunctionsWithDescriptor:pipeline error:&error] ||
       ![archive addRenderPipelineFunctionsWithDescriptor:renderPipeline error:&error])
    {
      NSLog(@"Binary archive probe: creation/add failed: %@", error);
      return 4;
    }

    NSString *name = [NSString stringWithFormat:@"renderdoc-metal-archive-probe-%@.metallib",
                                               [NSUUID UUID].UUIDString];
    NSURL *url = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:name]];
    if(![archive serializeToURL:url error:&error])
    {
      NSLog(@"Binary archive probe: serialization failed: %@", error);
      return 5;
    }
    NSData *bytes = [NSData dataWithContentsOfURL:url];
    if(argc == 2 && bytes && ![bytes writeToFile:[NSString stringWithUTF8String:argv[1]]
                                     atomically:YES]) return 6;
    NSURL *relocated = [NSURL fileURLWithPath:
        [NSTemporaryDirectory() stringByAppendingPathComponent:
            [NSString stringWithFormat:@"relocated-%@", name]]];
    if(!bytes || ![bytes writeToURL:relocated atomically:YES]) return 6;
    [[NSFileManager defaultManager] removeItemAtURL:url error:nil];

    MTLBinaryArchiveDescriptor *emptyDescriptor = [MTLBinaryArchiveDescriptor new];
    id<MTLBinaryArchive> empty = [device newBinaryArchiveWithDescriptor:emptyDescriptor error:&error];
    if(!empty) return 7;
    pipeline.binaryArchives = @[ empty ];
    id<MTLComputePipelineState> expectedMiss =
        [device newComputePipelineStateWithDescriptor:pipeline
                                             options:MTLPipelineOptionFailOnBinaryArchiveMiss
                                          reflection:nil error:&error];
    if(expectedMiss)
    {
      NSLog(@"Binary archive probe: archive miss unexpectedly compiled");
      [[NSFileManager defaultManager] removeItemAtURL:relocated error:nil];
      return 7;
    }
    error = nil;
    renderPipeline.binaryArchives = @[ empty ];
    id<MTLRenderPipelineState> expectedRenderMiss =
        [device newRenderPipelineStateWithDescriptor:renderPipeline
                                            options:MTLPipelineOptionFailOnBinaryArchiveMiss
                                         reflection:nil error:&error];
    if(expectedRenderMiss)
    {
      NSLog(@"Binary archive probe: render archive miss unexpectedly compiled");
      [[NSFileManager defaultManager] removeItemAtURL:relocated error:nil];
      return 7;
    }

    error = nil;
    descriptor.url = relocated;
    id<MTLBinaryArchive> reopened = [device newBinaryArchiveWithDescriptor:descriptor error:&error];
    pipeline.binaryArchives = @[ reopened ];
    id<MTLComputePipelineState> state = reopened ?
        [device newComputePipelineStateWithDescriptor:pipeline
                                             options:MTLPipelineOptionFailOnBinaryArchiveMiss
                                          reflection:nil error:&error] : nil;
    renderPipeline.binaryArchives = reopened ? @[ reopened ] : @[];
    id<MTLRenderPipelineState> renderState = reopened ?
        [device newRenderPipelineStateWithDescriptor:renderPipeline
                                            options:MTLPipelineOptionFailOnBinaryArchiveMiss
                                         reflection:nil error:&error] : nil;
    [[NSFileManager defaultManager] removeItemAtURL:relocated error:nil];
    if(!state || !renderState)
    {
      NSLog(@"Binary archive probe: reopening/archive-only pipeline failed: %@", error);
      return 8;
    }

    id<MTLBuffer> output = [device newBufferWithLength:32 * sizeof(uint32_t)
                                                options:MTLResourceStorageModeShared];
    MTLTextureDescriptor *textureDescriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                           width:4 height:4 mipmapped:NO];
    textureDescriptor.usage = MTLTextureUsageRenderTarget;
    textureDescriptor.storageMode = MTLStorageModeShared;
    id<MTLTexture> texture = [device newTextureWithDescriptor:textureDescriptor];
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLCommandBuffer> command = [queue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
    [encoder setComputePipelineState:state];
    [encoder setBuffer:output offset:0 atIndex:0];
    [encoder dispatchThreads:MTLSizeMake(32, 1, 1)
      threadsPerThreadgroup:MTLSizeMake(32, 1, 1)];
    [encoder endEncoding];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> render = [command renderCommandEncoderWithDescriptor:pass];
    [render setRenderPipelineState:renderState];
    [render drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [render endEncoding];
    [command commit];
    [command waitUntilCompleted];
    if(command.error)
    {
      NSLog(@"Binary archive probe: GPU execution failed: %@", command.error);
      return 9;
    }
    const uint32_t *values = (const uint32_t *)output.contents;
    for(uint32_t i = 0; i < 32; ++i)
      if(values[i] != i + 17) return 10;
    uint8_t pixel[4] = {};
    [texture getBytes:pixel bytesPerRow:4 fromRegion:MTLRegionMake2D(2, 2, 1, 1)
         mipmapLevel:0];
    if(pixel[0] < 76 || pixel[0] > 77 || pixel[1] < 178 || pixel[1] > 179 ||
       pixel[2] != 51 || pixel[3] != 255)
    {
      NSLog(@"Binary archive probe: unexpected render pixel %u %u %u %u",
            pixel[0], pixel[1], pixel[2], pixel[3]);
      return 11;
    }
    NSLog(@"Binary archive probe passed: relocated archive, compute/render enforced miss, GPU output");
    return 0;
  }
}
