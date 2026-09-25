/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "metal_compute_command_encoder.h"
#include "metal_types_bridge.h"

@implementation ObjCBridgeMTLComputeCommandEncoder

- (id<MTLComputeCommandEncoder>)real
{
  return id<MTLComputeCommandEncoder>(Unwrap(GetWrapped(self)));
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
  id fwd = self.real;
  return [fwd methodSignatureForSelector:selector];
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

- (void)endEncoding
{
  GetWrapped(self)->endEncoding();
}

- (void)setComputePipelineState:(id<MTLComputePipelineState>)pipeline
{
  GetWrapped(self)->setComputePipelineState(GetWrapped(pipeline));
}

- (void)setTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index
{
  GetWrapped(self)->setTexture(GetWrapped(texture), index);
}

- (void)setTextures:(const id<MTLTexture> _Nullable [_Nonnull])textures withRange:(NSRange)range
{
  rdcarray<WrappedMTLTexture *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++)
    wrapped.push_back(GetWrapped(textures[i]));
  GetWrapped(self)->setTextures(wrapped, NS::Range::Make(range.location, range.length));
}

- (void)setSamplerState:(id<MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
  GetWrapped(self)->setSamplerState(GetWrapped(sampler), index);
}

- (void)setSamplerStates:(const id<MTLSamplerState> _Nullable [_Nonnull])samplers
               withRange:(NSRange)range
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++)
    wrapped.push_back(GetWrapped(samplers[i]));
  GetWrapped(self)->setSamplerStates(wrapped, NS::Range::Make(range.location, range.length));
}

- (void)setBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
  GetWrapped(self)->setBuffer(GetWrapped(buffer), offset, index);
}

- (void)setBuffers:(const id<MTLBuffer> _Nullable [_Nonnull])buffers
          offsets:(const NSUInteger [_Nonnull])offsets withRange:(NSRange)range
{
  rdcarray<WrappedMTLBuffer *> wrapped;
  rdcarray<NS::UInteger> copiedOffsets;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(buffers[i]));
    copiedOffsets.push_back(offsets[i]);
  }
  GetWrapped(self)->setBuffers(wrapped, copiedOffsets,
                               NS::Range::Make(range.location, range.length));
}

- (void)dispatchThreadgroups:(MTLSize)groups threadsPerThreadgroup:(MTLSize)threadsPerGroup
{
  MTL::Size cppGroups = MTL::Size::Make(groups.width, groups.height, groups.depth);
  MTL::Size cppThreads =
      MTL::Size::Make(threadsPerGroup.width, threadsPerGroup.height, threadsPerGroup.depth);
  GetWrapped(self)->dispatchThreadgroups(cppGroups, cppThreads);
}

- (void)dispatchThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)indirectBuffer
                           indirectBufferOffset:(NSUInteger)indirectBufferOffset
                          threadsPerThreadgroup:(MTLSize)threadsPerGroup
{
  MTL::Size cppThreads =
      MTL::Size::Make(threadsPerGroup.width, threadsPerGroup.height, threadsPerGroup.depth);
  GetWrapped(self)->dispatchThreadgroups(GetWrapped(indirectBuffer), indirectBufferOffset, cppThreads);
}

- (void)dispatchThreads:(MTLSize)grid threadsPerThreadgroup:(MTLSize)threadsPerGroup
{
  MTL::Size cppGrid = MTL::Size::Make(grid.width, grid.height, grid.depth);
  MTL::Size cppThreads =
      MTL::Size::Make(threadsPerGroup.width, threadsPerGroup.height, threadsPerGroup.depth);
  GetWrapped(self)->dispatchThreads(cppGrid, cppThreads);
}

@end
