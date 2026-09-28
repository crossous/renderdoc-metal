// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_fence.h"
#include "metal_types_bridge.h"

@implementation ObjCBridgeMTLFence
- (id<MTLFence>)real
{
  return id<MTLFence>(Unwrap(GetWrapped(self)));
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
  id real = self.real;
  return [real methodSignatureForSelector:selector];
}
- (void)forwardInvocation:(NSInvocation *)invocation
{
  if([self.real respondsToSelector:[invocation selector]])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}
- (NSString *)label { return self.real.label; }
- (void)setLabel:(NSString *)value { self.real.label = value; }
- (id<MTLDevice>)device { return id<MTLDevice>(GetWrapped(self)->GetDevice()); }
@end
