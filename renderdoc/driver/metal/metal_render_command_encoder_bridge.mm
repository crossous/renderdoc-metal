/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2026 Baldur Karlsson
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

#include "metal_render_command_encoder.h"
#include "metal_visible_function_table.h"
#include "metal_heap.h"
#include "metal_types_bridge.h"

// Wrapper for MTLRenderCommandEncoder
@implementation ObjCBridgeMTLRenderCommandEncoder

// ObjCWrappedMTLRenderCommandEncoder specific
- (id<MTLRenderCommandEncoder>)real
{
  return id<MTLRenderCommandEncoder>(Unwrap(GetWrapped(self)));
}

// Silence compiler warning
// error: method possibly missing a [super dealloc] call [-Werror,-Wobjc-missing-super-calls]
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-missing-super-calls"
- (void)dealloc
{
  DeallocateObjCBridge(GetWrapped(self));
}
#pragma clang diagnostic pop

// Use the real MTLRenderCommandEncoder to find methods from messages
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

// Forward any unknown messages to the real MTLRenderCommandEncoder
- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL aSelector = [invocation selector];

  if([self.real respondsToSelector:aSelector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}

// MTLCommandEncoder : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX13.1.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLCommandEncoder.h

- (id<MTLDevice>)device
{
  return id<MTLDevice>(GetWrapped(self)->GetDevice());
}

- (nullable NSString *)label
{
  return self.real.label;
}

- (void)setLabel:value
{
  self.real.label = value;
}

- (void)endEncoding
{
  GetWrapped(self)->endEncoding();
}

- (void)insertDebugSignpost:(NSString *)string
{
  GetWrapped(self)->insertDebugSignpost((NS::String *)string);
}

- (void)pushDebugGroup:(NSString *)string
{
  GetWrapped(self)->pushDebugGroup((NS::String *)string);
}

- (void)popDebugGroup
{
  GetWrapped(self)->popDebugGroup();
}

// MTLRenderCommandEncoder : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.0.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLRenderCommandEncoder.h

- (void)setRenderPipelineState:(id<MTLRenderPipelineState>)pipelineState
{
  GetWrapped(self)->setRenderPipelineState(GetWrapped(pipelineState));
}

- (void)setVertexBytes:(const void *)bytes
                length:(NSUInteger)length
               atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3))
{
  rdcarray<byte> data;
  data.assign((const byte *)bytes, length);
  GetWrapped(self)->setVertexBytes(data, index);
}

- (void)setVertexBuffer:(nullable id<MTLBuffer>)buffer
                 offset:(NSUInteger)offset
                atIndex:(NSUInteger)index
{
  GetWrapped(self)->setVertexBuffer(GetWrapped(buffer), offset, index);
}

- (void)setVertexBufferOffset:(NSUInteger)offset
                      atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3))
{
  GetWrapped(self)->setVertexBufferOffset(offset, index);
}

- (void)setVertexBuffers:(const id<MTLBuffer> __nullable[__nonnull])buffers
                 offsets:(const NSUInteger[__nonnull])offsets
               withRange:(NSRange)range
{
  rdcarray<WrappedMTLBuffer *> wrapped;
  rdcarray<NS::UInteger> copiedOffsets;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(buffers[i]));
    copiedOffsets.push_back(offsets[i]);
  }
  GetWrapped(self)->setVertexBuffers(wrapped, copiedOffsets,
                                     NS::Range::Make(range.location, range.length));
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_14_0
- (void)setVertexBuffer:(nullable id<MTLBuffer>)buffer
                 offset:(NSUInteger)offset
        attributeStride:(NSUInteger)stride
                atIndex:(NSUInteger)index API_AVAILABLE(macos(14.0), ios(17.0))
{
  GetWrapped(self)->setVertexBufferWithStride(GetWrapped(buffer),offset,stride,index);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_14_0
- (void)setVertexBuffers:(id<MTLBuffer> const __nullable[__nonnull])buffers
                 offsets:(NSUInteger const[__nonnull])offsets
        attributeStrides:(NSUInteger const[__nonnull])strides
               withRange:(NSRange)range API_AVAILABLE(macos(14.0), ios(17.0))
{
  rdcarray<WrappedMTLBuffer *> wrapped;
  rdcarray<NS::UInteger> copiedOffsets, copiedStrides;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(buffers[i]));
    copiedOffsets.push_back(offsets[i]);
    copiedStrides.push_back(strides[i]);
  }
  GetWrapped(self)->setVertexBuffersWithStrides(wrapped,copiedOffsets,copiedStrides,
                                                 NS::Range::Make(range.location,range.length));
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_14_0
- (void)setVertexBufferOffset:(NSUInteger)offset
              attributeStride:(NSUInteger)stride
                      atIndex:(NSUInteger)index API_AVAILABLE(macos(14.0), ios(17.0))
{
  GetWrapped(self)->setVertexBufferOffsetWithStride(offset,stride,index);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_14_0
- (void)setVertexBytes:(void const *)bytes
                length:(NSUInteger)length
       attributeStride:(NSUInteger)stride
               atIndex:(NSUInteger)index API_AVAILABLE(macos(14.0), ios(17.0))
{
  rdcarray<byte> data;
  if(bytes && length) data.assign((const byte *)bytes,length);
  GetWrapped(self)->setVertexBytesWithStride(data,stride,index);
}
#endif

- (void)setVertexTexture:(nullable id<MTLTexture>)texture atIndex:(NSUInteger)index
{
  GetWrapped(self)->setVertexTexture(GetWrapped(texture), index);
}

- (void)setVertexTextures:(const id<MTLTexture> __nullable[__nonnull])textures
                withRange:(NSRange)range
{
  rdcarray<WrappedMTLTexture *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++)
    wrapped.push_back(GetWrapped(textures[i]));
  GetWrapped(self)->setVertexTextures(wrapped, NS::Range::Make(range.location, range.length));
}

- (void)setVertexSamplerState:(nullable id<MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
  GetWrapped(self)->setVertexSamplerState(GetWrapped(sampler), index);
}

- (void)setVertexSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
                     withRange:(NSRange)range
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++)
    wrapped.push_back(GetWrapped(samplers[i]));
  GetWrapped(self)->setVertexSamplerStates(wrapped, NS::Range::Make(range.location, range.length));
}

- (void)setVertexSamplerState:(nullable id<MTLSamplerState>)sampler
                  lodMinClamp:(float)lodMinClamp
                  lodMaxClamp:(float)lodMaxClamp
                      atIndex:(NSUInteger)index
{
  GetWrapped(self)->setVertexSamplerStateWithLOD(GetWrapped(sampler), lodMinClamp, lodMaxClamp, index);
}

- (void)setVertexSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
                  lodMinClamps:(const float[__nonnull])lodMinClamps
                  lodMaxClamps:(const float[__nonnull])lodMaxClamps
                     withRange:(NSRange)range
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  rdcarray<float> minimums, maximums;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(samplers[i]));
    minimums.push_back(lodMinClamps[i]);
    maximums.push_back(lodMaxClamps[i]);
  }
  GetWrapped(self)->setVertexSamplerStatesWithLOD(wrapped, minimums, maximums,
                                               NS::Range::Make(range.location, range.length));
}

