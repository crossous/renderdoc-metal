#pragma once

#include "metal_common.h"

class WrappedMTLAccelerationStructure : public WrappedMTLObject
{
public:
  WrappedMTLAccelerationStructure(MTL::AccelerationStructure *real, ResourceId id,
                                  WrappedMTLDevice *device);
  enum { TypeEnum = eResAccelerationStructure };
  NS::UInteger m_Size = 0;
  ResourceId m_LastCompactedSizeBuffer;
  NS::UInteger m_LastCompactedSizeOffset = 0;
  MTL::DataType m_LastCompactedSizeType = MTL::DataTypeNone;
  ResourceId m_LastCompactedWriteCommandBuffer;
  uint32_t m_LastBuildKind = 0; // 1=triangle, 2=indexed, 3=box, 4=refittable triangle, 5=instance, 6=refittable box, 7=refittable indexed triangle
  bool m_LastAllowDuplicateIntersectionFunctionInvocation = true;
  NS::UInteger m_LastVertexStride = 3 * sizeof(float);
  MTL::AttributeFormat m_LastVertexFormat = MTL::AttributeFormatFloat3;
  NS::UInteger m_LastTriangleCount = 0;
  ResourceId m_LastBuildCommandBuffer;
  ResourceId m_LastVertices;
  ResourceId m_LastIndices;
  NS::UInteger m_LastIndexedVertexOffset = 0;
  NS::UInteger m_LastIndexedIndexOffset = 0;
  MTL::IndexType m_LastIndexType = MTL::IndexTypeUInt16;
  NS::UInteger m_LastIndexedTableOffset = 0;
  bool m_LastIndexedOpaque = false;
  ResourceId m_LastBoxes;
  NS::UInteger m_LastBoxCount = 0;
  NS::UInteger m_LastBoxOffset = 0;
  NS::UInteger m_LastBoxStride = 0;
  NS::UInteger m_LastBoxTableOffset = 0;
  bool m_LastBoxOpaque = false;
  ResourceId m_LastInstanceChild;
  ResourceId m_LastInstanceBuffer;
};
