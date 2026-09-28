// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_rate_map.h"
#include "metal_buffer.h"
#include "metal_types_bridge.h"

@implementation ObjCBridgeMTLRasterizationRateMap
- (id<MTLRasterizationRateMap>)real
{
  return id<MTLRasterizationRateMap>(Unwrap(GetWrapped(self)));
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-missing-super-calls"
- (void)dealloc
{
  DeallocateObjCBridge(GetWrapped(self));
}
#pragma clang diagnostic pop
- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector
{
  return [(id)self.real methodSignatureForSelector:selector];
}
- (void)forwardInvocation:(NSInvocation *)invocation
{
  if([self.real respondsToSelector:[invocation selector]])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}
- (id<MTLDevice>)device { return id<MTLDevice>(GetWrapped(self)->GetDevice()); }
- (NSString *)label { return self.real.label; }
- (MTLSize)screenSize { return self.real.screenSize; }
- (MTLSize)physicalGranularity { return self.real.physicalGranularity; }
- (NSUInteger)layerCount { return self.real.layerCount; }
- (MTLSizeAndAlign)parameterBufferSizeAndAlign { return self.real.parameterBufferSizeAndAlign; }
- (void)copyParameterDataToBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset
{
  GetWrapped(self)->copyParameterDataToBuffer(GetWrapped(buffer), offset);
}
- (MTLSize)physicalSizeForLayer:(NSUInteger)layerIndex
{
  return [self.real physicalSizeForLayer:layerIndex];
}
- (MTLCoordinate2D)mapScreenToPhysicalCoordinates:(MTLCoordinate2D)coordinates
                                          forLayer:(NSUInteger)layerIndex
{
  return [self.real mapScreenToPhysicalCoordinates:coordinates forLayer:layerIndex];
}
- (MTLCoordinate2D)mapPhysicalToScreenCoordinates:(MTLCoordinate2D)coordinates
                                          forLayer:(NSUInteger)layerIndex
{
  return [self.real mapPhysicalToScreenCoordinates:coordinates forLayer:layerIndex];
}
@end