- (void)setVertexVisibleFunctionTable:(nullable id<MTLVisibleFunctionTable>)functionTable
                        atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(functionTable && ![functionTable isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
  {
    RDCERR("Cannot capture unwrapped Metal vertex visible function table");
    return;
  }
  GetWrapped(self)->setVertexVisibleFunctionTable(
      functionTable ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)functionTable) : NULL,
      (uint32_t)bufferIndex);
}

- (void)setVertexVisibleFunctionTables:
            (const id<MTLVisibleFunctionTable> __nullable[__nonnull])functionTables
                       withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(range.length == 0 || range.length > 31 || !functionTables)
  {
    RDCERR("Unsupported Metal vertex visible-function-table range");
    return;
  }
  rdcarray<WrappedMTLVisibleFunctionTable *> wrapped;
  wrapped.reserve(range.length);
  for(NSUInteger i = 0; i < range.length; i++)
  {
    id<MTLVisibleFunctionTable> table = functionTables[i];
    if(table && ![table isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
    {
      RDCERR("Cannot capture unwrapped Metal vertex visible function table");
      return;
    }
    wrapped.push_back(table ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)table) : NULL);
  }
  GetWrapped(self)->setVertexVisibleFunctionTables(wrapped,
      NS::Range::Make(range.location, range.length));
}

- (void)setVertexIntersectionFunctionTable:
            (nullable id<MTLIntersectionFunctionTable>)intersectionFunctionTable
                             atBufferIndex:(NSUInteger)bufferIndex
    API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(!intersectionFunctionTable ||
     ![intersectionFunctionTable isKindOfClass:[ObjCBridgeMTLIntersectionFunctionTable class]])
  {
    RDCERR("Unsupported or unwrapped Metal vertex intersection function table");
    return;
  }
  GetWrapped(self)->setVertexIntersectionFunctionTable(
      GetWrapped((ObjCBridgeMTLIntersectionFunctionTable *)intersectionFunctionTable),
      (uint32_t)bufferIndex);
}

- (void)setVertexIntersectionFunctionTables:
            (const id<MTLIntersectionFunctionTable> __nullable[__nonnull])intersectionFunctionTables
                            withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(!intersectionFunctionTables || range.length == 0 || range.length > 31 ||
     range.location > 31 - range.length)
  {
    RDCERR("Unsupported Metal vertex intersection-table range");
    return;
  }
  rdcarray<WrappedMTLIntersectionFunctionTable *> wrapped;
  for(NSUInteger i = 0; i < range.length; ++i)
  {
    id<MTLIntersectionFunctionTable> table = intersectionFunctionTables[i];
    if(!table || ![table isKindOfClass:[ObjCBridgeMTLIntersectionFunctionTable class]])
    {
      RDCERR("Unsupported Metal vertex intersection-table member");
      return;
    }
    wrapped.push_back(GetWrapped((ObjCBridgeMTLIntersectionFunctionTable *)table));
  }
  GetWrapped(self)->setIntersectionFunctionTables(wrapped,
      NS::Range::Make(range.location, range.length), MTL::RenderStageVertex);
}

- (void)setVertexAccelerationStructure:(nullable id<MTLAccelerationStructure>)accelerationStructure
                         atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(accelerationStructure &&
     ![(id)accelerationStructure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
    METAL_NOT_HOOKED();
  GetWrapped(self)->setVertexAccelerationStructure(
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)accelerationStructure), bufferIndex);
}

- (void)setViewport:(MTLViewport)viewport
{
  GetWrapped(self)->setViewport((MTL::Viewport &)viewport);
}

- (void)setViewports:(const MTLViewport[__nonnull])viewports
               count:(NSUInteger)count API_AVAILABLE(macos(10.13), ios(12.0), tvos(14.5))
{
  rdcarray<MTL::Viewport> wrapped;
  wrapped.assign((const MTL::Viewport *)viewports, count);
  GetWrapped(self)->setViewports(wrapped);
}

- (void)setFrontFacingWinding:(MTLWinding)frontFacingWinding
{
  GetWrapped(self)->setFrontFacingWinding((MTL::Winding)frontFacingWinding);
}

- (void)setVertexAmplificationCount:(NSUInteger)count
                       viewMappings:(nullable const MTLVertexAmplificationViewMapping *)viewMappings
    API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4))
{
  if(count > 32)
  {
    RDCERR("Invalid Metal vertex amplification count %llu",(uint64_t)count);
    return;
  }
  rdcarray<uint32_t> viewportOffsets, targetOffsets;
  if(viewMappings)
    for(NSUInteger i = 0; i < count; i++)
    {
      viewportOffsets.push_back(viewMappings[i].viewportArrayIndexOffset);
      targetOffsets.push_back(viewMappings[i].renderTargetArrayIndexOffset);
    }
  GetWrapped(self)->setVertexAmplificationCount(count,viewportOffsets,targetOffsets,
                                                 viewMappings != NULL);
}

- (void)setCullMode:(MTLCullMode)cullMode
{
  GetWrapped(self)->setCullMode((MTL::CullMode)cullMode);
}

- (void)setDepthClipMode:(MTLDepthClipMode)depthClipMode API_AVAILABLE(macos(10.11), ios(11.0))
{
  GetWrapped(self)->setDepthClipMode((MTL::DepthClipMode)depthClipMode);
}

- (void)setDepthBias:(float)depthBias slopeScale:(float)slopeScale clamp:(float)clamp
{
  GetWrapped(self)->setDepthBias(depthBias, slopeScale, clamp);
}

- (void)setScissorRect:(MTLScissorRect)rect
{
  GetWrapped(self)->setScissorRect((MTL::ScissorRect &)rect);
}

- (void)setScissorRects:(const MTLScissorRect[__nonnull])scissorRects
                  count:(NSUInteger)count API_AVAILABLE(macos(10.13), ios(12.0), tvos(14.5))
{
  rdcarray<MTL::ScissorRect> wrapped;
  wrapped.assign((const MTL::ScissorRect *)scissorRects, count);
  GetWrapped(self)->setScissorRects(wrapped);
}

- (void)setTriangleFillMode:(MTLTriangleFillMode)fillMode
{
  GetWrapped(self)->setTriangleFillMode((MTL::TriangleFillMode)fillMode);
}

- (void)setFragmentBytes:(const void *)bytes
                  length:(NSUInteger)length
                 atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3))
{
  rdcarray<byte> data;
  data.assign((const byte *)bytes, length);
  GetWrapped(self)->setFragmentBytes(data, index);
}

- (void)setFragmentBuffer:(nullable id<MTLBuffer>)buffer
                   offset:(NSUInteger)offset
                  atIndex:(NSUInteger)index
{
  GetWrapped(self)->setFragmentBuffer(GetWrapped(buffer), offset, index);
}

