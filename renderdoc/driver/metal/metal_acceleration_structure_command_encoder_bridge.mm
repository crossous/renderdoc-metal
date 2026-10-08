#include "metal_acceleration_structure_command_encoder.h"
#include "metal_acceleration_structure.h"
#include "metal_types_bridge.h"
#include "metal_command_buffer.h"
#include "metal_buffer.h"
#include "metal_device.h"
#include "metal_manager.h"

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
- (void)insertDebugSignpost:(NSString *)string
{
  GetWrapped(self)->insertDebugSignpost((NS::String *)string);
}
- (void)pushDebugGroup:(NSString *)string
{
  GetWrapped(self)->pushDebugGroup((NS::String *)string);
}
- (void)popDebugGroup { GetWrapped(self)->popDebugGroup(); }
- (void)updateFence:(id<MTLFence>)fence
{
  if(![(id)fence isKindOfClass:[ObjCBridgeMTLFence class]])
  {
    RDCERR("Invalid or unwrapped Metal acceleration structure fence");
    return;
  }
  GetWrapped(self)->updateFence(GetWrapped(fence));
}
- (void)waitForFence:(id<MTLFence>)fence
{
  if(![(id)fence isKindOfClass:[ObjCBridgeMTLFence class]])
  {
    RDCERR("Invalid or unwrapped Metal acceleration structure fence");
    return;
  }
  GetWrapped(self)->waitForFence(GetWrapped(fence));
}

- (void)copyAccelerationStructure:(id<MTLAccelerationStructure>)source
        toAccelerationStructure:(id<MTLAccelerationStructure>)destination
{
  if(![(id)source isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
     ![(id)destination isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
    METAL_NOT_HOOKED();
  GetWrapped(self)->TrackInitialCopy(GetWrapped((ObjCBridgeMTLAccelerationStructure *)source),
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination));
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
  GetWrapped(self)->TrackInitialCopy(GetWrapped((ObjCBridgeMTLAccelerationStructure *)source),
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination), true);
  GetWrapped(self)->copyAndCompactAccelerationStructure(
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)source),
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination));
}

