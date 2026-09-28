// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#pragma once
#include "metal_common.h"

class WrappedMTLEvent : public WrappedMTLObject
{
public:
  WrappedMTLEvent(MTL::Event *real, ResourceId id, WrappedMTLDevice *device);
  enum { TypeEnum = eResEvent };
  bool PrepareReplay(uint64_t epoch);
  void MarkUnsupportedHostMutation();
  void SetHostSignaledValue(uint64_t value);
  bool SetInitialHostValue(uint64_t value);
  void MarkGPUEventEncoded() { m_GPUEventEncoded = true; AliasRoot()->m_GPUEventEncoded = true; }
  bool IsSharedEvent() const { return m_IsSharedEvent; }
  void SetAliasSource(WrappedMTLEvent *source) { m_AliasSource = source; }
  WrappedMTLEvent *AliasRoot() { return m_AliasSource ? m_AliasSource->AliasRoot() : this; }
  uint64_t lastSignal = 0;
private:
  bool m_IsSharedEvent = false;
  bool m_GPUEventEncoded = false;
  uint64_t m_InitialHostValue = 0;
  uint64_t m_Epoch = 0;
  WrappedMTLEvent *m_AliasSource = NULL;
};