- (void)setFragmentBufferOffset:(NSUInteger)offset
                        atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3))
{
  GetWrapped(self)->setFragmentBufferOffset(offset, index);
}

- (void)setFragmentBuffers:(const id<MTLBuffer> __nullable[__nonnull])buffers
                   offsets:(const NSUInteger[__nonnull])offsets
                 withRange:(NSRange)range
{
  rdcarray<WrappedMTLBuffer *> wrapped;
  rdcarray<NS::UInteger> copiedOffsets;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(buffers[i]));
    copiedOffsets.push_back(offsets[i]);
  }
  GetWrapped(self)->setFragmentBuffers(wrapped, copiedOffsets,
                                       NS::Range::Make(range.location, range.length));
}

- (void)setFragmentTexture:(nullable id<MTLTexture>)texture atIndex:(NSUInteger)index
{
  GetWrapped(self)->setFragmentTexture(GetWrapped(texture), index);
}

- (void)setFragmentTextures:(const id<MTLTexture> __nullable[__nonnull])textures
                  withRange:(NSRange)range
{
  rdcarray<WrappedMTLTexture *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++)
    wrapped.push_back(GetWrapped(textures[i]));
  GetWrapped(self)->setFragmentTextures(wrapped, NS::Range::Make(range.location, range.length));
}

- (void)setFragmentSamplerState:(nullable id<MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
  GetWrapped(self)->setFragmentSamplerState(GetWrapped(sampler), index);
}

- (void)setFragmentSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
                       withRange:(NSRange)range
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++)
    wrapped.push_back(GetWrapped(samplers[i]));
  GetWrapped(self)->setFragmentSamplerStates(wrapped, NS::Range::Make(range.location, range.length));
}

- (void)setFragmentSamplerState:(nullable id<MTLSamplerState>)sampler
                    lodMinClamp:(float)lodMinClamp
                    lodMaxClamp:(float)lodMaxClamp
                        atIndex:(NSUInteger)index
{
  GetWrapped(self)->setFragmentSamplerStateWithLOD(GetWrapped(sampler), lodMinClamp, lodMaxClamp, index);
}

- (void)setFragmentSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
                    lodMinClamps:(const float[__nonnull])lodMinClamps
                    lodMaxClamps:(const float[__nonnull])lodMaxClamps
                       withRange:(NSRange)range
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  rdcarray<float> minimums, maximums;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(samplers[i]));
    minimums.push_back(lodMinClamps[i]);
    maximums.push_back(lodMaxClamps[i]);
  }
  GetWrapped(self)->setFragmentSamplerStatesWithLOD(wrapped, minimums, maximums,
                                               NS::Range::Make(range.location, range.length));
}

- (void)setFragmentVisibleFunctionTable:(nullable id<MTLVisibleFunctionTable>)functionTable
                          atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(functionTable && ![functionTable isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
  {
    RDCERR("Cannot capture unwrapped Metal visible function table");
    return;
  }
  GetWrapped(self)->setFragmentVisibleFunctionTable(
      functionTable ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)functionTable) : NULL,
      (uint32_t)bufferIndex);
}

- (void)setFragmentVisibleFunctionTables:
            (const id<MTLVisibleFunctionTable> __nullable[__nonnull])functionTables
                         withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(range.length == 0 || range.length > 31 || !functionTables)
  {
    RDCERR("Unsupported Metal fragment visible-function-table range");
    return;
  }
  rdcarray<WrappedMTLVisibleFunctionTable *> wrapped;
  wrapped.reserve(range.length);
  for(NSUInteger i = 0; i < range.length; i++)
  {
    id<MTLVisibleFunctionTable> table = functionTables[i];
    if(table && ![table isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
    {
      RDCERR("Cannot capture unwrapped Metal visible function table");
      return;
    }
    wrapped.push_back(table ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)table) : NULL);
  }
  GetWrapped(self)->setFragmentVisibleFunctionTables(wrapped,
      NS::Range::Make(range.location, range.length));
}

- (void)setFragmentIntersectionFunctionTable:
            (nullable id<MTLIntersectionFunctionTable>)intersectionFunctionTable
                               atBufferIndex:(NSUInteger)bufferIndex
    API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(!intersectionFunctionTable ||
     ![intersectionFunctionTable isKindOfClass:[ObjCBridgeMTLIntersectionFunctionTable class]])
  {
    RDCERR("Unsupported or unwrapped Metal intersection function table");
    return;
  }
  GetWrapped(self)->setFragmentIntersectionFunctionTable(
      intersectionFunctionTable ?
          GetWrapped((ObjCBridgeMTLIntersectionFunctionTable *)intersectionFunctionTable) : NULL,
      (uint32_t)bufferIndex);
}

- (void)setFragmentIntersectionFunctionTables:
            (const id<MTLIntersectionFunctionTable> __nullable[__nonnull])intersectionFunctionTables
                              withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(!intersectionFunctionTables || range.length == 0 || range.length > 31 ||
     range.location > 31 - range.length)
  {
    RDCERR("Unsupported Metal fragment intersection-table range");
    return;
  }
  rdcarray<WrappedMTLIntersectionFunctionTable *> wrapped;
  for(NSUInteger i = 0; i < range.length; ++i)
  {
    id<MTLIntersectionFunctionTable> table = intersectionFunctionTables[i];
    if(!table || ![table isKindOfClass:[ObjCBridgeMTLIntersectionFunctionTable class]])
    {
      RDCERR("Unsupported Metal fragment intersection-table member");
      return;
    }
    wrapped.push_back(GetWrapped((ObjCBridgeMTLIntersectionFunctionTable *)table));
  }
  GetWrapped(self)->setIntersectionFunctionTables(wrapped,
      NS::Range::Make(range.location, range.length), MTL::RenderStageFragment);
}

- (void)setFragmentAccelerationStructure:(nullable id<MTLAccelerationStructure>)accelerationStructure
                           atBufferIndex:(NSUInteger)bufferIndex
    API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(accelerationStructure &&
     ![(id)accelerationStructure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
    METAL_NOT_HOOKED();
  GetWrapped(self)->setFragmentAccelerationStructure(
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)accelerationStructure), bufferIndex);
}

- (void)setBlendColorRed:(float)red green:(float)green blue:(float)blue alpha:(float)alpha
{
  GetWrapped(self)->setBlendColor(red, green, blue, alpha);
}

- (void)setDepthStencilState:(nullable id<MTLDepthStencilState>)depthStencilState
{
  GetWrapped(self)->setDepthStencilState(GetWrapped(depthStencilState));
}

- (void)setStencilReferenceValue:(uint32_t)referenceValue
{
  GetWrapped(self)->setStencilReferenceValue(referenceValue);
}

- (void)setStencilFrontReferenceValue:(uint32_t)frontReferenceValue
                   backReferenceValue:(uint32_t)backReferenceValue
    API_AVAILABLE(macos(10.11), ios(9.0))
{
  GetWrapped(self)->setStencilReferenceValues(frontReferenceValue, backReferenceValue);
}

