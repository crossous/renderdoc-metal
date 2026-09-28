// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_counter_sample_buffer.h"
#include "metal_types_bridge.h"

@implementation ObjCBridgeMTLCounterSampleBuffer
- (id<MTLCounterSampleBuffer>)real
{
  return id<MTLCounterSampleBuffer>(Unwrap(GetWrapped(self)));
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
- (NSUInteger)sampleCount { return self.real.sampleCount; }
- (NSData *)resolveCounterRange:(NSRange)range { return [self.real resolveCounterRange:range]; }
@end
