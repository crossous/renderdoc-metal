// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_heap.h"
#include "metal_buffer.h"
#include "metal_types_bridge.h"

void MetalAssociateHeapProxy(MTL::Heap *real, WrappedMTLHeap *wrapped)
{
  objc_setAssociatedObject((id)real, real, (id)wrapped, OBJC_ASSOCIATION_ASSIGN);
}

id<MTLHeap> MetalWrappedHeap(id<MTLHeap> real)
{
  if(!real) return nil;
  id proxy = objc_getAssociatedObject(real, real);
  return proxy ? id<MTLHeap>(proxy) : real;
}

id<MTLResource> MetalWrappedResource(id<MTLResource> resource)
{
  if(!resource) return nil;
  id proxy = resource;
  if(![proxy isKindOfClass:[ObjCBridgeMTLBuffer class]] &&
     ![proxy isKindOfClass:[ObjCBridgeMTLTexture class]] &&
     ![proxy isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]] &&
     ![proxy isKindOfClass:[ObjCBridgeMTLIntersectionFunctionTable class]] &&
     ![proxy isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
    proxy = objc_getAssociatedObject(resource, resource);
  if([proxy isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
     [proxy isKindOfClass:[ObjCBridgeMTLTexture class]] ||
     [proxy isKindOfClass:[ObjCBridgeMTLVisibleFunctionTable class]] ||
     [proxy isKindOfClass:[ObjCBridgeMTLIntersectionFunctionTable class]] ||
     [proxy isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
    return id<MTLResource>(proxy);
  return nil;
}

@implementation ObjCBridgeMTLHeap
- (id<MTLHeap>)real { return id<MTLHeap>(Unwrap(GetWrapped(self))); }
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
- (NSString *)label { return self.real.label; }
- (void)setLabel:(NSString *)label { self.real.label = label; }
- (id<MTLDevice>)device { return id<MTLDevice>(GetWrapped(self)->GetDevice()); }
- (MTLStorageMode)storageMode { return self.real.storageMode; }
- (MTLCPUCacheMode)cpuCacheMode { return self.real.cpuCacheMode; }
- (MTLHazardTrackingMode)hazardTrackingMode { return self.real.hazardTrackingMode; }
- (MTLResourceOptions)resourceOptions { return self.real.resourceOptions; }
- (NSUInteger)size { return self.real.size; }
- (NSUInteger)usedSize { return self.real.usedSize; }
- (NSUInteger)currentAllocatedSize { return self.real.currentAllocatedSize; }
- (NSUInteger)maxAvailableSizeWithAlignment:(NSUInteger)alignment
{
  return [self.real maxAvailableSizeWithAlignment:alignment];
}
- (MTLHeapType)type { return self.real.type; }
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options
{
  return id<MTLBuffer>(GetWrapped(self)->newBuffer(length, (MTL::ResourceOptions)options));
}
- (id<MTLAccelerationStructure>)newAccelerationStructureWithSize:(NSUInteger)size
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  return id<MTLAccelerationStructure>(GetWrapped(self)->newAccelerationStructure(size, 0, false));
}
- (id<MTLAccelerationStructure>)newAccelerationStructureWithSize:(NSUInteger)size
                                                         offset:(NSUInteger)offset
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  return id<MTLAccelerationStructure>(GetWrapped(self)->newAccelerationStructure(size, offset, true));
}
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options
                            offset:(NSUInteger)offset
{
  return id<MTLBuffer>(GetWrapped(self)->newBufferWithOffset(
      length, (MTL::ResourceOptions)options, offset));
}
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor
{
  RDMTL::TextureDescriptor captured((MTL::TextureDescriptor *)descriptor);
  return id<MTLTexture>(GetWrapped(self)->newTexture(captured));
}
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor
                                    offset:(NSUInteger)offset
{
  RDMTL::TextureDescriptor captured((MTL::TextureDescriptor *)descriptor);
  return id<MTLTexture>(GetWrapped(self)->newTextureWithOffset(captured, offset));
}
@end