- (void)setVisibilityResultMode:(MTLVisibilityResultMode)mode offset:(NSUInteger)offset
{
  GetWrapped(self)->setVisibilityResultMode((MTL::VisibilityResultMode)mode, offset);
}

- (void)setColorStoreAction:(MTLStoreAction)storeAction
                    atIndex:(NSUInteger)colorAttachmentIndex API_AVAILABLE(macos(10.12), ios(10.0))
{
  GetWrapped(self)->setColorStoreAction((MTL::StoreAction)storeAction, colorAttachmentIndex);
}

- (void)setDepthStoreAction:(MTLStoreAction)storeAction API_AVAILABLE(macos(10.12), ios(10.0))
{
  GetWrapped(self)->setDepthStoreAction((MTL::StoreAction)storeAction);
}

- (void)setStencilStoreAction:(MTLStoreAction)storeAction API_AVAILABLE(macos(10.12), ios(10.0))
{
  GetWrapped(self)->setStencilStoreAction((MTL::StoreAction)storeAction);
}

- (void)setColorStoreActionOptions:(MTLStoreActionOptions)storeActionOptions
                           atIndex:(NSUInteger)colorAttachmentIndex
    API_AVAILABLE(macos(10.13), ios(11.0))
{
  GetWrapped(self)->setColorStoreActionOptions((MTL::StoreActionOptions)storeActionOptions,
                                               colorAttachmentIndex);
}

- (void)setDepthStoreActionOptions:(MTLStoreActionOptions)storeActionOptions
    API_AVAILABLE(macos(10.13), ios(11.0))
{
  GetWrapped(self)->setDepthStoreActionOptions((MTL::StoreActionOptions)storeActionOptions);
}

- (void)setStencilStoreActionOptions:(MTLStoreActionOptions)storeActionOptions
    API_AVAILABLE(macos(10.13), ios(11.0))
{
  GetWrapped(self)->setStencilStoreActionOptions((MTL::StoreActionOptions)storeActionOptions);
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectBytes:(const void *)bytes
                length:(NSUInteger)length
               atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  rdcarray<byte> data;
  if(bytes && length) data.assign((const byte *)bytes, length);
  GetWrapped(self)->setObjectBytes(data, index);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectBuffer:(nullable id<MTLBuffer>)buffer
                 offset:(NSUInteger)offset
                atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setObjectBuffer(GetWrapped(buffer), offset, index);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectBufferOffset:(NSUInteger)offset
                      atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setObjectBufferOffset(offset, index);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectBuffers:(const id<MTLBuffer> __nullable[__nonnull])buffers
                 offsets:(const NSUInteger[__nonnull])offsets
               withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0))
{
  rdcarray<WrappedMTLBuffer *> wrapped;
  rdcarray<NS::UInteger> capturedOffsets;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(buffers[i]));
    capturedOffsets.push_back(offsets[i]);
  }
  GetWrapped(self)->setObjectBuffers(wrapped, capturedOffsets,
                                      NS::Range::Make(range.location, range.length));
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectTexture:(nullable id<MTLTexture>)texture
                 atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setObjectTextures({GetWrapped(texture)}, NS::Range::Make(index, 1), 0);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectTextures:(const id<MTLTexture> __nullable[__nonnull])textures
                withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0))
{
  rdcarray<WrappedMTLTexture *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++) wrapped.push_back(GetWrapped(textures[i]));
  GetWrapped(self)->setObjectTextures(wrapped, NS::Range::Make(range.location, range.length), 1);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectSamplerState:(nullable id<MTLSamplerState>)sampler
                      atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setObjectSamplers({GetWrapped(sampler)}, {}, {}, NS::Range::Make(index, 1), 0);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
                     withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0))
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++) wrapped.push_back(GetWrapped(samplers[i]));
  GetWrapped(self)->setObjectSamplers(wrapped, {}, {}, NS::Range::Make(range.location, range.length), 1);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectSamplerState:(nullable id<MTLSamplerState>)sampler
                  lodMinClamp:(float)lodMinClamp
                  lodMaxClamp:(float)lodMaxClamp
                      atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setObjectSamplers({GetWrapped(sampler)}, {lodMinClamp}, {lodMaxClamp},
                                       NS::Range::Make(index, 1), 2);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
                  lodMinClamps:(const float[__nonnull])lodMinClamps
                  lodMaxClamps:(const float[__nonnull])lodMaxClamps
                     withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0))
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  rdcarray<float> minimums, maximums;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(samplers[i]));
    minimums.push_back(lodMinClamps[i]);
    maximums.push_back(lodMaxClamps[i]);
  }
  GetWrapped(self)->setObjectSamplers(wrapped, minimums, maximums,
                                      NS::Range::Make(range.location, range.length), 3);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setObjectThreadgroupMemoryLength:(NSUInteger)length
                                 atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setObjectThreadgroupMemoryLength(length, index);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setMeshBytes:(const void *)bytes
              length:(NSUInteger)length
             atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  rdcarray<byte> data;
  if(bytes && length) data.assign((const byte *)bytes, length);
  GetWrapped(self)->setMeshBytes(data, index);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setMeshBuffer:(nullable id<MTLBuffer>)buffer
               offset:(NSUInteger)offset
              atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setMeshBuffer(GetWrapped(buffer), offset, index);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setMeshBufferOffset:(NSUInteger)offset
                    atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setMeshBufferOffset(offset, index);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setMeshBuffers:(const id<MTLBuffer> __nullable[__nonnull])buffers
               offsets:(const NSUInteger[__nonnull])offsets
             withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0))
{
  rdcarray<WrappedMTLBuffer *> wrapped;
  rdcarray<NS::UInteger> capturedOffsets;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(buffers[i]));
    capturedOffsets.push_back(offsets[i]);
  }
  GetWrapped(self)->setMeshBuffers(wrapped, capturedOffsets,
                                    NS::Range::Make(range.location, range.length));
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setMeshTexture:(nullable id<MTLTexture>)texture
               atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setMeshTextures({GetWrapped(texture)}, NS::Range::Make(index, 1), 0);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setMeshTextures:(const id<MTLTexture> __nullable[__nonnull])textures
              withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0))
{
  rdcarray<WrappedMTLTexture *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++) wrapped.push_back(GetWrapped(textures[i]));
  GetWrapped(self)->setMeshTextures(wrapped, NS::Range::Make(range.location, range.length), 1);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setMeshSamplerState:(nullable id<MTLSamplerState>)sampler
                    atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setMeshSamplers({GetWrapped(sampler)}, {}, {}, NS::Range::Make(index, 1), 0);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setMeshSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
                   withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0))
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++) wrapped.push_back(GetWrapped(samplers[i]));
  GetWrapped(self)->setMeshSamplers(wrapped, {}, {}, NS::Range::Make(range.location, range.length), 1);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setMeshSamplerState:(nullable id<MTLSamplerState>)sampler
                lodMinClamp:(float)lodMinClamp
                lodMaxClamp:(float)lodMaxClamp
                    atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->setMeshSamplers({GetWrapped(sampler)}, {lodMinClamp}, {lodMaxClamp},
                                     NS::Range::Make(index, 1), 2);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)setMeshSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
                lodMinClamps:(const float[__nonnull])lodMinClamps
                lodMaxClamps:(const float[__nonnull])lodMaxClamps
                   withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0))
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  rdcarray<float> minimums, maximums;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(samplers[i]));
    minimums.push_back(lodMinClamps[i]);
    maximums.push_back(lodMaxClamps[i]);
  }
  GetWrapped(self)->setMeshSamplers(wrapped, minimums, maximums,
                                    NS::Range::Make(range.location, range.length), 3);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)drawMeshThreadgroups:(MTLSize)threadgroupsPerGrid
    threadsPerObjectThreadgroup:(MTLSize)threadsPerObjectThreadgroup
      threadsPerMeshThreadgroup:(MTLSize)threadsPerMeshThreadgroup
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->drawMeshThreadgroups((MTL::Size &)threadgroupsPerGrid,
                                         (MTL::Size &)threadsPerObjectThreadgroup,
                                         (MTL::Size &)threadsPerMeshThreadgroup);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)drawMeshThreads:(MTLSize)threadsPerGrid
    threadsPerObjectThreadgroup:(MTLSize)threadsPerObjectThreadgroup
      threadsPerMeshThreadgroup:(MTLSize)threadsPerMeshThreadgroup
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->drawMeshThreads((MTL::Size &)threadsPerGrid,
                                    (MTL::Size &)threadsPerObjectThreadgroup,
                                    (MTL::Size &)threadsPerMeshThreadgroup);
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)drawMeshThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)indirectBuffer
                          indirectBufferOffset:(NSUInteger)indirectBufferOffset
                   threadsPerObjectThreadgroup:(MTLSize)threadsPerObjectThreadgroup
                     threadsPerMeshThreadgroup:(MTLSize)threadsPerMeshThreadgroup
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  GetWrapped(self)->drawMeshThreadgroups(GetWrapped(indirectBuffer), indirectBufferOffset,
                                         (MTL::Size &)threadsPerObjectThreadgroup,
                                         (MTL::Size &)threadsPerMeshThreadgroup);
}
#endif

