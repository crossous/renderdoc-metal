#include "metal_visible_function_table.h"
#include "metal_types_bridge.h"

@implementation ObjCBridgeMTLFunctionHandle
- (id<MTLFunctionHandle>)real { return id<MTLFunctionHandle>(Unwrap(GetWrapped(self))); }
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
- (MTLFunctionType)functionType { return self.real.functionType; }
- (NSString *)name { return self.real.name; }
@end

@implementation ObjCBridgeMTLIntersectionFunctionTable
- (id<MTLIntersectionFunctionTable>)real
{
  return id<MTLIntersectionFunctionTable>(Unwrap(GetWrapped(self)));
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
  if([self.real respondsToSelector:[invocation selector]])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}
- (id<MTLDevice>)device { return id<MTLDevice>(GetWrapped(self)->GetDevice()); }
- (void)setFunction:(id<MTLFunctionHandle>)function atIndex:(NSUInteger)index
{
  if(index >= GetWrapped(self)->m_FunctionCount || !function ||
     ![function isKindOfClass:[ObjCBridgeMTLFunctionHandle class]])
  {
    RDCERR("Unsupported or unwrapped Metal intersection function handle");
    return;
  }
  GetWrapped(self)->setFunction(function ? GetWrapped((ObjCBridgeMTLFunctionHandle *)function) : NULL,
                                (uint32_t)index);
}
- (void)setFunctions:(const id<MTLFunctionHandle> [])functions withRange:(NSRange)range
{
  if(range.length > GetWrapped(self)->m_FunctionCount ||
     range.location > GetWrapped(self)->m_FunctionCount - range.length ||
     (range.length && !functions))
  {
    RDCERR("Invalid Metal intersection-function-table range");
    return;
  }
  for(NSUInteger i = 0; i < range.length; ++i)
    if(!functions[i] || ![functions[i] isKindOfClass:[ObjCBridgeMTLFunctionHandle class]])
    {
      RDCERR("Unsupported or unwrapped Metal intersection function handle range");
      return;
    }
  for(NSUInteger i = 0; i < range.length; ++i)
    [self setFunction:functions[i] atIndex:range.location + i];
}
- (void)setBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
  if(index >= 31 || (buffer && ![buffer isKindOfClass:[ObjCBridgeMTLBuffer class]]))
  {
    RDCERR("Invalid or unwrapped Metal intersection function buffer");
    return;
  }
  GetWrapped(self)->setBuffer(buffer ? GetWrapped((ObjCBridgeMTLBuffer *)buffer) : NULL,
                              offset, (uint32_t)index);
}
- (void)setBuffers:(const id<MTLBuffer> [])buffers offsets:(const NSUInteger [])offsets
        withRange:(NSRange)range
{
  if(range.location >= 31 || !range.length || range.length > 31 - range.location ||
     !buffers || !offsets)
  {
    RDCERR("Invalid Metal intersection function buffer range");
    return;
  }
  rdcarray<WrappedMTLBuffer *> wrapped;
  rdcarray<NS::UInteger> wrappedOffsets;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    if(buffers[i] && ![buffers[i] isKindOfClass:[ObjCBridgeMTLBuffer class]])
    {
      RDCERR("Unwrapped Metal intersection function buffer range");
      return;
    }
    wrapped.push_back(buffers[i] ? GetWrapped((ObjCBridgeMTLBuffer *)buffers[i]) : NULL);
    wrappedOffsets.push_back(offsets[i]);
  }
  GetWrapped(self)->setBuffers(wrapped, wrappedOffsets,
                                NS::Range::Make(range.location, range.length));
}
- (void)setVisibleFunctionTable:(id<MTLVisibleFunctionTable>)table atBufferIndex:(NSUInteger)index
{
  if(index >= 31 || (table && ![table isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]]))
  {
    RDCERR("Invalid or unwrapped nested Metal visible function table");
    return;
  }
  GetWrapped(self)->setVisibleFunctionTable(
      table ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)table) : NULL, (uint32_t)index);
}
- (void)setVisibleFunctionTables:(const id<MTLVisibleFunctionTable> [])tables
               withBufferRange:(NSRange)range
{
  if(range.location >= 31 || !range.length || range.length > 31 - range.location || !tables)
  {
    RDCERR("Invalid nested Metal visible function table range");
    return;
  }
  rdcarray<WrappedMTLVisibleFunctionTable *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    if(tables[i] && ![tables[i] isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
    {
      RDCERR("Unwrapped nested Metal visible function table range");
      return;
    }
    wrapped.push_back(tables[i] ?
        GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)tables[i]) : NULL);
  }
  GetWrapped(self)->setVisibleFunctionTables(wrapped,
                                              NS::Range::Make(range.location, range.length));
}
- (void)setOpaqueTriangleIntersectionFunctionWithSignature:(MTLIntersectionFunctionSignature)signature
                                                    atIndex:(NSUInteger)index
{
  if(index >= GetWrapped(self)->m_FunctionCount)
  {
    RDCERR("Invalid Metal opaque-triangle intersection table index");
    return;
  }
  GetWrapped(self)->setOpaqueTriangleFunction((MTL::IntersectionFunctionSignature)signature,
                                               (uint32_t)index);
}
- (void)setOpaqueTriangleIntersectionFunctionWithSignature:(MTLIntersectionFunctionSignature)signature
                                                  withRange:(NSRange)range
{
  GetWrapped(self)->setOpaqueTriangleFunctions((MTL::IntersectionFunctionSignature)signature,
                                                NS::Range::Make(range.location, range.length));
}
- (void)setOpaqueCurveIntersectionFunctionWithSignature:(MTLIntersectionFunctionSignature)signature
                                                 atIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
}
- (void)setOpaqueCurveIntersectionFunctionWithSignature:(MTLIntersectionFunctionSignature)signature
                                               withRange:(NSRange)range
{
  METAL_NOT_HOOKED();
}
@end

@implementation ObjCBridgeMTLVisibleFunctionTable
- (id<MTLVisibleFunctionTable>)real
{
  return id<MTLVisibleFunctionTable>(Unwrap(GetWrapped(self)));
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
  if([self.real respondsToSelector:[invocation selector]])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}
- (id<MTLDevice>)device { return id<MTLDevice>(GetWrapped(self)->GetDevice()); }
- (void)setFunction:(id<MTLFunctionHandle>)function atIndex:(NSUInteger)index
{
  if(index >= GetWrapped(self)->m_FunctionCount ||
     (function && ![function isKindOfClass:[ObjCBridgeMTLFunctionHandle class]]))
  {
    RDCERR("Cannot capture unwrapped Metal function handle in visible function table");
    return;
  }
  GetWrapped(self)->setFunction(function ? GetWrapped((ObjCBridgeMTLFunctionHandle *)function) : NULL,
                                (uint32_t)index);
}
- (void)setFunctions:(const id<MTLFunctionHandle> [])functions withRange:(NSRange)range
{
  if(range.length > GetWrapped(self)->m_FunctionCount ||
     range.location > GetWrapped(self)->m_FunctionCount - range.length ||
     (range.length && !functions))
  {
    RDCERR("Invalid Metal visible-function-table function range");
    return;
  }
  for(NSUInteger i = 0; i < range.length; ++i)
    if(functions[i] && ![functions[i] isKindOfClass:[ObjCBridgeMTLFunctionHandle class]])
    {
      RDCERR("Cannot capture unwrapped Metal function handle in visible function table");
      return;
    }
  for(NSUInteger i = 0; i < range.length; ++i)
    [self setFunction:functions[i] atIndex:range.location + i];
}
@end
