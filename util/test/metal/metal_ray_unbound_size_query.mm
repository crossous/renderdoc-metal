// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <initializer_list>

// UE asks for TLAS allocation sizes before it has an instance buffer or child AS array.
// This helper asks the same query; it does not claim the corresponding builds replay.
static int Queries(id<MTLDevice> device)
{
  @autoreleasepool
  {
    for(unsigned indirect : {0u, 1u})
      for(NSUInteger count : {NSUInteger(1), NSUInteger(64)})
      {
        auto descriptor = [MTLInstanceAccelerationStructureDescriptor descriptor];
        descriptor.instanceCount = count;
        if(indirect)
        {
          descriptor.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeIndirect;
          descriptor.instanceDescriptorStride = sizeof(MTLIndirectAccelerationStructureInstanceDescriptor);
        }
        auto sizes = [device accelerationStructureSizesWithDescriptor:descriptor];
        auto heap = [device heapAccelerationStructureSizeAndAlignWithDescriptor:descriptor];
        if(!sizes.accelerationStructureSize || !sizes.buildScratchBufferSize || !heap.size ||
           !heap.align || descriptor.instanceDescriptorBuffer ||
           descriptor.instancedAccelerationStructures.count || descriptor.instanceCount != count)
          return 2;
        printf("unbound indirect=%u count=%lu size=%lu scratch=%lu refit=%lu heap=%lu align=%lu\n",
               indirect, (unsigned long)count, (unsigned long)sizes.accelerationStructureSize,
               (unsigned long)sizes.buildScratchBufferSize, (unsigned long)sizes.refitScratchBufferSize,
               (unsigned long)heap.size, (unsigned long)heap.align);
      }
    id<MTLBuffer> buffer = [device newBufferWithLength:
        64 + 2*sizeof(MTLIndirectAccelerationStructureInstanceDescriptor)
        options:MTLResourceStorageModePrivate];
    if(!buffer) return 3;
    auto descriptor = [MTLInstanceAccelerationStructureDescriptor descriptor];
    descriptor.instanceCount = 2;
    descriptor.instanceDescriptorBuffer = buffer;
    descriptor.instanceDescriptorBufferOffset = 64;
    descriptor.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeIndirect;
    descriptor.instanceDescriptorStride = sizeof(MTLIndirectAccelerationStructureInstanceDescriptor);
    auto sizes = [device accelerationStructureSizesWithDescriptor:descriptor];
    auto heap = [device heapAccelerationStructureSizeAndAlignWithDescriptor:descriptor];
    if(!sizes.accelerationStructureSize || !sizes.buildScratchBufferSize || !heap.size || !heap.align ||
       descriptor.instanceDescriptorBuffer != buffer || descriptor.instanceDescriptorBufferOffset != 64 ||
       descriptor.instancedAccelerationStructures.count)
      return 4;
    printf("private indirect=1 offset=64 count=2 size=%lu scratch=%lu refit=%lu heap=%lu align=%lu\n",
           (unsigned long)sizes.accelerationStructureSize, (unsigned long)sizes.buildScratchBufferSize,
           (unsigned long)sizes.refitScratchBufferSize, (unsigned long)heap.size, (unsigned long)heap.align);
    return 0;
  }
}

int main()
{
  setbuf(stdout, nullptr);
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    return device ? Queries(device) : 1;
  }
}
