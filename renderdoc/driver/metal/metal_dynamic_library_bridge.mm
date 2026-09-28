// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_dynamic_library.h"
#include "metal_types_bridge.h"

bool MetalDynamicLibraryIsWrapped(MTL::DynamicLibrary *library)
{
  return [(id)library isKindOfClass:[ObjCBridgeMTLDynamicLibrary class]];
}

@implementation ObjCBridgeMTLDynamicLibrary
- (id<MTLDynamicLibrary>)real { return id<MTLDynamicLibrary>(Unwrap(GetWrapped(self))); }
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
  if([self.real respondsToSelector:[invocation selector]])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}
- (id<MTLDevice>)device { return id<MTLDevice>(GetWrapped(self)->GetDevice()); }
- (NSString *)label { return self.real.label; }
- (void)setLabel:(NSString *)label { self.real.label = label; }
- (NSString *)installName { return self.real.installName; }
- (BOOL)serializeToURL:(NSURL *)url error:(NSError **)error
{
  return [self.real serializeToURL:url error:error];
}
@end
