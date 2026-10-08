#include "metal_acceleration_structure_command_encoder.h"
#include "metal_acceleration_structure.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_device.h"
#include "metal_replay.h"
#include "metal_fence.h"
#include <cmath>

WrappedMTLAccelerationStructureCommandEncoder::WrappedMTLAccelerationStructureCommandEncoder(
    MTL::AccelerationStructureCommandEncoder *real, ResourceId id, WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

static MTL::PrimitiveAccelerationStructureDescriptor *TriangleDescriptor(
    MTL::Buffer *vertices, NS::UInteger offset, NS::UInteger count,
    MTL::Buffer *indices = NULL, MTL::IndexType indexType = MTL::IndexTypeUInt16,
    bool refittable = false, bool nonOpaque = false, bool opaque = false,
    NS::UInteger indexOffset = 0, NS::UInteger tableOffset = 0,
    bool allowDuplicate = true, NS::UInteger vertexStride = 3 * sizeof(float),
    MTL::AttributeFormat vertexFormat = MTL::AttributeFormatFloat3)
{
  MTL::AccelerationStructureTriangleGeometryDescriptor *triangle =
      MTL::AccelerationStructureTriangleGeometryDescriptor::descriptor();
  triangle->setVertexBuffer(vertices);
  triangle->setVertexBufferOffset(offset);
  triangle->setVertexStride(vertexStride);
  triangle->setVertexFormat(vertexFormat);
  triangle->setTriangleCount(count);
  triangle->setIntersectionFunctionTableOffset(tableOffset);
  triangle->setAllowDuplicateIntersectionFunctionInvocation(allowDuplicate);
  if(nonOpaque) triangle->setOpaque(false);
  if(opaque) triangle->setOpaque(true);
  if(indices)
  {
    triangle->setIndexBuffer(indices);
    triangle->setIndexBufferOffset(indexOffset);
    triangle->setIndexType(indexType);
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      MTL::PrimitiveAccelerationStructureDescriptor::descriptor();
  if(refittable) descriptor->setUsage(MTL::AccelerationStructureUsageRefit);
  descriptor->setGeometryDescriptors(NS::Array::array(triangle));
  return descriptor;
}

static bool ValidFormattedTriangleBuild(WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
    MTL::AttributeFormat vertexFormat, NS::UInteger count, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, NS::UInteger tableOffset, bool opaque,
    bool allowDuplicate, MTL::Device *device, bool refittable = false,
    bool refitOnly = false)
{
  const NS::UInteger vertexBytes = vertexFormat == MTL::AttributeFormatFloat3 ? 12 :
                                    vertexFormat == MTL::AttributeFormatFloat4 ? 16 : 0;
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     !vertices || vertices->m_Type != eResBuffer || !Unwrap(vertices) ||
     !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
     !vertexBytes || vertexStride < vertexBytes || vertexStride > 1024 * 1024 ||
     vertexStride % sizeof(float) || vertexOffset % sizeof(float) ||
     scratchOffset % 256 || !count || count > 1000000 || tableOffset > 31 ||
     vertexOffset > Unwrap(vertices)->length() ||
     scratchOffset > Unwrap(scratch)->length())
    return false;
  const NS::UInteger available = Unwrap(vertices)->length() - vertexOffset;
  if(available < vertexBytes || count * 3 - 1 > (available - vertexBytes) / vertexStride)
    return false;
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), vertexOffset, count, NULL, MTL::IndexTypeUInt16, refittable,
      !opaque, opaque, 0, tableOffset, allowDuplicate, vertexStride, vertexFormat);
  const MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize <= structure->m_Size &&
         (!refitOnly || sizes.refitScratchBufferSize) &&
         (refitOnly ? sizes.refitScratchBufferSize : sizes.buildScratchBufferSize) <=
             Unwrap(scratch)->length() - scratchOffset;
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildFormattedTriangle(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
    MTL::AttributeFormat vertexFormat, NS::UInteger triangleCount,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, NS::UInteger tableOffset,
    bool opaque, bool allowDuplicate)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(vertexOffset).Important();
  SERIALISE_ELEMENT(vertexStride).Important();
  SERIALISE_ELEMENT(vertexFormat).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_ELEMENT(allowDuplicate).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidFormattedTriangleBuild(structure, vertices, vertexOffset, vertexStride,
           vertexFormat, triangleCount, scratch, scratchOffset, tableOffset, opaque,
           allowDuplicate, Unwrap(m_Device)))
    {
      RDCERR("Invalid formatted Metal triangle acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), vertexOffset, triangleCount, NULL, MTL::IndexTypeUInt16,
        false, !opaque, opaque, 0, tableOffset, allowDuplicate,
        vertexStride, vertexFormat);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), scratchOffset);
    structure->m_LastBuildKind = 1;
    structure->m_LastTriangleCount = triangleCount;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastVertices = GetResID(vertices);
    structure->m_LastAllowDuplicateIntersectionFunctionInvocation = allowDuplicate;
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Formatted Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildFormattedTriangle(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
    NS::UInteger triangleCount, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
    NS::UInteger tableOffset, bool opaque, bool allowDuplicate)
{
  if(!ValidFormattedTriangleBuild(structure, vertices, vertexOffset, vertexStride,
      vertexFormat, triangleCount, scratch, scratchOffset, tableOffset, opaque,
      allowDuplicate, Unwrap(m_Device)))
  {
    RDCERR("Invalid formatted Metal triangle acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), vertexOffset, triangleCount, NULL, MTL::IndexTypeUInt16,
      false, !opaque, opaque, 0, tableOffset, allowDuplicate, vertexStride, vertexFormat);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = 1;
  structure->m_LastTriangleCount = triangleCount;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastVertices = GetResID(vertices);
  structure->m_LastAllowDuplicateIntersectionFunctionInvocation = allowDuplicate;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildFormattedTriangle);
    Serialise_buildFormattedTriangle(ser, structure, vertices, vertexOffset, vertexStride,
        vertexFormat, triangleCount, scratch, scratchOffset, tableOffset, opaque, allowDuplicate);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildFormattedTriangle(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, MTL::AttributeFormat, NS::UInteger, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, bool, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildFormattedTriangle(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, MTL::AttributeFormat, NS::UInteger, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, bool, bool);

static bool ValidIndexedFormattedTriangleBuild(WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
    MTL::AttributeFormat vertexFormat, WrappedMTLBuffer *indices, MTL::IndexType indexType,
    NS::UInteger indexOffset, NS::UInteger count, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, NS::UInteger tableOffset, bool opaque,
    bool allowDuplicate, MTL::Device *device, bool refittable = false,
    bool refitOnly = false)
{
  const NS::UInteger vertexBytes = vertexFormat == MTL::AttributeFormatFloat3 ? 12 :
                                    vertexFormat == MTL::AttributeFormatFloat4 ? 16 : 0;
  const NS::UInteger indexBytes = indexType == MTL::IndexTypeUInt16 ? 2 :
                                   indexType == MTL::IndexTypeUInt32 ? 4 : 0;
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     !vertices || vertices->m_Type != eResBuffer || !Unwrap(vertices) ||
     !indices || indices->m_Type != eResBuffer || !Unwrap(indices) ||
     !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
     !vertexBytes || !indexBytes || vertexStride < vertexBytes ||
     vertexStride > 1024 * 1024 || vertexStride % sizeof(float) ||
     vertexOffset % sizeof(float) || indexOffset % indexBytes || scratchOffset % 256 ||
     !count || count > 1000000 || tableOffset > 31 ||
     vertexOffset > Unwrap(vertices)->length() ||
     indexOffset > Unwrap(indices)->length() ||
     scratchOffset > Unwrap(scratch)->length())
    return false;
  const NS::UInteger available = Unwrap(vertices)->length() - vertexOffset;
  if(available < vertexBytes || 2 > (available - vertexBytes) / vertexStride ||
     count > (Unwrap(indices)->length() - indexOffset) / (3 * indexBytes))
    return false;
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), vertexOffset, count, Unwrap(indices), indexType, refittable,
      !opaque, opaque, indexOffset, tableOffset, allowDuplicate, vertexStride, vertexFormat);
  const MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize <= structure->m_Size &&
         (!refitOnly || sizes.refitScratchBufferSize) &&
         (refitOnly ? sizes.refitScratchBufferSize : sizes.buildScratchBufferSize) <=
             Unwrap(scratch)->length() - scratchOffset;
}

