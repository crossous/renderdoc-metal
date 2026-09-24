/******************************************************************************
 * The MIT License (MIT)
 * Copyright (c) 2026 Baldur Karlsson
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "metal_indirect_command_buffer.h"
#include "metal_types_bridge.h"

@implementation ObjCBridgeMTLIndirectCommandBuffer
- (id<MTLIndirectCommandBuffer>)real
{
  return id<MTLIndirectCommandBuffer>(Unwrap(GetWrapped(self)));
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
  if([self.real respondsToSelector:invocation.selector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}
- (id<MTLDevice>)device
{
  return id<MTLDevice>(GetWrapped(self)->GetDevice());
}
- (NSUInteger)size
{
  return self.real.size;
}
- (id<MTLIndirectRenderCommand>)indirectRenderCommandAtIndex:(NSUInteger)index
{
  return id<MTLIndirectRenderCommand>(GetWrapped(self)->indirectRenderCommand(index));
}
- (void)resetWithRange:(NSRange)range
{
  GetWrapped(self)->reset(NS::Range::Make(range.location, range.length));
}
@end

@implementation ObjCBridgeMTLIndirectRenderCommand
- (id<MTLIndirectRenderCommand>)real
{
  return id<MTLIndirectRenderCommand>(Unwrap(GetWrapped(self)));
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
  if([self.real respondsToSelector:invocation.selector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}
- (void)setRenderPipelineState:(id<MTLRenderPipelineState>)pipeline
{
  GetWrapped(self)->setRenderPipelineState(GetWrapped(pipeline));
}
- (void)setVertexBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)slot
{
  GetWrapped(self)->setVertexBuffer(GetWrapped(buffer), offset, slot);
}
- (void)drawPrimitives:(MTLPrimitiveType)primitive
           vertexStart:(NSUInteger)vertexStart
           vertexCount:(NSUInteger)vertexCount
         instanceCount:(NSUInteger)instanceCount
          baseInstance:(NSUInteger)baseInstance
{
  GetWrapped(self)->drawPrimitives((MTL::PrimitiveType)primitive, vertexStart, vertexCount,
                                   instanceCount, baseInstance);
}
- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitive
                  indexCount:(NSUInteger)indexCount
                   indexType:(MTLIndexType)indexType
                 indexBuffer:(id<MTLBuffer>)indexBuffer
           indexBufferOffset:(NSUInteger)indexBufferOffset
               instanceCount:(NSUInteger)instanceCount
                 baseVertex:(NSInteger)baseVertex
               baseInstance:(NSUInteger)baseInstance
{
  GetWrapped(self)->drawIndexedPrimitives(
      (MTL::PrimitiveType)primitive, indexCount, (MTL::IndexType)indexType,
      GetWrapped(indexBuffer), indexBufferOffset, instanceCount, baseVertex, baseInstance);
}
- (void)reset
{
  METAL_NOT_HOOKED();
  [self.real reset];
}
@end
