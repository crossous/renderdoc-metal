// SPDX-License-Identifier: MIT
#pragma once

#include "metal_common.h"
#include "metal_device.h"
#include "metal_resources.h"

class WrappedMTLParallelRenderCommandEncoder : public WrappedMTLObject
{
public:
  WrappedMTLParallelRenderCommandEncoder(MTL::ParallelRenderCommandEncoder *real, ResourceId id,
                                        WrappedMTLDevice *device);
  void SetCommandBuffer(WrappedMTLCommandBuffer *commandBuffer)
  {
    m_CommandBuffer = commandBuffer;
    m_DeferredStoreActions = 0;
  }
  WrappedMTLCommandBuffer *GetCommandBuffer() const { return m_CommandBuffer; }
  void SetDeferredStoreActions(uint16_t mask) { m_DeferredStoreActions = mask; }
  void ResolveDeferredStoreActions();

  WrappedMTLRenderCommandEncoder *renderCommandEncoder();
  template <typename SerialiserType>
  bool Serialise_renderCommandEncoder(SerialiserType &ser,
                                     WrappedMTLRenderCommandEncoder *encoder);
  void endEncoding();
  template <typename SerialiserType>
  bool Serialise_endEncoding(SerialiserType &ser);
  void setStoreAction(MTL::StoreAction action, NS::UInteger index, uint32_t variant);
  template <typename SerialiserType>
  bool Serialise_setStoreAction(SerialiserType &ser, MTL::StoreAction action,
                               NS::UInteger index, uint32_t variant);
  void setStoreActionOptions(MTL::StoreActionOptions options, NS::UInteger index,
                             uint32_t variant);
  template <typename SerialiserType>
  bool Serialise_setStoreActionOptions(SerialiserType &ser, MTL::StoreActionOptions options,
                                      NS::UInteger index, uint32_t variant);
  void debugLabel(NS::String *label, uint32_t variant);
  template <typename SerialiserType>
  bool Serialise_debugLabel(SerialiserType &ser, NS::String *label, uint32_t variant);

  enum { TypeEnum = eResParallelRenderCommandEncoder };

private:
  WrappedMTLCommandBuffer *m_CommandBuffer = NULL;
  uint16_t m_DeferredStoreActions = 0;
};