- (void)buildAccelerationStructure:(id<MTLAccelerationStructure>)structure
                        descriptor:(MTLAccelerationStructureDescriptor *)descriptor
                     scratchBuffer:(id<MTLBuffer>)scratch
               scratchBufferOffset:(NSUInteger)scratchOffset
{
  if([(id)scratch isKindOfClass:[ObjCBridgeMTLBuffer class]])
    GetWrapped(self)->TrackInitialBufferWrite(GetWrapped((ObjCBridgeMTLBuffer *)scratch));
  if([(id)structure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
  {
    WrappedMTLBuffer *input = NULL, *indexInput = NULL;
    uint32_t initialKind = 1;
    rdcarray<uint64_t> parameters;
    if([(id)descriptor isKindOfClass:[MTLPrimitiveAccelerationStructureDescriptor class]])
    {
      auto primitive = (MTLPrimitiveAccelerationStructureDescriptor *)descriptor;
      if(primitive.geometryDescriptors.count == 1 && primitive.motionKeyframeCount <= 1 &&
         (primitive.usage == MTLAccelerationStructureUsageNone ||
          primitive.usage == MTLAccelerationStructureUsageRefit) &&
         [primitive.geometryDescriptors[0] isKindOfClass:[MTLAccelerationStructureTriangleGeometryDescriptor class]])
      {
        auto triangle = (MTLAccelerationStructureTriangleGeometryDescriptor *)primitive.geometryDescriptors[0];
        bool extraData = false;
        MTLAttributeFormat format = MTLAttributeFormatFloat3;
        if(@available(macOS 13.0, *))
        {
          extraData = triangle.primitiveDataBuffer || triangle.transformationMatrixBuffer;
          format = triangle.vertexFormat;
        }
        if(!extraData &&
           (!triangle.indexBuffer || [(id)triangle.indexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]]) &&
           [(id)triangle.vertexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]])
        {
          input = GetWrapped((ObjCBridgeMTLBuffer *)triangle.vertexBuffer);
          parameters = {triangle.vertexBufferOffset, triangle.vertexStride, uint64_t(format),
            triangle.triangleCount, triangle.intersectionFunctionTableOffset, uint64_t(triangle.opaque),
            uint64_t(triangle.allowDuplicateIntersectionFunctionInvocation), uint64_t(primitive.usage)};
          if(triangle.indexBuffer)
          {
            indexInput = GetWrapped((ObjCBridgeMTLBuffer *)triangle.indexBuffer);
            initialKind = 2;
            parameters.push_back(triangle.indexBufferOffset);
            parameters.push_back(uint64_t(triangle.indexType));
          }
        }
      }
      if(primitive.geometryDescriptors.count == 1 && primitive.motionKeyframeCount <= 1 &&
         (primitive.usage == MTLAccelerationStructureUsageNone ||
          primitive.usage == MTLAccelerationStructureUsageRefit) &&
         [primitive.geometryDescriptors[0] isKindOfClass:[MTLAccelerationStructureBoundingBoxGeometryDescriptor class]])
      {
        auto box = (MTLAccelerationStructureBoundingBoxGeometryDescriptor *)primitive.geometryDescriptors[0];
        bool extraData = false;
        if(@available(macOS 13.0, *)) extraData = box.primitiveDataBuffer != nil;
        if(!extraData && [(id)box.boundingBoxBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]])
        {
          input = GetWrapped((ObjCBridgeMTLBuffer *)box.boundingBoxBuffer);
          initialKind = 3;
          parameters = {box.boundingBoxBufferOffset, box.boundingBoxStride, 0,
            box.boundingBoxCount, box.intersectionFunctionTableOffset, uint64_t(box.opaque),
            uint64_t(box.allowDuplicateIntersectionFunctionInvocation), uint64_t(primitive.usage)};
        }
      }
    }
    if(![(id)descriptor isKindOfClass:[MTLPrimitiveAccelerationStructureDescriptor class]] ||
       ((MTLPrimitiveAccelerationStructureDescriptor *)descriptor).geometryDescriptors.count == 1)
      GetWrapped(self)->TrackInitialBuild(GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure),
          input, parameters, initialKind, {}, indexInput);
    auto frozenGeometryInput = [](WrappedMTLBuffer *buffer) {
      return ValidMetalASPrivateInstanceInput(buffer);
    };
    if(IsActiveCapturing(GetWrapped(self)->m_State) && frozenGeometryInput(input) &&
       (initialKind == 1 || initialKind == 2) &&
       parameters.size() == (initialKind == 2 ? 10U : 8U) && parameters[7] == 0 &&
       [(id)scratch isKindOfClass:[ObjCBridgeMTLBuffer class]] &&
       Unwrap(GetWrapped((ObjCBridgeMTLBuffer *)scratch))->storageMode() == MTL::StorageModePrivate &&
       Unwrap(GetWrapped((ObjCBridgeMTLBuffer *)scratch))->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
       !GetWrapped((ObjCBridgeMTLBuffer *)scratch)->m_CapturedAliasable &&
       (!indexInput || frozenGeometryInput(indexInput)))
    {
      GetWrapped(self)->buildFrozenTriangles(GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure),
          input, indexInput, initialKind, parameters,
          GetWrapped((ObjCBridgeMTLBuffer *)scratch), scratchOffset);
      return;
    }
    if([(id)scratch isKindOfClass:[ObjCBridgeMTLBuffer class]])
      GetRecord(GetWrapped(self)->GetCommandBuffer())->MarkResourceFrameReferenced(
          GetResID(GetWrapped((ObjCBridgeMTLBuffer *)scratch)), eFrameRef_PartialWrite);
  }
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
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
    if(@available(macOS 14.0, iOS 17.0, *))
    if(instance.instanceDescriptorType == MTLAccelerationStructureInstanceDescriptorTypeIndirect)
    {
      if(![(id)structure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
         ![(id)scratch isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
         ![(id)instance.instanceDescriptorBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
         instance.usage != MTLAccelerationStructureUsageNone || instance.motionTransformBuffer ||
         instance.motionTransformCount || instance.instancedAccelerationStructures.count)
        METAL_NOT_HOOKED();
      auto input = GetWrapped((ObjCBridgeMTLBuffer *)instance.instanceDescriptorBuffer);
      auto native = Unwrap(input);
      rdcarray<uint64_t> parameters = {instance.instanceDescriptorBufferOffset,
          instance.instanceDescriptorStride, uint64_t(instance.instanceDescriptorType),
          instance.instanceCount, 0, 0, 0, 0};
      uint64_t bytes = 0;
      const bool privateInitial = native && native->storageMode() == MTL::StorageModePrivate &&
          IsCaptureMode(GetWrapped(self)->m_State);
      const bool emptyIndirect = native && ValidMetalASEmptyIndirectParameters(parameters, native->length());
      if(!native || (privateInitial ? !ValidMetalASPrivateInstanceInput(input) :
          native->storageMode() != MTL::StorageModeShared || native->heap()) ||
         (!privateInitial && !native->contents()) ||
         (!emptyIndirect && !MetalASIndirectInstanceSpan(parameters, native->length(), bytes)))
      {
        RDCERR("Unsupported Metal indirect instance input: native=%d storage=%u heap=%d "
               "hazard=%u aliasable=%d background=%d offset=%llu stride=%llu count=%llu length=%llu",
               native != NULL, native ? unsigned(native->storageMode()) : 0,
               native && native->heap(), native ? unsigned(native->hazardTrackingMode()) : 0,
               native && native->isAliasable(), IsBackgroundCapturing(GetWrapped(self)->m_State),
               (unsigned long long)parameters[0], (unsigned long long)parameters[1],
               (unsigned long long)parameters[3], (unsigned long long)(native ? native->length() : 0));
        METAL_NOT_HOOKED();
      }
      rdcarray<WrappedMTLAccelerationStructure *> children;
      const auto known = GetWrapped(self)->GetResourceManager()->GetAccelerationStructures();
      if(!emptyIndirect && privateInitial)
      {
        // The bytes become CPU-visible only after the encoder-local GPU copy.
        // Preserve queried primitive candidates; materialisation keeps only the
        // identities the actual descriptor packet references.
        for(auto object : known)
        {
          auto child = (WrappedMTLAccelerationStructure *)object;
          if(child->m_CapturedGPUResourceID && !Atomic::CmpExch32(&object->m_CapturedAliasable, 0, 0) &&
             (child->m_LastBuildKind == 1 || child->m_LastBuildKind == 2 || child->m_LastBuildKind == 3 ||
              child->m_LastBuildKind == 4 || child->m_LastBuildKind == 6 || child->m_LastBuildKind == 7 ||
              child->m_LastBuildKind == 8)) children.push_back(child);
        }
        if(children.size() > MetalMaxIndirectASChildren)
        {
          RDCERR("Unsupported Metal indirect primitive candidate count %llu (limit %llu)",
                 (unsigned long long)children.size(), (unsigned long long)MetalMaxIndirectASChildren);
          METAL_NOT_HOOKED();
        }
      }
      if(privateInitial && !emptyIndirect && children.empty())
        RDCLOG("Metal no-child indirect candidate: instances=%llu knownAS=%llu; snapshot must prove masked null IDs",
               (unsigned long long)parameters[3], (unsigned long long)known.size());
      if(!privateInitial && !emptyIndirect) for(uint64_t i = 0; i < parameters[3]; i++)
      {
        MTL::IndirectAccelerationStructureInstanceDescriptor data = {};
        memcpy(&data, (const byte *)native->contents()+parameters[0]+i*parameters[1], sizeof(data));
        if(!data.accelerationStructureID._impl && !data.mask) continue;
        WrappedMTLAccelerationStructure *child = NULL;
        for(auto object : known)
        {
          auto possible = (WrappedMTLAccelerationStructure *)object;
          if(data.accelerationStructureID._impl && !Atomic::CmpExch32(&object->m_CapturedAliasable, 0, 0) &&
             possible->m_CapturedGPUResourceID == data.accelerationStructureID._impl)
          {
            if(child) METAL_NOT_HOOKED();
            child = possible;
          }
        }
        if(!child) METAL_NOT_HOOKED();
        bool found = false;
        for(auto old : children) found |= old == child;
        if(!found) children.push_back(child);
        if(children.size() > MetalMaxIndirectASChildren) METAL_NOT_HOOKED();
      }
      auto target = GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure);
      if(!emptyIndirect && !children.empty()) GetWrapped(self)->TrackInitialBuild(target, input, parameters, 9, children);
      GetWrapped(self)->buildIndirectInstances(target, children, input,
          GetWrapped((ObjCBridgeMTLBuffer *)scratch), parameters, scratchOffset);
      return;
    }
#endif
    if(instance.instanceDescriptorType == MTLAccelerationStructureInstanceDescriptorTypeUserID)
    {
      if(![(id)structure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
         ![(id)scratch isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
         ![(id)instance.instanceDescriptorBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
         instance.usage != MTLAccelerationStructureUsageNone || instance.motionTransformBuffer ||
         instance.motionTransformCount || scratchOffset || !allChildrenWrapped ||
         instance.instancedAccelerationStructures.count < 1 ||
         instance.instancedAccelerationStructures.count > 4)
        METAL_NOT_HOOKED();
      rdcarray<WrappedMTLAccelerationStructure *> children;
      for(id child in instance.instancedAccelerationStructures)
        children.push_back(GetWrapped((ObjCBridgeMTLAccelerationStructure *)child));
      auto target = GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure);
      auto input = GetWrapped((ObjCBridgeMTLBuffer *)instance.instanceDescriptorBuffer);
      rdcarray<uint64_t> parameters = {instance.instanceDescriptorBufferOffset,
          instance.instanceDescriptorStride, uint64_t(instance.instanceDescriptorType),
          instance.instanceCount, 0, 0, 0, 0};
      GetWrapped(self)->TrackInitialBuild(target, input, parameters, 5, children);
      GetWrapped(self)->buildUserIDInstances(target, children, input,
          GetWrapped((ObjCBridgeMTLBuffer *)scratch), parameters);
      return;
    }
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
    rdcarray<WrappedMTLAccelerationStructure *> initialChildren;
    for(id child in instance.instancedAccelerationStructures)
      initialChildren.push_back(GetWrapped((ObjCBridgeMTLAccelerationStructure *)child));
    GetWrapped(self)->TrackInitialBuild(wrappedStructure, wrappedInstances,
        {0, sizeof(MTL::AccelerationStructureInstanceDescriptor), 0, instance.instanceCount,
         0, 0, 0, 0}, 5, initialChildren);
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
  {
    RDCERR("Unsupported Metal AS build object types: structure=%s scratch=%s descriptor=%s",
           object_getClassName((id)structure), object_getClassName((id)scratch),
           object_getClassName((id)descriptor));
    METAL_NOT_HOOKED();
  }
  MTLPrimitiveAccelerationStructureDescriptor *primitive =
      (MTLPrimitiveAccelerationStructureDescriptor *)descriptor;
  if(primitive.geometryDescriptors.count >= 2 && primitive.geometryDescriptors.count <= 64 &&
     primitive.motionKeyframeCount <= 1 &&
     (primitive.usage == MTLAccelerationStructureUsageNone ||
      primitive.usage == MTLAccelerationStructureUsageRefit))
  {
    id<MTLBuffer> vertices = nil, indices = nil;
    rdcarray<uint64_t> parameters;
    for(id geometry in primitive.geometryDescriptors)
    {
      if(![geometry isKindOfClass:[MTLAccelerationStructureTriangleGeometryDescriptor class]])
        METAL_NOT_HOOKED();
      auto triangle = (MTLAccelerationStructureTriangleGeometryDescriptor *)geometry;
      MTLAttributeFormat format = MTLAttributeFormatFloat3;
      if(@available(macOS 13.0, *))
      {
        if(triangle.primitiveDataBuffer || triangle.transformationMatrixBuffer) METAL_NOT_HOOKED();
        format = triangle.vertexFormat;
      }
      if(![(id)triangle.vertexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
         ![(id)triangle.indexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
         (vertices && vertices != triangle.vertexBuffer) ||
         (indices && indices != triangle.indexBuffer)) METAL_NOT_HOOKED();
      vertices = triangle.vertexBuffer; indices = triangle.indexBuffer;
      rdcarray<uint64_t> entry = {triangle.vertexBufferOffset, triangle.vertexStride, uint64_t(format),
          triangle.triangleCount, triangle.intersectionFunctionTableOffset, uint64_t(triangle.opaque),
          uint64_t(triangle.allowDuplicateIntersectionFunctionInvocation), uint64_t(primitive.usage),
          triangle.indexBufferOffset, uint64_t(triangle.indexType)};
      parameters.append(entry);
    }
    auto wrappedStructure = GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure);
    auto wrappedVertices = GetWrapped((ObjCBridgeMTLBuffer *)vertices);
    auto wrappedIndices = GetWrapped((ObjCBridgeMTLBuffer *)indices);
    GetWrapped(self)->TrackInitialBuild(wrappedStructure, wrappedVertices, parameters, 8, {}, wrappedIndices);
    auto frozenInput = [](WrappedMTLBuffer *buffer) {
      return ValidMetalASPrivateInstanceInput(buffer);
    };
    auto wrappedScratch = GetWrapped((ObjCBridgeMTLBuffer *)scratch);
    if(IsActiveCapturing(GetWrapped(self)->m_State) && parameters[7] == 0 &&
       frozenInput(wrappedVertices) && frozenInput(wrappedIndices) &&
       Unwrap(wrappedScratch)->storageMode() == MTL::StorageModePrivate &&
       Unwrap(wrappedScratch)->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
       !wrappedScratch->m_CapturedAliasable)
      GetWrapped(self)->buildFrozenTriangles(wrappedStructure, wrappedVertices, wrappedIndices,
          8, parameters, wrappedScratch, scratchOffset);
    else GetWrapped(self)->buildMultiIndexed(wrappedStructure, wrappedVertices, wrappedIndices,
        parameters, GetWrapped((ObjCBridgeMTLBuffer *)scratch), scratchOffset);
    return;
  }
  if((primitive.usage != MTLAccelerationStructureUsageNone &&
      primitive.usage != MTLAccelerationStructureUsageRefit) ||
     primitive.motionKeyframeCount > 1 || primitive.geometryDescriptors.count != 1)
  {
    RDCERR("Unsupported Metal primitive AS build: usage=%llu motion=%llu geometries=%llu scratchOffset=%llu",
           (unsigned long long)primitive.usage,
           (unsigned long long)primitive.motionKeyframeCount,
           (unsigned long long)primitive.geometryDescriptors.count,
           (unsigned long long)scratchOffset);
    for(NSUInteger i=0; i<primitive.geometryDescriptors.count && i<4; i++)
    {
      id entry=primitive.geometryDescriptors[i];
      if([entry isKindOfClass:[MTLAccelerationStructureTriangleGeometryDescriptor class]])
      {
        MTLAccelerationStructureTriangleGeometryDescriptor *triangle=entry;
        RDCERR("Metal AS geometry %llu: triangles=%llu stride=%llu vertexOffset=%llu indexOffset=%llu indexType=%llu storage=%llu tableOffset=%llu opaque=%u duplicate=%u",
               (unsigned long long)i, (unsigned long long)triangle.triangleCount,
               (unsigned long long)triangle.vertexStride,
               (unsigned long long)triangle.vertexBufferOffset,
               (unsigned long long)triangle.indexBufferOffset,
               (unsigned long long)triangle.indexType,
               (unsigned long long)triangle.vertexBuffer.storageMode,
               (unsigned long long)triangle.intersectionFunctionTableOffset,
               unsigned(triangle.opaque), unsigned(triangle.allowDuplicateIntersectionFunctionInvocation));
      }
    }
    METAL_NOT_HOOKED();
  }
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
  // Metal permits nil as the in-place destination. The shared serialised path
  // represents that operation with an explicit source/destination ResourceId.
  if(!destination) destination = source;
  if(![(id)source isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
     ![(id)destination isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]] ||
     ![(id)scratch isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
     ![(id)descriptor isKindOfClass:[MTLPrimitiveAccelerationStructureDescriptor class]])
    METAL_NOT_HOOKED();
  GetWrapped(self)->TrackInitialBufferWrite(GetWrapped((ObjCBridgeMTLBuffer *)scratch));
  GetWrapped(self)->TrackInitialBuild(GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination), NULL, {});
  GetRecord(GetWrapped(self)->GetCommandBuffer())->MarkResourceFrameReferenced(
      GetResID(GetWrapped((ObjCBridgeMTLBuffer *)scratch)), eFrameRef_PartialWrite);
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
  rdcarray<uint64_t> parameters = {triangle.vertexBufferOffset, triangle.vertexStride,
      uint64_t(vertexFormat), triangle.triangleCount, triangle.intersectionFunctionTableOffset,
      uint64_t(triangle.opaque), uint64_t(triangle.allowDuplicateIntersectionFunctionInvocation),
      uint64_t(primitive.usage)};
  const bool indexed = triangle.indexBuffer != nil;
  if(indexed)
  {
    parameters.push_back(triangle.indexBufferOffset);
    parameters.push_back(uint64_t(triangle.indexType));
  }
  GetWrapped(self)->TrackInitialBuild(
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)destination),
      GetWrapped((ObjCBridgeMTLBuffer *)triangle.vertexBuffer), parameters, indexed ? 2 : 1, {},
      indexed && [(id)triangle.indexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ?
          GetWrapped((ObjCBridgeMTLBuffer *)triangle.indexBuffer) : NULL, wrappedSource);
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