- (void)drawPrimitives:(MTLPrimitiveType)primitiveType
           vertexStart:(NSUInteger)vertexStart
           vertexCount:(NSUInteger)vertexCount
         instanceCount:(NSUInteger)instanceCount
{
  GetWrapped(self)->drawPrimitives((MTL::PrimitiveType)primitiveType, vertexStart, vertexCount,
                                   instanceCount);
}

- (void)drawPrimitives:(MTLPrimitiveType)primitiveType
           vertexStart:(NSUInteger)vertexStart
           vertexCount:(NSUInteger)vertexCount
{
  GetWrapped(self)->drawPrimitives((MTL::PrimitiveType)primitiveType, vertexStart, vertexCount);
}

- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType
                   indexCount:(NSUInteger)indexCount
                    indexType:(MTLIndexType)indexType
                  indexBuffer:(id<MTLBuffer>)indexBuffer
            indexBufferOffset:(NSUInteger)indexBufferOffset
                instanceCount:(NSUInteger)instanceCount
{
  GetWrapped(self)->drawIndexedPrimitives((MTL::PrimitiveType)primitiveType, indexCount,
                                          (MTL::IndexType)indexType, GetWrapped(indexBuffer),
                                          indexBufferOffset, instanceCount);
}

- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType
                   indexCount:(NSUInteger)indexCount
                    indexType:(MTLIndexType)indexType
                  indexBuffer:(id<MTLBuffer>)indexBuffer
            indexBufferOffset:(NSUInteger)indexBufferOffset
{
  GetWrapped(self)->drawIndexedPrimitives((MTL::PrimitiveType)primitiveType, indexCount,
                                          (MTL::IndexType)indexType, GetWrapped(indexBuffer),
                                          indexBufferOffset);
}

- (void)drawPrimitives:(MTLPrimitiveType)primitiveType
           vertexStart:(NSUInteger)vertexStart
           vertexCount:(NSUInteger)vertexCount
         instanceCount:(NSUInteger)instanceCount
          baseInstance:(NSUInteger)baseInstance API_AVAILABLE(macos(10.11), ios(9.0))
{
  GetWrapped(self)->drawPrimitives((MTL::PrimitiveType)primitiveType, vertexStart, vertexCount,
                                   instanceCount, baseInstance);
}

- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType
                   indexCount:(NSUInteger)indexCount
                    indexType:(MTLIndexType)indexType
                  indexBuffer:(id<MTLBuffer>)indexBuffer
            indexBufferOffset:(NSUInteger)indexBufferOffset
                instanceCount:(NSUInteger)instanceCount
                   baseVertex:(NSInteger)baseVertex
                 baseInstance:(NSUInteger)baseInstance API_AVAILABLE(macos(10.11), ios(9.0))
{
  GetWrapped(self)->drawIndexedPrimitives((MTL::PrimitiveType)primitiveType, indexCount,
                                          (MTL::IndexType)indexType, GetWrapped(indexBuffer),
                                          indexBufferOffset, instanceCount, baseVertex, baseInstance);
}

- (void)drawPrimitives:(MTLPrimitiveType)primitiveType
          indirectBuffer:(id<MTLBuffer>)indirectBuffer
    indirectBufferOffset:(NSUInteger)indirectBufferOffset API_AVAILABLE(macos(10.11), ios(9.0))
{
  GetWrapped(self)->drawPrimitives((MTL::PrimitiveType)primitiveType, GetWrapped(indirectBuffer),
                                   indirectBufferOffset);
}

- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType
                    indexType:(MTLIndexType)indexType
                  indexBuffer:(id<MTLBuffer>)indexBuffer
            indexBufferOffset:(NSUInteger)indexBufferOffset
               indirectBuffer:(id<MTLBuffer>)indirectBuffer
         indirectBufferOffset:(NSUInteger)indirectBufferOffset API_AVAILABLE(macos(10.11), ios(9.0))
{
  GetWrapped(self)->drawIndexedPrimitives((MTL::PrimitiveType)primitiveType,
                                          (MTL::IndexType)indexType, GetWrapped(indexBuffer),
                                          indexBufferOffset, GetWrapped(indirectBuffer),
                                          indirectBufferOffset);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-implementations"
- (void)textureBarrier API_DEPRECATED_WITH_REPLACEMENT(
    "memoryBarrierWithScope:MTLBarrierScopeRenderTargets", macos(10.11, 10.14))API_UNAVAILABLE(ios)
{
  GetWrapped(self)->textureBarrier();
}
#pragma clang diagnostic pop

- (void)updateFence:(id<MTLFence>)fence
        afterStages:(MTLRenderStages)stages API_AVAILABLE(macos(10.13), ios(10.0))
{
  GetWrapped(self)->updateFence(GetWrapped(fence), (MTL::RenderStages)stages);
}

- (void)waitForFence:(id<MTLFence>)fence
        beforeStages:(MTLRenderStages)stages API_AVAILABLE(macos(10.13), ios(10.0))
{
  GetWrapped(self)->waitForFence(GetWrapped(fence), (MTL::RenderStages)stages);
}

- (void)setTessellationFactorBuffer:(nullable id<MTLBuffer>)buffer
                             offset:(NSUInteger)offset
                     instanceStride:(NSUInteger)instanceStride API_AVAILABLE(macos(10.12), ios(10.0))
{
  GetWrapped(self)->setTessellationFactorBuffer(GetWrapped(buffer), offset, instanceStride);
}

- (void)setTessellationFactorScale:(float)scale API_AVAILABLE(macos(10.12), ios(10.0))
{
  GetWrapped(self)->setTessellationFactorScale(scale);
}

- (void)drawPatches:(NSUInteger)numberOfPatchControlPoints
                patchStart:(NSUInteger)patchStart
                patchCount:(NSUInteger)patchCount
          patchIndexBuffer:(nullable id<MTLBuffer>)patchIndexBuffer
    patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset
             instanceCount:(NSUInteger)instanceCount
              baseInstance:(NSUInteger)baseInstance API_AVAILABLE(macos(10.12), ios(10.0))
{
  GetWrapped(self)->drawPatches(numberOfPatchControlPoints, patchStart, patchCount,
                                GetWrapped(patchIndexBuffer), patchIndexBufferOffset,
                                instanceCount, baseInstance);
}

- (void)drawPatches:(NSUInteger)numberOfPatchControlPoints
          patchIndexBuffer:(nullable id<MTLBuffer>)patchIndexBuffer
    patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset
            indirectBuffer:(id<MTLBuffer>)indirectBuffer
      indirectBufferOffset:(NSUInteger)indirectBufferOffset
    API_AVAILABLE(macos(10.12), ios(12.0), tvos(14.5))
{
  GetWrapped(self)->drawPatchesIndirect(numberOfPatchControlPoints, GetWrapped(patchIndexBuffer),
                                         patchIndexBufferOffset, GetWrapped(indirectBuffer),
                                         indirectBufferOffset);
}

- (void)drawIndexedPatches:(NSUInteger)numberOfPatchControlPoints
                       patchStart:(NSUInteger)patchStart
                       patchCount:(NSUInteger)patchCount
                 patchIndexBuffer:(nullable id<MTLBuffer>)patchIndexBuffer
           patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset
          controlPointIndexBuffer:(id<MTLBuffer>)controlPointIndexBuffer
    controlPointIndexBufferOffset:(NSUInteger)controlPointIndexBufferOffset
                    instanceCount:(NSUInteger)instanceCount
                     baseInstance:(NSUInteger)baseInstance API_AVAILABLE(macos(10.12), ios(10.0))
{
  GetWrapped(self)->drawIndexedPatches(numberOfPatchControlPoints, patchStart, patchCount,
                                       GetWrapped(patchIndexBuffer), patchIndexBufferOffset,
                                       GetWrapped(controlPointIndexBuffer),
                                       controlPointIndexBufferOffset, instanceCount, baseInstance);
}

- (void)drawIndexedPatches:(NSUInteger)numberOfPatchControlPoints
                 patchIndexBuffer:(nullable id<MTLBuffer>)patchIndexBuffer
           patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset
          controlPointIndexBuffer:(id<MTLBuffer>)controlPointIndexBuffer
    controlPointIndexBufferOffset:(NSUInteger)controlPointIndexBufferOffset
                   indirectBuffer:(id<MTLBuffer>)indirectBuffer
             indirectBufferOffset:(NSUInteger)indirectBufferOffset
    API_AVAILABLE(macos(10.12), ios(12.0), tvos(14.5))
{
  GetWrapped(self)->drawIndexedPatchesIndirect(
      numberOfPatchControlPoints, GetWrapped(patchIndexBuffer), patchIndexBufferOffset,
      GetWrapped(controlPointIndexBuffer), controlPointIndexBufferOffset,
      GetWrapped(indirectBuffer), indirectBufferOffset);
}

- (NSUInteger)tileWidth API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  return self.real.tileWidth;
}

- (NSUInteger)tileHeight API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  return self.real.tileHeight;
}

- (void)setTileBytes:(const void *)bytes
              length:(NSUInteger)length
             atIndex:(NSUInteger)index
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  rdcarray<byte> data;
  if(bytes && length) data.assign((const byte *)bytes, length);
  GetWrapped(self)->setTileBytes(data, index);
}

- (void)setTileBuffer:(nullable id<MTLBuffer>)buffer
               offset:(NSUInteger)offset
              atIndex:(NSUInteger)index
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  GetWrapped(self)->setTileBuffer(GetWrapped(buffer), offset, index);
}

- (void)setTileBufferOffset:(NSUInteger)offset
                    atIndex:(NSUInteger)index
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  GetWrapped(self)->setTileBufferOffset(offset, index);
}

- (void)setTileBuffers:(const id<MTLBuffer> __nullable[__nonnull])buffers
               offsets:(const NSUInteger[__nonnull])offsets
             withRange:(NSRange)range
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  rdcarray<WrappedMTLBuffer *> wrapped;
  rdcarray<NS::UInteger> capturedOffsets;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(buffers[i]));
    capturedOffsets.push_back(offsets[i]);
  }
  GetWrapped(self)->setTileBuffers(wrapped, capturedOffsets,
                                    NS::Range::Make(range.location, range.length));
}

- (void)setTileTexture:(nullable id<MTLTexture>)texture
               atIndex:(NSUInteger)index
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  GetWrapped(self)->setTileTextures({GetWrapped(texture)}, NS::Range::Make(index, 1), 0);
}

- (void)setTileTextures:(const id<MTLTexture> __nullable[__nonnull])textures
              withRange:(NSRange)range
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  rdcarray<WrappedMTLTexture *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++) wrapped.push_back(GetWrapped(textures[i]));
  GetWrapped(self)->setTileTextures(wrapped, NS::Range::Make(range.location, range.length), 1);
}

- (void)setTileSamplerState:(nullable id<MTLSamplerState>)sampler
                    atIndex:(NSUInteger)index
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  GetWrapped(self)->setTileSamplers({GetWrapped(sampler)}, {}, {}, NS::Range::Make(index, 1), 0);
}

- (void)setTileSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
                   withRange:(NSRange)range
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  for(NSUInteger i = 0; i < range.length; i++) wrapped.push_back(GetWrapped(samplers[i]));
  GetWrapped(self)->setTileSamplers(wrapped, {}, {}, NS::Range::Make(range.location, range.length), 1);
}

- (void)setTileSamplerState:(nullable id<MTLSamplerState>)sampler
                lodMinClamp:(float)lodMinClamp
                lodMaxClamp:(float)lodMaxClamp
                    atIndex:(NSUInteger)index
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  GetWrapped(self)->setTileSamplers({GetWrapped(sampler)}, {lodMinClamp}, {lodMaxClamp},
                                    NS::Range::Make(index, 1), 2);
}

