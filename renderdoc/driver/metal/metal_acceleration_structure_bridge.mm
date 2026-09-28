#include "metal_acceleration_structure.h"
#include "metal_types_bridge.h"

@implementation ObjCBridgeMTLAccelerationStructure
- (id<MTLAccelerationStructure>)real
{
  return id<MTLAccelerationStructure>(Unwrap(GetWrapped(self)));
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
  RDCFATAL("Metal acceleration-structure selector %s not captured",
           sel_getName([invocation selector]));
}
- (id<MTLDevice>)device { return id<MTLDevice>(GetWrapped(self)->GetDevice()); }
- (NSUInteger)size { return GetWrapped(self)->m_Size; }
- (NSString *)label { return self.real.label; }
- (void)setLabel:(NSString *)value { self.real.label = value; }
- (MTLCPUCacheMode)cpuCacheMode { return self.real.cpuCacheMode; }
- (MTLStorageMode)storageMode { return self.real.storageMode; }
- (MTLHazardTrackingMode)hazardTrackingMode { return self.real.hazardTrackingMode; }
- (MTLResourceOptions)resourceOptions { return self.real.resourceOptions; }
- (id<MTLHeap>)heap { return nil; }
- (NSUInteger)heapOffset { return 0; }
- (NSUInteger)allocatedSize { return self.real.allocatedSize; }
- (BOOL)isAliasable { return self.real.isAliasable; }
@end