static bool ValidMultiIndexedBuild(WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, WrappedMTLBuffer *indices, const rdcarray<uint64_t> &parameters,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, MTL::Device *device);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildFrozenTriangles(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    WrappedMTLBuffer *indices, uint32_t kind, rdcarray<uint64_t> parameters,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, bytebuf vertexBytes, bytebuf indexBytes)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(indices).Important();
  SERIALISE_ELEMENT(kind).Important();
  SERIALISE_ELEMENT(parameters).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(vertexBytes).Important();
  SERIALISE_ELEMENT(indexBytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    const bool multi = ser.ChunkMetadata().chunkID == uint32_t(MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenMultiIndexed);
    if(multi != (kind == 8) || (!multi && kind != 1 && kind != 2) ||
       !Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !m_Device->RecordRayIRGeometryASBuild(GetResID(structure), GetResID(vertices), GetResID(indices),
           kind, parameters, GetResID(scratch), scratchOffset, vertexBytes, indexBytes))
    { RDCERR("Invalid Metal frozen triangle geometry build"); return false; }
    // Metal reads a complete final stride, while the frozen recipe stores only
    // the effective final attribute bytes, exactly like InitialASUpload.
    bytebuf upload = vertexBytes;
    const uint64_t format = parameters[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 12;
    if(kind != 8) upload.resize(upload.size() + parameters[1] - format);
    auto staging = NS::TransferPtr(Unwrap(m_Device)->newBuffer(upload.data(), upload.size(),
        MTL::ResourceStorageModeShared));
    NS::SharedPtr<MTL::Buffer> indexStaging;
    if(kind == 2 || kind == 8) indexStaging = NS::TransferPtr(Unwrap(m_Device)->newBuffer(indexBytes.data(),
        indexBytes.size(), MTL::ResourceStorageModeShared));
    if(!staging || ((kind == 2 || kind == 8) && !indexStaging)) return false;
    auto descriptor = kind == 8 ? MetalASMultiIndexedDescriptor(staging.get(), indexStaging.get(), parameters) : TriangleDescriptor(staging.get(), 0, parameters[3], indexStaging.get(),
        kind == 2 ? MTL::IndexType(parameters[9]) : MTL::IndexTypeUInt16,
        parameters[7] != 0, !parameters[5], parameters[5], 0, parameters[4], parameters[6],
        parameters[1], MTL::AttributeFormat(parameters[2]));
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset);
    // Native callbacks own only independent staging, including unretained submissions.
    Unwrap(Encoder->m_CommandBuffer)->addCompletedHandler([staging, indexStaging](MTL::CommandBuffer *) {});
    m_Device->RecordReplayFrozenASInput(GetResID(vertices));
    if(indices) m_Device->RecordReplayFrozenASInput(GetResID(indices));
    structure->m_LastBuildKind = kind;
    structure->m_LastTriangleCount = kind == 1 ? parameters[3] : 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastVertices = GetResID(vertices);
    structure->m_LastAllowDuplicateIntersectionFunctionInvocation = parameters[6];
    if(IsLoading(m_State))
    {
      AddEvent(); ActionDescription action;
      action.customName = kind == 8 ? "Build Metal Frozen Multiple Indexed Geometries" : kind == 2 ? "Build Metal Frozen Indexed Triangle Geometry" :
                                      "Build Metal Frozen Triangle Geometry";
      action.flags = ActionFlags::BuildAccStruct; AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildFrozenTriangles(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    WrappedMTLBuffer *indices, uint32_t kind, rdcarray<uint64_t> p,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset)
{
  auto validInput = [&](WrappedMTLBuffer *buffer) {
    return buffer && buffer->m_Type == eResBuffer &&
        ValidMetalASPrivateInstanceInput(buffer) && buffer != scratch;
  };
  if(!IsActiveCapturing(m_State) || !m_CommandBuffer || !validInput(vertices) ||
     ((kind == 2 || kind == 8) ? !validInput(indices) : kind != 1 || indices) ||
     (kind == 8 ? p.size() < 20 || p.size() > 640 || p.size() % 10 : p.size() != (kind == 2 ? 10U : 8U)) ||
     p[7] != 0 ||
     (kind == 8 ? !ValidMultiIndexedBuild(structure, vertices, indices, p, scratch, scratchOffset, Unwrap(m_Device)) :
      kind == 2 ? !ValidIndexedFormattedTriangleBuild(structure, vertices, p[0], p[1],
         MTL::AttributeFormat(p[2]), indices, MTL::IndexType(p[9]), p[8], p[3], scratch,
         scratchOffset, p[4], p[5], p[6], Unwrap(m_Device), p[7] != 0) :
         !ValidFormattedTriangleBuild(structure, vertices, p[0], p[1], MTL::AttributeFormat(p[2]),
         p[3], scratch, scratchOffset, p[4], p[5], p[6], Unwrap(m_Device), p[7] != 0)))
  { RDCERR("Invalid or unsupported Metal frame triangle geometry input"); return; }
  auto descriptor = kind == 8 ? MetalASMultiIndexedDescriptor(Unwrap(vertices), Unwrap(indices), p) : TriangleDescriptor(Unwrap(vertices), p[0], p[3], indices ? Unwrap(indices) : NULL,
      kind == 2 ? MTL::IndexType(p[9]) : MTL::IndexTypeUInt16,
      p[7] != 0, !p[5], p[5], kind == 2 ? p[8] : 0, p[4], p[6], p[1], MTL::AttributeFormat(p[2]));
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(Unwrap(structure), descriptor,
      Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = kind;
  structure->m_LastTriangleCount = kind == 1 ? p[3] : 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastVertices = GetResID(vertices);
  structure->m_LastAllowDuplicateIntersectionFunctionInvocation = p[6];
  CACHE_THREAD_SERIALISER();
  SCOPED_SERIALISE_CHUNK(kind == 8 ? MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenMultiIndexed :
      MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenTriangles);
  Serialise_buildFrozenTriangles(ser, structure, vertices, indices, kind, p, scratch, scratchOffset, {}, {});
  auto record = GetRecord(m_CommandBuffer);
  for(auto resource : {GetRecord(structure), GetRecord(vertices), GetRecord(scratch)}) record->AddParent(resource);
  if(indices) record->AddParent(GetRecord(indices));
  MetalASFrameBuild evidence;
  evidence.encoder = GetResID(this); evidence.target = GetResID(structure);
  evidence.source = GetResID(vertices); evidence.scratch = GetResID(scratch); evidence.scratchOffset = scratchOffset;
  evidence.metadata = ser.ChunkMetadata(); evidence.metadataFlags = ser.GetChunkMetadataRecording();
  evidence.build = record->cmdInfo->initialASBuilds.back().build;
  evidence.chunk = scope.Get(); record->AddChunk(evidence.chunk); record->cmdInfo->frameASBuilds.push_back(evidence);
  record->MarkResourceFrameReferenced(GetResID(structure), eFrameRef_CompleteWrite);
  record->MarkResourceFrameReferenced(GetResID(scratch), eFrameRef_PartialWrite);
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildFrozenTriangles(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, WrappedMTLBuffer *,
    uint32_t, rdcarray<uint64_t>, WrappedMTLBuffer *, NS::UInteger, bytebuf, bytebuf);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildFrozenTriangles(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, WrappedMTLBuffer *,
    uint32_t, rdcarray<uint64_t>, WrappedMTLBuffer *, NS::UInteger, bytebuf, bytebuf);

static void SetRefittableIndexedMetadata(WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
    MTL::AttributeFormat vertexFormat, WrappedMTLBuffer *indices,
    MTL::IndexType indexType, NS::UInteger indexOffset, NS::UInteger triangleCount,
    NS::UInteger tableOffset, bool opaque, bool allowDuplicate, ResourceId commandBuffer)
{
  structure->m_LastBuildKind = 7;
  structure->m_LastVertices = GetResID(vertices);
  structure->m_LastIndices = GetResID(indices);
  structure->m_LastIndexedVertexOffset = vertexOffset;
  structure->m_LastVertexStride = vertexStride;
  structure->m_LastVertexFormat = vertexFormat;
  structure->m_LastIndexType = indexType;
  structure->m_LastIndexedIndexOffset = indexOffset;
  structure->m_LastTriangleCount = triangleCount;
  structure->m_LastIndexedTableOffset = tableOffset;
  structure->m_LastIndexedOpaque = opaque;
  structure->m_LastAllowDuplicateIntersectionFunctionInvocation = allowDuplicate;
  structure->m_LastBuildCommandBuffer = commandBuffer;
  structure->m_LastCompactedSizeBuffer = ResourceId();
  structure->m_LastCompactedWriteCommandBuffer = ResourceId();
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableIndexedTriangle(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
    MTL::AttributeFormat vertexFormat, WrappedMTLBuffer *indices, MTL::IndexType indexType,
    NS::UInteger indexOffset, NS::UInteger triangleCount, NS::UInteger tableOffset,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, bool opaque, bool allowDuplicate)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(vertexOffset).Important();
  SERIALISE_ELEMENT(vertexStride).Important();
  SERIALISE_ELEMENT(vertexFormat).Important();
  SERIALISE_ELEMENT(indices).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexOffset).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_ELEMENT(allowDuplicate).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidIndexedFormattedTriangleBuild(structure, vertices, vertexOffset, vertexStride,
           vertexFormat, indices, indexType, indexOffset, triangleCount, scratch,
           scratchOffset, tableOffset, opaque, allowDuplicate, Unwrap(m_Device), true))
    {
      RDCERR("Invalid Metal refittable indexed triangle build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), vertexOffset, triangleCount, Unwrap(indices), indexType,
        true, !opaque, opaque, indexOffset, tableOffset, allowDuplicate,
        vertexStride, vertexFormat);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), scratchOffset);
    SetRefittableIndexedMetadata(structure, vertices, vertexOffset, vertexStride,
        vertexFormat, indices, indexType, indexOffset, triangleCount, tableOffset,
        opaque, allowDuplicate, GetResID(Encoder->m_CommandBuffer));
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Refittable Indexed Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildRefittableIndexedTriangle(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
    WrappedMTLBuffer *indices, MTL::IndexType indexType, NS::UInteger indexOffset,
    NS::UInteger triangleCount, NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, bool opaque, bool allowDuplicate)
{
  if(!ValidIndexedFormattedTriangleBuild(structure, vertices, vertexOffset, vertexStride,
      vertexFormat, indices, indexType, indexOffset, triangleCount, scratch,
      scratchOffset, tableOffset, opaque, allowDuplicate, Unwrap(m_Device), true))
  {
    RDCERR("Invalid Metal refittable indexed triangle build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), vertexOffset, triangleCount, Unwrap(indices), indexType,
      true, !opaque, opaque, indexOffset, tableOffset, allowDuplicate,
      vertexStride, vertexFormat);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  SetRefittableIndexedMetadata(structure, vertices, vertexOffset, vertexStride,
      vertexFormat, indices, indexType, indexOffset, triangleCount, tableOffset,
      opaque, allowDuplicate, GetResID(m_CommandBuffer));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildRefittableIndexedTriangle);
    Serialise_buildRefittableIndexedTriangle(ser, structure, vertices, vertexOffset,
        vertexStride, vertexFormat, indices, indexType, indexOffset, triangleCount,
        tableOffset, scratch, scratchOffset, opaque, allowDuplicate);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(indices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableIndexedTriangle(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, MTL::AttributeFormat, WrappedMTLBuffer *, MTL::IndexType, NS::UInteger,
    NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger, bool, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableIndexedTriangle(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, MTL::AttributeFormat, WrappedMTLBuffer *, MTL::IndexType, NS::UInteger,
    NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger, bool, bool);

static bool ValidIndexedTriangleRefit(WrappedMTLAccelerationStructure *source,
    WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
    WrappedMTLBuffer *indices, MTL::IndexType indexType, NS::UInteger indexOffset,
    NS::UInteger triangleCount, NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, bool opaque, bool allowDuplicate, MTL::Device *device,
    ResourceId commandBuffer)
{
  if(!ValidIndexedFormattedTriangleBuild(source, vertices, vertexOffset, vertexStride,
          vertexFormat, indices, indexType, indexOffset, triangleCount, scratch,
          scratchOffset, tableOffset, opaque, allowDuplicate, device, true, true) ||
     !destination || destination->m_Type != eResAccelerationStructure ||
     !Unwrap(destination) ||
     (destination != source && destination->m_LastBuildKind != 0) ||
     source->m_LastBuildKind != 7 ||
     source->m_LastIndexedVertexOffset != vertexOffset ||
     source->m_LastVertexStride != vertexStride ||
     source->m_LastVertexFormat != vertexFormat ||
     source->m_LastIndexType != indexType ||
     source->m_LastIndexedIndexOffset != indexOffset ||
     source->m_LastTriangleCount != triangleCount ||
     source->m_LastIndexedTableOffset != tableOffset ||
     source->m_LastIndexedOpaque != opaque ||
     source->m_LastAllowDuplicateIntersectionFunctionInvocation != allowDuplicate ||
     source->m_LastBuildCommandBuffer == ResourceId() ||
     source->m_LastBuildCommandBuffer == commandBuffer)
    return false;
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), vertexOffset, triangleCount, Unwrap(indices), indexType,
      true, !opaque, opaque, indexOffset, tableOffset, allowDuplicate,
      vertexStride, vertexFormat);
  return device->accelerationStructureSizes(descriptor).accelerationStructureSize <=
         destination->m_Size;
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitIndexedTriangle(
    SerialiserType &ser, WrappedMTLAccelerationStructure *source,
    WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
    WrappedMTLBuffer *indices, MTL::IndexType indexType, NS::UInteger indexOffset,
    NS::UInteger triangleCount, NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, bool opaque, bool allowDuplicate)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(source).Important();
  SERIALISE_ELEMENT(destination).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(vertexOffset).Important();
  SERIALISE_ELEMENT(vertexStride).Important();
  SERIALISE_ELEMENT(vertexFormat).Important();
  SERIALISE_ELEMENT(indices).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexOffset).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_ELEMENT(allowDuplicate).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidIndexedTriangleRefit(source, destination, vertices, vertexOffset, vertexStride,
           vertexFormat, indices, indexType, indexOffset, triangleCount, tableOffset,
           scratch, scratchOffset, opaque, allowDuplicate, Unwrap(m_Device),
           GetResID(Encoder->m_CommandBuffer)))
    {
      RDCERR("Invalid Metal indexed triangle refit");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), vertexOffset, triangleCount, Unwrap(indices), indexType,
        true, !opaque, opaque, indexOffset, tableOffset, allowDuplicate,
        vertexStride, vertexFormat);
    Unwrap(Encoder)->refitAccelerationStructure(Unwrap(source), descriptor,
        Unwrap(destination), Unwrap(scratch), scratchOffset);
    SetRefittableIndexedMetadata(destination, vertices, vertexOffset, vertexStride,
        vertexFormat, indices, indexType, indexOffset, triangleCount, tableOffset,
        opaque, allowDuplicate, GetResID(Encoder->m_CommandBuffer));
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Refit Metal Indexed Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::refitIndexedTriangle(
    WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination,
    WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
    MTL::AttributeFormat vertexFormat, WrappedMTLBuffer *indices, MTL::IndexType indexType,
    NS::UInteger indexOffset, NS::UInteger triangleCount, NS::UInteger tableOffset,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, bool opaque, bool allowDuplicate)
{
  if(!ValidIndexedTriangleRefit(source, destination, vertices, vertexOffset, vertexStride,
      vertexFormat, indices, indexType, indexOffset, triangleCount, tableOffset,
      scratch, scratchOffset, opaque, allowDuplicate, Unwrap(m_Device), GetResID(m_CommandBuffer)))
  {
    RDCERR("Invalid Metal indexed triangle refit");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), vertexOffset, triangleCount, Unwrap(indices), indexType,
      true, !opaque, opaque, indexOffset, tableOffset, allowDuplicate,
      vertexStride, vertexFormat);
  SERIALISE_TIME_CALL(Unwrap(this)->refitAccelerationStructure(
      Unwrap(source), descriptor, Unwrap(destination), Unwrap(scratch), scratchOffset));
  SetRefittableIndexedMetadata(destination, vertices, vertexOffset, vertexStride,
      vertexFormat, indices, indexType, indexOffset, triangleCount, tableOffset,
      opaque, allowDuplicate, GetResID(m_CommandBuffer));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_refitIndexedTriangle);
    Serialise_refitIndexedTriangle(ser, source, destination, vertices, vertexOffset,
        vertexStride, vertexFormat, indices, indexType, indexOffset, triangleCount,
        tableOffset, scratch, scratchOffset, opaque, allowDuplicate);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(source));
    record->AddParent(GetRecord(destination));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(indices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitIndexedTriangle(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, NS::UInteger, MTL::AttributeFormat, WrappedMTLBuffer *,
    MTL::IndexType, NS::UInteger, NS::UInteger, NS::UInteger, WrappedMTLBuffer *,
    NS::UInteger, bool, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitIndexedTriangle(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, NS::UInteger, MTL::AttributeFormat, WrappedMTLBuffer *,
    MTL::IndexType, NS::UInteger, NS::UInteger, NS::UInteger, WrappedMTLBuffer *,
    NS::UInteger, bool, bool);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedFormattedTriangle(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
    MTL::AttributeFormat vertexFormat, WrappedMTLBuffer *indices,
    MTL::IndexType indexType, NS::UInteger indexOffset, NS::UInteger triangleCount,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, NS::UInteger tableOffset,
    bool opaque, bool allowDuplicate)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(vertexOffset).Important();
  SERIALISE_ELEMENT(vertexStride).Important();
  SERIALISE_ELEMENT(vertexFormat).Important();
  SERIALISE_ELEMENT(indices).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexOffset).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_ELEMENT(allowDuplicate).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidIndexedFormattedTriangleBuild(structure, vertices, vertexOffset, vertexStride,
           vertexFormat, indices, indexType, indexOffset, triangleCount, scratch,
           scratchOffset, tableOffset, opaque, allowDuplicate, Unwrap(m_Device)))
    {
      RDCERR("Invalid indexed formatted Metal triangle acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), vertexOffset, triangleCount, Unwrap(indices), indexType,
        false, !opaque, opaque, indexOffset, tableOffset, allowDuplicate,
        vertexStride, vertexFormat);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), scratchOffset);
    structure->m_LastBuildKind = 2;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastAllowDuplicateIntersectionFunctionInvocation = allowDuplicate;
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Indexed Formatted Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildIndexedFormattedTriangle(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
    WrappedMTLBuffer *indices, MTL::IndexType indexType, NS::UInteger indexOffset,
    NS::UInteger triangleCount, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
    NS::UInteger tableOffset, bool opaque, bool allowDuplicate)
{
  if(!ValidIndexedFormattedTriangleBuild(structure, vertices, vertexOffset, vertexStride,
      vertexFormat, indices, indexType, indexOffset, triangleCount, scratch,
      scratchOffset, tableOffset, opaque, allowDuplicate, Unwrap(m_Device)))
  {
    RDCERR("Invalid indexed formatted Metal triangle acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), vertexOffset, triangleCount, Unwrap(indices), indexType,
      false, !opaque, opaque, indexOffset, tableOffset, allowDuplicate,
      vertexStride, vertexFormat);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = 2;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastAllowDuplicateIntersectionFunctionInvocation = allowDuplicate;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndexedFormattedTriangle);
    Serialise_buildIndexedFormattedTriangle(ser, structure, vertices, vertexOffset,
        vertexStride, vertexFormat, indices, indexType, indexOffset, triangleCount,
        scratch, scratchOffset, tableOffset, opaque, allowDuplicate);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(indices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedFormattedTriangle(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, MTL::AttributeFormat, WrappedMTLBuffer *, MTL::IndexType, NS::UInteger,
    NS::UInteger, WrappedMTLBuffer *, NS::UInteger, NS::UInteger, bool, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedFormattedTriangle(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, MTL::AttributeFormat, WrappedMTLBuffer *, MTL::IndexType, NS::UInteger,
    NS::UInteger, WrappedMTLBuffer *, NS::UInteger, NS::UInteger, bool, bool);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableFormattedTriangle(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, NS::UInteger vertexStride,
    MTL::AttributeFormat vertexFormat, NS::UInteger triangleCount,
    WrappedMTLBuffer *scratch, bool allowDuplicate)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(vertexStride).Important();
  SERIALISE_ELEMENT(vertexFormat).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(allowDuplicate).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidFormattedTriangleBuild(structure, vertices, 0, vertexStride, vertexFormat,
           triangleCount, scratch, 0, 0, false, allowDuplicate, Unwrap(m_Device), true))
    {
      RDCERR("Invalid formatted Metal refittable triangle build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), 0, triangleCount, NULL, MTL::IndexTypeUInt16, true,
        false, false, 0, 0, allowDuplicate, vertexStride, vertexFormat);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), 0);
    structure->m_LastBuildKind = 4;
    structure->m_LastVertexStride = vertexStride;
    structure->m_LastVertexFormat = vertexFormat;
    structure->m_LastAllowDuplicateIntersectionFunctionInvocation = allowDuplicate;
    structure->m_LastTriangleCount = triangleCount;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastVertices = GetResID(vertices);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Refittable Formatted Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildRefittableFormattedTriangle(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
    NS::UInteger triangleCount, WrappedMTLBuffer *scratch, bool allowDuplicate)
{
  if(!ValidFormattedTriangleBuild(structure, vertices, 0, vertexStride, vertexFormat,
      triangleCount, scratch, 0, 0, false, allowDuplicate, Unwrap(m_Device), true))
  {
    RDCERR("Invalid formatted Metal refittable triangle build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), 0, triangleCount, NULL, MTL::IndexTypeUInt16, true,
      false, false, 0, 0, allowDuplicate, vertexStride, vertexFormat);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), 0));
  structure->m_LastBuildKind = 4;
  structure->m_LastVertexStride = vertexStride;
  structure->m_LastVertexFormat = vertexFormat;
  structure->m_LastAllowDuplicateIntersectionFunctionInvocation = allowDuplicate;
  structure->m_LastTriangleCount = triangleCount;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastVertices = GetResID(vertices);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildRefittableFormattedTriangle);
    Serialise_buildRefittableFormattedTriangle(ser, structure, vertices, vertexStride,
        vertexFormat, triangleCount, scratch, allowDuplicate);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableFormattedTriangle(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    MTL::AttributeFormat, NS::UInteger, WrappedMTLBuffer *, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableFormattedTriangle(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    MTL::AttributeFormat, NS::UInteger, WrappedMTLBuffer *, bool);

static bool ValidFormattedTriangleRefit(WrappedMTLAccelerationStructure *source,
    WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *vertices,
    NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat, NS::UInteger count,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, bool allowDuplicate,
    MTL::Device *device, ResourceId commandBuffer)
{
  if(!ValidFormattedTriangleBuild(source, vertices, 0, vertexStride, vertexFormat,
          count, scratch, scratchOffset, 0, false, allowDuplicate, device, true, true) ||
     !destination || destination->m_Type != eResAccelerationStructure ||
     !Unwrap(destination) ||
     (destination != source && destination->m_LastBuildKind != 0) ||
     source->m_LastBuildKind != 4 || source->m_LastTriangleCount != count ||
     source->m_LastVertexStride != vertexStride ||
     source->m_LastVertexFormat != vertexFormat ||
     source->m_LastAllowDuplicateIntersectionFunctionInvocation != allowDuplicate ||
     source->m_LastBuildCommandBuffer == ResourceId() ||
     source->m_LastBuildCommandBuffer == commandBuffer)
    return false;
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), 0, count, NULL, MTL::IndexTypeUInt16, true,
      false, false, 0, 0, allowDuplicate, vertexStride, vertexFormat);
  return device->accelerationStructureSizes(descriptor).accelerationStructureSize <=
         destination->m_Size;
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitFormattedTriangle(
    SerialiserType &ser, WrappedMTLAccelerationStructure *source,
    WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *vertices,
    NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
    NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, bool allowDuplicate)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(source).Important();
  SERIALISE_ELEMENT(destination).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(vertexStride).Important();
  SERIALISE_ELEMENT(vertexFormat).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(allowDuplicate).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidFormattedTriangleRefit(source, destination, vertices, vertexStride,
           vertexFormat, triangleCount, scratch, scratchOffset, allowDuplicate,
           Unwrap(m_Device), GetResID(Encoder->m_CommandBuffer)))
    {
      RDCERR("Invalid formatted Metal triangle refit");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), 0, triangleCount, NULL, MTL::IndexTypeUInt16, true,
        false, false, 0, 0, allowDuplicate, vertexStride, vertexFormat);
    Unwrap(Encoder)->refitAccelerationStructure(Unwrap(source), descriptor,
        Unwrap(destination), Unwrap(scratch), scratchOffset);
    destination->m_LastBuildKind = 4;
    destination->m_LastVertexStride = vertexStride;
    destination->m_LastVertexFormat = vertexFormat;
    destination->m_LastAllowDuplicateIntersectionFunctionInvocation = allowDuplicate;
    destination->m_LastTriangleCount = triangleCount;
    destination->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    destination->m_LastVertices = GetResID(vertices);
    destination->m_LastCompactedSizeBuffer = ResourceId();
    destination->m_LastCompactedWriteCommandBuffer = ResourceId();
  destination->m_CompactedSizeBuildCommandBuffer = ResourceId();
  destination->m_CapturedCompactedSizeReadback.reset();
  destination->m_CapturedCompactedSizeSubmission.reset();
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Refit Metal Formatted Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::refitFormattedTriangle(
    WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination,
    WrappedMTLBuffer *vertices, NS::UInteger vertexStride,
    MTL::AttributeFormat vertexFormat, NS::UInteger triangleCount,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, bool allowDuplicate)
{
  if(!ValidFormattedTriangleRefit(source, destination, vertices, vertexStride,
      vertexFormat, triangleCount, scratch, scratchOffset, allowDuplicate,
      Unwrap(m_Device), GetResID(m_CommandBuffer)))
  {
    RDCERR("Invalid formatted Metal triangle refit");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), 0, triangleCount, NULL, MTL::IndexTypeUInt16, true,
      false, false, 0, 0, allowDuplicate, vertexStride, vertexFormat);
  SERIALISE_TIME_CALL(Unwrap(this)->refitAccelerationStructure(
      Unwrap(source), descriptor, Unwrap(destination), Unwrap(scratch), scratchOffset));
  destination->m_LastBuildKind = 4;
  destination->m_LastVertexStride = vertexStride;
  destination->m_LastVertexFormat = vertexFormat;
  destination->m_LastAllowDuplicateIntersectionFunctionInvocation = allowDuplicate;
  destination->m_LastTriangleCount = triangleCount;
  destination->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  destination->m_LastVertices = GetResID(vertices);
  destination->m_LastCompactedSizeBuffer = ResourceId();
  destination->m_LastCompactedWriteCommandBuffer = ResourceId();
  destination->m_LastInitialCompactedSize = 0;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_refitFormattedTriangle);
    Serialise_refitFormattedTriangle(ser, source, destination, vertices, vertexStride,
        vertexFormat, triangleCount, scratch, scratchOffset, allowDuplicate);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(source));
    record->AddParent(GetRecord(destination));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitFormattedTriangle(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, MTL::AttributeFormat, NS::UInteger,
    WrappedMTLBuffer *, NS::UInteger, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitFormattedTriangle(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, MTL::AttributeFormat, NS::UInteger,
    WrappedMTLBuffer *, NS::UInteger, bool);

static bool ValidTriangleBuild(WrappedMTLAccelerationStructure *structure,
                               WrappedMTLBuffer *vertices, NS::UInteger vertexOffset,
                               NS::UInteger count, WrappedMTLBuffer *scratch,
                               NS::UInteger scratchOffset, MTL::Device *device,
                               bool refittable = false, bool nonOpaque = false,
                               bool opaque = false, bool refitOnly = false,
                               NS::UInteger tableOffset = 0,
                               bool allowDuplicate = true)
{
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     !vertices || vertices->m_Type != eResBuffer || !Unwrap(vertices) ||
     !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
     !count || count > 1000000 || vertexOffset % sizeof(float) != 0 ||
     scratchOffset % 256 != 0 ||
     vertexOffset > Unwrap(vertices)->length() ||
     count > (Unwrap(vertices)->length() - vertexOffset) / (3 * 3 * sizeof(float)) ||
     scratchOffset > Unwrap(scratch)->length())
    return false;
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      TriangleDescriptor(Unwrap(vertices), vertexOffset, count, NULL,
                         MTL::IndexTypeUInt16, refittable, nonOpaque, opaque, 0, tableOffset,
                         allowDuplicate);
  MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize <= structure->m_Size &&
         (refitOnly ? sizes.refitScratchBufferSize : sizes.buildScratchBufferSize) <=
             Unwrap(scratch)->length() - scratchOffset;
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildTriangle(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, bool refittable, bool nonOpaque, bool opaque)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(vertexOffset).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || (opaque && (nonOpaque || refittable)) ||
       !ValidTriangleBuild(structure, vertices, vertexOffset, triangleCount, scratch,
                           scratchOffset, Unwrap(m_Device), refittable, nonOpaque, opaque))
    {
      RDCERR("Invalid Metal triangle acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
        TriangleDescriptor(Unwrap(vertices), vertexOffset, triangleCount, NULL,
                           MTL::IndexTypeUInt16, refittable, nonOpaque, opaque);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch),
                                                scratchOffset);
    structure->m_LastBuildKind = refittable ? 4 : 1;
    structure->m_LastVertexStride = 12;
    structure->m_LastVertexFormat = MTL::AttributeFormatFloat3;
    structure->m_LastAllowDuplicateIntersectionFunctionInvocation = true;
    structure->m_LastTriangleCount = triangleCount;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastVertices = GetResID(vertices);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = refittable ? "Build Metal Refittable Triangle Acceleration Structure" :
                          nonOpaque ? "Build Metal Non-Opaque Triangle Acceleration Structure" :
                          opaque ? "Build Metal Opaque Triangle Acceleration Structure" :
                                      "Build Metal Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildTriangle(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, bool refittable, bool nonOpaque, bool opaque)
{
  if((opaque && (nonOpaque || refittable)) ||
     !ValidTriangleBuild(structure, vertices, vertexOffset, triangleCount, scratch,
                         scratchOffset, Unwrap(m_Device), refittable, nonOpaque, opaque))
  {
    RDCERR("Invalid Metal triangle acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      TriangleDescriptor(Unwrap(vertices), vertexOffset, triangleCount, NULL,
                         MTL::IndexTypeUInt16, refittable, nonOpaque, opaque);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = refittable ? 4 : 1;
  structure->m_LastVertexStride = 12;
  structure->m_LastVertexFormat = MTL::AttributeFormatFloat3;
  structure->m_LastAllowDuplicateIntersectionFunctionInvocation = true;
  structure->m_LastTriangleCount = triangleCount;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastVertices = GetResID(vertices);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(opaque ?
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildOpaqueTriangle : nonOpaque ?
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildNonOpaqueTriangle : refittable ?
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildRefittableTriangle :
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildAccelerationStructure);
    Serialise_buildTriangle(ser, structure, vertices, vertexOffset, triangleCount,
                            scratch, scratchOffset, refittable, nonOpaque, opaque);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildTriangle(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, WrappedMTLBuffer *, NS::UInteger, bool, bool, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildTriangle(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, WrappedMTLBuffer *, NS::UInteger, bool, bool, bool);

static bool ValidTriangleRefit(WrappedMTLAccelerationStructure *structure,
                               WrappedMTLBuffer *vertices, NS::UInteger count,
                               WrappedMTLBuffer *scratch, MTL::Device *device,
                               ResourceId commandBuffer, bool allowDuplicate = true)
{
  if(!ValidTriangleBuild(structure, vertices, 0, count, scratch, 0, device,
                         true, false, false, true, 0, allowDuplicate) ||
     structure->m_LastBuildKind != 4 || structure->m_LastTriangleCount != count ||
     structure->m_LastVertexStride != 12 ||
     structure->m_LastVertexFormat != MTL::AttributeFormatFloat3 ||
     structure->m_LastAllowDuplicateIntersectionFunctionInvocation != allowDuplicate ||
     structure->m_LastBuildCommandBuffer == ResourceId() ||
     structure->m_LastBuildCommandBuffer == commandBuffer)
    return false;
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      TriangleDescriptor(Unwrap(vertices), 0, count, NULL, MTL::IndexTypeUInt16, true,
                         false, false, 0, 0, allowDuplicate);
  MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
  return sizes.refitScratchBufferSize &&
         sizes.refitScratchBufferSize <= Unwrap(scratch)->length();
}

static bool ValidTriangleRefitExtended(WrappedMTLAccelerationStructure *source,
                                       WrappedMTLAccelerationStructure *destination,
                                       WrappedMTLBuffer *vertices, NS::UInteger count,
                                       WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
                                       MTL::Device *device, ResourceId commandBuffer,
                                       bool allowDuplicate = true)
{
  if(!ValidTriangleRefit(source, vertices, count, scratch, device, commandBuffer,
                         allowDuplicate) ||
     !destination || destination->m_Type != eResAccelerationStructure ||
     !Unwrap(destination) ||
     (destination == source && scratchOffset == 0) ||
     (destination != source && destination->m_LastBuildKind != 0) ||
     scratchOffset % 256 != 0 || scratchOffset > Unwrap(scratch)->length())
    return false;
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      TriangleDescriptor(Unwrap(vertices), 0, count, NULL, MTL::IndexTypeUInt16, true,
                         false, false, 0, 0, allowDuplicate);
  MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize <= destination->m_Size &&
         sizes.refitScratchBufferSize <= Unwrap(scratch)->length() - scratchOffset;
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitTriangle(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, NS::UInteger triangleCount, WrappedMTLBuffer *scratch)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || !ValidTriangleRefit(structure, vertices, triangleCount, scratch,
           Unwrap(m_Device), GetResID(Encoder->m_CommandBuffer)))
    {
      RDCERR("Invalid Metal triangle acceleration structure refit");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
        TriangleDescriptor(Unwrap(vertices), 0, triangleCount, NULL,
                           MTL::IndexTypeUInt16, true);
    Unwrap(Encoder)->refitAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(structure), Unwrap(scratch), 0);
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastVertices = GetResID(vertices);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Refit Metal Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::refitTriangle(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger triangleCount, WrappedMTLBuffer *scratch)
{
  if(!ValidTriangleRefit(structure, vertices, triangleCount, scratch,
                         Unwrap(m_Device), GetResID(m_CommandBuffer)))
  {
    RDCERR("Invalid Metal triangle acceleration structure refit");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      TriangleDescriptor(Unwrap(vertices), 0, triangleCount, NULL,
                         MTL::IndexTypeUInt16, true);
  SERIALISE_TIME_CALL(Unwrap(this)->refitAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(structure), Unwrap(scratch), 0));
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastVertices = GetResID(vertices);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_refitTriangle);
    Serialise_refitTriangle(ser, structure, vertices, triangleCount, scratch);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitTriangle(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitTriangle(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitTriangleExtended(
    SerialiserType &ser, WrappedMTLAccelerationStructure *source,
    WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *vertices,
    NS::UInteger triangleCount, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(source).Important();
  SERIALISE_ELEMENT(destination).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidTriangleRefitExtended(source, destination, vertices, triangleCount, scratch,
                                   scratchOffset, Unwrap(m_Device),
                                   GetResID(Encoder->m_CommandBuffer)))
    {
      RDCERR("Invalid extended Metal triangle acceleration structure refit");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
        TriangleDescriptor(Unwrap(vertices), 0, triangleCount, NULL, MTL::IndexTypeUInt16, true);
    Unwrap(Encoder)->refitAccelerationStructure(Unwrap(source), descriptor,
                                                Unwrap(destination), Unwrap(scratch),
                                                scratchOffset);
    destination->m_LastBuildKind = 4;
    destination->m_LastAllowDuplicateIntersectionFunctionInvocation = true;
    destination->m_LastTriangleCount = triangleCount;
    destination->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    destination->m_LastVertices = GetResID(vertices);
    destination->m_LastCompactedSizeBuffer = ResourceId();
    destination->m_LastCompactedWriteCommandBuffer = ResourceId();
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Refit Metal Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::refitTriangleExtended(
    WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination,
    WrappedMTLBuffer *vertices, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset)
{
  if(!ValidTriangleRefitExtended(source, destination, vertices, triangleCount, scratch,
                                 scratchOffset, Unwrap(m_Device), GetResID(m_CommandBuffer)))
  {
    RDCERR("Invalid extended Metal triangle acceleration structure refit");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      TriangleDescriptor(Unwrap(vertices), 0, triangleCount, NULL, MTL::IndexTypeUInt16, true);
  SERIALISE_TIME_CALL(Unwrap(this)->refitAccelerationStructure(
      Unwrap(source), descriptor, Unwrap(destination), Unwrap(scratch), scratchOffset));
  destination->m_LastBuildKind = 4;
  destination->m_LastAllowDuplicateIntersectionFunctionInvocation = true;
  destination->m_LastTriangleCount = triangleCount;
  destination->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  destination->m_LastVertices = GetResID(vertices);
  destination->m_LastCompactedSizeBuffer = ResourceId();
  destination->m_LastCompactedWriteCommandBuffer = ResourceId();
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_refitTriangleExtended);
    Serialise_refitTriangleExtended(ser, source, destination, vertices, triangleCount, scratch,
                                    scratchOffset);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(source));
    record->AddParent(GetRecord(destination));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitTriangleExtended(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, WrappedMTLBuffer *, NS::UInteger);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitTriangleExtended(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, WrappedMTLBuffer *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableTriangleNoDuplicate(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, NS::UInteger triangleCount, WrappedMTLBuffer *scratch)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidTriangleBuild(structure, vertices, 0, triangleCount, scratch, 0,
                           Unwrap(m_Device), true, false, false, false, 0, false))
    {
      RDCERR("Invalid no-duplicate Metal refittable triangle build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), 0, triangleCount, NULL, MTL::IndexTypeUInt16, true,
        false, false, 0, 0, false);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), 0);
    structure->m_LastBuildKind = 4;
    structure->m_LastVertexStride = 12;
    structure->m_LastVertexFormat = MTL::AttributeFormatFloat3;
    structure->m_LastAllowDuplicateIntersectionFunctionInvocation = false;
    structure->m_LastTriangleCount = triangleCount;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastVertices = GetResID(vertices);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Refittable Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildRefittableTriangleNoDuplicate(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger triangleCount, WrappedMTLBuffer *scratch)
{
  if(!ValidTriangleBuild(structure, vertices, 0, triangleCount, scratch, 0,
                         Unwrap(m_Device), true, false, false, false, 0, false))
  {
    RDCERR("Invalid no-duplicate Metal refittable triangle build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), 0, triangleCount, NULL, MTL::IndexTypeUInt16, true,
      false, false, 0, 0, false);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), 0));
  structure->m_LastBuildKind = 4;
  structure->m_LastVertexStride = 12;
  structure->m_LastVertexFormat = MTL::AttributeFormatFloat3;
  structure->m_LastAllowDuplicateIntersectionFunctionInvocation = false;
  structure->m_LastTriangleCount = triangleCount;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastVertices = GetResID(vertices);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildRefittableTriangleNoDuplicate);
    Serialise_buildRefittableTriangleNoDuplicate(ser, structure, vertices, triangleCount, scratch);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableTriangleNoDuplicate(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableTriangleNoDuplicate(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitTriangleNoDuplicate(
    SerialiserType &ser, WrappedMTLAccelerationStructure *source,
    WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *vertices,
    NS::UInteger triangleCount, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(source).Important();
  SERIALISE_ELEMENT(destination).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder))
    {
      RDCERR("Invalid no-duplicate Metal triangle refit encoder");
      return false;
    }
    const ResourceId commandBuffer = GetResID(Encoder->m_CommandBuffer);
    const bool valid = source == destination && scratchOffset == 0 ?
        ValidTriangleRefit(source, vertices, triangleCount, scratch, Unwrap(m_Device),
                           commandBuffer, false) :
        ValidTriangleRefitExtended(source, destination, vertices, triangleCount, scratch,
                                   scratchOffset, Unwrap(m_Device), commandBuffer, false);
    if(!valid)
    {
      RDCERR("Invalid no-duplicate Metal triangle refit");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), 0, triangleCount, NULL, MTL::IndexTypeUInt16, true,
        false, false, 0, 0, false);
    Unwrap(Encoder)->refitAccelerationStructure(Unwrap(source), descriptor,
                                                Unwrap(destination), Unwrap(scratch),
                                                scratchOffset);
    destination->m_LastBuildKind = 4;
    destination->m_LastAllowDuplicateIntersectionFunctionInvocation = false;
    destination->m_LastTriangleCount = triangleCount;
    destination->m_LastBuildCommandBuffer = commandBuffer;
    destination->m_LastVertices = GetResID(vertices);
    destination->m_LastCompactedSizeBuffer = ResourceId();
    destination->m_LastCompactedWriteCommandBuffer = ResourceId();
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Refit Metal Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::refitTriangleNoDuplicate(
    WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination,
    WrappedMTLBuffer *vertices, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset)
{
  const bool valid = source == destination && scratchOffset == 0 ?
      ValidTriangleRefit(source, vertices, triangleCount, scratch, Unwrap(m_Device),
                         GetResID(m_CommandBuffer), false) :
      ValidTriangleRefitExtended(source, destination, vertices, triangleCount, scratch,
                                 scratchOffset, Unwrap(m_Device),
                                 GetResID(m_CommandBuffer), false);
  if(!valid)
  {
    RDCERR("Invalid no-duplicate Metal triangle refit");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), 0, triangleCount, NULL, MTL::IndexTypeUInt16, true,
      false, false, 0, 0, false);
  SERIALISE_TIME_CALL(Unwrap(this)->refitAccelerationStructure(
      Unwrap(source), descriptor, Unwrap(destination), Unwrap(scratch), scratchOffset));
  destination->m_LastBuildKind = 4;
  destination->m_LastAllowDuplicateIntersectionFunctionInvocation = false;
  destination->m_LastTriangleCount = triangleCount;
  destination->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  destination->m_LastVertices = GetResID(vertices);
  destination->m_LastCompactedSizeBuffer = ResourceId();
  destination->m_LastCompactedWriteCommandBuffer = ResourceId();
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_refitTriangleNoDuplicate);
    Serialise_refitTriangleNoDuplicate(ser, source, destination, vertices, triangleCount,
                                      scratch, scratchOffset);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(source));
    record->AddParent(GetRecord(destination));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitTriangleNoDuplicate(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, WrappedMTLBuffer *, NS::UInteger);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitTriangleNoDuplicate(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, WrappedMTLBuffer *, NS::UInteger);

static MTL::InstanceAccelerationStructureDescriptor *InstanceDescriptor(
    MTL::Buffer *instances, MTL::AccelerationStructure *child, NS::UInteger count)
{
  MTL::InstanceAccelerationStructureDescriptor *descriptor =
      MTL::InstanceAccelerationStructureDescriptor::descriptor();
  descriptor->setInstanceDescriptorBuffer(instances);
  descriptor->setInstanceCount(count);
  descriptor->setInstancedAccelerationStructures(NS::Array::array(child));
  return descriptor;
}

static bool ValidInstanceBuild(WrappedMTLAccelerationStructure *structure,
                               WrappedMTLAccelerationStructure *child,
                               WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch,
                               MTL::Device *device, ResourceId commandBuffer,
                               NS::UInteger count = 1)
{
  // m_LastBuildKind/owner are published only after a real earlier build call.
  // A BLAS and its TLAS may be encoded in the same submission; initial-state
  // freezing and frame preflight separately validate the actual dependency.
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     !child || child == structure || child->m_Type != eResAccelerationStructure ||
     !Unwrap(child) || Unwrap(child)->device() != device || child->m_LastBuildKind < 1 ||
     (child->m_LastBuildKind > 4 && child->m_LastBuildKind != 6 &&
      child->m_LastBuildKind != 7 && child->m_LastBuildKind != 8) ||
     child->m_LastBuildCommandBuffer == ResourceId() ||
     !instances || instances->m_Type != eResBuffer || !Unwrap(instances) ||
     (Unwrap(instances)->storageMode() != MTL::StorageModeShared &&
      !(Unwrap(instances)->storageMode() == MTL::StorageModeManaged &&
        IsBackgroundCapturing(instances->m_State))) ||
     count < 1 || count > 65536 ||
     Unwrap(instances)->length() / sizeof(MTL::AccelerationStructureInstanceDescriptor) < count ||
     !Unwrap(instances)->contents() ||
     !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch))
    return false;
  for(NS::UInteger i = 0; i < count; i++)
  {
    MTL::AccelerationStructureInstanceDescriptor data = {};
    memcpy(&data, (const byte *)Unwrap(instances)->contents() + i * sizeof(data), sizeof(data));
    if(!ValidMetalASInstance(data, 1))
      return false;
    for(int column = 0; column < 4; column++)
      for(int row = 0; row < 3; row++)
        if(!std::isfinite(data.transformationMatrix[column][row])) return false;
  }
  MTL::InstanceAccelerationStructureDescriptor *descriptor =
      InstanceDescriptor(Unwrap(instances), Unwrap(child), count);
  MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize &&
         sizes.accelerationStructureSize <= structure->m_Size &&
         sizes.buildScratchBufferSize &&
         sizes.buildScratchBufferSize <= Unwrap(scratch)->length();
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildInstance(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    WrappedMTLAccelerationStructure *child, WrappedMTLBuffer *instances,
    WrappedMTLBuffer *scratch, rdcarray<byte> descriptorBytes)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(child).Important();
  SERIALISE_ELEMENT(instances).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(descriptorBytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || descriptorBytes.size() !=
           sizeof(MTL::AccelerationStructureInstanceDescriptor) ||
       !ValidInstanceBuild(structure, child, instances, scratch, Unwrap(m_Device),
                           GetResID(Encoder->m_CommandBuffer)) ||
       memcmp(descriptorBytes.data(), Unwrap(instances)->contents(), descriptorBytes.size()))
    {
      RDCERR("Invalid Metal single-instance acceleration structure build");
      return false;
    }
    MTL::InstanceAccelerationStructureDescriptor *descriptor =
        InstanceDescriptor(Unwrap(instances), Unwrap(child), 1);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), 0);
    structure->m_LastBuildKind = 5;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastInstanceChild = GetResID(child);
    structure->m_LastInstanceBuffer = GetResID(instances);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Single-Instance Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildInstance(
    WrappedMTLAccelerationStructure *structure, WrappedMTLAccelerationStructure *child,
    WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch)
{
  if(!ValidInstanceBuild(structure, child, instances, scratch, Unwrap(m_Device),
                         GetResID(m_CommandBuffer)))
  {
    RDCERR("Invalid Metal single-instance acceleration structure build");
    return;
  }
  MTL::InstanceAccelerationStructureDescriptor *descriptor =
      InstanceDescriptor(Unwrap(instances), Unwrap(child), 1);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), 0));
  structure->m_LastBuildKind = 5;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastInstanceChild = GetResID(child);
  structure->m_LastInstanceBuffer = GetResID(instances);
  if(IsCaptureMode(m_State))
  {
    rdcarray<byte> descriptorBytes;
    const byte *data = (const byte *)Unwrap(instances)->contents();
    descriptorBytes.assign(data, sizeof(MTL::AccelerationStructureInstanceDescriptor));
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildInstance);
    Serialise_buildInstance(ser, structure, child, instances, scratch, descriptorBytes);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(child));
    record->AddParent(GetRecord(instances));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildInstance(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, WrappedMTLBuffer *, rdcarray<byte>);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildInstance(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, WrappedMTLBuffer *, rdcarray<byte>);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildInstances(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    WrappedMTLAccelerationStructure *child, WrappedMTLBuffer *instances,
    WrappedMTLBuffer *scratch, NS::UInteger count, rdcarray<byte> descriptorBytes)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(child).Important();
  SERIALISE_ELEMENT(instances).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(descriptorBytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || count < 2 || count > 65536 || descriptorBytes.size() !=
           count * sizeof(MTL::AccelerationStructureInstanceDescriptor) ||
       !ValidInstanceBuild(structure, child, instances, scratch, Unwrap(m_Device),
                           GetResID(Encoder->m_CommandBuffer), count) ||
       memcmp(descriptorBytes.data(), Unwrap(instances)->contents(), descriptorBytes.size()))
    {
      RDCERR("Invalid Metal multi-instance acceleration structure build");
      return false;
    }
    MTL::InstanceAccelerationStructureDescriptor *descriptor =
        InstanceDescriptor(Unwrap(instances), Unwrap(child), count);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), 0);
    structure->m_LastBuildKind = 5;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastInstanceChild = GetResID(child);
    structure->m_LastInstanceBuffer = GetResID(instances);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("Build Metal %u-Instance Acceleration Structure",
                                              (unsigned)count);
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildInstances(
    WrappedMTLAccelerationStructure *structure, WrappedMTLAccelerationStructure *child,
    WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch, NS::UInteger count)
{
  if(count < 2 || count > 65536 ||
     !ValidInstanceBuild(structure, child, instances, scratch, Unwrap(m_Device),
                                        GetResID(m_CommandBuffer), count))
  {
    RDCERR("Invalid Metal multi-instance acceleration structure build");
    return;
  }
  MTL::InstanceAccelerationStructureDescriptor *descriptor =
      InstanceDescriptor(Unwrap(instances), Unwrap(child), count);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), 0));
  structure->m_LastBuildKind = 5;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastInstanceChild = GetResID(child);
  structure->m_LastInstanceBuffer = GetResID(instances);
  if(IsCaptureMode(m_State))
  {
    rdcarray<byte> descriptorBytes;
    const byte *data = (const byte *)Unwrap(instances)->contents();
    descriptorBytes.assign(data, count * sizeof(MTL::AccelerationStructureInstanceDescriptor));
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildInstances);
    Serialise_buildInstances(ser, structure, child, instances, scratch, count, descriptorBytes);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(child));
    record->AddParent(GetRecord(instances));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildInstances(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, WrappedMTLBuffer *, NS::UInteger, rdcarray<byte>);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildInstances(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, WrappedMTLBuffer *, NS::UInteger, rdcarray<byte>);