- (void)setTileSamplerStates:(const id<MTLSamplerState> __nullable[__nonnull])samplers
                lodMinClamps:(const float[__nonnull])lodMinClamps
                lodMaxClamps:(const float[__nonnull])lodMaxClamps
                   withRange:(NSRange)range
    API_AVAILABLE(ios(11.0), tvos(14.5), macos(11.0), macCatalyst(14.0))
{
  rdcarray<WrappedMTLSamplerState *> wrapped;
  rdcarray<float> minClamps, maxClamps;
  for(NSUInteger i = 0; i < range.length; i++)
  {
    wrapped.push_back(GetWrapped(samplers[i]));
    minClamps.push_back(lodMinClamps[i]);
    maxClamps.push_back(lodMaxClamps[i]);
  }
  GetWrapped(self)->setTileSamplers(wrapped, minClamps, maxClamps,
                                    NS::Range::Make(range.location, range.length), 3);
}

- (void)setTileVisibleFunctionTable:(nullable id<MTLVisibleFunctionTable>)functionTable
                      atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(functionTable && ![functionTable isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
  {
    RDCERR("Cannot capture unwrapped Metal tile visible function table");
    return;
  }
  GetWrapped(self)->setTileVisibleFunctionTable(
      functionTable ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)functionTable) : NULL,
      (uint32_t)bufferIndex);
}

- (void)setTileVisibleFunctionTables:(const id<MTLVisibleFunctionTable> __nullable[__nonnull])functionTables
                     withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(range.length == 0 || range.length > 31 || !functionTables)
  {
    RDCERR("Unsupported Metal tile visible-function-table range");
    return;
  }
  rdcarray<WrappedMTLVisibleFunctionTable *> wrapped;
  wrapped.reserve(range.length);
  for(NSUInteger i = 0; i < range.length; i++)
  {
    id<MTLVisibleFunctionTable> table = functionTables[i];
    if(table && ![table isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
    {
      RDCERR("Cannot capture unwrapped Metal tile visible function table");
      return;
    }
    wrapped.push_back(table ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)table) : NULL);
  }
  GetWrapped(self)->setTileVisibleFunctionTables(wrapped,
      NS::Range::Make(range.location, range.length));
}

- (void)setTileIntersectionFunctionTable:
            (nullable id<MTLIntersectionFunctionTable>)intersectionFunctionTable
                           atBufferIndex:(NSUInteger)bufferIndex
    API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(!intersectionFunctionTable ||
     ![intersectionFunctionTable isKindOfClass:[ObjCBridgeMTLIntersectionFunctionTable class]])
  {
    RDCERR("Unsupported or unwrapped Metal tile intersection function table");
    return;
  }
  GetWrapped(self)->setTileIntersectionFunctionTable(
      GetWrapped((ObjCBridgeMTLIntersectionFunctionTable *)intersectionFunctionTable),
      (uint32_t)bufferIndex);
}

- (void)setTileIntersectionFunctionTables:
            (const id<MTLIntersectionFunctionTable> __nullable[__nonnull])intersectionFunctionTable
                          withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(!intersectionFunctionTable || range.length == 0 || range.length > 31 ||
     range.location > 31 - range.length)
  {
    RDCERR("Unsupported Metal tile intersection-table range");
    return;
  }
  rdcarray<WrappedMTLIntersectionFunctionTable *> wrapped;
  for(NSUInteger i = 0; i < range.length; ++i)
  {
    id<MTLIntersectionFunctionTable> table = intersectionFunctionTable[i];
    if(!table || ![table isKindOfClass:[ObjCBridgeMTLIntersectionFunctionTable class]])
    {
      RDCERR("Unsupported Metal tile intersection-table member");
      return;
    }
    wrapped.push_back(GetWrapped((ObjCBridgeMTLIntersectionFunctionTable *)table));
  }
  GetWrapped(self)->setIntersectionFunctionTables(wrapped,
      NS::Range::Make(range.location, range.length), MTL::RenderStageTile);
}

- (void)setTileAccelerationStructure:(nullable id<MTLAccelerationStructure>)accelerationStructure
                       atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0))
{
  if(accelerationStructure &&
     ![(id)accelerationStructure isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
    METAL_NOT_HOOKED();
  GetWrapped(self)->setTileAccelerationStructure(
      GetWrapped((ObjCBridgeMTLAccelerationStructure *)accelerationStructure), bufferIndex);
}

- (void)dispatchThreadsPerTile:(MTLSize)threadsPerTile
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  GetWrapped(self)->dispatchThreadsPerTile((MTL::Size &)threadsPerTile);
}

- (void)setThreadgroupMemoryLength:(NSUInteger)length
                            offset:(NSUInteger)offset
                           atIndex:(NSUInteger)index
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  GetWrapped(self)->setThreadgroupMemoryLength(length, offset, index);
}

- (void)useResource:(id<MTLResource>)resource
              usage:(MTLResourceUsage)usage API_AVAILABLE(macos(10.13), ios(11.0))
{
  id<MTLResource> proxy = MetalWrappedResource(resource);
  if(!proxy)
  {
    RDCERR("Cannot capture Metal render resource %p class %s without a wrapper",
           resource, resource ? object_getClassName(resource) : "nil");
    return;
  }
  GetWrapped(self)->useResource(GetWrapped(proxy), (MTL::ResourceUsage)usage);
}

- (void)useResources:(const id<MTLResource> __nonnull[__nonnull])resources
               count:(NSUInteger)count
               usage:(MTLResourceUsage)usage API_AVAILABLE(macos(10.13), ios(11.0))
{
  if(count > 4096 || (count && !resources))
  {
    RDCERR("Invalid Metal render resource array");
    return;
  }
  rdcarray<WrappedMTLResource *> wrapped;
  for(NSUInteger i = 0; i < count; i++)
  {
    id<MTLResource> proxy = MetalWrappedResource(resources[i]);
    if(!proxy)
    {
      RDCERR("Cannot capture Metal render resource array entry %lu: %p class %s without a wrapper",
             (unsigned long)i, resources[i], resources[i] ? object_getClassName(resources[i]) : "nil");
      return;
    }
    wrapped.push_back(GetWrapped(proxy));
  }
  GetWrapped(self)->useResources(wrapped, (MTL::ResourceUsage)usage);
}

- (void)useResource:(id<MTLResource>)resource
              usage:(MTLResourceUsage)usage
             stages:(MTLRenderStages)stages API_AVAILABLE(macos(10.15), ios(13.0))
{
  id<MTLResource> proxy = MetalWrappedResource(resource);
  if(!proxy)
  {
    RDCERR("Cannot capture staged Metal render resource %p class %s without a wrapper",
           resource, resource ? object_getClassName(resource) : "nil");
    return;
  }
  GetWrapped(self)->useResourceWithStages(GetWrapped(proxy), (MTL::ResourceUsage)usage,
                                          (MTL::RenderStages)stages);
}

