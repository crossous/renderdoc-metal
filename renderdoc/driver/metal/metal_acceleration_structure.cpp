#include "metal_acceleration_structure.h"
#include "metal_device.h"

WrappedMTLAccelerationStructure::WrappedMTLAccelerationStructure(
    MTL::AccelerationStructure *real, ResourceId id, WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}