static MTL::InstanceAccelerationStructureDescriptor *DistinctInstanceDescriptor(
    MTL::Buffer *instances, MTL::AccelerationStructure *child0,
    MTL::AccelerationStructure *child1)
{
  MTL::InstanceAccelerationStructureDescriptor *descriptor =
      MTL::InstanceAccelerationStructureDescriptor::descriptor();
  descriptor->setInstanceDescriptorBuffer(instances);
  descriptor->setInstanceCount(2);
  const NS::Object *children[] = {child0, child1};
  descriptor->setInstancedAccelerationStructures(NS::Array::array(children, 2));
  return descriptor;
}

static bool ValidDistinctInstanceBuild(WrappedMTLAccelerationStructure *structure,
                                       WrappedMTLAccelerationStructure *child0,
                                       WrappedMTLAccelerationStructure *child1,
                                       WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch,
                                       MTL::Device *device, ResourceId commandBuffer)
{
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     !child0 || !child1 || child0 == child1 || child0 == structure || child1 == structure ||
     child0->m_Type != eResAccelerationStructure ||
     child1->m_Type != eResAccelerationStructure || !Unwrap(child0) || !Unwrap(child1) ||
     Unwrap(child0)->device() != device || Unwrap(child1)->device() != device ||
     child0->m_LastBuildKind < 1 ||
     (child0->m_LastBuildKind > 4 && child0->m_LastBuildKind != 6 &&
      child0->m_LastBuildKind != 7 && child0->m_LastBuildKind != 8) ||
     child1->m_LastBuildKind < 1 ||
     (child1->m_LastBuildKind > 4 && child1->m_LastBuildKind != 6 &&
      child1->m_LastBuildKind != 7 && child1->m_LastBuildKind != 8) ||
     child0->m_LastBuildCommandBuffer == ResourceId() ||
     child1->m_LastBuildCommandBuffer == ResourceId() ||
     !instances || instances->m_Type != eResBuffer || !Unwrap(instances) ||
     (Unwrap(instances)->storageMode() != MTL::StorageModeShared &&
      !(Unwrap(instances)->storageMode() == MTL::StorageModeManaged &&
        IsBackgroundCapturing(instances->m_State))) ||
     Unwrap(instances)->length() / sizeof(MTL::AccelerationStructureInstanceDescriptor) < 2 ||
     !Unwrap(instances)->contents() || !scratch || scratch->m_Type != eResBuffer ||
     !Unwrap(scratch))
    return false;
  for(uint32_t i = 0; i < 2; i++)
  {
    MTL::AccelerationStructureInstanceDescriptor data = {};
    memcpy(&data, (const byte *)Unwrap(instances)->contents() + i * sizeof(data), sizeof(data));
    if(!ValidMetalASInstance(data, 2) || data.accelerationStructureIndex != i)
      return false;
    for(int column = 0; column < 4; column++)
      for(int row = 0; row < 3; row++)
        if(!std::isfinite(data.transformationMatrix[column][row])) return false;
  }
  MTL::InstanceAccelerationStructureDescriptor *descriptor = DistinctInstanceDescriptor(
      Unwrap(instances), Unwrap(child0), Unwrap(child1));
  MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize && sizes.accelerationStructureSize <= structure->m_Size &&
         sizes.buildScratchBufferSize &&
         sizes.buildScratchBufferSize <= Unwrap(scratch)->length();
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildDistinctInstances(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    WrappedMTLAccelerationStructure *child0, WrappedMTLAccelerationStructure *child1,
    WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch, rdcarray<byte> descriptorBytes)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(child0).Important();
  SERIALISE_ELEMENT(child1).Important();
  SERIALISE_ELEMENT(instances).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(descriptorBytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || descriptorBytes.size() !=
           2 * sizeof(MTL::AccelerationStructureInstanceDescriptor) ||
       !ValidDistinctInstanceBuild(structure, child0, child1, instances, scratch,
                                   Unwrap(m_Device), GetResID(Encoder->m_CommandBuffer)) ||
       memcmp(descriptorBytes.data(), Unwrap(instances)->contents(), descriptorBytes.size()))
    {
      RDCERR("Invalid Metal distinct-child acceleration structure build");
      return false;
    }
    MTL::InstanceAccelerationStructureDescriptor *descriptor = DistinctInstanceDescriptor(
        Unwrap(instances), Unwrap(child0), Unwrap(child1));
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), 0);
    structure->m_LastBuildKind = 5;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastInstanceChild = GetResID(child0);
    structure->m_LastInstanceBuffer = GetResID(instances);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Distinct-Child Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildDistinctInstances(
    WrappedMTLAccelerationStructure *structure, WrappedMTLAccelerationStructure *child0,
    WrappedMTLAccelerationStructure *child1, WrappedMTLBuffer *instances,
    WrappedMTLBuffer *scratch)
{
  if(!ValidDistinctInstanceBuild(structure, child0, child1, instances, scratch,
                                 Unwrap(m_Device), GetResID(m_CommandBuffer)))
  {
    RDCERR("Invalid Metal distinct-child acceleration structure build");
    return;
  }
  MTL::InstanceAccelerationStructureDescriptor *descriptor = DistinctInstanceDescriptor(
      Unwrap(instances), Unwrap(child0), Unwrap(child1));
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), 0));
  structure->m_LastBuildKind = 5;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastInstanceChild = GetResID(child0);
  structure->m_LastInstanceBuffer = GetResID(instances);
  if(IsCaptureMode(m_State))
  {
    rdcarray<byte> descriptorBytes;
    const byte *data = (const byte *)Unwrap(instances)->contents();
    descriptorBytes.assign(data, 2 * sizeof(MTL::AccelerationStructureInstanceDescriptor));
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildDistinctInstances);
    Serialise_buildDistinctInstances(ser, structure, child0, child1, instances, scratch,
                                    descriptorBytes);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(child0));
    record->AddParent(GetRecord(child1));
    record->AddParent(GetRecord(instances));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildDistinctInstances(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, WrappedMTLBuffer *, rdcarray<byte>);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildDistinctInstances(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, WrappedMTLBuffer *, rdcarray<byte>);

