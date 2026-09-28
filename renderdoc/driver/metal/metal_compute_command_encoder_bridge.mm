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
#include "metal_visible_function_table.h"
#include "metal_acceleration_structure.h"
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

- (void)useResource:(id<MTLResource>)resource usage:(MTLResourceUsage)usage
{
  GetWrapped(self)->useResource(GetWrapped(resource), (MTL::ResourceUsage)usage);
}

- (void)useResources:(const id<MTLResource> _Nonnull [_Nonnull])resources
               count:(NSUInteger)count usage:(MTLResourceUsage)usage
{
  rdcarray<WrappedMTLResource *> wrapped;
  for(NSUInteger i = 0; i < count; i++)
    wrapped.push_back(GetWrapped(resources[i]));
  GetWrapped(self)->useResources(wrapped, (MTL::ResourceUsage)usage);
}

- (void)memoryBarrierWithScope:(MTLBarrierScope)scope
{
  GetWrapped(self)->memoryBarrierWithScope((MTL::BarrierScope)scope);
}

- (void)memoryBarrierWithResources:(const id<MTLResource> _Nonnull [_Nonnull])resources
                             count:(NSUInteger)count
{
  rdcarray<WrappedMTLResource *> wrapped;
  for(NSUInteger i = 0; i < count; i++)
    wrapped.push_back(GetWrapped(resources[i]));
  GetWrapped(self)->memoryBarrierWithResources(wrapped);
}

- (void)pushDebugGroup:(NSString *)string
{
  GetWrapped(self)->pushDebugGroup((NS::String *)string);
}

- (void)insertDebugSignpost:(NSString *)string
{
  GetWrapped(self)->insertDebugSignpost((NS::String *)string);
}

- (void)popDebugGroup
{
  GetWrapped(self)->popDebugGroup();
}

- (void)updateFence:(id<MTLFence>)fence
{
  GetWrapped(self)->updateFence(GetWrapped(fence));
}

- (void)waitForFence:(id<MTLFence>)fence
{
  GetWrapped(self)->waitForFence(GetWrapped(fence));
}

- (void)setComputePipelineState:(id<MTLComputePipelineState>)pipeline
{
  GetWrapped(self)->setComputePipelineState(GetWrapped(pipeline));
}

- (void)setVisibleFunctionTable:(nullable id<MTLVisibleFunctionTable>)table
                  atBufferIndex:(NSUInteger)index API_AVAILABLE(macos(11.0), ios(14.0))
{
  if(table && ![table isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
  {
    RDCERR("Cannot capture unwrapped Metal compute visible function table");
    return;
  }
  GetWrapped(self)->setVisibleFunctionTable(
      table ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)table) : NULL,
      (uint32_t)index);
}

- (void)setVisibleFunctionTables:(const id<MTLVisibleFunctionTable> [])tables
                 withBufferRange:(NSRange)range API_AVAILABLE(macos(11.0), ios(14.0))
{
  if(range.length == 0 || range.length > 31 || !tables)
  {
    RDCERR("Unsupported Metal compute visible-function-table range");
    return;
  }
  rdcarray<WrappedMTLVisibleFunctionTable *> wrapped;
  wrapped.reserve(range.length);
  for(NSUInteger i = 0; i < range.length; ++i)
  {
    id<MTLVisibleFunctionTable> table = tables[i];
    if(table && ![table isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
    {
      RDCERR("Cannot capture unwrapped Metal compute visible function table");
      return;
    }
    wrapped.push_back(table ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)table) : NULL);
  }
  GetWrapped(self)->setVisibleFunctionTables(wrapped,
      NS::Range::Make(range.location, range.length));
}

- (void)setIntersectionFunctionTable:(nullable id<MTLIntersectionFunctionTable>)table
                      atBufferIndex:(NSUInteger)index API_AVAILABLE(macos(11.0), ios(14.0))
{
  if(table && ![table isKindOfClass:[ObjCBridgeMTLIntersectionFunctionTable class]])
  {
    RDCERR("Cannot capture unwrapped Metal compute intersection function table");
    return;
  }
  GetWrapped(self)->setIntersectionFunctionTable(
      table ? GetWrapped((ObjCBridgeMTLIntersectionFunctionTable *)table) : NULL,
      (uint32_t)index);
}

- (void)setIntersectionFunctionTables:(const id<MTLIntersectionFunctionTable> [])tables
                      withBufferRange:(NSRange)range API_AVAILABLE(macos(11.0), ios(14.0))
{
  if(range.length == 0 || range.length > 31 || !tables)
  {
    RDCERR("Unsupported Metal compute intersection-function-table range");
    return;
  }
  rdcarray<WrappedMTLIntersectionFunctionTable *> wrapped;
  wrapped.reserve(range.length);
  for(NSUInteger i = 0; i < range.length; ++i)
  {
    id<MTLIntersectionFunctionTable> table = tables[i];
    if(table && ![table isKindOfClass:[ObjCBridgeMTLIntersectionFunctionTable class]])
    {
      RDCERR("Cannot capture unwrapped Metal compute intersection function table");
      return;
    }
    wrapped.push_back(table ? GetWrapped((ObjCBridgeMTLIntersectionFunctionTable *)table) : NULL);
  }
  GetWrapped(self)->setIntersectionFunctionTables(wrapped,
      NS::Range::Make(range.location, range.length));
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

- (void)setSamplerState:(nullable id<MTLSamplerState>)sampler
           lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp
               atIndex:(NSUInteger)index
{
  GetWrapped(self)->setSamplerStateWithLOD(GetWrapped(sampler), lodMinClamp, lodMaxClamp, index);
}

- (void)setSamplerStates:(const id<MTLSamplerState> _Nullable [_Nonnull])samplers
           lodMinClamps:(const float [_Nonnull])lodMinClamps
           lodMaxClamps:(const float [_Nonnull])lodMaxClamps withRange:(NSRange)range
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  rdcarray<float> minimums, maximums;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(samplers[i]));
    minimums.push_back(lodMinClamps[i]);
    maximums.push_back(lodMaxClamps[i]);
  }
  GetWrapped(self)->setSamplerStatesWithLOD(wrapped, minimums, maximums,
                                            NS::Range::Make(range.location, range.length));
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

- (void)setAccelerationStructure:(id<MTLAccelerationStructure>)structure
                 atBufferIndex:(NSUInteger)index
{
  if(structure && ![(id)structure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
    METAL_NOT_HOOKED();
  GetWrapped(self)->setAccelerationStructure(
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)structure), index);
}

- (void)setBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
  rdcarray<byte> data;
  data.assign((const byte *)bytes, length);
  GetWrapped(self)->setBytes(data, index);
}

- (void)setBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index
{
  GetWrapped(self)->setBufferOffset(offset, index);
}

- (void)setThreadgroupMemoryLength:(NSUInteger)length atIndex:(NSUInteger)index
{
  GetWrapped(self)->setThreadgroupMemoryLength(length, index);
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
