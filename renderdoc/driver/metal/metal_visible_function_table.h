#pragma once

#include "metal_common.h"

class WrappedMTLFunctionHandle : public WrappedMTLObject
{
public:
  WrappedMTLFunctionHandle(MTL::FunctionHandle *real, ResourceId id, WrappedMTLDevice *device);
  enum { TypeEnum = eResFunctionHandle };
  WrappedMTLObject *m_Pipeline = NULL;
  WrappedMTLFunction *m_Function = NULL;
  MTL::RenderStages m_Stage = MTL::RenderStageFragment;
};

class WrappedMTLVisibleFunctionTable : public WrappedMTLObject
{
public:
  WrappedMTLVisibleFunctionTable(MTL::VisibleFunctionTable *real, ResourceId id,
                                 WrappedMTLDevice *device);
  enum { TypeEnum = eResVisibleFunctionTable };
  WrappedMTLObject *m_Pipeline = NULL;
  MTL::RenderStages m_Stage = MTL::RenderStageFragment;
  uint32_t m_FunctionCount = 0;
  void setFunction(WrappedMTLFunctionHandle *function, uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setFunction(SerialiserType &ser, WrappedMTLFunctionHandle *function,
                             uint32_t index);
};

class WrappedMTLIntersectionFunctionTable : public WrappedMTLObject
{
public:
  WrappedMTLIntersectionFunctionTable(MTL::IntersectionFunctionTable *real, ResourceId id,
                                      WrappedMTLDevice *device);
  enum { TypeEnum = eResIntersectionFunctionTable };
  WrappedMTLObject *m_Pipeline = NULL;
  MTL::RenderStages m_Stage = MTL::RenderStageFragment;
  uint32_t m_FunctionCount = 0;
  void setFunction(WrappedMTLFunctionHandle *function, uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setFunction(SerialiserType &ser, WrappedMTLFunctionHandle *function,
                             uint32_t index);
  void setBuffer(WrappedMTLBuffer *buffer, NS::UInteger offset, uint32_t index);
  void setBuffers(rdcarray<WrappedMTLBuffer *> buffers, rdcarray<NS::UInteger> offsets,
                  NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setBuffer(SerialiserType &ser, WrappedMTLBuffer *buffer,
                           NS::UInteger offset, uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setBuffers(SerialiserType &ser, rdcarray<WrappedMTLBuffer *> buffers,
                            rdcarray<NS::UInteger> offsets, NS::Range range);
  void setVisibleFunctionTable(WrappedMTLVisibleFunctionTable *table, uint32_t index);
  void setVisibleFunctionTables(rdcarray<WrappedMTLVisibleFunctionTable *> tables,
                                NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setVisibleFunctionTable(SerialiserType &ser,
                                        WrappedMTLVisibleFunctionTable *table, uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setVisibleFunctionTables(SerialiserType &ser,
      rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range);
  void setOpaqueTriangleFunction(MTL::IntersectionFunctionSignature signature, uint32_t index);
  void setOpaqueTriangleFunctions(MTL::IntersectionFunctionSignature signature, NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setOpaqueTriangleFunction(SerialiserType &ser,
      MTL::IntersectionFunctionSignature signature, uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setOpaqueTriangleFunctions(SerialiserType &ser,
      MTL::IntersectionFunctionSignature signature, NS::Range range);
};