static MTL::InstanceAccelerationStructureDescriptor *MultipleDistinctInstanceDescriptor(
    MTL::Buffer *instances, const rdcarray<WrappedMTLAccelerationStructure *> &children,
    NS::UInteger count)
{
  MTL::InstanceAccelerationStructureDescriptor *descriptor =
      MTL::InstanceAccelerationStructureDescriptor::descriptor();
  descriptor->setInstanceDescriptorBuffer(instances);
  descriptor->setInstanceCount(count);
  rdcarray<const NS::Object *> nativeChildren;
  nativeChildren.reserve(children.size());
  for(auto child : children) nativeChildren.push_back(Unwrap(child));
  descriptor->setInstancedAccelerationStructures(NS::Array::array(nativeChildren.data(), nativeChildren.size()));
  return descriptor;
}

static bool ValidMultipleDistinctInstanceBuild(
    WrappedMTLAccelerationStructure *structure,
    const rdcarray<WrappedMTLAccelerationStructure *> &children,
    WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch,
    MTL::Device *device, ResourceId commandBuffer, NS::UInteger count,
    bool identityIndices)
{
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     children.size() < (identityIndices ? 3U : 2U) || children.size() > 4 ||
     count < children.size() || count > 65536 ||
     (identityIndices && count != children.size()) ||
     (!identityIndices && count == children.size()) ||
     !instances || instances->m_Type != eResBuffer || !Unwrap(instances) ||
     (Unwrap(instances)->storageMode() != MTL::StorageModeShared &&
      !(Unwrap(instances)->storageMode() == MTL::StorageModeManaged &&
        IsBackgroundCapturing(instances->m_State))) ||
     Unwrap(instances)->length() / sizeof(MTL::AccelerationStructureInstanceDescriptor) <
         count ||
     !Unwrap(instances)->contents() || !scratch || scratch->m_Type != eResBuffer ||
     !Unwrap(scratch))
    return false;
  for(size_t i = 0; i < children.size(); i++)
  {
    WrappedMTLAccelerationStructure *child = children[i];
    if(!child || child == structure || child->m_Type != eResAccelerationStructure ||
       !Unwrap(child) || Unwrap(child)->device() != device || child->m_LastBuildKind < 1 ||
       (child->m_LastBuildKind > 4 && child->m_LastBuildKind != 6 &&
        child->m_LastBuildKind != 7 && child->m_LastBuildKind != 8) ||
       child->m_LastBuildCommandBuffer == ResourceId())
      return false;
    for(size_t j = 0; j < i; j++)
      if(child == children[j]) return false;
  }
  for(NS::UInteger i = 0; i < count; i++)
  {
    MTL::AccelerationStructureInstanceDescriptor data = {};
    memcpy(&data, (const byte *)Unwrap(instances)->contents() + i * sizeof(data), sizeof(data));
    if(!ValidMetalASInstance(data, children.size()) ||
       (identityIndices && data.accelerationStructureIndex != i))
      return false;
    for(int column = 0; column < 4; column++)
      for(int row = 0; row < 3; row++)
        if(!std::isfinite(data.transformationMatrix[column][row])) return false;
  }
  MTL::InstanceAccelerationStructureDescriptor *descriptor =
      MultipleDistinctInstanceDescriptor(Unwrap(instances), children, count);
  MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize && sizes.accelerationStructureSize <= structure->m_Size &&
         sizes.buildScratchBufferSize &&
         sizes.buildScratchBufferSize <= Unwrap(scratch)->length();
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildMultipleDistinctInstances(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    rdcarray<WrappedMTLAccelerationStructure *> children,
    WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch,
    rdcarray<byte> descriptorBytes)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(children).Important();
  SERIALISE_ELEMENT(instances).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(descriptorBytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       descriptorBytes.size() !=
           children.size() * sizeof(MTL::AccelerationStructureInstanceDescriptor) ||
       !ValidMultipleDistinctInstanceBuild(structure, children, instances, scratch,
                                           Unwrap(m_Device), GetResID(Encoder->m_CommandBuffer),
                                           children.size(), true) ||
       memcmp(descriptorBytes.data(), Unwrap(instances)->contents(), descriptorBytes.size()))
    {
      RDCERR("Invalid Metal multiple distinct-child acceleration structure build");
      return false;
    }
    MTL::InstanceAccelerationStructureDescriptor *descriptor =
        MultipleDistinctInstanceDescriptor(Unwrap(instances), children, children.size());
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), 0);
    structure->m_LastBuildKind = 5;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastInstanceChild = GetResID(children[0]);
    structure->m_LastInstanceBuffer = GetResID(instances);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Multiple Distinct-Child Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildMultipleDistinctInstances(
    WrappedMTLAccelerationStructure *structure,
    rdcarray<WrappedMTLAccelerationStructure *> children,
    WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch)
{
  if(!ValidMultipleDistinctInstanceBuild(structure, children, instances, scratch,
                                         Unwrap(m_Device), GetResID(m_CommandBuffer),
                                         children.size(), true))
  {
    RDCERR("Invalid Metal multiple distinct-child acceleration structure build");
    return;
  }
  MTL::InstanceAccelerationStructureDescriptor *descriptor =
      MultipleDistinctInstanceDescriptor(Unwrap(instances), children, children.size());
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), 0));
  structure->m_LastBuildKind = 5;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastInstanceChild = GetResID(children[0]);
  structure->m_LastInstanceBuffer = GetResID(instances);
  if(IsCaptureMode(m_State))
  {
    rdcarray<byte> descriptorBytes;
    const byte *data = (const byte *)Unwrap(instances)->contents();
    descriptorBytes.assign(data, children.size() *
                                 sizeof(MTL::AccelerationStructureInstanceDescriptor));
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildMultipleDistinctInstances);
    Serialise_buildMultipleDistinctInstances(ser, structure, children, instances, scratch,
                                            descriptorBytes);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    for(WrappedMTLAccelerationStructure *child : children)
      record->AddParent(GetRecord(child));
    record->AddParent(GetRecord(instances));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildMultipleDistinctInstances(
    ReadSerialiser &, WrappedMTLAccelerationStructure *,
    rdcarray<WrappedMTLAccelerationStructure *>, WrappedMTLBuffer *, WrappedMTLBuffer *,
    rdcarray<byte>);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildMultipleDistinctInstances(
    WriteSerialiser &, WrappedMTLAccelerationStructure *,
    rdcarray<WrappedMTLAccelerationStructure *>, WrappedMTLBuffer *, WrappedMTLBuffer *,
    rdcarray<byte>);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRepeatedDistinctInstances(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    rdcarray<WrappedMTLAccelerationStructure *> children,
    WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch, NS::UInteger count,
    rdcarray<byte> descriptorBytes)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(children).Important();
  SERIALISE_ELEMENT(instances).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(descriptorBytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       count > 65536 || descriptorBytes.size() !=
           count * sizeof(MTL::AccelerationStructureInstanceDescriptor) ||
       !ValidMultipleDistinctInstanceBuild(structure, children, instances, scratch,
                                           Unwrap(m_Device), GetResID(Encoder->m_CommandBuffer),
                                           count, false) ||
       memcmp(descriptorBytes.data(), Unwrap(instances)->contents(), descriptorBytes.size()))
    {
      RDCERR("Invalid Metal repeated distinct-child acceleration structure build");
      return false;
    }
    MTL::InstanceAccelerationStructureDescriptor *descriptor =
        MultipleDistinctInstanceDescriptor(Unwrap(instances), children, count);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), 0);
    structure->m_LastBuildKind = 5;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastInstanceChild = GetResID(children[0]);
    structure->m_LastInstanceBuffer = GetResID(instances);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt(
          "Build Metal %u-Instance %u-Child Acceleration Structure",
          (unsigned)count, (unsigned)children.size());
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildRepeatedDistinctInstances(
    WrappedMTLAccelerationStructure *structure,
    rdcarray<WrappedMTLAccelerationStructure *> children,
    WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch, NS::UInteger count)
{
  if(!ValidMultipleDistinctInstanceBuild(structure, children, instances, scratch,
                                         Unwrap(m_Device), GetResID(m_CommandBuffer),
                                         count, false))
  {
    RDCERR("Invalid Metal repeated distinct-child acceleration structure build");
    return;
  }
  MTL::InstanceAccelerationStructureDescriptor *descriptor =
      MultipleDistinctInstanceDescriptor(Unwrap(instances), children, count);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), 0));
  structure->m_LastBuildKind = 5;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastInstanceChild = GetResID(children[0]);
  structure->m_LastInstanceBuffer = GetResID(instances);
  if(IsCaptureMode(m_State))
  {
    rdcarray<byte> descriptorBytes;
    const byte *data = (const byte *)Unwrap(instances)->contents();
    descriptorBytes.assign(data, count * sizeof(MTL::AccelerationStructureInstanceDescriptor));
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildRepeatedDistinctInstances);
    Serialise_buildRepeatedDistinctInstances(ser, structure, children, instances, scratch,
                                            count, descriptorBytes);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    for(WrappedMTLAccelerationStructure *child : children)
      record->AddParent(GetRecord(child));
    record->AddParent(GetRecord(instances));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRepeatedDistinctInstances(
    ReadSerialiser &, WrappedMTLAccelerationStructure *,
    rdcarray<WrappedMTLAccelerationStructure *>, WrappedMTLBuffer *, WrappedMTLBuffer *,
    NS::UInteger, rdcarray<byte>);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRepeatedDistinctInstances(
    WriteSerialiser &, WrappedMTLAccelerationStructure *,
    rdcarray<WrappedMTLAccelerationStructure *>, WrappedMTLBuffer *, WrappedMTLBuffer *,
    NS::UInteger, rdcarray<byte>);

static MTL::InstanceAccelerationStructureDescriptor *UserIDInstanceDescriptor(
    MTL::Buffer *instances, const rdcarray<WrappedMTLAccelerationStructure *> &children,
    const rdcarray<uint64_t> &p)
{
  auto descriptor = MultipleDistinctInstanceDescriptor(instances, children, p[3]);
  descriptor->setInstanceDescriptorType(MTL::AccelerationStructureInstanceDescriptorTypeUserID);
  descriptor->setInstanceDescriptorBufferOffset(p[0]);
  descriptor->setInstanceDescriptorStride(p[1]);
  return descriptor;
}

