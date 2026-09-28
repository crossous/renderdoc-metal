#include "metal_acceleration_structure_command_encoder.h"
#include "metal_acceleration_structure.h"
#include "metal_types_bridge.h"

@implementation ObjCBridgeMTLAccelerationStructureCommandEncoder
- (id<MTLAccelerationStructureCommandEncoder>)real
{
  return id<MTLAccelerationStructureCommandEncoder>(Unwrap(GetWrapped(self)));
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-missing-super-calls"
- (void)dealloc { DeallocateObjCBridge(GetWrapped(self)); }
#pragma clang diagnostic pop
- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector
{
  return [(id)self.real methodSignatureForSelector:selector];
}
- (void)forwardInvocation:(NSInvocation *)invocation
{
  RDCFATAL("Metal acceleration-structure encoder selector %s not captured",
           sel_getName([invocation selector]));
}
- (id<MTLDevice>)device { return id<MTLDevice>(GetWrapped(self)->GetDevice()); }
- (NSString *)label { return self.real.label; }
- (void)setLabel:(NSString *)label { self.real.label = label; }
- (void)endEncoding { GetWrapped(self)->endEncoding(); }

- (void)copyAccelerationStructure:(id<MTLAccelerationStructure>)source
        toAccelerationStructure:(id<MTLAccelerationStructure>)destination
{
  if(![(id)source isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
     ![(id)destination isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
    METAL_NOT_HOOKED();
  GetWrapped(self)->copyAccelerationStructure(
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)source),
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination));
}

- (void)copyAndCompactAccelerationStructure:(id<MTLAccelerationStructure>)source
              toAccelerationStructure:(id<MTLAccelerationStructure>)destination
{
  if(![(id)source isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
     ![(id)destination isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
    METAL_NOT_HOOKED();
  GetWrapped(self)->copyAndCompactAccelerationStructure(
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)source),
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination));
}

- (void)buildAccelerationStructure:(id<MTLAccelerationStructure>)structure
                        descriptor:(MTLAccelerationStructureDescriptor *)descriptor
                     scratchBuffer:(id<MTLBuffer>)scratch
               scratchBufferOffset:(NSUInteger)scratchOffset
{
  if([(id)descriptor isKindOfClass:[MTLInstanceAccelerationStructureDescriptor class]])
  {
    MTLInstanceAccelerationStructureDescriptor *instance =
        (MTLInstanceAccelerationStructureDescriptor *)descriptor;
    MTLInstanceAccelerationStructureDescriptor *defaults =
        [MTLInstanceAccelerationStructureDescriptor descriptor];
    BOOL allChildrenWrapped = YES;
    for(id child in instance.instancedAccelerationStructures)
      if(![(id)child isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
        allChildrenWrapped = NO;
    if(![(id)structure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
       ![(id)scratch isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
       ![(id)instance.instanceDescriptorBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
       instance.usage != MTLAccelerationStructureUsageNone ||
       instance.instanceCount < 1 || instance.instanceCount > 65536 ||
       instance.instanceDescriptorBufferOffset != 0 ||
       instance.instanceDescriptorStride != defaults.instanceDescriptorStride ||
       instance.instanceDescriptorType != MTLAccelerationStructureInstanceDescriptorTypeDefault ||
       instance.motionTransformBuffer || instance.motionTransformCount != 0 ||
       instance.instancedAccelerationStructures.count < 1 ||
       instance.instancedAccelerationStructures.count > 4 ||
       instance.instanceCount < instance.instancedAccelerationStructures.count ||
       scratchOffset != 0 ||
       !allChildrenWrapped ||
       ![(id)instance.instancedAccelerationStructures[0]
           isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
      METAL_NOT_HOOKED();
    WrappedMTLAccelerationStructure *wrappedStructure =
        GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure);
    WrappedMTLAccelerationStructure *wrappedChild = GetWrapped(
        (ObjCBridgeMTLAccelerationStructure *)instance.instancedAccelerationStructures[0]);
    WrappedMTLBuffer *wrappedInstances =
        GetWrapped((ObjCBridgeMTLBuffer *)instance.instanceDescriptorBuffer);
    WrappedMTLBuffer *wrappedScratch = GetWrapped((ObjCBridgeMTLBuffer *)scratch);
    if(instance.instanceCount == 1)
      GetWrapped(self)->buildInstance(wrappedStructure, wrappedChild, wrappedInstances,
                                      wrappedScratch);
    else if(instance.instancedAccelerationStructures.count == 1)
      GetWrapped(self)->buildInstances(wrappedStructure, wrappedChild, wrappedInstances,
                                       wrappedScratch, instance.instanceCount);
    else if(instance.instancedAccelerationStructures.count == 2 &&
            instance.instanceCount == 2)
      GetWrapped(self)->buildDistinctInstances(
          wrappedStructure, wrappedChild,
          GetWrapped((ObjCBridgeMTLAccelerationStructure *)instance.instancedAccelerationStructures[1]),
          wrappedInstances, wrappedScratch);
    else
    {
      rdcarray<WrappedMTLAccelerationStructure *> children;
      for(id child in instance.instancedAccelerationStructures)
        children.push_back(GetWrapped((ObjCBridgeMTLAccelerationStructure *)child));
      if(instance.instanceCount == children.size())
        GetWrapped(self)->buildMultipleDistinctInstances(wrappedStructure, children,
                                                         wrappedInstances, wrappedScratch);
      else
        GetWrapped(self)->buildRepeatedDistinctInstances(wrappedStructure, children,
                                                         wrappedInstances, wrappedScratch,
                                                         instance.instanceCount);
    }
    return;
  }
  if(![(id)structure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
     ![(id)scratch isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
     ![(id)descriptor isKindOfClass:[MTLPrimitiveAccelerationStructureDescriptor class]])
    METAL_NOT_HOOKED();
  MTLPrimitiveAccelerationStructureDescriptor *primitive =
      (MTLPrimitiveAccelerationStructureDescriptor *)descriptor;
  if((primitive.usage != MTLAccelerationStructureUsageNone &&
      primitive.usage != MTLAccelerationStructureUsageRefit) ||
     primitive.motionKeyframeCount > 1 || primitive.geometryDescriptors.count != 1)
    METAL_NOT_HOOKED();
  MTLAccelerationStructureGeometryDescriptor *geometry = primitive.geometryDescriptors[0];
  if([(id)geometry isKindOfClass:[MTLAccelerationStructureBoundingBoxGeometryDescriptor class]])
  {
    MTLAccelerationStructureBoundingBoxGeometryDescriptor *box =
        (MTLAccelerationStructureBoundingBoxGeometryDescriptor *)geometry;
    MTLAccelerationStructureBoundingBoxGeometryDescriptor *defaults =
        [MTLAccelerationStructureBoundingBoxGeometryDescriptor descriptor];
    if(![(id)box.boundingBoxBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
       box.boundingBoxCount == 0 ||
       box.intersectionFunctionTableOffset > 31)
      METAL_NOT_HOOKED();
    if(@available(macOS 13.0, iOS 16.0, *))
    {
      if(box.primitiveDataBuffer) METAL_NOT_HOOKED();
    }
    WrappedMTLAccelerationStructure *wrappedStructure =
        GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure);
    WrappedMTLBuffer *wrappedBoxes =
        GetWrapped((ObjCBridgeMTLBuffer *)box.boundingBoxBuffer);
    WrappedMTLBuffer *wrappedScratch = GetWrapped((ObjCBridgeMTLBuffer *)scratch);
    if(primitive.usage == MTLAccelerationStructureUsageRefit)
      GetWrapped(self)->buildRefittableBoundingBox(wrappedStructure, wrappedBoxes,
          box.boundingBoxBufferOffset, box.boundingBoxStride, box.boundingBoxCount,
          box.intersectionFunctionTableOffset, wrappedScratch, scratchOffset,
          box.opaque, box.allowDuplicateIntersectionFunctionInvocation);
    else if(!box.allowDuplicateIntersectionFunctionInvocation)
      GetWrapped(self)->buildBoundingBoxNoDuplicate(wrappedStructure, wrappedBoxes,
          box.boundingBoxBufferOffset, box.boundingBoxStride, box.boundingBoxCount,
          box.intersectionFunctionTableOffset, wrappedScratch, scratchOffset, box.opaque);
    else if(box.opaque)
      GetWrapped(self)->buildBoundingBoxOpaque(wrappedStructure, wrappedBoxes,
          box.boundingBoxBufferOffset, box.boundingBoxStride, box.boundingBoxCount,
          box.intersectionFunctionTableOffset, wrappedScratch, scratchOffset);
    else if(box.intersectionFunctionTableOffset)
      GetWrapped(self)->buildBoundingBoxTableOffset(wrappedStructure, wrappedBoxes,
          box.boundingBoxBufferOffset, box.boundingBoxStride, box.boundingBoxCount,
          box.intersectionFunctionTableOffset, wrappedScratch, scratchOffset);
    else if(box.boundingBoxStride != defaults.boundingBoxStride)
      GetWrapped(self)->buildBoundingBoxStrided(wrappedStructure, wrappedBoxes,
          box.boundingBoxBufferOffset, box.boundingBoxStride, box.boundingBoxCount,
          wrappedScratch, scratchOffset);
    else if(!box.boundingBoxBufferOffset && !scratchOffset)
      GetWrapped(self)->buildBoundingBox(wrappedStructure, wrappedBoxes, box.boundingBoxCount,
                                         wrappedScratch);
    else
      GetWrapped(self)->buildBoundingBoxExtended(wrappedStructure, wrappedBoxes,
          box.boundingBoxBufferOffset, box.boundingBoxCount, wrappedScratch, scratchOffset);
    return;
  }
  if(![(id)geometry isKindOfClass:[MTLAccelerationStructureTriangleGeometryDescriptor class]])
    METAL_NOT_HOOKED();
  MTLAccelerationStructureTriangleGeometryDescriptor *triangle =
      (MTLAccelerationStructureTriangleGeometryDescriptor *)geometry;
  MTLAccelerationStructureTriangleGeometryDescriptor *defaults =
      [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
  MTLAttributeFormat vertexFormat = MTLAttributeFormatFloat3;
  if(@available(macOS 13.0, iOS 16.0, *))
    vertexFormat = triangle.vertexFormat;
  else
    METAL_NOT_HOOKED();
  if(![(id)triangle.vertexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
     triangle.triangleCount == 0 ||
     triangle.intersectionFunctionTableOffset > 31 ||
     (triangle.intersectionFunctionTableOffset &&
      primitive.usage != MTLAccelerationStructureUsageNone &&
      !(primitive.usage == MTLAccelerationStructureUsageRefit && triangle.indexBuffer)) ||
     (triangle.indexBuffer &&
      (![(id)triangle.indexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
       (triangle.indexType != MTLIndexTypeUInt16 && triangle.indexType != MTLIndexTypeUInt32))) ||
     (primitive.usage == MTLAccelerationStructureUsageRefit &&
      !triangle.allowDuplicateIntersectionFunctionInvocation &&
      ((!triangle.indexBuffer && triangle.vertexBufferOffset) || scratchOffset ||
       (!triangle.indexBuffer &&
        (triangle.intersectionFunctionTableOffset || triangle.opaque != defaults.opaque)))))
    METAL_NOT_HOOKED();
  if(@available(macOS 13.0, iOS 16.0, *))
  {
    if(triangle.transformationMatrixBuffer || triangle.primitiveDataBuffer)
      METAL_NOT_HOOKED();
  }
  WrappedMTLAccelerationStructure *wrappedStructure =
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure);
  WrappedMTLBuffer *wrappedVertices =
      GetWrapped((ObjCBridgeMTLBuffer *)triangle.vertexBuffer);
  WrappedMTLBuffer *wrappedScratch = GetWrapped((ObjCBridgeMTLBuffer *)scratch);
  if(primitive.usage == MTLAccelerationStructureUsageRefit && triangle.indexBuffer)
  {
    if(scratchOffset)
      METAL_NOT_HOOKED();
    GetWrapped(self)->buildRefittableIndexedTriangle(wrappedStructure, wrappedVertices,
        triangle.vertexBufferOffset, triangle.vertexStride,
        (MTL::AttributeFormat)vertexFormat,
        GetWrapped((ObjCBridgeMTLBuffer *)triangle.indexBuffer),
        (MTL::IndexType)triangle.indexType, triangle.indexBufferOffset,
        triangle.triangleCount, triangle.intersectionFunctionTableOffset,
        wrappedScratch, scratchOffset, triangle.opaque,
        triangle.allowDuplicateIntersectionFunctionInvocation);
    return;
  }
  if(triangle.vertexStride != 3 * sizeof(float) ||
     vertexFormat != MTLAttributeFormatFloat3)
  {
    if(primitive.usage == MTLAccelerationStructureUsageRefit)
    {
      if(triangle.vertexBufferOffset || triangle.indexBuffer || scratchOffset ||
         triangle.intersectionFunctionTableOffset || triangle.opaque != defaults.opaque)
        METAL_NOT_HOOKED();
      GetWrapped(self)->buildRefittableFormattedTriangle(wrappedStructure,
          wrappedVertices, triangle.vertexStride, (MTL::AttributeFormat)vertexFormat,
          triangle.triangleCount, wrappedScratch,
          triangle.allowDuplicateIntersectionFunctionInvocation);
    }
    else if(triangle.indexBuffer)
      GetWrapped(self)->buildIndexedFormattedTriangle(wrappedStructure, wrappedVertices,
          triangle.vertexBufferOffset, triangle.vertexStride,
          (MTL::AttributeFormat)vertexFormat,
          GetWrapped((ObjCBridgeMTLBuffer *)triangle.indexBuffer),
          (MTL::IndexType)triangle.indexType, triangle.indexBufferOffset,
          triangle.triangleCount, wrappedScratch, scratchOffset,
          triangle.intersectionFunctionTableOffset, triangle.opaque,
          triangle.allowDuplicateIntersectionFunctionInvocation);
    else
      GetWrapped(self)->buildFormattedTriangle(wrappedStructure, wrappedVertices,
          triangle.vertexBufferOffset, triangle.vertexStride,
          (MTL::AttributeFormat)vertexFormat, triangle.triangleCount,
          wrappedScratch, scratchOffset, triangle.intersectionFunctionTableOffset,
          triangle.opaque, triangle.allowDuplicateIntersectionFunctionInvocation);
    return;
  }
  if(!triangle.allowDuplicateIntersectionFunctionInvocation)
  {
    if(primitive.usage == MTLAccelerationStructureUsageRefit)
    {
      GetWrapped(self)->buildRefittableTriangleNoDuplicate(
          wrappedStructure, wrappedVertices, triangle.triangleCount, wrappedScratch);
    }
    else
      GetWrapped(self)->buildTriangleNoDuplicate(
          wrappedStructure, wrappedVertices, triangle.vertexBufferOffset,
          triangle.indexBuffer ? GetWrapped((ObjCBridgeMTLBuffer *)triangle.indexBuffer) : NULL,
          triangle.indexBuffer ? (MTL::IndexType)triangle.indexType : MTL::IndexTypeUInt16,
          triangle.indexBufferOffset, triangle.triangleCount, wrappedScratch, scratchOffset,
          triangle.intersectionFunctionTableOffset, triangle.opaque);
    return;
  }
  if(triangle.intersectionFunctionTableOffset)
  {
    GetWrapped(self)->buildTriangleTableOffset(
        wrappedStructure, wrappedVertices, triangle.vertexBufferOffset,
        triangle.indexBuffer ? GetWrapped((ObjCBridgeMTLBuffer *)triangle.indexBuffer) : NULL,
        triangle.indexBuffer ? (MTL::IndexType)triangle.indexType : MTL::IndexTypeUInt16,
        triangle.indexBufferOffset, triangle.triangleCount, wrappedScratch, scratchOffset,
        triangle.intersectionFunctionTableOffset, triangle.opaque);
    return;
  }
  if(triangle.indexBuffer)
  {
    if(primitive.usage != MTLAccelerationStructureUsageNone ||
       triangle.vertexBufferOffset % (3 * sizeof(float)) != 0)
      METAL_NOT_HOOKED();
    if(![(id)triangle.indexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
       triangle.indexBufferOffset % (triangle.indexType == MTLIndexTypeUInt16 ? 2 : 4) != 0 ||
       (triangle.indexType != MTLIndexTypeUInt16 && triangle.indexType != MTLIndexTypeUInt32))
      METAL_NOT_HOOKED();
    WrappedMTLBuffer *wrappedIndices =
        GetWrapped((ObjCBridgeMTLBuffer *)triangle.indexBuffer);
    if(triangle.vertexBufferOffset || scratchOffset)
      GetWrapped(self)->buildIndexedTriangleExtended(
          wrappedStructure, wrappedVertices, triangle.vertexBufferOffset, wrappedIndices,
          (MTL::IndexType)triangle.indexType, triangle.indexBufferOffset,
          triangle.triangleCount, wrappedScratch, scratchOffset, triangle.opaque);
    else if(triangle.indexBufferOffset)
      GetWrapped(self)->buildIndexedTriangleOffset(
          wrappedStructure, wrappedVertices, wrappedIndices,
          (MTL::IndexType)triangle.indexType, triangle.indexBufferOffset,
          triangle.triangleCount, wrappedScratch, triangle.opaque);
    else
      GetWrapped(self)->buildIndexedTriangle(
          wrappedStructure, wrappedVertices, wrappedIndices,
          (MTL::IndexType)triangle.indexType, triangle.triangleCount, wrappedScratch,
          triangle.opaque);
  }
  else
  {
    const bool refittable = primitive.usage == MTLAccelerationStructureUsageRefit;
    if(refittable && (triangle.opaque != defaults.opaque || triangle.vertexBufferOffset != 0 ||
                     scratchOffset != 0))
      METAL_NOT_HOOKED();
    GetWrapped(self)->buildTriangle(wrappedStructure, wrappedVertices,
                                    triangle.vertexBufferOffset, triangle.triangleCount,
                                    wrappedScratch, scratchOffset,
                                    refittable, !refittable && !triangle.opaque,
                                    !refittable && triangle.opaque);
  }
}

- (void)refitAccelerationStructure:(id<MTLAccelerationStructure>)source
                        descriptor:(MTLAccelerationStructureDescriptor *)descriptor
                       destination:(id<MTLAccelerationStructure>)destination
                     scratchBuffer:(id<MTLBuffer>)scratch
               scratchBufferOffset:(NSUInteger)scratchOffset
{
  if(![(id)source isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
     ![(id)destination isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
     ![(id)scratch isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
     ![(id)descriptor isKindOfClass:[MTLPrimitiveAccelerationStructureDescriptor class]])
    METAL_NOT_HOOKED();
  MTLPrimitiveAccelerationStructureDescriptor *primitive =
      (MTLPrimitiveAccelerationStructureDescriptor *)descriptor;
  if(primitive.usage != MTLAccelerationStructureUsageRefit ||
     primitive.motionKeyframeCount > 1 || primitive.geometryDescriptors.count != 1 ||
     (![(id)primitive.geometryDescriptors[0]
          isKindOfClass:[MTLAccelerationStructureTriangleGeometryDescriptor class]] &&
      ![(id)primitive.geometryDescriptors[0]
          isKindOfClass:[MTLAccelerationStructureBoundingBoxGeometryDescriptor class]]))
    METAL_NOT_HOOKED();
  if([(id)primitive.geometryDescriptors[0]
        isKindOfClass:[MTLAccelerationStructureBoundingBoxGeometryDescriptor class]])
  {
    MTLAccelerationStructureBoundingBoxGeometryDescriptor *box =
        (MTLAccelerationStructureBoundingBoxGeometryDescriptor *)primitive.geometryDescriptors[0];
    if(![(id)box.boundingBoxBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
       !box.boundingBoxCount || box.intersectionFunctionTableOffset > 31)
      METAL_NOT_HOOKED();
    if(@available(macOS 13.0, iOS 16.0, *))
    {
      if(box.primitiveDataBuffer) METAL_NOT_HOOKED();
    }
    GetWrapped(self)->refitBoundingBox(
        GetWrapped((ObjCBridgeMTLAccelerationStructure *)source),
        GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination),
        GetWrapped((ObjCBridgeMTLBuffer *)box.boundingBoxBuffer),
        box.boundingBoxBufferOffset, box.boundingBoxStride, box.boundingBoxCount,
        box.intersectionFunctionTableOffset, GetWrapped((ObjCBridgeMTLBuffer *)scratch),
        scratchOffset, box.opaque, box.allowDuplicateIntersectionFunctionInvocation);
    return;
  }
  MTLAccelerationStructureTriangleGeometryDescriptor *triangle =
      (MTLAccelerationStructureTriangleGeometryDescriptor *)primitive.geometryDescriptors[0];
  MTLAccelerationStructureTriangleGeometryDescriptor *defaults =
      [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
  MTLAttributeFormat vertexFormat = MTLAttributeFormatFloat3;
  if(@available(macOS 13.0, iOS 16.0, *))
    vertexFormat = triangle.vertexFormat;
  else
    METAL_NOT_HOOKED();
  if(![(id)triangle.vertexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
     !triangle.triangleCount ||
     (triangle.vertexBufferOffset && !triangle.indexBuffer) ||
     (triangle.intersectionFunctionTableOffset != defaults.intersectionFunctionTableOffset &&
      !triangle.indexBuffer) ||
     (triangle.opaque != defaults.opaque && !triangle.indexBuffer))
    METAL_NOT_HOOKED();
  if(@available(macOS 13.0, iOS 16.0, *))
  {
    if(triangle.transformationMatrixBuffer || triangle.primitiveDataBuffer)
      METAL_NOT_HOOKED();
  }
  WrappedMTLAccelerationStructure *wrappedSource =
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)source);
  WrappedMTLBuffer *wrappedVertices =
      GetWrapped((ObjCBridgeMTLBuffer *)triangle.vertexBuffer);
  WrappedMTLBuffer *wrappedScratch = GetWrapped((ObjCBridgeMTLBuffer *)scratch);
  if(triangle.indexBuffer)
  {
    if(![(id)triangle.indexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
       (triangle.indexType != MTLIndexTypeUInt16 && triangle.indexType != MTLIndexTypeUInt32))
      METAL_NOT_HOOKED();
    GetWrapped(self)->refitIndexedTriangle(wrappedSource,
        GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination), wrappedVertices,
        triangle.vertexBufferOffset, triangle.vertexStride,
        (MTL::AttributeFormat)vertexFormat,
        GetWrapped((ObjCBridgeMTLBuffer *)triangle.indexBuffer),
        (MTL::IndexType)triangle.indexType, triangle.indexBufferOffset,
        triangle.triangleCount, triangle.intersectionFunctionTableOffset,
        wrappedScratch, scratchOffset, triangle.opaque,
        triangle.allowDuplicateIntersectionFunctionInvocation);
    return;
  }
  if(triangle.vertexStride != 3 * sizeof(float) ||
     vertexFormat != MTLAttributeFormatFloat3)
  {
    GetWrapped(self)->refitFormattedTriangle(wrappedSource,
        GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination),
        wrappedVertices, triangle.vertexStride, (MTL::AttributeFormat)vertexFormat,
        triangle.triangleCount, wrappedScratch, scratchOffset,
        triangle.allowDuplicateIntersectionFunctionInvocation);
    return;
  }
  if(!triangle.allowDuplicateIntersectionFunctionInvocation)
    GetWrapped(self)->refitTriangleNoDuplicate(
        wrappedSource, GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination),
        wrappedVertices, triangle.triangleCount, wrappedScratch, scratchOffset);
  else if(source == destination && scratchOffset == 0)
    GetWrapped(self)->refitTriangle(wrappedSource, wrappedVertices, triangle.triangleCount,
                                    wrappedScratch);
  else
    GetWrapped(self)->refitTriangleExtended(
        wrappedSource, GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination),
        wrappedVertices, triangle.triangleCount, wrappedScratch, scratchOffset);
}

- (void)writeCompactedAccelerationStructureSize:(id<MTLAccelerationStructure>)structure
                                        toBuffer:(id<MTLBuffer>)buffer
                                          offset:(NSUInteger)offset
{
  if(![(id)structure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
     ![(id)buffer isKindOfClass:[ObjCBridgeMTLBuffer class]])
    METAL_NOT_HOOKED();
  GetWrapped(self)->writeCompactedSize(
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure),
      GetWrapped((ObjCBridgeMTLBuffer *)buffer), offset, MTL::DataTypeUInt);
}

- (void)writeCompactedAccelerationStructureSize:(id<MTLAccelerationStructure>)structure
                                        toBuffer:(id<MTLBuffer>)buffer
                                          offset:(NSUInteger)offset
                                    sizeDataType:(MTLDataType)type
{
  if(![(id)structure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
     ![(id)buffer isKindOfClass:[ObjCBridgeMTLBuffer class]])
    METAL_NOT_HOOKED();
  GetWrapped(self)->writeCompactedSize(
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure),
      GetWrapped((ObjCBridgeMTLBuffer *)buffer), offset, (MTL::DataType)type);
}
@end
