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

#include "metal_argument_encoder.h"
#include "metal_types_bridge.h"

@implementation ObjCBridgeMTLArgumentEncoder

- (id<MTLArgumentEncoder>)real
{
  return id<MTLArgumentEncoder>(Unwrap(GetWrapped(self)));
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-missing-super-calls"
- (void)dealloc
{
  DeallocateObjCBridge(GetWrapped(self));
}
#pragma clang diagnostic pop

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL selector = [invocation selector];
  if([self.real respondsToSelector:selector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}

- (id<MTLDevice>)device
{
  return id<MTLDevice>(GetWrapped(self)->GetDevice());
}

- (nullable NSString *)label
{
  return self.real.label;
}

- (void)setLabel:(nullable NSString *)label
{
  self.real.label = label;
}

- (NSUInteger)encodedLength
{
  return self.real.encodedLength;
}

- (NSUInteger)alignment API_AVAILABLE(macos(10.15), ios(13.0))
{
  return self.real.alignment;
}

- (void)setArgumentBuffer:(nullable id<MTLBuffer>)argumentBuffer offset:(NSUInteger)offset
{
  GetWrapped(self)->setArgumentBuffer(GetWrapped(argumentBuffer), offset);
}

- (void)setArgumentBuffer:(nullable id<MTLBuffer>)argumentBuffer
               startOffset:(NSUInteger)startOffset
              arrayElement:(NSUInteger)arrayElement API_AVAILABLE(macos(10.14), ios(12.0))
{
  METAL_NOT_HOOKED();
  return [self.real setArgumentBuffer:argumentBuffer
                          startOffset:startOffset
                         arrayElement:arrayElement];
}

- (void)setBuffer:(nullable id<MTLBuffer>)buffer
            offset:(NSUInteger)offset
           atIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
  return [self.real setBuffer:buffer offset:offset atIndex:index];
}

- (void)setBuffers:(const id<MTLBuffer> _Nullable [_Nonnull])buffers
           offsets:(const NSUInteger[_Nonnull])offsets
         withRange:(NSRange)range
{
  METAL_NOT_HOOKED();
  return [self.real setBuffers:buffers offsets:offsets withRange:range];
}

- (void)setTexture:(nullable id<MTLTexture>)texture atIndex:(NSUInteger)index
{
  GetWrapped(self)->setTexture(GetWrapped(texture), index);
}

- (void)setTextures:(const id<MTLTexture> _Nullable [_Nonnull])textures
           withRange:(NSRange)range
{
  METAL_NOT_HOOKED();
  return [self.real setTextures:textures withRange:range];
}

- (void)setSamplerState:(nullable id<MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
  GetWrapped(self)->setSamplerState(GetWrapped(sampler), index);
}

- (void)setSamplerStates:(const id<MTLSamplerState> _Nullable [_Nonnull])samplers
                withRange:(NSRange)range
{
  METAL_NOT_HOOKED();
  return [self.real setSamplerStates:samplers withRange:range];
}

- (void *)constantDataAtIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
  return [self.real constantDataAtIndex:index];
}

- (nullable id<MTLArgumentEncoder>)newArgumentEncoderForBufferAtIndex:(NSUInteger)index
{
  METAL_NOT_HOOKED();
  return [self.real newArgumentEncoderForBufferAtIndex:index];
}

@end