static bool ValidUserIDInstanceBuild(WrappedMTLAccelerationStructure *structure,
    const rdcarray<WrappedMTLAccelerationStructure *> &children,
    WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch,
    const rdcarray<uint64_t> &p, MTL::Device *device, ResourceId command, bytebuf &packed)
{
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     Unwrap(structure)->device() != device || children.empty() || children.size() > 4 ||
     !instances || instances->m_Type != eResBuffer || !Unwrap(instances) ||
     Unwrap(instances)->device() != device || Unwrap(instances)->heap() ||
     (Unwrap(instances)->storageMode() != MTL::StorageModeShared &&
      !(Unwrap(instances)->storageMode() == MTL::StorageModeManaged &&
        IsBackgroundCapturing(instances->m_State))) ||
     !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
     Unwrap(scratch)->device() != device || p.size() != 8 ||
     p[2] != uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeUserID) ||
     !PackMetalASInitialInstances((const byte *)Unwrap(instances)->contents(),
         Unwrap(instances)->length(), p, children.size(), packed)) return false;
  for(size_t i = 0; i < children.size(); i++)
  {
    auto child = children[i];
    if(!child || child == structure || child->m_Type != eResAccelerationStructure ||
       !Unwrap(child) || Unwrap(child)->device() != device ||
       child->m_LastBuildCommandBuffer == ResourceId() || child->m_LastBuildCommandBuffer == command ||
       (child->m_LastBuildKind != 1 && child->m_LastBuildKind != 2 &&
        child->m_LastBuildKind != 3 && child->m_LastBuildKind != 4 &&
        child->m_LastBuildKind != 6 && child->m_LastBuildKind != 7 &&
        child->m_LastBuildKind != 8)) return false;
    for(size_t j = 0; j < i; j++) if(child == children[j]) return false;
  }
  auto sizes = device->accelerationStructureSizes(UserIDInstanceDescriptor(Unwrap(instances), children, p));
  return sizes.accelerationStructureSize && sizes.accelerationStructureSize <= structure->m_Size &&
      sizes.buildScratchBufferSize && sizes.buildScratchBufferSize <= Unwrap(scratch)->length();
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildUserIDInstances(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    rdcarray<WrappedMTLAccelerationStructure *> children, WrappedMTLBuffer *instances,
    WrappedMTLBuffer *scratch, rdcarray<uint64_t> parameters, bytebuf descriptorBytes)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(children).Important();
  SERIALISE_ELEMENT(instances).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(parameters).Important();
  SERIALISE_ELEMENT(descriptorBytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    bytebuf packed;
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidUserIDInstanceBuild(structure, children, instances, scratch, parameters,
           Unwrap(m_Device), GetResID(Encoder->m_CommandBuffer), packed) || packed != descriptorBytes)
    {
      RDCERR("Invalid Metal user-ID instance acceleration structure build");
      return false;
    }
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure),
        UserIDInstanceDescriptor(Unwrap(instances), children, parameters), Unwrap(scratch), 0);
    structure->m_LastBuildKind = 5;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastInstanceChild = GetResID(children[0]);
    structure->m_LastInstanceBuffer = GetResID(instances);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal User-ID Instance Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildUserIDInstances(
    WrappedMTLAccelerationStructure *structure,
    rdcarray<WrappedMTLAccelerationStructure *> children, WrappedMTLBuffer *instances,
    WrappedMTLBuffer *scratch, rdcarray<uint64_t> parameters)
{
  bytebuf packed;
  // This direct CPU-input path does not infer an earlier same-CB GPU producer.
  // GPU instance evidence and indirect AS identity relocation are separate gates.
  if(!m_CommandBuffer || (IsCaptureMode(m_State) &&
      !GetRecord(m_CommandBuffer)->HasOnlyASInitialCommands()) ||
     !ValidUserIDInstanceBuild(structure, children, instances, scratch, parameters,
      Unwrap(m_Device), GetResID(m_CommandBuffer), packed))
  {
    RDCERR("Invalid Metal user-ID instance acceleration structure build");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(Unwrap(structure),
      UserIDInstanceDescriptor(Unwrap(instances), children, parameters), Unwrap(scratch), 0));
  structure->m_LastBuildKind = 5;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastInstanceChild = GetResID(children[0]);
  structure->m_LastInstanceBuffer = GetResID(instances);
  if(IsCaptureMode(m_State))
  {
    const bytebuf &descriptorBytes = packed;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildUserIDInstances);
    Serialise_buildUserIDInstances(ser, structure, children, instances, scratch, parameters, descriptorBytes);
    auto record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    for(auto child : children) record->AddParent(GetRecord(child));
    record->AddParent(GetRecord(instances)); record->AddParent(GetRecord(scratch));
    record->MarkResourceFrameReferenced(GetResID(structure), eFrameRef_CompleteWrite);
    record->MarkResourceFrameReferenced(GetResID(instances), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(scratch), eFrameRef_PartialWrite);
    record->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLAccelerationStructureCommandEncoder, void,
    buildUserIDInstances, WrappedMTLAccelerationStructure *,
    rdcarray<WrappedMTLAccelerationStructure *>, WrappedMTLBuffer *, WrappedMTLBuffer *,
    rdcarray<uint64_t>, bytebuf);

static MTL::InstanceAccelerationStructureDescriptor *NoChildIndirectInstanceDescriptor(
    MTL::Buffer *instances, const rdcarray<uint64_t> &p)
{
  auto descriptor = MTL::InstanceAccelerationStructureDescriptor::descriptor();
  descriptor->setInstanceDescriptorBuffer(instances);
  descriptor->setInstanceDescriptorBufferOffset(p[0]);
  descriptor->setInstanceDescriptorStride(p[1]);
  descriptor->setInstanceDescriptorType(MTL::AccelerationStructureInstanceDescriptorTypeIndirect);
  descriptor->setInstanceCount(p[3]);
  return descriptor;
}

static bool PrepareIndirectInstanceBuild(WrappedMTLAccelerationStructure *structure,
    const rdcarray<WrappedMTLAccelerationStructure *> &children,
    WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch, const rdcarray<uint64_t> &p,
    MTL::Device *device, ResourceId command, bytebuf &raw,
    NS::SharedPtr<MTL::Buffer> &staging, NS::UInteger scratchOffset)
{
  if(!scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
     scratchOffset % 256 || scratchOffset > Unwrap(scratch)->length()) return false;
  if(p.size() == 8 && p[3] == 0)
  {
    if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
       Unwrap(structure)->device() != device || !children.empty() ||
       !instances || instances->m_Type != eResBuffer || !Unwrap(instances) ||
       Unwrap(instances)->device() != device ||
       !ValidMetalASEmptyIndirectParameters(p, Unwrap(instances)->length()) ||
       !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
       Unwrap(scratch)->device() != device) return false;
    auto sizes = device->accelerationStructureSizes(NoChildIndirectInstanceDescriptor(Unwrap(instances), p));
    raw.clear(); staging.reset();
    return sizes.accelerationStructureSize && sizes.accelerationStructureSize <= structure->m_Size &&
        sizes.buildScratchBufferSize && sizes.buildScratchBufferSize <= Unwrap(scratch)->length() - scratchOffset;
  }
  if(children.empty())
  {
    if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
       Unwrap(structure)->device() != device || !instances || instances->m_Type != eResBuffer ||
       !Unwrap(instances) || Unwrap(instances)->device() != device || Unwrap(instances)->heap() ||
       Unwrap(instances)->storageMode() != MTL::StorageModeShared ||
       !PackMetalASInactiveInstances((const byte *)Unwrap(instances)->contents(),
           Unwrap(instances)->length(), p, raw) ||
       !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
       Unwrap(scratch)->device() != device) return false;
    auto sizes = device->accelerationStructureSizes(NoChildIndirectInstanceDescriptor(Unwrap(instances), p));
    staging.reset();
    return sizes.accelerationStructureSize && sizes.accelerationStructureSize <= structure->m_Size &&
        sizes.buildScratchBufferSize && sizes.buildScratchBufferSize <= Unwrap(scratch)->length() - scratchOffset;
  }
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     Unwrap(structure)->device() != device || children.empty() || children.size() > MetalMaxIndirectASChildren ||
     !instances || instances->m_Type != eResBuffer || !Unwrap(instances) ||
     Unwrap(instances)->device() != device || Unwrap(instances)->heap() ||
     Unwrap(instances)->storageMode() != MTL::StorageModeShared ||
     !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
     Unwrap(scratch)->device() != device) return false;
  rdcarray<uint64_t> identities;
  for(size_t i = 0; i < children.size(); i++)
  {
    auto child = children[i];
    if(!child || child == structure || child->m_Type != eResAccelerationStructure ||
       !Unwrap(child) || Unwrap(child)->device() != device ||
       child->m_LastBuildCommandBuffer == ResourceId() || child->m_LastBuildCommandBuffer == command ||
       (child->m_LastBuildKind != 1 && child->m_LastBuildKind != 2 &&
        child->m_LastBuildKind != 3 && child->m_LastBuildKind != 4 &&
        child->m_LastBuildKind != 6 && child->m_LastBuildKind != 7 && child->m_LastBuildKind != 8)) return false;
    for(size_t j = 0; j < i; j++) if(child == children[j]) return false;
    identities.push_back(child->m_CapturedGPUResourceID);
  }
  bytebuf direct;
  if(!PackMetalASIndirectInstances((const byte *)Unwrap(instances)->contents(),
      Unwrap(instances)->length(), p, identities, raw) ||
     !ConvertMetalASIndirectInstances(raw, p[3], identities, direct)) return false;
  staging = NS::TransferPtr(device->newBuffer(direct.data(), direct.size(), MTL::ResourceStorageModeShared));
  if(!staging) return false;
  rdcarray<uint64_t> packedParameters = {0, 68,
      uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeUserID), p[3], 0, 0, 0, 0};
  auto sizes = device->accelerationStructureSizes(UserIDInstanceDescriptor(staging.get(), children, packedParameters));
  return sizes.accelerationStructureSize && sizes.accelerationStructureSize <= structure->m_Size &&
      sizes.buildScratchBufferSize && sizes.buildScratchBufferSize <= Unwrap(scratch)->length() - scratchOffset;
}

// Frame-private input is an encoder-point snapshot, never CPU contents from the
// original allocation. Validate the typed packet then rebuild on live children.
static bool PreparePrivateIndirectInstanceBuild(WrappedMTLAccelerationStructure *structure,
    const rdcarray<WrappedMTLAccelerationStructure *> &children, WrappedMTLBuffer *instances,
    WrappedMTLBuffer *scratch, const rdcarray<uint64_t> &p, const bytebuf &raw,
    MTL::Device *device, NS::SharedPtr<MTL::Buffer> &staging, NS::UInteger scratchOffset)
{
  if(!scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
     scratchOffset % 256 || scratchOffset > Unwrap(scratch)->length()) return false;
  uint64_t bytes = 0;
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     Unwrap(structure)->device() != device || !instances || instances->m_Type != eResBuffer ||
     !Unwrap(instances) || Unwrap(instances)->device() != device ||
     !ValidMetalASPrivateInstanceInput(instances) ||
     !MetalASIndirectInstanceSpan(p, Unwrap(instances)->length(), bytes) || raw.size() != bytes ||
     children.size() > MetalMaxIndirectASChildren || !scratch || scratch->m_Type != eResBuffer ||
     !Unwrap(scratch) || Unwrap(scratch)->device() != device) return false;
  rdcarray<uint64_t> identities;
  for(size_t i = 0; i < children.size(); i++)
  {
    auto child = children[i];
    if(!child || child == structure || child->m_Type != eResAccelerationStructure || !Unwrap(child) ||
       Unwrap(child)->device() != device || child->m_LastBuildCommandBuffer == ResourceId() ||
       (child->m_LastBuildKind != 1 && child->m_LastBuildKind != 2 && child->m_LastBuildKind != 3 &&
        child->m_LastBuildKind != 4 && child->m_LastBuildKind != 6 && child->m_LastBuildKind != 7 &&
        child->m_LastBuildKind != 8)) return false;
    for(size_t j = 0; j < i; j++) if(child == children[j]) return false;
    identities.push_back(child->m_CapturedGPUResourceID);
  }
  bytebuf packed;
  if(children.empty())
  {
    if(!ValidMetalASInactiveInstances(raw, p[3])) return false;
    packed = raw;
  }
  else if(!ConvertMetalASIndirectInstances(raw, p[3], identities, packed)) return false;
  staging = NS::TransferPtr(device->newBuffer(packed.data(), packed.size(), MTL::ResourceStorageModeShared));
  if(!staging) return false;
  rdcarray<uint64_t> packedParameters = {0, children.empty() ? 72ULL : 68ULL,
      uint64_t(children.empty() ? MTL::AccelerationStructureInstanceDescriptorTypeIndirect :
          MTL::AccelerationStructureInstanceDescriptorTypeUserID), p[3], 0, 0, 0, 0};
  auto descriptor = children.empty() ? NoChildIndirectInstanceDescriptor(staging.get(), packedParameters) :
      UserIDInstanceDescriptor(staging.get(), children, packedParameters);
  auto sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize && sizes.accelerationStructureSize <= structure->m_Size &&
      sizes.buildScratchBufferSize && sizes.buildScratchBufferSize <= Unwrap(scratch)->length() - scratchOffset;
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndirectInstances(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    rdcarray<WrappedMTLAccelerationStructure *> children, WrappedMTLBuffer *instances,
    WrappedMTLBuffer *scratch, rdcarray<uint64_t> parameters, bytebuf descriptorBytes, NS::UInteger scratchOffset)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(children).Important();
  SERIALISE_ELEMENT(instances).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(parameters).Important();
  SERIALISE_ELEMENT(descriptorBytes).Important();
  if(ser.ChunkMetadata().chunkID == uint32_t(MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstancesWithScratchOffset))
  {
    SERIALISE_ELEMENT(scratchOffset).Important();
  }
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    bytebuf raw;
    NS::SharedPtr<MTL::Buffer> staging;
    const bool privateSnapshot = instances && instances->m_Type == eResBuffer && Unwrap(instances) &&
        Unwrap(instances)->storageMode() == MTL::StorageModePrivate && !descriptorBytes.empty();
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       (privateSnapshot ? !PreparePrivateIndirectInstanceBuild(structure, children, instances, scratch,
           parameters, descriptorBytes, Unwrap(m_Device), staging, scratchOffset) :
           !PrepareIndirectInstanceBuild(structure, children, instances, scratch, parameters,
               Unwrap(m_Device), GetResID(Encoder->m_CommandBuffer), raw, staging, scratchOffset) || raw != descriptorBytes))
    {
      RDCERR("Invalid Metal typed indirect instance acceleration structure build");
      return false;
    }
    rdcarray<uint64_t> packedParameters = {0, 68,
        uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeUserID), parameters[3], 0, 0, 0, 0};
    rdcarray<uint64_t> inactiveParameters = {0, 72,
        uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeIndirect), parameters[3], 0, 0, 0, 0};
    rdcarray<ResourceId> childIDs;
    for(auto child : children) childIDs.push_back(GetResID(child));
    if(!m_Device->RecordRayIRIndirectASBuild(GetResID(structure), childIDs, GetResID(instances),
        GetResID(scratch), parameters, descriptorBytes, scratchOffset)) return false;
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure),
        (children.empty() ? NoChildIndirectInstanceDescriptor(privateSnapshot ? staging.get() : Unwrap(instances),
            privateSnapshot ? inactiveParameters : parameters) :
         UserIDInstanceDescriptor(staging.get(), children, packedParameters)), Unwrap(scratch), scratchOffset);
    if(privateSnapshot) m_Device->RecordReplayFrozenASInput(GetResID(instances));
    // The callback retains only the native staging allocation, not a wrapper.
    Unwrap(Encoder->m_CommandBuffer)->addCompletedHandler([staging](MTL::CommandBuffer *) {});
    structure->m_LastBuildKind = 5;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    structure->m_LastInstanceChild = children.empty() ? ResourceId() : GetResID(children[0]); structure->m_LastInstanceBuffer = GetResID(instances);
    if(IsLoading(m_State))
    {
      AddEvent(); ActionDescription action;
      action.customName = "Build Metal Typed Indirect Instance Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct; AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildIndirectInstances(
    WrappedMTLAccelerationStructure *structure,
    rdcarray<WrappedMTLAccelerationStructure *> children, WrappedMTLBuffer *instances,
    WrappedMTLBuffer *scratch, rdcarray<uint64_t> parameters, NS::UInteger scratchOffset)
{
  bytebuf raw; NS::SharedPtr<MTL::Buffer> staging;
  const bool privateInitial = IsCaptureMode(m_State) && instances && Unwrap(instances) &&
      Unwrap(instances)->storageMode() == MTL::StorageModePrivate;
  bool valid = m_CommandBuffer != NULL;
  const bool emptyIndirect = parameters.size() == 8 && parameters[3] == 0;
  if(emptyIndirect)
    valid &= PrepareIndirectInstanceBuild(structure, children, instances, scratch, parameters,
        Unwrap(m_Device), GetResID(m_CommandBuffer), raw, staging, scratchOffset);
  else if(privateInitial)
  {
    uint64_t bytes = 0;
    valid &= structure && structure->m_Type == eResAccelerationStructure && Unwrap(structure) &&
        Unwrap(structure)->device() == Unwrap(m_Device) &&
        instances->m_Type == eResBuffer && Unwrap(instances)->device() == Unwrap(m_Device) &&
        ValidMetalASPrivateInstanceInput(instances) &&
        MetalASIndirectInstanceSpan(parameters, Unwrap(instances)->length(), bytes) &&
        scratch && scratch->m_Type == eResBuffer && Unwrap(scratch) &&
        Unwrap(scratch)->device() == Unwrap(m_Device) && scratchOffset % 256 == 0 &&
        scratchOffset <= Unwrap(scratch)->length() && children.size() <= MetalMaxIndirectASChildren;
    for(auto child : children)
      valid &= child && child != structure && child->m_Type == eResAccelerationStructure &&
          Unwrap(child) && Unwrap(child)->device() == Unwrap(m_Device) &&
          child->m_LastBuildCommandBuffer != ResourceId() &&
          (IsActiveCapturing(m_State) || child->m_LastBuildCommandBuffer != GetResID(m_CommandBuffer)) &&
          (child->m_LastBuildKind == 1 || child->m_LastBuildKind == 2 || child->m_LastBuildKind == 3 ||
           child->m_LastBuildKind == 4 || child->m_LastBuildKind == 6 || child->m_LastBuildKind == 7 ||
           child->m_LastBuildKind == 8);
  }
  else valid &= m_CommandBuffer && GetRecord(m_CommandBuffer)->HasOnlyASInitialCommands() &&
      PrepareIndirectInstanceBuild(structure, children, instances, scratch, parameters,
          Unwrap(m_Device), GetResID(m_CommandBuffer), raw, staging, scratchOffset);
  if(!valid)
  {
    RDCERR("Invalid or unsupported Metal typed indirect instance build input"); return;
  }
  auto descriptor = MTL::InstanceAccelerationStructureDescriptor::descriptor();
  descriptor->setInstanceDescriptorBuffer(Unwrap(instances));
  descriptor->setInstanceDescriptorBufferOffset(parameters[0]); descriptor->setInstanceDescriptorStride(parameters[1]);
  descriptor->setInstanceDescriptorType(MTL::AccelerationStructureInstanceDescriptorTypeIndirect);
  descriptor->setInstanceCount(parameters[3]);
  if(privateInitial)
  {
    auto sizes = Unwrap(m_Device)->accelerationStructureSizes(descriptor);
    if(!sizes.accelerationStructureSize || sizes.accelerationStructureSize > structure->m_Size ||
       !sizes.buildScratchBufferSize || sizes.buildScratchBufferSize > Unwrap(scratch)->length() - scratchOffset)
    { RDCERR("Invalid Metal Private indirect instance allocation"); return; }
  }
  if(children.empty()) TrackInitialBuild(structure, instances, parameters, emptyIndirect ? 10 : 11);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = 5; structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_LastInstanceChild = children.empty() ? ResourceId() : GetResID(children[0]); structure->m_LastInstanceBuffer = GetResID(instances);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(scratchOffset ? MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstancesWithScratchOffset :
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstances);
    Serialise_buildIndirectInstances(ser, structure, children, instances, scratch, parameters, raw, scratchOffset);
    auto record = GetRecord(m_CommandBuffer); record->AddParent(GetRecord(structure));
    for(auto child : children) record->AddParent(GetRecord(child));
    record->AddParent(GetRecord(instances)); record->AddParent(GetRecord(scratch));
    MetalASFrameBuild evidence;
    if(privateInitial && !emptyIndirect && IsActiveCapturing(m_State))
    {
      evidence.encoder = GetResID(this); evidence.target = GetResID(structure);
      evidence.source = GetResID(instances); evidence.scratch = GetResID(scratch);
      evidence.scratchOffset = scratchOffset;
      evidence.metadata = ser.ChunkMetadata();
      evidence.metadataFlags = ser.GetChunkMetadataRecording();
      evidence.build = record->cmdInfo->initialASBuilds.back().build;
    }
    Chunk *chunk = scope.Get(); record->AddChunk(chunk);
    if(evidence.build)
    { evidence.chunk = chunk; record->cmdInfo->frameASBuilds.push_back(evidence); }
    record->MarkResourceFrameReferenced(GetResID(structure), eFrameRef_CompleteWrite);
    record->MarkResourceFrameReferenced(GetResID(instances), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(scratch), eFrameRef_PartialWrite);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLAccelerationStructureCommandEncoder, void,
    buildIndirectInstances, WrappedMTLAccelerationStructure *,
    rdcarray<WrappedMTLAccelerationStructure *>, WrappedMTLBuffer *, WrappedMTLBuffer *,
    rdcarray<uint64_t>, bytebuf, NS::UInteger);