- (void)useResources:(const id<MTLResource> __nonnull[__nonnull])resources
               count:(NSUInteger)count
               usage:(MTLResourceUsage)usage
              stages:(MTLRenderStages)stages API_AVAILABLE(macos(10.15), ios(13.0))
{
  if(count > 4096 || (count && !resources))
  {
    RDCERR("Invalid staged Metal render resource array");
    return;
  }
  rdcarray<WrappedMTLResource *> wrapped;
  for(NSUInteger i = 0; i < count; i++)
  {
    id<MTLResource> proxy = MetalWrappedResource(resources[i]);
    if(!proxy)
    {
      RDCERR("Cannot capture staged Metal render resource array entry %lu: %p class %s without a wrapper",
             (unsigned long)i, resources[i], resources[i] ? object_getClassName(resources[i]) : "nil");
      return;
    }
    wrapped.push_back(GetWrapped(proxy));
  }
  GetWrapped(self)->useResourcesWithStages(wrapped, (MTL::ResourceUsage)usage,
                                           (MTL::RenderStages)stages);
}

- (void)useHeap:(id<MTLHeap>)heap API_AVAILABLE(macos(10.13), ios(11.0))
{
  id<MTLHeap> proxy = MetalWrappedHeap(heap);
  if(!proxy || ![proxy isKindOfClass:[ObjCBridgeMTLHeap class]])
  {
    RDCERR("Cannot capture Metal render heap %p class %s without a wrapper",
           heap, heap ? object_getClassName(heap) : "nil");
    return;
  }
  GetWrapped(self)->declareHeaps({GetWrapped((ObjCBridgeMTLHeap *)proxy)}, MTL::RenderStageVertex, 0);
}

- (void)useHeaps:(const id<MTLHeap> __nonnull[__nonnull])heaps
           count:(NSUInteger)count API_AVAILABLE(macos(10.13), ios(11.0))
{
  rdcarray<WrappedMTLHeap *> wrapped;
  if(count > 32 || (count && !heaps))
  {
    RDCERR("Invalid Metal render heap array shape");
    return;
  }
  for(NSUInteger i = 0; i < count; i++)
  {
    id<MTLHeap> proxy = MetalWrappedHeap(heaps[i]);
    if(!proxy || ![proxy isKindOfClass:[ObjCBridgeMTLHeap class]])
    {
      RDCERR("Cannot capture Metal render heap array entry %lu: %p class %s without a wrapper",
             (unsigned long)i, heaps[i], heaps[i] ? object_getClassName(heaps[i]) : "nil");
      return;
    }
    wrapped.push_back(GetWrapped((ObjCBridgeMTLHeap *)proxy));
  }
  GetWrapped(self)->declareHeaps(wrapped, MTL::RenderStageVertex, 2);
}

- (void)useHeap:(id<MTLHeap>)heap
         stages:(MTLRenderStages)stages API_AVAILABLE(macos(10.15), ios(13.0))
{
  id<MTLHeap> proxy = MetalWrappedHeap(heap);
  if(!proxy || ![proxy isKindOfClass:[ObjCBridgeMTLHeap class]])
  {
    RDCERR("Cannot capture Metal render heap %p class %s without a wrapper",
           heap, heap ? object_getClassName(heap) : "nil");
    return;
  }
  GetWrapped(self)->declareHeaps({GetWrapped((ObjCBridgeMTLHeap *)proxy)}, (MTL::RenderStages)stages, 1);
}

- (void)useHeaps:(const id<MTLHeap> __nonnull[__nonnull])heaps
           count:(NSUInteger)count
          stages:(MTLRenderStages)stages API_AVAILABLE(macos(10.15), ios(13.0))
{
  rdcarray<WrappedMTLHeap *> wrapped;
  if(count > 32 || (count && !heaps))
  {
    RDCERR("Invalid Metal render heap array shape");
    return;
  }
  for(NSUInteger i = 0; i < count; i++)
  {
    id<MTLHeap> proxy = MetalWrappedHeap(heaps[i]);
    if(!proxy || ![proxy isKindOfClass:[ObjCBridgeMTLHeap class]])
    {
      RDCERR("Cannot capture Metal render heap array entry %lu: %p class %s without a wrapper",
             (unsigned long)i, heaps[i], heaps[i] ? object_getClassName(heaps[i]) : "nil");
      return;
    }
    wrapped.push_back(GetWrapped((ObjCBridgeMTLHeap *)proxy));
  }
  GetWrapped(self)->declareHeaps(wrapped, (MTL::RenderStages)stages, 3);
}

- (void)executeCommandsInBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandBuffer
                      withRange:(NSRange)executionRange API_AVAILABLE(macos(10.14), ios(12.0))
{
  GetWrapped(self)->executeCommandsInBuffer(GetWrapped(indirectCommandBuffer),
                                             NS::Range::Make(executionRange.location,
                                                             executionRange.length));
}

- (void)executeCommandsInBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandbuffer
                 indirectBuffer:(id<MTLBuffer>)indirectRangeBuffer
           indirectBufferOffset:(NSUInteger)indirectBufferOffset
    API_AVAILABLE(macos(10.14), macCatalyst(13.0), ios(13.0))
{
  GetWrapped(self)->executeCommandsInBufferIndirect(GetWrapped(indirectCommandbuffer),
                                                     GetWrapped(indirectRangeBuffer),
                                                     indirectBufferOffset);
}

- (void)memoryBarrierWithScope:(MTLBarrierScope)scope
                   afterStages:(MTLRenderStages)after
                  beforeStages:(MTLRenderStages)before
    API_AVAILABLE(macos(10.14), macCatalyst(13.0), ios(16.0))
{
  GetWrapped(self)->memoryBarrierWithScope((MTL::BarrierScope)scope, (MTL::RenderStages)after,
                                            (MTL::RenderStages)before);
}

- (void)memoryBarrierWithResources:(const id<MTLResource> __nonnull[__nonnull])resources
                             count:(NSUInteger)count
                       afterStages:(MTLRenderStages)after
                      beforeStages:(MTLRenderStages)before
    API_AVAILABLE(macos(10.14), macCatalyst(13.0), ios(16.0))
{
  rdcarray<WrappedMTLResource *> wrapped;
  for(NSUInteger i = 0; i < count; i++)
    wrapped.push_back(GetWrapped(resources[i]));
  GetWrapped(self)->memoryBarrierWithResources(wrapped, (MTL::RenderStages)after,
                                                (MTL::RenderStages)before);
}

- (void)sampleCountersInBuffer:(id<MTLCounterSampleBuffer>)sampleBuffer
                 atSampleIndex:(NSUInteger)sampleIndex
                   withBarrier:(BOOL)barrier API_AVAILABLE(macos(10.15), ios(14.0))
{
  METAL_NOT_HOOKED();
  return [self.real sampleCountersInBuffer:sampleBuffer
                             atSampleIndex:sampleIndex
                               withBarrier:barrier];
}

@end
