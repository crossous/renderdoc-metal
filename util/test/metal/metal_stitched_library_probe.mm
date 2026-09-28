// SPDX-License-Identifier: MIT
// Native-only capability probe for a single stitched visible function.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device) return 2;
    NSError *error = nil;
    id<MTLLibrary> source = [device newLibraryWithSource:
        @"#include <metal_stdlib>\nusing namespace metal;\n"
         "[[stitchable]] float scale(float x) { return x * 2.0; }\n"
        options:nil error:&error];
    id<MTLFunction> function = [source newFunctionWithName:@"scale"];
    if(!function)
    {
      NSLog(@"Stitched library source failed: %@", error);
      return 3;
    }
    MTLFunctionStitchingInputNode *input = [[MTLFunctionStitchingInputNode alloc]
        initWithArgumentIndex:0];
    MTLFunctionStitchingFunctionNode *output = [[MTLFunctionStitchingFunctionNode alloc]
        initWithName:@"scale" arguments:@[ input ] controlDependencies:@[]];
    MTLFunctionStitchingGraph *graph = [[MTLFunctionStitchingGraph alloc]
        initWithFunctionName:@"stitched_scale" nodes:@[ output ]
                     outputNode:output attributes:@[]];
    MTLStitchedLibraryDescriptor *descriptor = [MTLStitchedLibraryDescriptor new];
    descriptor.functions = @[ function ];
    descriptor.functionGraphs = @[ graph ];
    MTLBinaryArchiveDescriptor *archiveDescriptor = [MTLBinaryArchiveDescriptor new];
    id<MTLBinaryArchive> archive = [device newBinaryArchiveWithDescriptor:archiveDescriptor
                                                                  error:&error];
    MTLFunctionDescriptor *functionDescriptor = [MTLFunctionDescriptor new];
    functionDescriptor.name = @"scale";
    if(!archive || ![archive addFunctionWithDescriptor:functionDescriptor library:source
                                                 error:&error])
    {
      NSLog(@"Stitched archive function add failed: %@", error);
      return 9;
    }
    if([archive respondsToSelector:@selector(addLibraryWithDescriptor:error:)] &&
       ![archive addLibraryWithDescriptor:descriptor error:&error])
    {
      NSLog(@"Stitched archive library add failed: %@", error);
      return 10;
    }
    id<MTLLibrary> stitched = [device newLibraryWithStitchedDescriptor:descriptor error:&error];
    id<MTLFunction> result = [stitched newFunctionWithName:@"stitched_scale"];
    if(!result)
    {
      NSLog(@"Stitched library build/function failed: %@", error);
      return 4;
    }
    id<MTLLibrary> executable = [device newLibraryWithSource:
        @"#include <metal_stdlib>\nusing namespace metal;\n"
         "kernel void stitched_probe(device float *out [[buffer(0)]], "
         "visible_function_table<float(float)> table [[buffer(1)]]) "
         "{ out[0] = table[0](3.0); }\n"
        options:nil error:&error];
    id<MTLFunction> compute = [executable newFunctionWithName:@"stitched_probe"];
    if(!compute) { NSLog(@"Stitched probe compute failed: %@", error); return 5; }
    MTLComputePipelineDescriptor *pipelineDescriptor = [MTLComputePipelineDescriptor new];
    pipelineDescriptor.computeFunction = compute;
    MTLLinkedFunctions *linked = [MTLLinkedFunctions new];
    linked.functions = @[ result ];
    pipelineDescriptor.linkedFunctions = linked;
    id<MTLComputePipelineState> pipeline = [device
        newComputePipelineStateWithDescriptor:pipelineDescriptor options:0
                                    reflection:nil error:&error];
    if(!pipeline) { NSLog(@"Stitched probe pipeline failed: %@", error); return 6; }
    id<MTLFunctionHandle> handle = [pipeline functionHandleWithFunction:result];
    MTLVisibleFunctionTableDescriptor *tableDescriptor =
        [MTLVisibleFunctionTableDescriptor new];
    tableDescriptor.functionCount = 1;
    id<MTLVisibleFunctionTable> table = [pipeline
        newVisibleFunctionTableWithDescriptor:tableDescriptor];
    if(!handle || !table) return 7;
    [table setFunction:handle atIndex:0];
    id<MTLBuffer> buffer = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLCommandBuffer> command = [queue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
    [encoder setComputePipelineState:pipeline];
    [encoder setBuffer:buffer offset:0 atIndex:0];
    [encoder setVisibleFunctionTable:table atBufferIndex:1];
    [encoder dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
    [encoder endEncoding];
    [command commit];
    [command waitUntilCompleted];
    if(command.error || *(float *)buffer.contents != 6.0f)
    {
      NSLog(@"Stitched probe output failed: %@ %f", command.error,
            *(float *)buffer.contents);
      return 8;
    }
    NSLog(@"Stitched library probe passed: %@, type=%lu, output=%f", result.name,
          (unsigned long)result.functionType, *(float *)buffer.contents);
    return 0;
  }
}