static bool ValidIndexedTriangleBuild(WrappedMTLAccelerationStructure *structure,
                                      WrappedMTLBuffer *vertices, WrappedMTLBuffer *indices,
                                      MTL::IndexType indexType, NS::UInteger count,
                                      WrappedMTLBuffer *scratch, MTL::Device *device,
                                      bool opaque = false, NS::UInteger indexOffset = 0,
                                      NS::UInteger vertexOffset = 0,
                                      NS::UInteger scratchOffset = 0,
                                      NS::UInteger tableOffset = 0,
                                      bool allowDuplicate = true,
                                      bool nonOpaque = false)
{
  const uint64_t indexBytes = indexType == MTL::IndexTypeUInt16 ? 2 :
                              indexType == MTL::IndexTypeUInt32 ? 4 : 0;
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     !vertices || vertices->m_Type != eResBuffer || !Unwrap(vertices) ||
     !indices || indices->m_Type != eResBuffer || !Unwrap(indices) ||
     !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
     !indexBytes || !count || count > 1000000 ||
     vertexOffset % (3 * sizeof(float)) || vertexOffset > Unwrap(vertices)->length() ||
     Unwrap(vertices)->length() - vertexOffset < 12 ||
     indexOffset % indexBytes || indexOffset > Unwrap(indices)->length() ||
     count > (Unwrap(indices)->length() - indexOffset) / (3 * indexBytes) ||
     scratchOffset % 256 || scratchOffset > Unwrap(scratch)->length())
    return false;
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      TriangleDescriptor(Unwrap(vertices), vertexOffset, count, Unwrap(indices), indexType,
                         false, nonOpaque, opaque, indexOffset, tableOffset, allowDuplicate);
  MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize <= structure->m_Size &&
         sizes.buildScratchBufferSize <= Unwrap(scratch)->length() - scratchOffset;
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedTriangle(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    WrappedMTLBuffer *indices, MTL::IndexType indexType, NS::UInteger triangleCount,
    WrappedMTLBuffer *scratch, bool opaque)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(indices).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || !ValidIndexedTriangleBuild(
           structure, vertices, indices, indexType, triangleCount, scratch, Unwrap(m_Device), opaque))
    {
      RDCERR("Invalid Metal indexed triangle acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), 0, triangleCount, Unwrap(indices), indexType,
        false, false, opaque);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch), 0);
    structure->m_LastBuildKind = 2;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = opaque ? "Build Metal Indexed Opaque Triangle Acceleration Structure" :
                                   "Build Metal Indexed Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildIndexedTriangle(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    WrappedMTLBuffer *indices, MTL::IndexType indexType, NS::UInteger triangleCount,
    WrappedMTLBuffer *scratch, bool opaque)
{
  if(!ValidIndexedTriangleBuild(structure, vertices, indices, indexType, triangleCount,
                                scratch, Unwrap(m_Device), opaque))
  {
    RDCERR("Invalid Metal indexed triangle acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), 0, triangleCount, Unwrap(indices), indexType,
      false, false, opaque);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), 0));
  structure->m_LastBuildKind = 2;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(opaque ?
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndexedOpaqueTriangle :
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndexedTriangle);
    Serialise_buildIndexedTriangle(ser, structure, vertices, indices, indexType,
                                   triangleCount, scratch, opaque);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(indices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedTriangle(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, WrappedMTLBuffer *,
    MTL::IndexType, NS::UInteger, WrappedMTLBuffer *, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedTriangle(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, WrappedMTLBuffer *,
    MTL::IndexType, NS::UInteger, WrappedMTLBuffer *, bool);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedTriangleOffset(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    WrappedMTLBuffer *indices, MTL::IndexType indexType, NS::UInteger indexOffset,
    NS::UInteger triangleCount, WrappedMTLBuffer *scratch, bool opaque)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(indices).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexOffset).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !indexOffset || !ValidIndexedTriangleBuild(structure, vertices, indices, indexType,
           triangleCount, scratch, Unwrap(m_Device), opaque, indexOffset))
    {
      RDCERR("Invalid Metal indexed triangle offset acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), 0, triangleCount, Unwrap(indices), indexType,
        false, false, opaque, indexOffset);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch), 0);
    structure->m_LastBuildKind = 2;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = opaque ? "Build Metal Indexed Opaque Triangle Acceleration Structure" :
                                   "Build Metal Indexed Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildIndexedTriangleOffset(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    WrappedMTLBuffer *indices, MTL::IndexType indexType, NS::UInteger indexOffset,
    NS::UInteger triangleCount, WrappedMTLBuffer *scratch, bool opaque)
{
  if(!indexOffset || !ValidIndexedTriangleBuild(structure, vertices, indices, indexType,
      triangleCount, scratch, Unwrap(m_Device), opaque, indexOffset))
  {
    RDCERR("Invalid Metal indexed triangle offset acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), 0, triangleCount, Unwrap(indices), indexType,
      false, false, opaque, indexOffset);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), 0));
  structure->m_LastBuildKind = 2;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndexedTriangleOffset);
    Serialise_buildIndexedTriangleOffset(ser, structure, vertices, indices, indexType,
                                         indexOffset, triangleCount, scratch, opaque);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(indices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedTriangleOffset(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, WrappedMTLBuffer *,
    MTL::IndexType, NS::UInteger, NS::UInteger, WrappedMTLBuffer *, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedTriangleOffset(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, WrappedMTLBuffer *,
    MTL::IndexType, NS::UInteger, NS::UInteger, WrappedMTLBuffer *, bool);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedTriangleExtended(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, WrappedMTLBuffer *indices, MTL::IndexType indexType,
    NS::UInteger indexOffset, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, bool opaque)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(vertexOffset).Important();
  SERIALISE_ELEMENT(indices).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexOffset).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       (!vertexOffset && !scratchOffset) ||
       !ValidIndexedTriangleBuild(structure, vertices, indices, indexType, triangleCount,
                                  scratch, Unwrap(m_Device), opaque, indexOffset,
                                  vertexOffset, scratchOffset))
    {
      RDCERR("Invalid Metal extended indexed triangle acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), vertexOffset, triangleCount, Unwrap(indices), indexType,
        false, false, opaque, indexOffset);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch),
                                                scratchOffset);
    structure->m_LastBuildKind = 2;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = opaque ? "Build Metal Indexed Opaque Triangle Acceleration Structure" :
                                   "Build Metal Indexed Triangle Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildIndexedTriangleExtended(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, WrappedMTLBuffer *indices, MTL::IndexType indexType,
    NS::UInteger indexOffset, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, bool opaque)
{
  if((!vertexOffset && !scratchOffset) ||
     !ValidIndexedTriangleBuild(structure, vertices, indices, indexType, triangleCount,
                                scratch, Unwrap(m_Device), opaque, indexOffset,
                                vertexOffset, scratchOffset))
  {
    RDCERR("Invalid Metal extended indexed triangle acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), vertexOffset, triangleCount, Unwrap(indices), indexType,
      false, false, opaque, indexOffset);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = 2;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndexedTriangleExtended);
    Serialise_buildIndexedTriangleExtended(ser, structure, vertices, vertexOffset, indices,
                                           indexType, indexOffset, triangleCount, scratch,
                                           scratchOffset, opaque);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(indices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedTriangleExtended(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *, MTL::IndexType, NS::UInteger, NS::UInteger, WrappedMTLBuffer *,
    NS::UInteger, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildIndexedTriangleExtended(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *, MTL::IndexType, NS::UInteger, NS::UInteger, WrappedMTLBuffer *,
    NS::UInteger, bool);

static bool ValidTriangleTableOffsetBuild(WrappedMTLAccelerationStructure *structure,
                                          WrappedMTLBuffer *vertices, NS::UInteger vertexOffset,
                                          WrappedMTLBuffer *indices, MTL::IndexType indexType,
                                          NS::UInteger indexOffset, NS::UInteger triangleCount,
                                          WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
                                          NS::UInteger tableOffset, bool opaque, MTL::Device *device)
{
  if(!tableOffset || tableOffset > 31) return false;
  if(indices)
    return ValidIndexedTriangleBuild(structure, vertices, indices, indexType, triangleCount,
                                     scratch, device, opaque, indexOffset, vertexOffset,
                                     scratchOffset, tableOffset);
  return indexType == MTL::IndexTypeUInt16 && indexOffset == 0 &&
         ValidTriangleBuild(structure, vertices, vertexOffset, triangleCount, scratch,
                            scratchOffset, device, false, !opaque, opaque, false, tableOffset);
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildTriangleTableOffset(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, WrappedMTLBuffer *indices, MTL::IndexType indexType,
    NS::UInteger indexOffset, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, NS::UInteger tableOffset, bool opaque)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(vertexOffset).Important();
  SERIALISE_ELEMENT(indices).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexOffset).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidTriangleTableOffsetBuild(structure, vertices, vertexOffset, indices, indexType,
                                      indexOffset, triangleCount, scratch, scratchOffset,
                                      tableOffset, opaque, Unwrap(m_Device)))
    {
      RDCERR("Invalid Metal triangle intersection table offset build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), vertexOffset, triangleCount, indices ? Unwrap(indices) : NULL,
        indexType, false, !opaque, opaque, indexOffset, tableOffset);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch),
                                                scratchOffset);
    structure->m_LastBuildKind = indices ? 2 : 1;
    structure->m_LastTriangleCount = indices ? 0 : triangleCount;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(!indices) structure->m_LastVertices = GetResID(vertices);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Triangle Acceleration Structure (Intersection Table Offset)";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildTriangleTableOffset(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, WrappedMTLBuffer *indices, MTL::IndexType indexType,
    NS::UInteger indexOffset, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, NS::UInteger tableOffset, bool opaque)
{
  if(!ValidTriangleTableOffsetBuild(structure, vertices, vertexOffset, indices, indexType,
                                    indexOffset, triangleCount, scratch, scratchOffset,
                                    tableOffset, opaque, Unwrap(m_Device)))
  {
    RDCERR("Invalid Metal triangle intersection table offset build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), vertexOffset, triangleCount, indices ? Unwrap(indices) : NULL,
      indexType, false, !opaque, opaque, indexOffset, tableOffset);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = indices ? 2 : 1;
  structure->m_LastTriangleCount = indices ? 0 : triangleCount;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(!indices) structure->m_LastVertices = GetResID(vertices);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildTriangleTableOffset);
    Serialise_buildTriangleTableOffset(ser, structure, vertices, vertexOffset, indices,
                                       indexType, indexOffset, triangleCount, scratch,
                                       scratchOffset, tableOffset, opaque);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    if(indices) record->AddParent(GetRecord(indices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildTriangleTableOffset(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *, MTL::IndexType, NS::UInteger, NS::UInteger, WrappedMTLBuffer *,
    NS::UInteger, NS::UInteger, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildTriangleTableOffset(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *, MTL::IndexType, NS::UInteger, NS::UInteger, WrappedMTLBuffer *,
    NS::UInteger, NS::UInteger, bool);

static bool ValidTriangleNoDuplicateBuild(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, WrappedMTLBuffer *indices, MTL::IndexType indexType,
    NS::UInteger indexOffset, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, NS::UInteger tableOffset, bool opaque, MTL::Device *device)
{
  if(tableOffset > 31) return false;
  if(indices)
    return ValidIndexedTriangleBuild(structure, vertices, indices, indexType, triangleCount,
                                     scratch, device, opaque, indexOffset, vertexOffset,
                                     scratchOffset, tableOffset, false, !opaque);
  return indexType == MTL::IndexTypeUInt16 && indexOffset == 0 &&
         ValidTriangleBuild(structure, vertices, vertexOffset, triangleCount, scratch,
                            scratchOffset, device, false, !opaque, opaque, false,
                            tableOffset, false);
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildTriangleNoDuplicate(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, WrappedMTLBuffer *indices, MTL::IndexType indexType,
    NS::UInteger indexOffset, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, NS::UInteger tableOffset, bool opaque)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(vertexOffset).Important();
  SERIALISE_ELEMENT(indices).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexOffset).Important();
  SERIALISE_ELEMENT(triangleCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidTriangleNoDuplicateBuild(structure, vertices, vertexOffset, indices, indexType,
                                      indexOffset, triangleCount, scratch, scratchOffset,
                                      tableOffset, opaque, Unwrap(m_Device)))
    {
      RDCERR("Invalid no-duplicate Metal triangle acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
        Unwrap(vertices), vertexOffset, triangleCount, indices ? Unwrap(indices) : NULL,
        indexType, false, !opaque, opaque, indexOffset, tableOffset, false);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch),
                                                scratchOffset);
    structure->m_LastBuildKind = indices ? 2 : 1;
    structure->m_LastTriangleCount = indices ? 0 : triangleCount;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(!indices) structure->m_LastVertices = GetResID(vertices);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Triangle Acceleration Structure (No Duplicate Intersections)";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildTriangleNoDuplicate(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    NS::UInteger vertexOffset, WrappedMTLBuffer *indices, MTL::IndexType indexType,
    NS::UInteger indexOffset, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, NS::UInteger tableOffset, bool opaque)
{
  if(!ValidTriangleNoDuplicateBuild(structure, vertices, vertexOffset, indices, indexType,
                                    indexOffset, triangleCount, scratch, scratchOffset,
                                    tableOffset, opaque, Unwrap(m_Device)))
  {
    RDCERR("Invalid no-duplicate Metal triangle acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = TriangleDescriptor(
      Unwrap(vertices), vertexOffset, triangleCount, indices ? Unwrap(indices) : NULL,
      indexType, false, !opaque, opaque, indexOffset, tableOffset, false);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = indices ? 2 : 1;
  structure->m_LastTriangleCount = indices ? 0 : triangleCount;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(!indices) structure->m_LastVertices = GetResID(vertices);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildTriangleNoDuplicate);
    Serialise_buildTriangleNoDuplicate(ser, structure, vertices, vertexOffset, indices,
                                       indexType, indexOffset, triangleCount, scratch,
                                       scratchOffset, tableOffset, opaque);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(vertices));
    if(indices) record->AddParent(GetRecord(indices));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildTriangleNoDuplicate(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *, MTL::IndexType, NS::UInteger, NS::UInteger, WrappedMTLBuffer *,
    NS::UInteger, NS::UInteger, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildTriangleNoDuplicate(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *, MTL::IndexType, NS::UInteger, NS::UInteger, WrappedMTLBuffer *,
    NS::UInteger, NS::UInteger, bool);

static MTL::PrimitiveAccelerationStructureDescriptor *BoundingBoxDescriptor(
    MTL::Buffer *boxes, NS::UInteger count, NS::UInteger boxOffset = 0,
    NS::UInteger boxStride = 6 * sizeof(float), NS::UInteger tableOffset = 0,
    bool opaque = false, bool allowDuplicate = true, bool refittable = false)
{
  MTL::AccelerationStructureBoundingBoxGeometryDescriptor *box =
      MTL::AccelerationStructureBoundingBoxGeometryDescriptor::descriptor();
  box->setBoundingBoxBuffer(boxes);
  box->setBoundingBoxBufferOffset(boxOffset);
  box->setBoundingBoxStride(boxStride);
  box->setBoundingBoxCount(count);
  box->setIntersectionFunctionTableOffset(tableOffset);
  box->setOpaque(opaque);
  box->setAllowDuplicateIntersectionFunctionInvocation(allowDuplicate);
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      MTL::PrimitiveAccelerationStructureDescriptor::descriptor();
  if(refittable) descriptor->setUsage(MTL::AccelerationStructureUsageRefit);
  descriptor->setGeometryDescriptors(NS::Array::array(box));
  return descriptor;
}

static bool ValidBoundingBoxBuild(WrappedMTLAccelerationStructure *structure,
                                  WrappedMTLBuffer *boxes, NS::UInteger count,
                                  WrappedMTLBuffer *scratch, MTL::Device *device,
                                  NS::UInteger boxOffset = 0,
                                  NS::UInteger scratchOffset = 0,
                                  NS::UInteger boxStride = 6 * sizeof(float),
                                  NS::UInteger tableOffset = 0, bool opaque = false,
                                  bool allowDuplicate = true,
                                  bool refittable = false, bool refitOnly = false)
{
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     !boxes || boxes->m_Type != eResBuffer || !Unwrap(boxes) ||
     !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
     !count || count > 1000000 || tableOffset > 31 || boxOffset % 16 != 0 ||
     boxStride < 6 * sizeof(float) || boxStride > 1024 * 1024 || boxStride % 8 != 0 ||
     scratchOffset % 256 != 0 || boxOffset > Unwrap(boxes)->length() ||
     scratchOffset > Unwrap(scratch)->length() ||
     Unwrap(boxes)->length() - boxOffset < 6 * sizeof(float) ||
     count - 1 > (Unwrap(boxes)->length() - boxOffset - 6 * sizeof(float)) / boxStride)
    return false;
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      BoundingBoxDescriptor(Unwrap(boxes), count, boxOffset, boxStride, tableOffset,
                            opaque, allowDuplicate, refittable);
  MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize <= structure->m_Size &&
         (!refitOnly || sizes.refitScratchBufferSize) &&
         (refitOnly ? sizes.refitScratchBufferSize : sizes.buildScratchBufferSize) <=
             Unwrap(scratch)->length() - scratchOffset;
}

static void SetRefittableBoxMetadata(WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *boxes, NS::UInteger boxOffset, NS::UInteger boxStride,
    NS::UInteger boxCount, NS::UInteger tableOffset, bool opaque,
    bool allowDuplicate, ResourceId commandBuffer)
{
  structure->m_LastBuildKind = 6;
  structure->m_LastBoxes = GetResID(boxes);
  structure->m_LastBoxCount = boxCount;
  structure->m_LastBoxOffset = boxOffset;
  structure->m_LastBoxStride = boxStride;
  structure->m_LastBoxTableOffset = tableOffset;
  structure->m_LastBoxOpaque = opaque;
  structure->m_LastAllowDuplicateIntersectionFunctionInvocation = allowDuplicate;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = commandBuffer;
  structure->m_LastCompactedSizeBuffer = ResourceId();
  structure->m_LastCompactedWriteCommandBuffer = ResourceId();
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableBoundingBox(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *boxes, NS::UInteger boxOffset, NS::UInteger boxStride,
    NS::UInteger boxCount, NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, bool opaque, bool allowDuplicate)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(boxes).Important();
  SERIALISE_ELEMENT(boxOffset).Important();
  SERIALISE_ELEMENT(boxStride).Important();
  SERIALISE_ELEMENT(boxCount).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_ELEMENT(allowDuplicate).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
          boxOffset, scratchOffset, boxStride, tableOffset, opaque, allowDuplicate, true))
    {
      RDCERR("Invalid Metal refittable bounding-box build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
        Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset,
        opaque, allowDuplicate, true);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), scratchOffset);
    SetRefittableBoxMetadata(structure, boxes, boxOffset, boxStride, boxCount,
        tableOffset, opaque, allowDuplicate, GetResID(Encoder->m_CommandBuffer));
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Refittable Bounding Box Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildRefittableBoundingBox(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
    bool opaque, bool allowDuplicate)
{
  if(!ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
      boxOffset, scratchOffset, boxStride, tableOffset, opaque, allowDuplicate, true))
  {
    RDCERR("Invalid Metal refittable bounding-box build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
      Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset,
      opaque, allowDuplicate, true);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  SetRefittableBoxMetadata(structure, boxes, boxOffset, boxStride, boxCount,
      tableOffset, opaque, allowDuplicate, GetResID(m_CommandBuffer));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildRefittableBoundingBox);
    Serialise_buildRefittableBoundingBox(ser, structure, boxes, boxOffset, boxStride,
        boxCount, tableOffset, scratch, scratchOffset, opaque, allowDuplicate);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(boxes));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableBoundingBox(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger, bool, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildRefittableBoundingBox(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger, bool, bool);

static bool ValidBoundingBoxRefit(WrappedMTLAccelerationStructure *source,
    WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
    bool opaque, bool allowDuplicate, MTL::Device *device, ResourceId commandBuffer)
{
  if(!ValidBoundingBoxBuild(source, boxes, boxCount, scratch, device, boxOffset,
          scratchOffset, boxStride, tableOffset, opaque, allowDuplicate, true, true) ||
     !destination || destination->m_Type != eResAccelerationStructure ||
     !Unwrap(destination) ||
     (destination != source && destination->m_LastBuildKind != 0) ||
     source->m_LastBuildKind != 6 ||
     source->m_LastBoxCount != boxCount || source->m_LastBoxOffset != boxOffset ||
     source->m_LastBoxStride != boxStride ||
     source->m_LastBoxTableOffset != tableOffset || source->m_LastBoxOpaque != opaque ||
     source->m_LastAllowDuplicateIntersectionFunctionInvocation != allowDuplicate ||
     source->m_LastBuildCommandBuffer == ResourceId() ||
     source->m_LastBuildCommandBuffer == commandBuffer)
    return false;
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
      Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset,
      opaque, allowDuplicate, true);
  return device->accelerationStructureSizes(descriptor).accelerationStructureSize <=
         destination->m_Size;
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitBoundingBox(
    SerialiserType &ser, WrappedMTLAccelerationStructure *source,
    WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
    bool opaque, bool allowDuplicate)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(source).Important();
  SERIALISE_ELEMENT(destination).Important();
  SERIALISE_ELEMENT(boxes).Important();
  SERIALISE_ELEMENT(boxOffset).Important();
  SERIALISE_ELEMENT(boxStride).Important();
  SERIALISE_ELEMENT(boxCount).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_ELEMENT(allowDuplicate).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidBoundingBoxRefit(source, destination, boxes, boxOffset, boxStride,
           boxCount, tableOffset, scratch, scratchOffset, opaque, allowDuplicate,
           Unwrap(m_Device), GetResID(Encoder->m_CommandBuffer)))
    {
      RDCERR("Invalid Metal bounding-box refit");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
        Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset,
        opaque, allowDuplicate, true);
    Unwrap(Encoder)->refitAccelerationStructure(Unwrap(source), descriptor,
        Unwrap(destination), Unwrap(scratch), scratchOffset);
    SetRefittableBoxMetadata(destination, boxes, boxOffset, boxStride, boxCount,
        tableOffset, opaque, allowDuplicate, GetResID(Encoder->m_CommandBuffer));
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Refit Metal Bounding Box Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::refitBoundingBox(
    WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination,
    WrappedMTLBuffer *boxes, NS::UInteger boxOffset, NS::UInteger boxStride,
    NS::UInteger boxCount, NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset, bool opaque, bool allowDuplicate)
{
  if(!ValidBoundingBoxRefit(source, destination, boxes, boxOffset, boxStride,
      boxCount, tableOffset, scratch, scratchOffset, opaque, allowDuplicate,
      Unwrap(m_Device), GetResID(m_CommandBuffer)))
  {
    RDCERR("Invalid Metal bounding-box refit");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
      Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset,
      opaque, allowDuplicate, true);
  SERIALISE_TIME_CALL(Unwrap(this)->refitAccelerationStructure(
      Unwrap(source), descriptor, Unwrap(destination), Unwrap(scratch), scratchOffset));
  SetRefittableBoxMetadata(destination, boxes, boxOffset, boxStride, boxCount,
      tableOffset, opaque, allowDuplicate, GetResID(m_CommandBuffer));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_refitBoundingBox);
    Serialise_refitBoundingBox(ser, source, destination, boxes, boxOffset, boxStride,
        boxCount, tableOffset, scratch, scratchOffset, opaque, allowDuplicate);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(source));
    record->AddParent(GetRecord(destination));
    record->AddParent(GetRecord(boxes));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitBoundingBox(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, NS::UInteger, NS::UInteger, NS::UInteger,
    WrappedMTLBuffer *, NS::UInteger, bool, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_refitBoundingBox(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, NS::UInteger, NS::UInteger, NS::UInteger,
    WrappedMTLBuffer *, NS::UInteger, bool, bool);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBox(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxCount, WrappedMTLBuffer *scratch)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(boxes).Important();
  SERIALISE_ELEMENT(boxCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || !ValidBoundingBoxBuild(
           structure, boxes, boxCount, scratch, Unwrap(m_Device)))
    {
      RDCERR("Invalid Metal bounding-box acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
        BoundingBoxDescriptor(Unwrap(boxes), boxCount);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch), 0);
    structure->m_LastBuildKind = 3;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Bounding Box Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildBoundingBox(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxCount, WrappedMTLBuffer *scratch)
{
  if(!ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device)))
  {
    RDCERR("Invalid Metal bounding-box acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      BoundingBoxDescriptor(Unwrap(boxes), boxCount);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), 0));
  structure->m_LastBuildKind = 3;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBox);
    Serialise_buildBoundingBox(ser, structure, boxes, boxCount, scratch);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(boxes));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBox(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBox(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    WrappedMTLBuffer *);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxExtended(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(boxes).Important();
  SERIALISE_ELEMENT(boxOffset).Important();
  SERIALISE_ELEMENT(boxCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       (!boxOffset && !scratchOffset) ||
       !ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
                              boxOffset, scratchOffset))
    {
      RDCERR("Invalid extended Metal bounding-box acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
        BoundingBoxDescriptor(Unwrap(boxes), boxCount, boxOffset);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch),
                                                scratchOffset);
    structure->m_LastBuildKind = 3;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Bounding Box Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildBoundingBoxExtended(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxCount, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset)
{
  if((!boxOffset && !scratchOffset) ||
     !ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
                            boxOffset, scratchOffset))
  {
    RDCERR("Invalid extended Metal bounding-box acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      BoundingBoxDescriptor(Unwrap(boxes), boxCount, boxOffset);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = 3;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBoxExtended);
    Serialise_buildBoundingBoxExtended(ser, structure, boxes, boxOffset, boxCount, scratch,
                                       scratchOffset);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(boxes));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxExtended(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, WrappedMTLBuffer *, NS::UInteger);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxExtended(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, WrappedMTLBuffer *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxStrided(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(boxes).Important();
  SERIALISE_ELEMENT(boxOffset).Important();
  SERIALISE_ELEMENT(boxStride).Important();
  SERIALISE_ELEMENT(boxCount).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       boxStride == 6 * sizeof(float) ||
       !ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
                              boxOffset, scratchOffset, boxStride))
    {
      RDCERR("Invalid strided Metal bounding-box acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
        BoundingBoxDescriptor(Unwrap(boxes), boxCount, boxOffset, boxStride);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch),
                                                scratchOffset);
    structure->m_LastBuildKind = 3;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Bounding Box Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildBoundingBoxStrided(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset)
{
  if(boxStride == 6 * sizeof(float) ||
     !ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
                            boxOffset, scratchOffset, boxStride))
  {
    RDCERR("Invalid strided Metal bounding-box acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
      BoundingBoxDescriptor(Unwrap(boxes), boxCount, boxOffset, boxStride);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = 3;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBoxStrided);
    Serialise_buildBoundingBoxStrided(ser, structure, boxes, boxOffset, boxStride, boxCount,
                                      scratch, scratchOffset);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(boxes));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxStrided(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxStrided(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxTableOffset(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(boxes).Important();
  SERIALISE_ELEMENT(boxOffset).Important();
  SERIALISE_ELEMENT(boxStride).Important();
  SERIALISE_ELEMENT(boxCount).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       tableOffset == 0 ||
       !ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
                              boxOffset, scratchOffset, boxStride, tableOffset))
    {
      RDCERR("Invalid Metal bounding-box intersection table offset build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
        Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch),
                                                scratchOffset);
    structure->m_LastBuildKind = 3;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Bounding Box Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildBoundingBoxTableOffset(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset)
{
  if(!tableOffset || !ValidBoundingBoxBuild(structure, boxes, boxCount, scratch,
                                            Unwrap(m_Device), boxOffset, scratchOffset,
                                            boxStride, tableOffset))
  {
    RDCERR("Invalid Metal bounding-box intersection table offset build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
      Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = 3;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBoxTableOffset);
    Serialise_buildBoundingBoxTableOffset(ser, structure, boxes, boxOffset, boxStride,
                                         boxCount, tableOffset, scratch, scratchOffset);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(boxes));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxTableOffset(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxTableOffset(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxOpaque(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(boxes).Important();
  SERIALISE_ELEMENT(boxOffset).Important();
  SERIALISE_ELEMENT(boxStride).Important();
  SERIALISE_ELEMENT(boxCount).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
                              boxOffset, scratchOffset, boxStride, tableOffset, true))
    {
      RDCERR("Invalid opaque Metal bounding-box acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
        Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset, true);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch),
                                                scratchOffset);
    structure->m_LastBuildKind = 3;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Bounding Box Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildBoundingBoxOpaque(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset)
{
  if(!ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
                            boxOffset, scratchOffset, boxStride, tableOffset, true))
  {
    RDCERR("Invalid opaque Metal bounding-box acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
      Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset, true);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = 3;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBoxOpaque);
    Serialise_buildBoundingBoxOpaque(ser, structure, boxes, boxOffset, boxStride,
                                    boxCount, tableOffset, scratch, scratchOffset);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(boxes));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxOpaque(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxOpaque(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxNoDuplicate(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
    bool opaque)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(boxes).Important();
  SERIALISE_ELEMENT(boxOffset).Important();
  SERIALISE_ELEMENT(boxStride).Important();
  SERIALISE_ELEMENT(boxCount).Important();
  SERIALISE_ELEMENT(tableOffset).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_ELEMENT(opaque).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
                              boxOffset, scratchOffset, boxStride, tableOffset, opaque, false))
    {
      RDCERR("Invalid no-duplicate Metal bounding-box acceleration structure build");
      return false;
    }
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
        Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset, opaque, false);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor, Unwrap(scratch),
                                                scratchOffset);
    structure->m_LastBuildKind = 3;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Bounding Box Acceleration Structure";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildBoundingBoxNoDuplicate(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
    NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
    NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
    bool opaque)
{
  if(!ValidBoundingBoxBuild(structure, boxes, boxCount, scratch, Unwrap(m_Device),
                            boxOffset, scratchOffset, boxStride, tableOffset, opaque, false))
  {
    RDCERR("Invalid no-duplicate Metal bounding-box acceleration structure build");
    return;
  }
  MTL::PrimitiveAccelerationStructureDescriptor *descriptor = BoundingBoxDescriptor(
      Unwrap(boxes), boxCount, boxOffset, boxStride, tableOffset, opaque, false);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = 3;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBoxNoDuplicate);
    Serialise_buildBoundingBoxNoDuplicate(ser, structure, boxes, boxOffset, boxStride,
                                         boxCount, tableOffset, scratch, scratchOffset, opaque);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(boxes));
    record->AddParent(GetRecord(scratch));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxNoDuplicate(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger, bool);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildBoundingBoxNoDuplicate(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    NS::UInteger, NS::UInteger, NS::UInteger, WrappedMTLBuffer *, NS::UInteger, bool);

