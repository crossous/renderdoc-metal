#pragma once

#include "metal_common.h"

class WrappedMTLAccelerationStructureCommandEncoder : public WrappedMTLObject
{
public:
  WrappedMTLAccelerationStructureCommandEncoder(MTL::AccelerationStructureCommandEncoder *real,
                                                ResourceId id, WrappedMTLDevice *device);
  enum { TypeEnum = eResAccelerationStructureCommandEncoder };
  void SetCommandBuffer(WrappedMTLCommandBuffer *buffer) { m_CommandBuffer = buffer; }
  WrappedMTLCommandBuffer *GetCommandBuffer() const { return m_CommandBuffer; }
  void buildFormattedTriangle(WrappedMTLAccelerationStructure *structure,
      WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
      MTL::AttributeFormat vertexFormat, NS::UInteger triangleCount,
      WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, NS::UInteger tableOffset,
      bool opaque, bool allowDuplicate);
  template <typename SerialiserType>
  bool Serialise_buildFormattedTriangle(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
      NS::UInteger vertexOffset, NS::UInteger vertexStride,
      MTL::AttributeFormat vertexFormat, NS::UInteger triangleCount,
      WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, NS::UInteger tableOffset,
      bool opaque, bool allowDuplicate);
  void buildIndexedFormattedTriangle(WrappedMTLAccelerationStructure *structure,
      WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
      MTL::AttributeFormat vertexFormat, WrappedMTLBuffer *indices,
      MTL::IndexType indexType, NS::UInteger indexOffset, NS::UInteger triangleCount,
      WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, NS::UInteger tableOffset,
      bool opaque, bool allowDuplicate);
  template <typename SerialiserType>
  bool Serialise_buildIndexedFormattedTriangle(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
      NS::UInteger vertexOffset, NS::UInteger vertexStride,
      MTL::AttributeFormat vertexFormat, WrappedMTLBuffer *indices,
      MTL::IndexType indexType, NS::UInteger indexOffset, NS::UInteger triangleCount,
      WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, NS::UInteger tableOffset,
      bool opaque, bool allowDuplicate);
  void buildRefittableFormattedTriangle(WrappedMTLAccelerationStructure *structure,
      WrappedMTLBuffer *vertices, NS::UInteger vertexStride,
      MTL::AttributeFormat vertexFormat, NS::UInteger triangleCount,
      WrappedMTLBuffer *scratch, bool allowDuplicate);
  template <typename SerialiserType>
  bool Serialise_buildRefittableFormattedTriangle(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
      NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
      NS::UInteger triangleCount, WrappedMTLBuffer *scratch, bool allowDuplicate);
  void refitFormattedTriangle(WrappedMTLAccelerationStructure *source,
      WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *vertices,
      NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
      NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
      NS::UInteger scratchOffset, bool allowDuplicate);
  template <typename SerialiserType>
  bool Serialise_refitFormattedTriangle(SerialiserType &ser,
      WrappedMTLAccelerationStructure *source,
      WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *vertices,
      NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
      NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
      NS::UInteger scratchOffset, bool allowDuplicate);
  void buildTriangle(WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
                     NS::UInteger vertexOffset, NS::UInteger triangleCount,
                     WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
                     bool refittable = false, bool nonOpaque = false,
                     bool opaque = false);
  template <typename SerialiserType>
  bool Serialise_buildTriangle(SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
                              WrappedMTLBuffer *vertices, NS::UInteger vertexOffset,
                              NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
                              NS::UInteger scratchOffset, bool refittable = false,
                              bool nonOpaque = false, bool opaque = false);
  void refitTriangle(WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
                     NS::UInteger triangleCount, WrappedMTLBuffer *scratch);
  template <typename SerialiserType>
  bool Serialise_refitTriangle(SerialiserType &ser,
                              WrappedMTLAccelerationStructure *structure,
                              WrappedMTLBuffer *vertices, NS::UInteger triangleCount,
                              WrappedMTLBuffer *scratch);
  void buildRefittableTriangleNoDuplicate(WrappedMTLAccelerationStructure *structure,
      WrappedMTLBuffer *vertices, NS::UInteger triangleCount, WrappedMTLBuffer *scratch);
  template <typename SerialiserType>
  bool Serialise_buildRefittableTriangleNoDuplicate(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
      NS::UInteger triangleCount, WrappedMTLBuffer *scratch);
  void refitTriangleNoDuplicate(WrappedMTLAccelerationStructure *source,
      WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *vertices,
      NS::UInteger triangleCount, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset);
  template <typename SerialiserType>
  bool Serialise_refitTriangleNoDuplicate(SerialiserType &ser,
      WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination,
      WrappedMTLBuffer *vertices, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
      NS::UInteger scratchOffset);
  void refitTriangleExtended(WrappedMTLAccelerationStructure *source,
                             WrappedMTLAccelerationStructure *destination,
                             WrappedMTLBuffer *vertices, NS::UInteger triangleCount,
                             WrappedMTLBuffer *scratch, NS::UInteger scratchOffset);
  template <typename SerialiserType>
  bool Serialise_refitTriangleExtended(SerialiserType &ser,
                                      WrappedMTLAccelerationStructure *source,
                                      WrappedMTLAccelerationStructure *destination,
                                      WrappedMTLBuffer *vertices, NS::UInteger triangleCount,
                                      WrappedMTLBuffer *scratch, NS::UInteger scratchOffset);
  void buildInstance(WrappedMTLAccelerationStructure *structure,
                     WrappedMTLAccelerationStructure *child, WrappedMTLBuffer *instances,
                     WrappedMTLBuffer *scratch);
  template <typename SerialiserType>
  bool Serialise_buildInstance(SerialiserType &ser,
                              WrappedMTLAccelerationStructure *structure,
                              WrappedMTLAccelerationStructure *child,
                              WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch,
                              rdcarray<byte> descriptorBytes);
  void buildInstances(WrappedMTLAccelerationStructure *structure,
                      WrappedMTLAccelerationStructure *child, WrappedMTLBuffer *instances,
                      WrappedMTLBuffer *scratch, NS::UInteger count);
  template <typename SerialiserType>
  bool Serialise_buildInstances(SerialiserType &ser,
                               WrappedMTLAccelerationStructure *structure,
                               WrappedMTLAccelerationStructure *child,
                               WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch,
                               NS::UInteger count, rdcarray<byte> descriptorBytes);
  void buildDistinctInstances(WrappedMTLAccelerationStructure *structure,
                              WrappedMTLAccelerationStructure *child0,
                              WrappedMTLAccelerationStructure *child1,
                              WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch);
  template <typename SerialiserType>
  bool Serialise_buildDistinctInstances(SerialiserType &ser,
                                       WrappedMTLAccelerationStructure *structure,
                                       WrappedMTLAccelerationStructure *child0,
                                       WrappedMTLAccelerationStructure *child1,
                                       WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch,
                                       rdcarray<byte> descriptorBytes);
  void buildMultipleDistinctInstances(
      WrappedMTLAccelerationStructure *structure,
      rdcarray<WrappedMTLAccelerationStructure *> children,
      WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch);
  template <typename SerialiserType>
  bool Serialise_buildMultipleDistinctInstances(
      SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
      rdcarray<WrappedMTLAccelerationStructure *> children,
      WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch,
      rdcarray<byte> descriptorBytes);
  void buildRepeatedDistinctInstances(
      WrappedMTLAccelerationStructure *structure,
      rdcarray<WrappedMTLAccelerationStructure *> children,
      WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch, NS::UInteger count);
  template <typename SerialiserType>
  bool Serialise_buildRepeatedDistinctInstances(
      SerialiserType &ser, WrappedMTLAccelerationStructure *structure,
      rdcarray<WrappedMTLAccelerationStructure *> children,
      WrappedMTLBuffer *instances, WrappedMTLBuffer *scratch, NS::UInteger count,
      rdcarray<byte> descriptorBytes);
  void buildIndexedTriangle(WrappedMTLAccelerationStructure *structure,
                            WrappedMTLBuffer *vertices, WrappedMTLBuffer *indices,
                            MTL::IndexType indexType, NS::UInteger triangleCount,
                            WrappedMTLBuffer *scratch, bool opaque = false);
  template <typename SerialiserType>
  bool Serialise_buildIndexedTriangle(SerialiserType &ser,
                                      WrappedMTLAccelerationStructure *structure,
                                      WrappedMTLBuffer *vertices, WrappedMTLBuffer *indices,
                                      MTL::IndexType indexType, NS::UInteger triangleCount,
                                      WrappedMTLBuffer *scratch, bool opaque = false);
  void buildIndexedTriangleOffset(WrappedMTLAccelerationStructure *structure,
                                  WrappedMTLBuffer *vertices, WrappedMTLBuffer *indices,
                                  MTL::IndexType indexType, NS::UInteger indexOffset,
                                  NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
                                  bool opaque);
  template <typename SerialiserType>
  bool Serialise_buildIndexedTriangleOffset(SerialiserType &ser,
                                            WrappedMTLAccelerationStructure *structure,
                                            WrappedMTLBuffer *vertices, WrappedMTLBuffer *indices,
                                            MTL::IndexType indexType, NS::UInteger indexOffset,
                                            NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
                                            bool opaque);
  void buildIndexedTriangleExtended(WrappedMTLAccelerationStructure *structure,
                                    WrappedMTLBuffer *vertices, NS::UInteger vertexOffset,
                                    WrappedMTLBuffer *indices, MTL::IndexType indexType,
                                    NS::UInteger indexOffset, NS::UInteger triangleCount,
                                    WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
                                    bool opaque);
  template <typename SerialiserType>
  bool Serialise_buildIndexedTriangleExtended(SerialiserType &ser,
                                              WrappedMTLAccelerationStructure *structure,
                                              WrappedMTLBuffer *vertices, NS::UInteger vertexOffset,
                                              WrappedMTLBuffer *indices, MTL::IndexType indexType,
                                              NS::UInteger indexOffset, NS::UInteger triangleCount,
                                              WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
                                              bool opaque);
  void buildTriangleTableOffset(WrappedMTLAccelerationStructure *structure,
                                WrappedMTLBuffer *vertices, NS::UInteger vertexOffset,
                                WrappedMTLBuffer *indices, MTL::IndexType indexType,
                                NS::UInteger indexOffset, NS::UInteger triangleCount,
                                WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
                                NS::UInteger tableOffset, bool opaque);
  template <typename SerialiserType>
  bool Serialise_buildTriangleTableOffset(SerialiserType &ser,
                                          WrappedMTLAccelerationStructure *structure,
                                          WrappedMTLBuffer *vertices, NS::UInteger vertexOffset,
                                          WrappedMTLBuffer *indices, MTL::IndexType indexType,
                                          NS::UInteger indexOffset, NS::UInteger triangleCount,
                                          WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
                                          NS::UInteger tableOffset, bool opaque);
  void buildBoundingBox(WrappedMTLAccelerationStructure *structure,
                        WrappedMTLBuffer *boxes, NS::UInteger boxCount,
                        WrappedMTLBuffer *scratch);
  template <typename SerialiserType>
  bool Serialise_buildBoundingBox(SerialiserType &ser,
                                  WrappedMTLAccelerationStructure *structure,
                                  WrappedMTLBuffer *boxes, NS::UInteger boxCount,
                                  WrappedMTLBuffer *scratch);
  void buildRefittableBoundingBox(WrappedMTLAccelerationStructure *structure,
      WrappedMTLBuffer *boxes, NS::UInteger boxOffset, NS::UInteger boxStride,
      NS::UInteger boxCount, NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
      NS::UInteger scratchOffset, bool opaque, bool allowDuplicate);
  template <typename SerialiserType>
  bool Serialise_buildRefittableBoundingBox(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
      NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
      NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
      NS::UInteger scratchOffset, bool opaque, bool allowDuplicate);
  void refitBoundingBox(WrappedMTLAccelerationStructure *source,
      WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *boxes,
      NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
      NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
      NS::UInteger scratchOffset, bool opaque, bool allowDuplicate);
  template <typename SerialiserType>
  bool Serialise_refitBoundingBox(SerialiserType &ser,
      WrappedMTLAccelerationStructure *source,
      WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *boxes,
      NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
      NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
      NS::UInteger scratchOffset, bool opaque, bool allowDuplicate);
  void buildRefittableIndexedTriangle(WrappedMTLAccelerationStructure *structure,
      WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
      MTL::AttributeFormat vertexFormat, WrappedMTLBuffer *indices, MTL::IndexType indexType,
      NS::UInteger indexOffset, NS::UInteger triangleCount, NS::UInteger tableOffset,
      WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, bool opaque,
      bool allowDuplicate);
  template <typename SerialiserType>
  bool Serialise_buildRefittableIndexedTriangle(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
      NS::UInteger vertexOffset, NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
      WrappedMTLBuffer *indices, MTL::IndexType indexType, NS::UInteger indexOffset,
      NS::UInteger triangleCount, NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
      NS::UInteger scratchOffset, bool opaque, bool allowDuplicate);
  void refitIndexedTriangle(WrappedMTLAccelerationStructure *source,
      WrappedMTLAccelerationStructure *destination, WrappedMTLBuffer *vertices,
      NS::UInteger vertexOffset, NS::UInteger vertexStride, MTL::AttributeFormat vertexFormat,
      WrappedMTLBuffer *indices, MTL::IndexType indexType, NS::UInteger indexOffset,
      NS::UInteger triangleCount, NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
      NS::UInteger scratchOffset, bool opaque, bool allowDuplicate);
  template <typename SerialiserType>
  bool Serialise_refitIndexedTriangle(SerialiserType &ser,
      WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination,
      WrappedMTLBuffer *vertices, NS::UInteger vertexOffset, NS::UInteger vertexStride,
      MTL::AttributeFormat vertexFormat, WrappedMTLBuffer *indices, MTL::IndexType indexType,
      NS::UInteger indexOffset, NS::UInteger triangleCount, NS::UInteger tableOffset,
      WrappedMTLBuffer *scratch, NS::UInteger scratchOffset, bool opaque, bool allowDuplicate);
  void buildBoundingBoxExtended(WrappedMTLAccelerationStructure *structure,
                                WrappedMTLBuffer *boxes, NS::UInteger boxOffset,
                                NS::UInteger boxCount, WrappedMTLBuffer *scratch,
                                NS::UInteger scratchOffset);
  template <typename SerialiserType>
  bool Serialise_buildBoundingBoxExtended(SerialiserType &ser,
                                          WrappedMTLAccelerationStructure *structure,
                                          WrappedMTLBuffer *boxes, NS::UInteger boxOffset,
                                          NS::UInteger boxCount, WrappedMTLBuffer *scratch,
                                          NS::UInteger scratchOffset);
  void buildBoundingBoxStrided(WrappedMTLAccelerationStructure *structure,
                               WrappedMTLBuffer *boxes, NS::UInteger boxOffset,
                               NS::UInteger boxStride, NS::UInteger boxCount,
                               WrappedMTLBuffer *scratch, NS::UInteger scratchOffset);
  template <typename SerialiserType>
  bool Serialise_buildBoundingBoxStrided(SerialiserType &ser,
                                         WrappedMTLAccelerationStructure *structure,
                                         WrappedMTLBuffer *boxes, NS::UInteger boxOffset,
                                         NS::UInteger boxStride, NS::UInteger boxCount,
                                         WrappedMTLBuffer *scratch, NS::UInteger scratchOffset);
  void buildBoundingBoxTableOffset(WrappedMTLAccelerationStructure *structure,
                                   WrappedMTLBuffer *boxes, NS::UInteger boxOffset,
                                   NS::UInteger boxStride, NS::UInteger boxCount,
                                   NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
                                   NS::UInteger scratchOffset);
  template <typename SerialiserType>
  bool Serialise_buildBoundingBoxTableOffset(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
      NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
      NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset);
  void buildBoundingBoxOpaque(WrappedMTLAccelerationStructure *structure,
                              WrappedMTLBuffer *boxes, NS::UInteger boxOffset,
                              NS::UInteger boxStride, NS::UInteger boxCount,
                              NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
                              NS::UInteger scratchOffset);
  template <typename SerialiserType>
  bool Serialise_buildBoundingBoxOpaque(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
      NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
      NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset);
  void buildBoundingBoxNoDuplicate(WrappedMTLAccelerationStructure *structure,
                                   WrappedMTLBuffer *boxes, NS::UInteger boxOffset,
                                   NS::UInteger boxStride, NS::UInteger boxCount,
                                   NS::UInteger tableOffset, WrappedMTLBuffer *scratch,
                                   NS::UInteger scratchOffset, bool opaque);
  template <typename SerialiserType>
  bool Serialise_buildBoundingBoxNoDuplicate(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *boxes,
      NS::UInteger boxOffset, NS::UInteger boxStride, NS::UInteger boxCount,
      NS::UInteger tableOffset, WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
      bool opaque);
  void buildTriangleNoDuplicate(WrappedMTLAccelerationStructure *structure,
                                WrappedMTLBuffer *vertices, NS::UInteger vertexOffset,
                                WrappedMTLBuffer *indices, MTL::IndexType indexType,
                                NS::UInteger indexOffset, NS::UInteger triangleCount,
                                WrappedMTLBuffer *scratch, NS::UInteger scratchOffset,
                                NS::UInteger tableOffset, bool opaque);
  template <typename SerialiserType>
  bool Serialise_buildTriangleNoDuplicate(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
      NS::UInteger vertexOffset, WrappedMTLBuffer *indices, MTL::IndexType indexType,
      NS::UInteger indexOffset, NS::UInteger triangleCount, WrappedMTLBuffer *scratch,
      NS::UInteger scratchOffset, NS::UInteger tableOffset, bool opaque);
  void copyAccelerationStructure(WrappedMTLAccelerationStructure *source,
                                 WrappedMTLAccelerationStructure *destination);
  template <typename SerialiserType>
  bool Serialise_copyAccelerationStructure(SerialiserType &ser,
      WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination);
  void copyAndCompactAccelerationStructure(WrappedMTLAccelerationStructure *source,
                                           WrappedMTLAccelerationStructure *destination);
  template <typename SerialiserType>
  bool Serialise_copyAndCompactAccelerationStructure(SerialiserType &ser,
      WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination,
      WrappedMTLBuffer *sizeBuffer, NS::UInteger sizeOffset, MTL::DataType sizeType,
      uint64_t expectedSize);
  void writeCompactedSize(WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *buffer,
                          NS::UInteger offset, MTL::DataType type);
  template <typename SerialiserType>
  bool Serialise_writeCompactedSize(SerialiserType &ser,
                                   WrappedMTLAccelerationStructure *structure,
                                   WrappedMTLBuffer *buffer, NS::UInteger offset,
                                   MTL::DataType type);
  DECLARE_FUNCTION_SERIALISED(void, endEncoding);

private:
  WrappedMTLCommandBuffer *m_CommandBuffer = NULL;
};
