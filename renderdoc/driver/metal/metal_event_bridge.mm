// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_event.h"
#include "metal_types_bridge.h"
#include <objc/runtime.h>
static char sharedEventSourceKey;
void MetalAssociateSharedEventHandle(MTLSharedEventHandle *handle, id<MTLSharedEvent> source)
{
  if(handle && source)
    objc_setAssociatedObject(handle, &sharedEventSourceKey, source, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
id<MTLSharedEvent> MetalSharedEventHandleSource(MTLSharedEventHandle *handle)
{
  return handle ? objc_getAssociatedObject(handle, &sharedEventSourceKey) : nil;
}
@implementation ObjCBridgeMTLEvent
- (id<MTLEvent>)real { return id<MTLEvent>(Unwrap(GetWrapped(self))); }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-missing-super-calls"
- (void)dealloc { DeallocateObjCBridge(GetWrapped(self)); }
#pragma clang diagnostic pop
- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector
{
  id real = self.real;
  return [real methodSignatureForSelector:selector];
}
- (void)forwardInvocation:(NSInvocation *)invocation
{
  if([self.real respondsToSelector:[invocation selector]]) [invocation invokeWithTarget:self.real];
  else [super forwardInvocation:invocation];
}
- (NSString *)label { return self.real.label; }
- (void)setLabel:(NSString *)value { self.real.label = value; }
- (id<MTLDevice>)device { return id<MTLDevice>(GetWrapped(self)->GetDevice()); }
@end

@implementation ObjCBridgeMTLSharedEvent
- (uint64_t)signaledValue
{
  return ((id<MTLSharedEvent>)Unwrap(GetWrapped(self))).signaledValue;
}
- (void)setSignaledValue:(uint64_t)value
{
  GetWrapped(self)->SetHostSignaledValue(value);
}
- (MTLSharedEventHandle *)newSharedEventHandle
{
  MTLSharedEventHandle *handle = [(id<MTLSharedEvent>)Unwrap(GetWrapped(self)) newSharedEventHandle];
  MetalAssociateSharedEventHandle(handle, self);
  return handle;
}
- (void)notifyListener:(MTLSharedEventListener *)listener
               atValue:(uint64_t)value
                 block:(MTLSharedEventNotificationBlock)block
{
  [(id<MTLSharedEvent>)Unwrap(GetWrapped(self)) notifyListener:listener atValue:value block:block];
}
@end