static bool ValidAccelerationStructureCopy(WrappedMTLAccelerationStructure *source,
                                           WrappedMTLAccelerationStructure *destination)
{
  return source && destination && source != destination &&
         source->m_Type == eResAccelerationStructure &&
         destination->m_Type == eResAccelerationStructure && Unwrap(source) &&
         Unwrap(destination) && source->m_Size && destination->m_Size >= source->m_Size &&
         source->m_LastBuildKind && source->m_LastBuildCommandBuffer != ResourceId();
}

static void CopyAccelerationStructureBuildMetadata(WrappedMTLAccelerationStructure *source,
                                                   WrappedMTLAccelerationStructure *destination,
                                                   ResourceId commandBuffer)
{
  destination->m_LastBuildKind = source->m_LastBuildKind;
  destination->m_LastAllowDuplicateIntersectionFunctionInvocation =
      source->m_LastAllowDuplicateIntersectionFunctionInvocation;
  destination->m_LastVertexStride = source->m_LastVertexStride;
  destination->m_LastVertexFormat = source->m_LastVertexFormat;
  destination->m_LastTriangleCount = source->m_LastTriangleCount;
  destination->m_LastBuildCommandBuffer = commandBuffer;
  destination->m_LastVertices = source->m_LastVertices;
  destination->m_LastIndices = source->m_LastIndices;
  destination->m_LastIndexedVertexOffset = source->m_LastIndexedVertexOffset;
  destination->m_LastIndexedIndexOffset = source->m_LastIndexedIndexOffset;
  destination->m_LastIndexType = source->m_LastIndexType;
  destination->m_LastIndexedTableOffset = source->m_LastIndexedTableOffset;
  destination->m_LastIndexedOpaque = source->m_LastIndexedOpaque;
  destination->m_LastBoxes = source->m_LastBoxes;
  destination->m_LastBoxCount = source->m_LastBoxCount;
  destination->m_LastBoxOffset = source->m_LastBoxOffset;
  destination->m_LastBoxStride = source->m_LastBoxStride;
  destination->m_LastBoxTableOffset = source->m_LastBoxTableOffset;
  destination->m_LastBoxOpaque = source->m_LastBoxOpaque;
  destination->m_LastInstanceChild = source->m_LastInstanceChild;
  destination->m_LastInstanceBuffer = source->m_LastInstanceBuffer;
  destination->m_LastCompactedSizeBuffer = ResourceId();
  destination->m_LastCompactedSizeOffset = 0;
  destination->m_LastCompactedSizeType = MTL::DataTypeNone;
  destination->m_LastCompactedWriteCommandBuffer = ResourceId();
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_copyAccelerationStructure(
    SerialiserType &ser, WrappedMTLAccelerationStructure *source,
    WrappedMTLAccelerationStructure *destination)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(source).Important();
  SERIALISE_ELEMENT(destination).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || !ValidAccelerationStructureCopy(source, destination))
    {
      RDCERR("Invalid Metal acceleration structure copy");
      return false;
    }
    Unwrap(Encoder)->copyAccelerationStructure(Unwrap(source), Unwrap(destination));
    CopyAccelerationStructureBuildMetadata(source, destination,
                                           GetResID(Encoder->m_CommandBuffer));
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Copy Metal Acceleration Structure";
      action.flags = ActionFlags::Copy;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::copyAccelerationStructure(
    WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination)
{
  if(!ValidAccelerationStructureCopy(source, destination))
  {
    RDCERR("Invalid Metal acceleration structure copy");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->copyAccelerationStructure(Unwrap(source), Unwrap(destination)));
  CopyAccelerationStructureBuildMetadata(source, destination, GetResID(m_CommandBuffer));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_copyAccelerationStructure);
    Serialise_copyAccelerationStructure(ser, source, destination);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(source));
    record->AddParent(GetRecord(destination));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_copyAccelerationStructure(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_copyAccelerationStructure(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *);

static bool ReadCompactedSize(WrappedMTLBuffer *buffer, NS::UInteger offset,
                              MTL::DataType type, uint64_t &size)
{
  const uint64_t bytes = type == MTL::DataTypeUInt ? 4 : type == MTL::DataTypeULong ? 8 : 0;
  if(!buffer || buffer->m_Type != eResBuffer || !Unwrap(buffer) || !bytes ||
     Unwrap(buffer)->storageMode() != MTL::StorageModeShared ||
     offset > Unwrap(buffer)->length() || bytes > Unwrap(buffer)->length() - offset ||
     !Unwrap(buffer)->contents())
    return false;
  size = 0;
  memcpy(&size, (const byte *)Unwrap(buffer)->contents() + offset, bytes);
  return size > 0;
}

static bool ValidCompactedSource(WrappedMTLAccelerationStructure *source)
{
  if(!source) return false;
  if(source->m_LastBuildKind == 2 || source->m_LastBuildKind == 3 ||
     source->m_LastBuildKind == 6 || source->m_LastBuildKind == 8)
    return source->m_LastTriangleCount == 0;
  if(source->m_LastBuildKind == 1 || source->m_LastBuildKind == 4 ||
     source->m_LastBuildKind == 7)
    return source->m_LastTriangleCount >= 1 && source->m_LastTriangleCount <= 1000000;
  if(source->m_LastBuildKind == 5)
    return source->m_LastInstanceChild != ResourceId() &&
           source->m_LastInstanceBuffer != ResourceId();
  return false;
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_copyAndCompactAccelerationStructure(
    SerialiserType &ser, WrappedMTLAccelerationStructure *source,
    WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *sizeBuffer,
    NS::UInteger sizeOffset, MTL::DataType sizeType, uint64_t expectedSize)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(source).Important();
  SERIALISE_ELEMENT(destination).Important();
  SERIALISE_ELEMENT(sizeBuffer).Important();
  SERIALISE_ELEMENT(sizeOffset).Important();
  uint64_t sizeDataType = (uint64_t)sizeType;
  SERIALISE_ELEMENT(sizeDataType).Important();
  SERIALISE_ELEMENT(expectedSize).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    sizeType = (MTL::DataType)sizeDataType;
    uint64_t actualSize = 0;
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || !source || !destination || source == destination ||
       source->m_Type != eResAccelerationStructure ||
       destination->m_Type != eResAccelerationStructure || !Unwrap(source) ||
       !Unwrap(destination) || !ReadCompactedSize(sizeBuffer, sizeOffset, sizeType, actualSize) ||
       source->m_LastCompactedSizeBuffer != GetResID(sizeBuffer) ||
       source->m_LastCompactedSizeOffset != sizeOffset ||
       source->m_LastCompactedSizeType != sizeType ||
       !ValidCompactedSource(source) ||
       source->m_LastBuildCommandBuffer != source->m_LastCompactedWriteCommandBuffer ||
       source->m_LastCompactedWriteCommandBuffer == GetResID(Encoder->m_CommandBuffer) ||
       actualSize != expectedSize || expectedSize > destination->m_Size ||
       (source->m_LastBuildCommandBuffer == GetResID(source) &&
        source->m_LastInitialCompactedSize > destination->m_Size) ||
       destination->m_Size >= source->m_Size)
    {
      RDCERR("Invalid Metal acceleration structure compact copy or GPU size readback");
      return false;
    }
    Unwrap(Encoder)->copyAndCompactAccelerationStructure(Unwrap(source), Unwrap(destination));
    CopyAccelerationStructureBuildMetadata(source, destination,
                                           GetResID(Encoder->m_CommandBuffer));
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Copy and Compact Metal Acceleration Structure";
      action.flags = ActionFlags::Copy;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::copyAndCompactAccelerationStructure(
    WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination)
{
  WrappedMTLBuffer *sizeBuffer = source ?
      (WrappedMTLBuffer *)GetResourceManager()->GetResource(source->m_LastCompactedSizeBuffer) : NULL;
  uint64_t requiredSize = 0;
  bool sizeKnown = source && ReadCompactedSize(sizeBuffer, source->m_LastCompactedSizeOffset,
      source->m_LastCompactedSizeType, requiredSize);
  if(!sizeKnown && IsCaptureMode(m_State) && source && sizeBuffer && Unwrap(sizeBuffer) &&
     Unwrap(sizeBuffer)->storageMode() == MTL::StorageModePrivate &&
     source->m_CapturedCompactedSizeReadback && source->m_CapturedCompactedSizeSubmission &&
     source->m_CapturedCompactedSizeSubmission->status() == MTL::CommandBufferStatusCompleted &&
     !source->m_CapturedCompactedSizeSubmission->error())
  {
    const uint64_t bytes = source->m_LastCompactedSizeType == MTL::DataTypeUInt ? 4 :
        source->m_LastCompactedSizeType == MTL::DataTypeULong ? 8 : 0;
    auto snapshot = source->m_CapturedCompactedSizeReadback.get();
    if(bytes && snapshot->length() == bytes && snapshot->contents())
    { memcpy(&requiredSize, snapshot->contents(), bytes); sizeKnown = requiredSize > 0; }
  }
  if(!source || !destination || source == destination ||
     source->m_Type != eResAccelerationStructure ||
     destination->m_Type != eResAccelerationStructure || !Unwrap(source) ||
     !Unwrap(destination) || !sizeKnown ||
     !ValidCompactedSource(source) ||
     source->m_LastBuildCommandBuffer != source->m_CompactedSizeBuildCommandBuffer ||
     source->m_LastCompactedWriteCommandBuffer == GetResID(m_CommandBuffer) ||
     requiredSize > destination->m_Size || destination->m_Size >= source->m_Size)
  {
    // Capture validation must not drop a valid application's native command.
    // An unsupported record stays explicitly invalid for replay (expectedSize=0).
    if(IsCaptureMode(m_State) && source && destination && source != destination &&
       source->m_Type == eResAccelerationStructure && destination->m_Type == eResAccelerationStructure &&
       Unwrap(source) && Unwrap(destination) && m_CommandBuffer)
    {
      SERIALISE_TIME_CALL(Unwrap(this)->copyAndCompactAccelerationStructure(Unwrap(source), Unwrap(destination)));
      RDCERR("Metal compact copy forwarded natively; missing typed source/completed size evidence, replay unsupported");
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_copyAndCompactAccelerationStructure);
      Serialise_copyAndCompactAccelerationStructure(ser, source, destination, sizeBuffer,
          source->m_LastCompactedSizeOffset, source->m_LastCompactedSizeType, 0);
      auto record = GetRecord(m_CommandBuffer);
      record->AddParent(GetRecord(source)); record->AddParent(GetRecord(destination));
      if(sizeBuffer) record->AddParent(GetRecord(sizeBuffer));
      record->AddChunk(scope.Get());
    }
    else RDCERR("Invalid Metal acceleration structure compact copy or missing completed GPU size");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->copyAndCompactAccelerationStructure(Unwrap(source),
                                                                        Unwrap(destination)));
  CopyAccelerationStructureBuildMetadata(source, destination, GetResID(m_CommandBuffer));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_copyAndCompactAccelerationStructure);
    Serialise_copyAndCompactAccelerationStructure(ser, source, destination, sizeBuffer,
        source->m_LastCompactedSizeOffset, source->m_LastCompactedSizeType, requiredSize);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(source));
    record->AddParent(GetRecord(destination));
    record->AddParent(GetRecord(sizeBuffer));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_copyAndCompactAccelerationStructure(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, MTL::DataType, uint64_t);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_copyAndCompactAccelerationStructure(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLAccelerationStructure *,
    WrappedMTLBuffer *, NS::UInteger, MTL::DataType, uint64_t);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_writeCompactedSize(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *buffer,
    NS::UInteger offset, MTL::DataType type)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  uint64_t sizeDataType = (uint64_t)type;
  SERIALISE_ELEMENT(sizeDataType).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    type = (MTL::DataType)sizeDataType;
    uint64_t bytes = type == MTL::DataTypeUInt ? 4 : type == MTL::DataTypeULong ? 8 : 0;
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
       !buffer || buffer->m_Type != eResBuffer || !Unwrap(buffer) || !bytes ||
       offset > Unwrap(buffer)->length() || bytes > Unwrap(buffer)->length() - offset)
    {
      RDCERR("Invalid Metal compacted acceleration structure size write");
      return false;
    }
    Unwrap(Encoder)->writeCompactedAccelerationStructureSize(Unwrap(structure), Unwrap(buffer),
                                                             offset, type);
    structure->m_LastCompactedSizeBuffer = GetResID(buffer);
    structure->m_LastCompactedSizeOffset = offset;
    structure->m_LastCompactedSizeType = type;
    structure->m_LastCompactedWriteCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Write Metal Compacted Acceleration Structure Size";
      action.flags = ActionFlags::Copy;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::writeCompactedSize(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *buffer,
    NS::UInteger offset, MTL::DataType type)
{
  uint64_t bytes = type == MTL::DataTypeUInt ? 4 : type == MTL::DataTypeULong ? 8 : 0;
  if(!structure || !buffer || !Unwrap(structure) || !Unwrap(buffer) || !bytes ||
     offset > Unwrap(buffer)->length() || bytes > Unwrap(buffer)->length() - offset)
  {
    RDCERR("Invalid Metal compacted acceleration structure size write");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->writeCompactedAccelerationStructureSize(
      Unwrap(structure), Unwrap(buffer), offset, type));
  TrackInitialBufferWrite(buffer);
  structure->m_LastCompactedSizeBuffer = GetResID(buffer);
  structure->m_LastCompactedSizeOffset = offset;
  structure->m_LastCompactedSizeType = type;
  structure->m_LastCompactedWriteCommandBuffer = GetResID(m_CommandBuffer);
  structure->m_CompactedSizeBuildCommandBuffer = structure->m_LastBuildCommandBuffer;
  structure->m_CapturedCompactedSizeReadback.reset();
  structure->m_CapturedCompactedSizeSubmission.reset();
  if(IsCaptureMode(m_State) && Unwrap(buffer)->storageMode() == MTL::StorageModePrivate)
  {
    bool found = false;
    for(ResourceId id : m_PrivateCompactedSizeQueries) found |= id == GetResID(structure);
    if(!found) m_PrivateCompactedSizeQueries.push_back(GetResID(structure));
  }
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(
        MetalChunk::MTLAccelerationStructureCommandEncoder_writeCompactedAccelerationStructureSize);
    Serialise_writeCompactedSize(ser, structure, buffer, offset, type);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure));
    record->AddParent(GetRecord(buffer));
    record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_PartialWrite);
    GetRecord(structure)->AddParent(GetRecord(buffer));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_writeCompactedSize(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    MTL::DataType);
template bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_writeCompactedSize(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, WrappedMTLBuffer *, NS::UInteger,
    MTL::DataType);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_endEncoding(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder))
      return false;
    Unwrap(Encoder)->endEncoding();
    m_Device->SetReplayAccelerationStructureCommandEncoder(NULL);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "End Metal Acceleration Structure Pass";
      action.flags = ActionFlags::PassBoundary | ActionFlags::EndPass;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::endEncoding()
{
  SERIALISE_TIME_CALL(Unwrap(this)->endEncoding());
  SnapshotInitialInputsAtEnd();
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_endEncoding);
    Serialise_endEncoding(ser);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLAccelerationStructureCommandEncoder, void, endEncoding);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_insertDebugSignpost(
    SerialiserType &ser, NS::String *string)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(string).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (!Encoder || Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
      !Unwrap(Encoder) || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder)))
    return false;
  if(IsLoading(m_State))
  {
    AddEvent();
    m_Device->GetReplay()->AddDebugGroup(string, ActionFlags::SetMarker);
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::insertDebugSignpost(NS::String *string)
{
  SERIALISE_TIME_CALL(Unwrap(this)->insertDebugSignpost(string));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_insertDebugSignpost);
    Serialise_insertDebugSignpost(ser, string);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLAccelerationStructureCommandEncoder, void,
                                insertDebugSignpost, NS::String *string);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_pushDebugGroup(
    SerialiserType &ser, NS::String *string)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(string).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (!Encoder || Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
      !Unwrap(Encoder) || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder)))
    return false;
  if(IsLoading(m_State))
  {
    AddEvent();
    m_Device->GetReplay()->AddDebugGroup(string, ActionFlags::PushMarker);
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::pushDebugGroup(NS::String *string)
{
  SERIALISE_TIME_CALL(Unwrap(this)->pushDebugGroup(string));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_pushDebugGroup);
    Serialise_pushDebugGroup(ser, string);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLAccelerationStructureCommandEncoder, void,
                                pushDebugGroup, NS::String *string);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_popDebugGroup(
    SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (!Encoder || Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
      !Unwrap(Encoder) || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder)))
    return false;
  if(IsLoading(m_State))
  {
    AddEvent();
    m_Device->GetReplay()->AddDebugGroup(NULL, ActionFlags::PopMarker);
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::popDebugGroup()
{
  SERIALISE_TIME_CALL(Unwrap(this)->popDebugGroup());
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_popDebugGroup);
    Serialise_popDebugGroup(ser);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLAccelerationStructureCommandEncoder, void,
                                popDebugGroup);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_updateFence(
    SerialiserType &ser, WrappedMTLFence *fence)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(fence).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       !ValidMetalFence(fence))
    {
      RDCERR("Invalid Metal acceleration structure updateFence resource or dependency");
      return false;
    }
    Unwrap(Encoder)->updateFence(Unwrap(fence));
    fence->Updated(m_Device->GetReplayEpoch(), GetResID(Encoder));
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::updateFence(WrappedMTLFence *fence)
{
  if(!ValidMetalFence(fence))
  {
    RDCERR("Invalid Metal acceleration structure fence");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->updateFence(Unwrap(fence)));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_updateFence);
    Serialise_updateFence(ser, fence);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLAccelerationStructureCommandEncoder, void,
                                updateFence, WrappedMTLFence *fence);

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_waitForFence(
    SerialiserType &ser, WrappedMTLFence *fence)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(fence).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder->m_Type != eResAccelerationStructureCommandEncoder ||
       !Unwrap(Encoder) || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       !ValidMetalFence(fence) || !fence->CanWait(m_Device->GetReplayEpoch(), GetResID(Encoder)))
    {
      RDCERR("Invalid Metal acceleration structure waitForFence resource or dependency");
      return false;
    }
    Unwrap(Encoder)->waitForFence(Unwrap(fence));
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::waitForFence(WrappedMTLFence *fence)
{
  if(!ValidMetalFence(fence))
  {
    RDCERR("Invalid Metal acceleration structure fence");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->waitForFence(Unwrap(fence)));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_waitForFence);
    Serialise_waitForFence(ser, fence);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLAccelerationStructureCommandEncoder, void,
                                waitForFence, WrappedMTLFence *fence);

static bool ValidMultiIndexedBuild(WrappedMTLAccelerationStructure *structure,
    WrappedMTLBuffer *vertices, WrappedMTLBuffer *indices, const rdcarray<uint64_t> &parameters,
    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, MTL::Device *device)
{
  if(!structure || structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
     !vertices || vertices->m_Type != eResBuffer || !Unwrap(vertices) ||
     !indices || indices->m_Type != eResBuffer || !Unwrap(indices) ||
     !scratch || scratch->m_Type != eResBuffer || !Unwrap(scratch) ||
     Unwrap(structure)->device() != device || Unwrap(vertices)->device() != device ||
     Unwrap(indices)->device() != device || Unwrap(scratch)->device() != device ||
     scratchOffset % 256 || scratchOffset > Unwrap(scratch)->length() ||
     !ValidMetalASMultiIndexedParameters(parameters, Unwrap(vertices)->length(),
                                         Unwrap(indices)->length())) return false;
  auto descriptor = MetalASMultiIndexedDescriptor(Unwrap(vertices), Unwrap(indices), parameters);
  auto sizes = device->accelerationStructureSizes(descriptor);
  return sizes.accelerationStructureSize && sizes.buildScratchBufferSize &&
      sizes.accelerationStructureSize <= structure->m_Size &&
      sizes.buildScratchBufferSize <= Unwrap(scratch)->length() - scratchOffset;
}

template <typename SerialiserType>
bool WrappedMTLAccelerationStructureCommandEncoder::Serialise_buildMultiIndexed(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    WrappedMTLBuffer *indices, rdcarray<uint64_t> parameters, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(vertices).Important();
  SERIALISE_ELEMENT(indices).Important();
  SERIALISE_ELEMENT(parameters).Important();
  SERIALISE_ELEMENT(scratch).Important();
  SERIALISE_ELEMENT(scratchOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Encoder || Encoder != m_Device->GetReplayAccelerationStructureCommandEncoder(Encoder) ||
       Encoder->m_Type != eResAccelerationStructureCommandEncoder || !Unwrap(Encoder) ||
       !ValidMultiIndexedBuild(structure, vertices, indices, parameters, scratch,
                               scratchOffset, Unwrap(m_Device)))
    {
      RDCERR("Invalid Metal multi-geometry indexed acceleration structure build");
      return false;
    }
    auto descriptor = MetalASMultiIndexedDescriptor(Unwrap(vertices), Unwrap(indices), parameters);
    Unwrap(Encoder)->buildAccelerationStructure(Unwrap(structure), descriptor,
                                                Unwrap(scratch), scratchOffset);
    structure->m_LastBuildKind = 8;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = GetResID(Encoder->m_CommandBuffer);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "Build Metal Acceleration Structure (Multiple Indexed Geometries)";
      action.flags = ActionFlags::BuildAccStruct;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLAccelerationStructureCommandEncoder::buildMultiIndexed(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    WrappedMTLBuffer *indices, rdcarray<uint64_t> parameters, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset)
{
  if(!ValidMultiIndexedBuild(structure, vertices, indices, parameters, scratch,
                             scratchOffset, Unwrap(m_Device)))
  {
    RDCERR("Invalid Metal multi-geometry indexed acceleration structure build");
    return;
  }
  auto descriptor = MetalASMultiIndexedDescriptor(Unwrap(vertices), Unwrap(indices), parameters);
  SERIALISE_TIME_CALL(Unwrap(this)->buildAccelerationStructure(
      Unwrap(structure), descriptor, Unwrap(scratch), scratchOffset));
  structure->m_LastBuildKind = 8;
  structure->m_LastTriangleCount = 0;
  structure->m_LastBuildCommandBuffer = GetResID(m_CommandBuffer);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLAccelerationStructureCommandEncoder_buildMultiIndexed);
    Serialise_buildMultiIndexed(ser, structure, vertices, indices, parameters, scratch, scratchOffset);
    auto record = GetRecord(m_CommandBuffer);
    record->AddParent(GetRecord(structure)); record->AddParent(GetRecord(vertices));
    record->AddParent(GetRecord(indices)); record->AddParent(GetRecord(scratch));
    record->MarkResourceFrameReferenced(GetResID(scratch), eFrameRef_PartialWrite);
    record->AddChunk(scope.Get());
  }
}
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLAccelerationStructureCommandEncoder, void, buildMultiIndexed,
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    WrappedMTLBuffer *indices, rdcarray<uint64_t> parameters, WrappedMTLBuffer *scratch,
    NS::UInteger scratchOffset);
