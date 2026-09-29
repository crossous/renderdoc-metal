// SPDX-License-Identifier: MIT
#include "metal_parallel_render_command_encoder.h"
#include "metal_types_bridge.h"

@implementation ObjCBridgeMTLParallelRenderCommandEncoder
- (id<MTLParallelRenderCommandEncoder>)real
{
  return id<MTLParallelRenderCommandEncoder>(Unwrap(GetWrapped(self)));
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

- (id<MTLDevice>)device
{
  return id<MTLDevice>(GetWrapped(self)->GetDevice());
}

- (nullable NSString *)label { return self.real.label; }
- (void)setLabel:(NSString *)label { self.real.label = label; }
- (void)endEncoding { GetWrapped(self)->endEncoding(); }
- (void)pushDebugGroup:(NSString *)string
{
  GetWrapped(self)->debugLabel((NS::String *)string, 0);
}
- (void)popDebugGroup { GetWrapped(self)->debugLabel(NULL, 1); }
- (void)insertDebugSignpost:(NSString *)string
{
  GetWrapped(self)->debugLabel((NS::String *)string, 2);
}

- (id<MTLRenderCommandEncoder>)renderCommandEncoder
{
  return id<MTLRenderCommandEncoder>(GetWrapped(self)->renderCommandEncoder());
}

- (void)setColorStoreAction:(MTLStoreAction)storeAction atIndex:(NSUInteger)index
{
  GetWrapped(self)->setStoreAction((MTL::StoreAction)storeAction, index, 0);
}
- (void)setDepthStoreAction:(MTLStoreAction)storeAction
{
  GetWrapped(self)->setStoreAction((MTL::StoreAction)storeAction, 0, 1);
}
- (void)setStencilStoreAction:(MTLStoreAction)storeAction
{
  GetWrapped(self)->setStoreAction((MTL::StoreAction)storeAction, 0, 2);
}
- (void)setColorStoreActionOptions:(MTLStoreActionOptions)options atIndex:(NSUInteger)index
{
  GetWrapped(self)->setStoreActionOptions((MTL::StoreActionOptions)options, index, 0);
}
- (void)setDepthStoreActionOptions:(MTLStoreActionOptions)options
{
  GetWrapped(self)->setStoreActionOptions((MTL::StoreActionOptions)options, 0, 1);
}
- (void)setStencilStoreActionOptions:(MTLStoreActionOptions)options
{
  GetWrapped(self)->setStoreActionOptions((MTL::StoreActionOptions)options, 0, 2);
}
@end
