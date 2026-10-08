// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <initializer_list>

static int RunQueries(id<MTLDevice> device)
{
  @autoreleasepool
  {
    const float vertices[12] = {-1,-1,0,0, 1,-1,0,0, 0,1,0,0};
    const uint16_t indices[3] = {0,1,2};
    id<MTLBuffer> vertex = [device newBufferWithBytes:vertices length:sizeof(vertices)
                                           options:MTLResourceStorageModeShared];
    id<MTLBuffer> index = [device newBufferWithBytes:indices length:sizeof(indices)
                                          options:MTLResourceStorageModeShared];
    MTLAccelerationStructureTriangleGeometryDescriptor *geometry =
        [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    geometry.vertexBuffer = vertex;
    geometry.vertexStride = 16;
    geometry.triangleCount = 1;
    MTLPrimitiveAccelerationStructureDescriptor *primitive =
        [MTLPrimitiveAccelerationStructureDescriptor descriptor];
    primitive.geometryDescriptors = @[geometry];
    for(unsigned indexed = 0; indexed < 2; indexed++)
    {
      geometry.indexBuffer = indexed ? index : nil;
      geometry.indexType = MTLIndexTypeUInt16;
      auto sizes = [device accelerationStructureSizesWithDescriptor:primitive];
      auto layout = [device heapAccelerationStructureSizeAndAlignWithDescriptor:primitive];
      if(!sizes.accelerationStructureSize || !sizes.buildScratchBufferSize ||
         !layout.size || !layout.align || geometry.vertexBuffer != vertex ||
         geometry.indexBuffer != (indexed ? index : nil) ||
         primitive.geometryDescriptors[0] != geometry)
      {
        fprintf(stderr, "Primitive query invalid/mutated: sizes=%lu/%lu layout=%lu/%lu vertexSame=%d indexSame=%d geometrySame=%d\n",
                (unsigned long)sizes.accelerationStructureSize, (unsigned long)sizes.buildScratchBufferSize,
                (unsigned long)layout.size, (unsigned long)layout.align, geometry.vertexBuffer == vertex,
                geometry.indexBuffer == (indexed ? index : nil), primitive.geometryDescriptors[0] == geometry);
        return 2;
      }
      printf("primitive indexed=%u size=%lu build=%lu refit=%lu heap=%lu align=%lu\n",
             indexed, (unsigned long)sizes.accelerationStructureSize,
             (unsigned long)sizes.buildScratchBufferSize, (unsigned long)sizes.refitScratchBufferSize,
             (unsigned long)layout.size, (unsigned long)layout.align);
    }
    auto sizes = [device accelerationStructureSizesWithDescriptor:primitive];
    NSArray *children = @[[device newAccelerationStructureWithSize:sizes.accelerationStructureSize],
                          [device newAccelerationStructureWithSize:sizes.accelerationStructureSize]];
    for(MTLStorageMode storage : {MTLStorageModeShared, MTLStorageModeManaged, MTLStorageModePrivate})
    {
      id<MTLBuffer> input = [device newBufferWithLength:2 * sizeof(MTLAccelerationStructureInstanceDescriptor)
                                              options:MTLResourceOptions(storage << MTLResourceStorageModeShift)];
      if(!input) return 3;
      MTLInstanceAccelerationStructureDescriptor *instance =
          [MTLInstanceAccelerationStructureDescriptor descriptor];
      instance.instanceCount = 2;
      instance.instanceDescriptorBuffer = input;
      instance.instancedAccelerationStructures = children;
      auto result = [device accelerationStructureSizesWithDescriptor:instance];
      auto layout = [device heapAccelerationStructureSizeAndAlignWithDescriptor:instance];
      if(!result.accelerationStructureSize || !result.buildScratchBufferSize ||
         !layout.size || !layout.align || instance.instanceDescriptorBuffer != input ||
         instance.instancedAccelerationStructures != children ||
         instance.instanceCount != 2 || instance.instanceDescriptorBufferOffset != 0)
      {
        fprintf(stderr, "Instance query invalid/mutated storage=%lu\n", (unsigned long)storage);
        return 4;
      }
      printf("instance storage=%lu size=%lu build=%lu refit=%lu heap=%lu align=%lu\n",
             (unsigned long)storage, (unsigned long)result.accelerationStructureSize,
             (unsigned long)result.buildScratchBufferSize, (unsigned long)result.refitScratchBufferSize,
             (unsigned long)layout.size, (unsigned long)layout.align);
    }
    return 0;
  }
}

int main()
{
  setbuf(stdout, nullptr);
  @autoreleasepool
  {
    // Drain descriptor/resource autoreleases while the caller still owns the device.
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    return device ? RunQueries(device) : 1;
  }
}
