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
#include "metal_visible_function_table.h"
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
  GetWrapped(self)->setArgumentBufferArray(GetWrapped(argumentBuffer), startOffset, arrayElement);
}

- (void)setBuffer:(nullable id<MTLBuffer>)buffer
            offset:(NSUInteger)offset
           atIndex:(NSUInteger)index
{
  GetWrapped(self)->setBuffer(GetWrapped(buffer), offset, index);
}

- (void)setBuffers:(const id<MTLBuffer> _Nullable [_Nonnull])buffers
           offsets:(const NSUInteger[_Nonnull])offsets
         withRange:(NSRange)range
{
  for(NSUInteger i = 0; i < range.length; i++)
    GetWrapped(self)->setBuffer(GetWrapped(buffers[i]), offsets[i], range.location + i);
}

- (void)setTexture:(nullable id<MTLTexture>)texture atIndex:(NSUInteger)index
{
  GetWrapped(self)->setTexture(GetWrapped(texture), index);
}

- (void)setTextures:(const id<MTLTexture> _Nullable [_Nonnull])textures
           withRange:(NSRange)range
{
  // Normalize the contiguous batch to the established per-member capture format.
  // A zero-length range must not dereference the caller's array.
  for(NSUInteger i = 0; i < range.length; i++)
    GetWrapped(self)->setTexture(GetWrapped(textures[i]), range.location + i);
}

- (void)setSamplerState:(nullable id<MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
  GetWrapped(self)->setSamplerState(GetWrapped(sampler), index);
}

- (void)setSamplerStates:(const id<MTLSamplerState> _Nullable [_Nonnull])samplers
                withRange:(NSRange)range
{
  for(NSUInteger i = 0; i < range.length; i++)
    GetWrapped(self)->setSamplerState(GetWrapped(samplers[i]), range.location + i);
}

- (void)setVisibleFunctionTable:(nullable id<MTLVisibleFunctionTable>)table
                        atIndex:(NSUInteger)index API_AVAILABLE(macos(11.0), ios(14.0))
{
  if(table && ![table isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
  {
    RDCERR("Cannot capture unwrapped Metal argument visible function table");
    return;
  }
  GetWrapped(self)->setVisibleFunctionTable(
      table ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)table) : NULL, index);
}

- (void)setVisibleFunctionTables:(const id<MTLVisibleFunctionTable> [])tables
                       withRange:(NSRange)range API_AVAILABLE(macos(11.0), ios(14.0))
{
  if(range.length > 32 || range.location > 32 - range.length ||
     (range.length && !tables))
  {
    RDCERR("Unsupported Metal argument visible-function-table range");
    return;
  }
  for(NSUInteger i = 0; i < range.length; i++)
    if(tables[i] && ![tables[i] isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]])
    {
      RDCERR("Cannot capture unwrapped Metal argument visible function table");
      return;
    }
  for(NSUInteger i = 0; i < range.length; i++)
    GetWrapped(self)->setVisibleFunctionTable(
        tables[i] ? GetWrapped((ObjCBridgeMTLVisibleFunctionTable *)tables[i]) : NULL,
        range.location + i);
}

- (void *)constantDataAtIndex:(NSUInteger)index
{
  return GetWrapped(self)->constantDataAtIndex(index);
}

- (nullable id<MTLArgumentEncoder>)newArgumentEncoderForBufferAtIndex:(NSUInteger)index
{
  return id<MTLArgumentEncoder>(GetWrapped(self)->newArgumentEncoder(index));
}

@end
