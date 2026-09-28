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

#include "metal_device.h"
#include <Availability.h>
#include "metal_command_queue.h"
#include "metal_library.h"
#include "metal_function.h"
#include "metal_render_pipeline_state.h"
#include "metal_compute_pipeline_state.h"
#include "metal_heap.h"
#include "metal_rate_map.h"
#include "metal_acceleration_structure.h"
#include "metal_buffer.h"
#include "metal_types_bridge.h"

bool SnapshotStitchedDescriptor(MTLStitchedLibraryDescriptor *descriptor,
                                       StitchedDescriptorSnapshot &snapshot)
{
  if(!descriptor || descriptor.functions.count != 1 || descriptor.functionGraphs.count != 1)
    return false;
  id<MTLFunction> function = descriptor.functions[0];
  MTLFunctionStitchingGraph *graph = descriptor.functionGraphs[0];
  if(![(id)function isKindOfClass:[ObjCBridgeMTLFunction class]] ||
     ![graph isKindOfClass:[MTLFunctionStitchingGraph class]] ||
     graph.nodes.count != 1 || graph.attributes.count || !graph.outputNode ||
     graph.outputNode != graph.nodes[0])
    return false;
  MTLFunctionStitchingFunctionNode *node = graph.outputNode;
  if(![node isKindOfClass:[MTLFunctionStitchingFunctionNode class]] ||
     node.arguments.count != 1 || node.controlDependencies.count ||
     ![node.arguments[0] isKindOfClass:[MTLFunctionStitchingInputNode class]] ||
     ((MTLFunctionStitchingInputNode *)node.arguments[0]).argumentIndex != 0 ||
     !node.name.length || !graph.functionName.length || node.name.length > 128 ||
     graph.functionName.length > 128 || ![node.name isEqualToString:function.name] ||
     function.functionType != MTLFunctionTypeVisible ||
     !graph.functionName.UTF8String || !node.name.UTF8String)
    return false;
  snapshot.function = GetWrapped((ObjCBridgeMTLFunction *)function);
  snapshot.graphName = graph.functionName.UTF8String;
  snapshot.functionName = node.name.UTF8String;
  return snapshot.function && snapshot.function->m_Type == eResFunction &&
         snapshot.function->m_Real;
}

// Keep the application's descriptor (and wrapped function references) alive until completion,
// but pass a separate descriptor containing native functions to Metal.
static NSArray<id<MTLDynamicLibrary>> *NativeDynamicLibraries(
    NSArray<id<MTLDynamicLibrary>> *libraries)
{
  NSMutableArray<id<MTLDynamicLibrary>> *native = [NSMutableArray arrayWithCapacity:libraries.count];
  for(id<MTLDynamicLibrary> library in libraries)
  {
    if([(id)library isKindOfClass:[ObjCBridgeMTLDynamicLibrary class]])
      [native addObject:id<MTLDynamicLibrary>(
          Unwrap(GetWrapped((ObjCBridgeMTLDynamicLibrary *)library)))];
    else
      [native addObject:library];
  }
  return native;
}

static NSArray<id<MTLBinaryArchive>> *NativeBinaryArchives(
    NSArray<id<MTLBinaryArchive>> *archives)
{
  NSMutableArray<id<MTLBinaryArchive>> *native = [NSMutableArray arrayWithCapacity:archives.count];
  for(id<MTLBinaryArchive> archive in archives)
  {
    if([(id)archive isKindOfClass:[ObjCBridgeMTLBinaryArchive class]])
      [native addObject:id<MTLBinaryArchive>(
          Unwrap(GetWrapped((ObjCBridgeMTLBinaryArchive *)archive)))];
    else
      [native addObject:archive];
  }
  return native;
}

static id<MTLBuffer> NativeAccelerationBuffer(id<MTLBuffer> buffer)
{
  if([(id)buffer isKindOfClass:[ObjCBridgeMTLBuffer class]])
    return id<MTLBuffer>(Unwrap(GetWrapped((ObjCBridgeMTLBuffer *)buffer)));
  return buffer;
}

// Size queries do not create a resource or need a replay chunk. Metal must still see
// native buffers in the descriptor rather than our application-facing wrappers.
// Keep this deliberately limited to static triangle/box geometry until the remaining
// acceleration-structure descriptor families have complete capture/replay support.
static MTLPrimitiveAccelerationStructureDescriptor *NativePrimitiveAccelerationDescriptor(
    MTLAccelerationStructureDescriptor *descriptor)
{
  if(![(id)descriptor isKindOfClass:[MTLPrimitiveAccelerationStructureDescriptor class]])
    return nil;
  MTLPrimitiveAccelerationStructureDescriptor *primitive =
      (MTLPrimitiveAccelerationStructureDescriptor *)descriptor;
  if(primitive.motionKeyframeCount > 1 || !primitive.geometryDescriptors.count)
    return nil;
  MTLPrimitiveAccelerationStructureDescriptor *native = [primitive copy];
  NSMutableArray *geometries = [NSMutableArray arrayWithCapacity:primitive.geometryDescriptors.count];
  for(MTLAccelerationStructureGeometryDescriptor *geometry in primitive.geometryDescriptors)
  {
    if(![(id)geometry isKindOfClass:[MTLAccelerationStructureTriangleGeometryDescriptor class]] &&
       ![(id)geometry isKindOfClass:[MTLAccelerationStructureBoundingBoxGeometryDescriptor class]])
    {
      [native release];
      return nil;
    }
    MTLAccelerationStructureGeometryDescriptor *nativeGeometry = [geometry copy];
    if([(id)nativeGeometry isKindOfClass:[MTLAccelerationStructureTriangleGeometryDescriptor class]])
    {
      MTLAccelerationStructureTriangleGeometryDescriptor *triangle =
          (MTLAccelerationStructureTriangleGeometryDescriptor *)nativeGeometry;
      triangle.vertexBuffer = NativeAccelerationBuffer(triangle.vertexBuffer);
      triangle.indexBuffer = NativeAccelerationBuffer(triangle.indexBuffer);
      if(@available(macOS 13.0, iOS 16.0, *))
        triangle.transformationMatrixBuffer =
            NativeAccelerationBuffer(triangle.transformationMatrixBuffer);
    }
    else
    {
      MTLAccelerationStructureBoundingBoxGeometryDescriptor *box =
          (MTLAccelerationStructureBoundingBoxGeometryDescriptor *)nativeGeometry;
      box.boundingBoxBuffer = NativeAccelerationBuffer(box.boundingBoxBuffer);
    }
    if(@available(macOS 13.0, iOS 16.0, *))
      nativeGeometry.primitiveDataBuffer =
          NativeAccelerationBuffer(nativeGeometry.primitiveDataBuffer);
    [geometries addObject:nativeGeometry];
    [nativeGeometry release];
  }
  native.geometryDescriptors = geometries;
  return native;
}

static MTLInstanceAccelerationStructureDescriptor *NativeInstanceDescriptor(
    MTLAccelerationStructureDescriptor *descriptor)
{
  if(![(id)descriptor isKindOfClass:[MTLInstanceAccelerationStructureDescriptor class]])
    return nil;
  MTLInstanceAccelerationStructureDescriptor *instance =
      (MTLInstanceAccelerationStructureDescriptor *)descriptor;
  MTLInstanceAccelerationStructureDescriptor *defaults =
      [MTLInstanceAccelerationStructureDescriptor descriptor];
  if(instance.usage != MTLAccelerationStructureUsageNone ||
     instance.instanceCount < 1 || instance.instanceCount > 65536 ||
     instance.instanceDescriptorBufferOffset != 0 ||
     instance.instanceDescriptorStride != defaults.instanceDescriptorStride ||
     instance.instanceDescriptorType != MTLAccelerationStructureInstanceDescriptorTypeDefault ||
     instance.motionTransformBuffer || instance.motionTransformCount != 0 ||
     instance.instancedAccelerationStructures.count < 1 ||
     instance.instancedAccelerationStructures.count > 4 ||
     instance.instanceCount < instance.instancedAccelerationStructures.count ||
     ![(id)instance.instanceDescriptorBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
     ![(id)instance.instancedAccelerationStructures[0]
         isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
    return nil;
  for(id wrappedChild in instance.instancedAccelerationStructures)
    if(![(id)wrappedChild isKindOfClass:[ObjCBridgeMTLAccelerationStructure class]])
      return nil;
  WrappedMTLBuffer *buffer =
      GetWrapped((ObjCBridgeMTLBuffer *)instance.instanceDescriptorBuffer);
  NSMutableArray<id<MTLAccelerationStructure>> *nativeChildren = [NSMutableArray array];
  rdcarray<WrappedMTLAccelerationStructure *> wrappedChildren;
  for(id wrappedChild in instance.instancedAccelerationStructures)
  {
    WrappedMTLAccelerationStructure *child =
        GetWrapped((ObjCBridgeMTLAccelerationStructure *)wrappedChild);
    if(!child || child->m_Type != eResAccelerationStructure || !Unwrap(child))
      return nil;
    for(WrappedMTLAccelerationStructure *previous : wrappedChildren)
      if(child == previous && instance.instancedAccelerationStructures.count > 1)
        return nil;
    wrappedChildren.push_back(child);
    [nativeChildren addObject:id<MTLAccelerationStructure>(Unwrap(child))];
  }
  if(!buffer || buffer->m_Type != eResBuffer || !Unwrap(buffer) ||
     Unwrap(buffer)->storageMode() != MTL::StorageModeShared ||
     Unwrap(buffer)->length() / sizeof(MTL::AccelerationStructureInstanceDescriptor) <
         instance.instanceCount ||
     wrappedChildren.empty())
    return nil;
  MTLInstanceAccelerationStructureDescriptor *native = [instance copy];
  native.instanceDescriptorBuffer = id<MTLBuffer>(Unwrap(buffer));
  native.instancedAccelerationStructures = nativeChildren;
  return native;
}

static MTLRenderPipelineDescriptor *AsyncRenderDescriptor(MTLRenderPipelineDescriptor *snapshot)
{
  MTLRenderPipelineDescriptor *real = [snapshot copy];
  RDMTL::RenderPipelineDescriptor captured((MTL::RenderPipelineDescriptor *)snapshot);
  real.vertexFunction = id<MTLFunction>(Unwrap(captured.vertexFunction));
  real.fragmentFunction = id<MTLFunction>(Unwrap(captured.fragmentFunction));
  if(snapshot.binaryArchives.count)
    real.binaryArchives = NativeBinaryArchives(snapshot.binaryArchives);
  MTL::LinkedFunctions *vertex = RDMTL::MetalNativeLinkedFunctions(
      (MTL::LinkedFunctions *)snapshot.vertexLinkedFunctions);
  MTL::LinkedFunctions *fragment = RDMTL::MetalNativeLinkedFunctions(
      (MTL::LinkedFunctions *)snapshot.fragmentLinkedFunctions);
  real.vertexLinkedFunctions = (MTLLinkedFunctions *)vertex;
  real.fragmentLinkedFunctions = (MTLLinkedFunctions *)fragment;
  if(snapshot.vertexPreloadedLibraries.count)
    real.vertexPreloadedLibraries = NativeDynamicLibraries(snapshot.vertexPreloadedLibraries);
  if(snapshot.fragmentPreloadedLibraries.count)
    real.fragmentPreloadedLibraries = NativeDynamicLibraries(snapshot.fragmentPreloadedLibraries);
  vertex->release(); fragment->release();
  return real;
}

static MTLComputePipelineDescriptor *AsyncComputeDescriptor(MTLComputePipelineDescriptor *snapshot)
{
  MTLComputePipelineDescriptor *real = [snapshot copy];
  RDMTL::ComputePipelineDescriptor captured((MTL::ComputePipelineDescriptor *)snapshot);
  real.computeFunction = id<MTLFunction>(Unwrap(captured.computeFunction));
  if(snapshot.binaryArchives.count)
    real.binaryArchives = NativeBinaryArchives(snapshot.binaryArchives);
  MTL::LinkedFunctions *links = MTL::LinkedFunctions::alloc()->init();
  captured.linkedFunctions.CopyTo(links);
  real.linkedFunctions = (MTLLinkedFunctions *)links;
  if(snapshot.preloadedLibraries.count)
    real.preloadedLibraries = NativeDynamicLibraries(snapshot.preloadedLibraries);
  links->release();
  return real;
}

bool MetalTileDescriptorSupported(MTLTileRenderPipelineDescriptor *descriptor,
                                  bool allowArchives)
{
  bool supported = (allowArchives || descriptor.binaryArchives.count == 0) &&
                   descriptor.preloadedLibraries.count == 0 &&
                   !descriptor.supportAddingBinaryFunctions &&
                   (!descriptor.linkedFunctions ||
                    (descriptor.linkedFunctions.functions.count <= 8 &&
                     descriptor.linkedFunctions.binaryFunctions.count == 0 &&
                     descriptor.linkedFunctions.privateFunctions.count == 0 &&
                     descriptor.linkedFunctions.groups.count == 0));
  MTLTileRenderPipelineDescriptor *defaults = [MTLTileRenderPipelineDescriptor new];
  supported = supported && descriptor.maxCallStackDepth == defaults.maxCallStackDepth;
  for(NSUInteger i = 0; i < 31; i++)
    supported = supported &&
                [descriptor.tileBuffers objectAtIndexedSubscript:i].mutability ==
                    [defaults.tileBuffers objectAtIndexedSubscript:i].mutability;
  [defaults release];
  return supported;
}

static rdcarray<WrappedMTLFunction *> TileVisibleFunctions(MTLTileRenderPipelineDescriptor *descriptor)
{
  rdcarray<WrappedMTLFunction *> result;
  for(id<MTLFunction> function in descriptor.linkedFunctions.functions)
    result.push_back(MetalFunctionIsWrapped((MTL::Function *)function) ?
                         GetWrapped((MTL::Function *)function) : NULL);
  return result;
}

bool MetalMeshDescriptorSupported(MTLMeshRenderPipelineDescriptor *descriptor,
                                  bool allowArchives)
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  MTLMeshRenderPipelineDescriptor *defaults = [MTLMeshRenderPipelineDescriptor new];
  const bool objectStage = descriptor.objectFunction != nil;
  bool supported = descriptor.meshFunction != nil &&
                   descriptor.fragmentFunction != nil && descriptor.rasterSampleCount == 1 &&
                   descriptor.payloadMemoryLength ==
                       (objectStage ? 16U : defaults.payloadMemoryLength) &&
                   descriptor.maxTotalThreadsPerObjectThreadgroup ==
                       (objectStage ? 32U : defaults.maxTotalThreadsPerObjectThreadgroup) &&
                   (objectStage ? descriptor.maxTotalThreadgroupsPerMeshGrid == 1U :
                                  descriptor.maxTotalThreadgroupsPerMeshGrid <= 1048575U) &&
                   descriptor.objectThreadgroupSizeIsMultipleOfThreadExecutionWidth ==
                       defaults.objectThreadgroupSizeIsMultipleOfThreadExecutionWidth &&
                   descriptor.meshThreadgroupSizeIsMultipleOfThreadExecutionWidth ==
                       defaults.meshThreadgroupSizeIsMultipleOfThreadExecutionWidth &&
                   descriptor.alphaToCoverageEnabled == defaults.alphaToCoverageEnabled &&
                   descriptor.alphaToOneEnabled == defaults.alphaToOneEnabled &&
                   descriptor.rasterizationEnabled == defaults.rasterizationEnabled &&
                   descriptor.maxVertexAmplificationCount == defaults.maxVertexAmplificationCount &&
                   descriptor.depthAttachmentPixelFormat == defaults.depthAttachmentPixelFormat &&
                   descriptor.stencilAttachmentPixelFormat == defaults.stencilAttachmentPixelFormat;
  if(@available(macOS 14.0, *))
    supported = supported &&
                descriptor.supportIndirectCommandBuffers == defaults.supportIndirectCommandBuffers &&
                (!descriptor.objectLinkedFunctions ||
                 descriptor.objectLinkedFunctions.functions.count == 0) &&
                (!descriptor.meshLinkedFunctions ||
                 descriptor.meshLinkedFunctions.functions.count == 0) &&
                (!descriptor.fragmentLinkedFunctions ||
                 descriptor.fragmentLinkedFunctions.functions.count == 0);
  if(@available(macOS 15.0, *))
    supported = supported && (allowArchives || descriptor.binaryArchives.count == 0);
  for(NSUInteger i = 0; i < 31; i++)
    supported = supported &&
                [descriptor.objectBuffers objectAtIndexedSubscript:i].mutability ==
                    [defaults.objectBuffers objectAtIndexedSubscript:i].mutability &&
                [descriptor.meshBuffers objectAtIndexedSubscript:i].mutability ==
                    [defaults.meshBuffers objectAtIndexedSubscript:i].mutability &&
                [descriptor.fragmentBuffers objectAtIndexedSubscript:i].mutability ==
                    [defaults.fragmentBuffers objectAtIndexedSubscript:i].mutability;
  for(NSUInteger i = 0; i < 8; i++)
    supported = supported &&
                ![descriptor.colorAttachments objectAtIndexedSubscript:i].blendingEnabled &&
                [descriptor.colorAttachments objectAtIndexedSubscript:i].writeMask ==
                    [defaults.colorAttachments objectAtIndexedSubscript:i].writeMask;
  [defaults release];
  return supported;
}

// Bridge for MTLDevice
@implementation ObjCBridgeMTLDevice

// ObjCBridgeMTLDevice specific
- (id<MTLDevice>)real
{
  return id<MTLDevice>(Unwrap(GetWrapped(self)));
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

// Use the real MTLDevice to find methods from messages
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
  id fwd = self.real;
  return [fwd methodSignatureForSelector:aSelector];
}

// Forward any unknown messages to the real MTLDevice
- (void)forwardInvocation:(NSInvocation *)invocation
{
  SEL aSelector = [invocation selector];

  if([self.real respondsToSelector:aSelector])
    [invocation invokeWithTarget:self.real];
  else
    [super forwardInvocation:invocation];
}

// MTLDevice : based on the protocol defined in
// Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.0.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLDevice.h

- (NSString *)name
{
  return self.real.name;
}

- (uint64_t)registryID API_AVAILABLE(macos(10.13), ios(11.0))
{
  return self.real.registryID;
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_14_0
- (MTLArchitecture *)architecture API_AVAILABLE(macos(14.0), ios(17.0))
{
  return self.real.architecture;
}
#endif

- (MTLSize)maxThreadsPerThreadgroup API_AVAILABLE(macos(10.11), ios(9.0))
{
  return self.real.maxThreadsPerThreadgroup;
}

- (BOOL)isLowPower API_AVAILABLE(macos(10.11), macCatalyst(13.0))API_UNAVAILABLE(ios)
{
  return self.real.lowPower;
}

- (BOOL)isHeadless API_AVAILABLE(macos(10.11), macCatalyst(13.0))API_UNAVAILABLE(ios)
{
  return self.real.headless;
}

- (BOOL)isRemovable API_AVAILABLE(macos(10.13), macCatalyst(13.0))API_UNAVAILABLE(ios)
{
  return self.real.removable;
}

- (BOOL)hasUnifiedMemory API_AVAILABLE(macos(10.15), ios(13.0))
{
  return self.real.hasUnifiedMemory;
}

- (uint64_t)recommendedMaxWorkingSetSize API_AVAILABLE(macos(10.12), macCatalyst(13.0), ios(16.0))
{
  return self.real.recommendedMaxWorkingSetSize;
}

- (MTLDeviceLocation)location API_AVAILABLE(macos(10.15))API_UNAVAILABLE(ios)
{
  return self.real.location;
}

- (NSUInteger)locationNumber API_AVAILABLE(macos(10.15))API_UNAVAILABLE(ios)
{
  return self.real.locationNumber;
}

- (uint64_t)maxTransferRate API_AVAILABLE(macos(10.15))API_UNAVAILABLE(ios)
{
  return self.real.maxTransferRate;
}

- (BOOL)isDepth24Stencil8PixelFormatSupported API_AVAILABLE(macos(10.11), macCatalyst(13.0))
    API_UNAVAILABLE(ios)
{
  return GetWrapped(self)->isDepth24Stencil8PixelFormatSupported();
}

- (MTLReadWriteTextureTier)readWriteTextureSupport API_AVAILABLE(macos(10.13), ios(11.0))
{
  return (MTLReadWriteTextureTier)GetWrapped(self)->readWriteTextureSupport();
}

- (MTLArgumentBuffersTier)argumentBuffersSupport API_AVAILABLE(macos(10.13), ios(11.0))
{
  return (MTLArgumentBuffersTier)GetWrapped(self)->argumentBuffersSupport();
}

- (BOOL)areRasterOrderGroupsSupported API_AVAILABLE(macos(10.13), ios(11.0))
{
  return GetWrapped(self)->areRasterOrderGroupsSupported();
}

- (BOOL)supports32BitFloatFiltering API_AVAILABLE(macos(11.0), ios(14.0))
{
  return GetWrapped(self)->supports32BitFloatFiltering();
}

- (BOOL)supports32BitMSAA API_AVAILABLE(macos(11.0), ios(14.0))
{
  return GetWrapped(self)->supports32BitMSAA();
}

- (BOOL)supportsQueryTextureLOD API_AVAILABLE(macos(11.0), ios(14.0))
{
  return GetWrapped(self)->supportsQueryTextureLOD();
}

- (BOOL)supportsBCTextureCompression API_AVAILABLE(macos(11.0), ios(16.4))
{
  return GetWrapped(self)->supportsBCTextureCompression();
}

- (BOOL)supportsPullModelInterpolation API_AVAILABLE(macos(11.0), ios(14.0))
{
  return GetWrapped(self)->supportsPullModelInterpolation();
}

- (BOOL)areBarycentricCoordsSupported API_DEPRECATED_WITH_REPLACEMENT(
    "supportsShaderBarycentricCoordinates", macos(10.15, 13.0), ios(14.0, 16.0))API_UNAVAILABLE(tvos)
{
  return GetWrapped(self)->areBarycentricCoordsSupported();
}

- (BOOL)supportsShaderBarycentricCoordinates API_AVAILABLE(macos(10.15), ios(14.0))
{
  return GetWrapped(self)->supportsShaderBarycentricCoordinates();
}

- (NSUInteger)currentAllocatedSize API_AVAILABLE(macos(10.13), ios(11.0))
{
  return self.real.currentAllocatedSize;
}

- (nullable id<MTLCommandQueue>)newCommandQueue
{
  return id<MTLCommandQueue>(GetWrapped(self)->newCommandQueue());
}

- (nullable id<MTLCommandQueue>)newCommandQueueWithMaxCommandBufferCount:(NSUInteger)maxCommandBufferCount
{
  return id<MTLCommandQueue>(GetWrapped(self)->newCommandQueue(maxCommandBufferCount));
}

- (MTLSizeAndAlign)heapTextureSizeAndAlignWithDescriptor:(MTLTextureDescriptor *)desc
    API_AVAILABLE(macos(10.13), ios(10.0))
{
  return [self.real heapTextureSizeAndAlignWithDescriptor:desc];
}

- (MTLSizeAndAlign)heapBufferSizeAndAlignWithLength:(NSUInteger)length
                                            options:(MTLResourceOptions)options
    API_AVAILABLE(macos(10.13), ios(10.0))
{
  return [self.real heapBufferSizeAndAlignWithLength:length options:options];
}

- (nullable id<MTLHeap>)newHeapWithDescriptor:(MTLHeapDescriptor *)descriptor
    API_AVAILABLE(macos(10.13), ios(10.0))
{
  id<MTLHeap> real = [self.real newHeapWithDescriptor:descriptor];
  return id<MTLHeap>(GetWrapped(self)->WrapNewHeap(
      (MTL::Heap *)real, descriptor.size, (MTL::StorageMode)descriptor.storageMode,
      (MTL::CPUCacheMode)descriptor.cpuCacheMode,
      (MTL::HazardTrackingMode)descriptor.hazardTrackingMode, (MTL::HeapType)descriptor.type));
}

- (nullable id<MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options
{
  return id<MTLBuffer>(GetWrapped(self)->newBufferWithLength(length, (MTL::ResourceOptions)options));
}

- (nullable id<MTLBuffer>)newBufferWithBytes:(const void *)pointer
                                      length:(NSUInteger)length
                                     options:(MTLResourceOptions)options
{
  return id<MTLBuffer>(
      GetWrapped(self)->newBufferWithBytes(pointer, length, (MTL::ResourceOptions)options));
}

- (nullable id<MTLBuffer>)newBufferWithBytesNoCopy:(void *)pointer
                                            length:(NSUInteger)length
                                           options:(MTLResourceOptions)options
                                       deallocator:(void (^__nullable)(void *pointer,
                                                                       NSUInteger length))deallocator
{
  id<MTLBuffer> real = [self.real newBufferWithBytesNoCopy:pointer
                                                    length:length
                                                   options:options
                                               deallocator:deallocator];
  if(!real) return nil;
  return id<MTLBuffer>(GetWrapped(self)->WrapNewBufferNoCopy(
      (MTL::Buffer *)real, pointer, length, (MTL::ResourceOptions)options));
}

- (nullable id<MTLDepthStencilState>)newDepthStencilStateWithDescriptor:
    (MTLDepthStencilDescriptor *)descriptor
{
  RDMTL::DepthStencilDescriptor rdDescriptor((MTL::DepthStencilDescriptor *)descriptor);
  return id<MTLDepthStencilState>(
      GetWrapped(self)->newDepthStencilStateWithDescriptor(rdDescriptor));
}

- (nullable id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor
{
  RDMTL::TextureDescriptor rdDescriptor((MTL::TextureDescriptor *)descriptor);
  return id<MTLTexture>(GetWrapped(self)->newTextureWithDescriptor(rdDescriptor));
}

- (nullable id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor
                                          iosurface:(IOSurfaceRef)iosurface
                                              plane:(NSUInteger)plane
    API_AVAILABLE(macos(10.11), ios(11.0))
{
  RDMTL::TextureDescriptor rdDescriptor((MTL::TextureDescriptor *)descriptor);
  return id<MTLTexture>(GetWrapped(self)->newTextureWithDescriptor(rdDescriptor, iosurface, plane));
}

- (nullable id<MTLTexture>)newSharedTextureWithDescriptor:(MTLTextureDescriptor *)descriptor
    API_AVAILABLE(macos(10.14), ios(13.0))
{
  RDMTL::TextureDescriptor rdDescriptor((MTL::TextureDescriptor *)descriptor);
  return id<MTLTexture>(GetWrapped(self)->newSharedTextureWithDescriptor(rdDescriptor));
}

- (nullable id<MTLTexture>)newSharedTextureWithHandle:(MTLSharedTextureHandle *)sharedHandle
    API_AVAILABLE(macos(10.14), ios(13.0))
{
  id<MTLTexture> source = MetalSharedTextureHandleSource(sharedHandle);
  id<MTLTexture> real = [self.real newSharedTextureWithHandle:sharedHandle];
  if(!real) return nil;
  return id<MTLTexture>(GetWrapped(self)->WrapNewSharedTextureWithHandle(
      (MTL::Texture *)real, source ? GetWrapped(source) : NULL));
}

- (nullable id<MTLSamplerState>)newSamplerStateWithDescriptor:(MTLSamplerDescriptor *)descriptor
{
  RDMTL::SamplerDescriptor rdDescriptor((MTL::SamplerDescriptor *)descriptor);
  return id<MTLSamplerState>(GetWrapped(self)->newSamplerStateWithDescriptor(rdDescriptor));
}

- (nullable id<MTLLibrary>)newDefaultLibrary
{
  return id<MTLLibrary>(GetWrapped(self)->newDefaultLibrary());
}

- (nullable id<MTLLibrary>)newDefaultLibraryWithBundle:(NSBundle *)bundle
                                                 error:(__autoreleasing NSError **)error
    API_AVAILABLE(macos(10.12), ios(10.0))
{
  return id<MTLLibrary>(GetWrapped(self)->newDefaultLibraryWithBundle(
      (NS::Bundle *)bundle, (NS::Error **)error));
}

- (nullable id<MTLLibrary>)newLibraryWithFile:(NSString *)filepath
                                        error:(__autoreleasing NSError **)error
    API_DEPRECATED("Use -newLibraryWithURL:error: instead", macos(10.11, 13.0), ios(8.0, 16.0))
{
  return id<MTLLibrary>(GetWrapped(self)->newLibraryWithFile(
      (NS::String *)filepath, (NS::Error **)error));
}

- (nullable id<MTLLibrary>)newLibraryWithURL:(NSURL *)url
                                       error:(__autoreleasing NSError **)error
    API_AVAILABLE(macos(10.13), ios(11.0))
{
  return id<MTLLibrary>(GetWrapped(self)->newLibraryWithURL((NS::URL *)url, (NS::Error **)error));
}

- (nullable id<MTLLibrary>)newLibraryWithData:(dispatch_data_t)data
                                        error:(__autoreleasing NSError **)error
{
  return id<MTLLibrary>(GetWrapped(self)->newLibraryWithData(data, (NS::Error **)error));
}

- (nullable id<MTLLibrary>)newLibraryWithSource:(NSString *)source
                                        options:(nullable MTLCompileOptions *)options
                                          error:(__autoreleasing NSError **)error
{
  return (id<MTLLibrary>)(GetWrapped(self)->newLibraryWithSource(
      (NS::String *)source, (MTL::CompileOptions *)options, (NS::Error **)error));
}

- (void)newLibraryWithSource:(NSString *)source
                     options:(nullable MTLCompileOptions *)options
           completionHandler:(MTLNewLibraryCompletionHandler)completionHandler
{
  NSString *snapshot = [source copy];
  MTLCompileOptions *optionSnapshot = [options copy];
  MTLCompileOptions *nativeOptions = [optionSnapshot copy];
  if(optionSnapshot.libraries.count)
  {
    NSMutableArray *nativeLibraries = [NSMutableArray arrayWithCapacity:optionSnapshot.libraries.count];
    for(id<MTLDynamicLibrary> dependency in optionSnapshot.libraries)
    {
      if([(id)dependency isKindOfClass:[ObjCBridgeMTLDynamicLibrary class]])
        [nativeLibraries addObject:id<MTLDynamicLibrary>(
            Unwrap(GetWrapped((ObjCBridgeMTLDynamicLibrary *)dependency)))];
      else
        [nativeLibraries addObject:dependency];
    }
    nativeOptions.libraries = nativeLibraries;
  }
  [self.real newLibraryWithSource:snapshot options:nativeOptions
      completionHandler:^(id<MTLLibrary> library, NSError *error) {
        id<MTLLibrary> wrapped = id<MTLLibrary>(GetWrapped(self)->CaptureAsyncLibrary(
            (MTL::Library *)library, (NS::String *)snapshot,
            (MTL::CompileOptions *)optionSnapshot, true));
        if(completionHandler) completionHandler(wrapped, error);
        [wrapped release];
      }];
  [snapshot release]; [optionSnapshot release]; [nativeOptions release];
}

- (nullable id<MTLLibrary>)newLibraryWithStitchedDescriptor:(MTLStitchedLibraryDescriptor *)descriptor
                                                      error:(__autoreleasing NSError **)error
    API_AVAILABLE(macos(12.0), ios(15.0))
{
  StitchedDescriptorSnapshot snapshot;
  if(!SnapshotStitchedDescriptor(descriptor, snapshot)) return nil;
  return id<MTLLibrary>(GetWrapped(self)->newStitchedLibrary(
      snapshot.function, snapshot.graphName, snapshot.functionName, 0, (NS::Error **)error));
}

- (void)newLibraryWithStitchedDescriptor:(MTLStitchedLibraryDescriptor *)descriptor
                       completionHandler:(MTLNewLibraryCompletionHandler)completionHandler
    API_AVAILABLE(macos(12.0), ios(15.0))
{
  StitchedDescriptorSnapshot snapshot;
  if(!SnapshotStitchedDescriptor(descriptor, snapshot)) { METAL_NOT_HOOKED(); return; }
  id<MTLFunction> function = descriptor.functions[0];
  MTLFunctionStitchingInputNode *nativeInput = [[MTLFunctionStitchingInputNode alloc]
      initWithArgumentIndex:0];
  MTLFunctionStitchingFunctionNode *nativeOutput = [[MTLFunctionStitchingFunctionNode alloc]
      initWithName:[NSString stringWithUTF8String:snapshot.functionName.c_str()]
         arguments:@[ nativeInput ] controlDependencies:@[]];
  MTLFunctionStitchingGraph *nativeGraph = [[MTLFunctionStitchingGraph alloc]
      initWithFunctionName:[NSString stringWithUTF8String:snapshot.graphName.c_str()]
      nodes:@[ nativeOutput ]
      outputNode:nativeOutput attributes:@[]];
  MTLStitchedLibraryDescriptor *native = [MTLStitchedLibraryDescriptor new];
  native.functions = @[ id<MTLFunction>(Unwrap(snapshot.function)) ];
  native.functionGraphs = @[ nativeGraph ];
  [function retain];
  [self.real newLibraryWithStitchedDescriptor:native
                          completionHandler:^(id<MTLLibrary> library, NSError *failure) {
    id<MTLLibrary> wrapped = id<MTLLibrary>(GetWrapped(self)->CaptureAsyncStitchedLibrary(
        (MTL::Library *)library, snapshot.function, snapshot.graphName, snapshot.functionName, 0));
    if(completionHandler) completionHandler(wrapped, failure);
    [wrapped release];
    [function release];
  }];
  [native release]; [nativeGraph release]; [nativeOutput release]; [nativeInput release];
}

- (nullable id<MTLRenderPipelineState>)
    newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor
                                   error:(__autoreleasing NSError **)error
{
  RDMTL::RenderPipelineDescriptor rdDescriptor((MTL::RenderPipelineDescriptor *)descriptor);
  return id<MTLRenderPipelineState>(
      GetWrapped(self)->newRenderPipelineStateWithDescriptor(rdDescriptor, (NS::Error **)error));
}

- (nullable id<MTLRenderPipelineState>)
    newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor
                                 options:(MTLPipelineOption)options
                              reflection:(MTLAutoreleasedRenderPipelineReflection *__nullable)reflection
                                   error:(__autoreleasing NSError **)error
{
  return id<MTLRenderPipelineState>(GetWrapped(self)->newRenderPipelineStateWithDescriptorOptions(
      (MTL::RenderPipelineDescriptor *)descriptor, (MTL::PipelineOption)options,
      (MTL::AutoreleasedRenderPipelineReflection *)reflection, (NS::Error **)error));
}

- (void)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor
                           completionHandler:(MTLNewRenderPipelineStateCompletionHandler)completionHandler
{
  MTLRenderPipelineDescriptor *snapshot = [descriptor copy];
  MTLRenderPipelineDescriptor *real = AsyncRenderDescriptor(snapshot);
  [self.real newRenderPipelineStateWithDescriptor:real
      completionHandler:^(id<MTLRenderPipelineState> pipeline, NSError *error) {
        id<MTLRenderPipelineState> wrapped = id<MTLRenderPipelineState>(
            GetWrapped(self)->CaptureAsyncRenderPipeline((MTL::RenderPipelineState *)pipeline,
                (MTL::RenderPipelineDescriptor *)snapshot, MTL::PipelineOptionNone,
                MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor_async));
        if(completionHandler) completionHandler(wrapped, error);
        [wrapped release];
      }];
  [real release]; [snapshot release];
}

- (void)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor
                                     options:(MTLPipelineOption)options
                           completionHandler:
                               (MTLNewRenderPipelineStateWithReflectionCompletionHandler)completionHandler
{
  MTLRenderPipelineDescriptor *snapshot = [descriptor copy];
  MTLRenderPipelineDescriptor *real = AsyncRenderDescriptor(snapshot);
  [self.real newRenderPipelineStateWithDescriptor:real options:options
      completionHandler:^(id<MTLRenderPipelineState> pipeline, MTLRenderPipelineReflection *reflection,
                           NSError *error) {
        id<MTLRenderPipelineState> wrapped = id<MTLRenderPipelineState>(
            GetWrapped(self)->CaptureAsyncRenderPipeline((MTL::RenderPipelineState *)pipeline,
                (MTL::RenderPipelineDescriptor *)snapshot, (MTL::PipelineOption)options,
                MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor_options_async));
        if(completionHandler) completionHandler(wrapped, reflection, error);
        [wrapped release];
      }];
  [real release]; [snapshot release];
}

- (nullable id<MTLComputePipelineState>)
    newComputePipelineStateWithFunction:(id<MTLFunction>)computeFunction
                                  error:(__autoreleasing NSError **)error
{
  return id<MTLComputePipelineState>(GetWrapped(self)->newComputePipelineStateWithFunction(
      GetWrapped(computeFunction), (NS::Error **)error));
}

- (nullable id<MTLComputePipelineState>)
    newComputePipelineStateWithFunction:(id<MTLFunction>)computeFunction
                                options:(MTLPipelineOption)options
                             reflection:(MTLAutoreleasedComputePipelineReflection *__nullable)reflection
                                  error:(__autoreleasing NSError **)error
{
  return id<MTLComputePipelineState>(GetWrapped(self)->newComputePipelineStateWithFunctionOptions(
      GetWrapped(computeFunction), (MTL::PipelineOption)options,
      (MTL::AutoreleasedComputePipelineReflection *)reflection, (NS::Error **)error));
}

- (void)newComputePipelineStateWithFunction:(id<MTLFunction>)computeFunction
                          completionHandler:(MTLNewComputePipelineStateCompletionHandler)completionHandler
{
  [self.real newComputePipelineStateWithFunction:id<MTLFunction>(Unwrap(GetWrapped(computeFunction)))
      completionHandler:^(id<MTLComputePipelineState> pipeline, NSError *error) {
        id<MTLComputePipelineState> wrapped = id<MTLComputePipelineState>(
            GetWrapped(self)->CaptureAsyncComputePipeline((MTL::ComputePipelineState *)pipeline,
                GetWrapped(computeFunction), MTL::PipelineOptionNone,
                MetalChunk::MTLDevice_newComputePipelineStateWithFunction_async));
        if(completionHandler) completionHandler(wrapped, error);
        [wrapped release];
      }];
}

- (void)newComputePipelineStateWithFunction:(id<MTLFunction>)computeFunction
                                    options:(MTLPipelineOption)options
                          completionHandler:
                              (MTLNewComputePipelineStateWithReflectionCompletionHandler)completionHandler
{
  [self.real newComputePipelineStateWithFunction:id<MTLFunction>(Unwrap(GetWrapped(computeFunction)))
      options:options completionHandler:^(id<MTLComputePipelineState> pipeline,
                                         MTLComputePipelineReflection *reflection, NSError *error) {
        id<MTLComputePipelineState> wrapped = id<MTLComputePipelineState>(
            GetWrapped(self)->CaptureAsyncComputePipeline((MTL::ComputePipelineState *)pipeline,
                GetWrapped(computeFunction), (MTL::PipelineOption)options,
                MetalChunk::MTLDevice_newComputePipelineStateWithFunction_options_async));
        if(completionHandler) completionHandler(wrapped, reflection, error);
        [wrapped release];
      }];
}

- (nullable id<MTLComputePipelineState>)
    newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor
                                  options:(MTLPipelineOption)options
                               reflection:(MTLAutoreleasedComputePipelineReflection *__nullable)reflection
                                    error:(__autoreleasing NSError **)error
    API_AVAILABLE(macos(10.11), ios(9.0))
{
  return id<MTLComputePipelineState>(GetWrapped(self)->newComputePipelineStateWithDescriptor(
      (MTL::ComputePipelineDescriptor *)descriptor, (MTL::PipelineOption)options,
      (MTL::AutoreleasedComputePipelineReflection *)reflection, (NS::Error **)error));
}

- (void)newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor
                                      options:(MTLPipelineOption)options
                            completionHandler:
                                (MTLNewComputePipelineStateWithReflectionCompletionHandler)completionHandler
    API_AVAILABLE(macos(10.11), ios(9.0))
{
  MTLComputePipelineDescriptor *snapshot = [descriptor copy];
  MTLComputePipelineDescriptor *real = AsyncComputeDescriptor(snapshot);
  [self.real newComputePipelineStateWithDescriptor:real options:options
      completionHandler:^(id<MTLComputePipelineState> pipeline,
                           MTLComputePipelineReflection *reflection, NSError *error) {
        id<MTLComputePipelineState> wrapped = id<MTLComputePipelineState>(
            GetWrapped(self)->CaptureAsyncComputeDescriptor((MTL::ComputePipelineState *)pipeline,
                (MTL::ComputePipelineDescriptor *)snapshot, (MTL::PipelineOption)options));
        if(completionHandler) completionHandler(wrapped, reflection, error);
        [wrapped release];
      }];
  [real release]; [snapshot release];
}

- (nullable id<MTLFence>)newFence API_AVAILABLE(macos(10.13), ios(10.0))
{
  return id<MTLFence>(GetWrapped(self)->newFence());
}

- (BOOL)supportsFeatureSet:(MTLFeatureSet)featureSet
    API_DEPRECATED("Use supportsFamily instead", macos(10.11, 13.0), ios(8.0, 16.0), tvos(9.0, 16.0))
{
  return GetWrapped(self)->supportsFeatureSet((MTL::FeatureSet)featureSet);
}

- (BOOL)supportsFamily:(MTLGPUFamily)gpuFamily API_AVAILABLE(macos(10.15), ios(13.0))
{
  return GetWrapped(self)->supportsFamily((MTL::GPUFamily)gpuFamily);
}

- (BOOL)supportsTextureSampleCount:(NSUInteger)sampleCount API_AVAILABLE(macos(10.11), ios(9.0))
{
  return GetWrapped(self)->supportsTextureSampleCount(sampleCount);
}

- (NSUInteger)minimumLinearTextureAlignmentForPixelFormat:(MTLPixelFormat)format
    API_AVAILABLE(macos(10.13), ios(11.0))
{
  // Device capability query: capture the resulting creation arguments, not the queried value.
  return [self.real minimumLinearTextureAlignmentForPixelFormat:format];
}

- (NSUInteger)minimumTextureBufferAlignmentForPixelFormat:(MTLPixelFormat)format
    API_AVAILABLE(macos(10.14), ios(12.0))
{
  return [self.real minimumTextureBufferAlignmentForPixelFormat:format];
}

- (nullable id<MTLRenderPipelineState>)
    newRenderPipelineStateWithTileDescriptor:(MTLTileRenderPipelineDescriptor *)descriptor
                                     options:(MTLPipelineOption)options
                                  reflection:(MTLAutoreleasedRenderPipelineReflection *__nullable)reflection
                                       error:(__autoreleasing NSError **)error
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  MTLTileRenderPipelineDescriptor *real = [descriptor copy];
  WrappedMTLFunction *function = descriptor.tileFunction ? GetWrapped(descriptor.tileFunction) : NULL;
  rdcarray<WrappedMTLBinaryArchive *> archives;
  bool validArchives = descriptor.binaryArchives.count <= 4;
  for(id<MTLBinaryArchive> archive in descriptor.binaryArchives)
  {
    if(![(id)archive isKindOfClass:[ObjCBridgeMTLBinaryArchive class]])
      validArchives = false;
    else
      archives.push_back(GetWrapped((ObjCBridgeMTLBinaryArchive *)archive));
  }
  rdcarray<WrappedMTLFunction *> visibleFunctions = TileVisibleFunctions(descriptor);
  real.tileFunction = id<MTLFunction>(Unwrap(function));
  real.binaryArchives = NativeBinaryArchives(descriptor.binaryArchives);
  MTL::LinkedFunctions *nativeLinks = RDMTL::MetalNativeLinkedFunctions(
      (MTL::LinkedFunctions *)descriptor.linkedFunctions);
  real.linkedFunctions = (MTLLinkedFunctions *)nativeLinks;
  nativeLinks->release();
  bool supported = validArchives && MetalTileDescriptorSupported(descriptor, true);
  id<MTLRenderPipelineState> wrapped = id<MTLRenderPipelineState>(
      GetWrapped(self)->newTileRenderPipelineState(
          (MTL::TileRenderPipelineDescriptor *)real, function, (MTL::PipelineOption)options,
          (MTL::AutoreleasedRenderPipelineReflection *)reflection, (NS::Error **)error,
          supported, visibleFunctions, archives));
  [real release];
  return wrapped;
}

- (void)newRenderPipelineStateWithTileDescriptor:(MTLTileRenderPipelineDescriptor *)descriptor
                                         options:(MTLPipelineOption)options
                               completionHandler:
                                   (MTLNewRenderPipelineStateWithReflectionCompletionHandler)completionHandler
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
{
  if(descriptor.binaryArchives.count) { METAL_NOT_HOOKED(); return; }
  MTLTileRenderPipelineDescriptor *snapshot = [descriptor copy];
  MTLTileRenderPipelineDescriptor *real = [snapshot copy];
  WrappedMTLFunction *function = snapshot.tileFunction ? GetWrapped(snapshot.tileFunction) : NULL;
  real.tileFunction = id<MTLFunction>(Unwrap(function));
  MTL::LinkedFunctions *nativeLinks = RDMTL::MetalNativeLinkedFunctions(
      (MTL::LinkedFunctions *)snapshot.linkedFunctions);
  real.linkedFunctions = (MTLLinkedFunctions *)nativeLinks;
  nativeLinks->release();
  const bool supported = MetalTileDescriptorSupported(snapshot);
  [self.real newRenderPipelineStateWithTileDescriptor:real options:options
      completionHandler:^(id<MTLRenderPipelineState> pipeline,
                          MTLRenderPipelineReflection *reflection, NSError *error) {
        id<MTLRenderPipelineState> wrapped = id<MTLRenderPipelineState>(
            GetWrapped(self)->CaptureAsyncTilePipeline((MTL::RenderPipelineState *)pipeline,
                (MTL::TileRenderPipelineDescriptor *)snapshot, function,
                (MTL::PipelineOption)options, supported));
        if(completionHandler) completionHandler(wrapped, reflection, error);
        [wrapped release];
      }];
  [real release]; [snapshot release];
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (nullable id<MTLRenderPipelineState>)
    newRenderPipelineStateWithMeshDescriptor:(MTLMeshRenderPipelineDescriptor *)descriptor
                                     options:(MTLPipelineOption)options
                                  reflection:(MTLAutoreleasedRenderPipelineReflection *__nullable)reflection
                                       error:(__autoreleasing NSError **)error
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  MTLMeshRenderPipelineDescriptor *real = [descriptor copy];
  NSArray<id<MTLBinaryArchive>> *archiveArray = nil;
  if(@available(macOS 15.0, *)) archiveArray = descriptor.binaryArchives;
  rdcarray<WrappedMTLBinaryArchive *> archives;
  bool validArchives = archiveArray.count <= 4;
  for(id<MTLBinaryArchive> archive in archiveArray)
  {
    if(![(id)archive isKindOfClass:[ObjCBridgeMTLBinaryArchive class]])
      validArchives = false;
    else
      archives.push_back(GetWrapped((ObjCBridgeMTLBinaryArchive *)archive));
  }
  WrappedMTLFunction *objectFunction = descriptor.objectFunction ?
      GetWrapped(descriptor.objectFunction) : NULL;
  WrappedMTLFunction *meshFunction = descriptor.meshFunction ?
      GetWrapped(descriptor.meshFunction) : NULL;
  WrappedMTLFunction *fragmentFunction = descriptor.fragmentFunction ?
      GetWrapped(descriptor.fragmentFunction) : NULL;
  real.objectFunction = id<MTLFunction>(Unwrap(objectFunction));
  real.meshFunction = id<MTLFunction>(Unwrap(meshFunction));
  real.fragmentFunction = id<MTLFunction>(Unwrap(fragmentFunction));
  if(@available(macOS 15.0, *))
    real.binaryArchives = NativeBinaryArchives(archiveArray);
  id<MTLRenderPipelineState> wrapped = id<MTLRenderPipelineState>(
      GetWrapped(self)->newMeshRenderPipelineState(
          (MTL::MeshRenderPipelineDescriptor *)real, objectFunction, meshFunction,
          fragmentFunction, (MTL::PipelineOption)options,
          (MTL::AutoreleasedRenderPipelineReflection *)reflection, (NS::Error **)error,
          validArchives && (!objectFunction || archives.empty()) &&
          MetalMeshDescriptorSupported(descriptor, true), archives));
  [real release];
  return wrapped;
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (void)newRenderPipelineStateWithMeshDescriptor:(MTLMeshRenderPipelineDescriptor *)descriptor
                                         options:(MTLPipelineOption)options
                               completionHandler:
                                   (MTLNewRenderPipelineStateWithReflectionCompletionHandler)
                                       completionHandler API_AVAILABLE(macos(13.0), ios(16.0))
{
  if(@available(macOS 15.0, *))
    if(descriptor.binaryArchives.count) { METAL_NOT_HOOKED(); return; }
  MTLMeshRenderPipelineDescriptor *snapshot = [descriptor copy];
  MTLMeshRenderPipelineDescriptor *real = [snapshot copy];
  WrappedMTLFunction *objectFunction = snapshot.objectFunction ?
      GetWrapped(snapshot.objectFunction) : NULL;
  WrappedMTLFunction *meshFunction = snapshot.meshFunction ?
      GetWrapped(snapshot.meshFunction) : NULL;
  WrappedMTLFunction *fragmentFunction = snapshot.fragmentFunction ?
      GetWrapped(snapshot.fragmentFunction) : NULL;
  real.objectFunction = id<MTLFunction>(Unwrap(objectFunction));
  real.meshFunction = id<MTLFunction>(Unwrap(meshFunction));
  real.fragmentFunction = id<MTLFunction>(Unwrap(fragmentFunction));
  const bool supported = MetalMeshDescriptorSupported(snapshot);
  [self.real newRenderPipelineStateWithMeshDescriptor:real options:options
      completionHandler:^(id<MTLRenderPipelineState> pipeline,
                          MTLRenderPipelineReflection *reflection, NSError *error) {
        id<MTLRenderPipelineState> wrapped = id<MTLRenderPipelineState>(
            GetWrapped(self)->CaptureAsyncMeshPipeline((MTL::RenderPipelineState *)pipeline,
                (MTL::MeshRenderPipelineDescriptor *)snapshot, objectFunction, meshFunction,
                fragmentFunction, (MTL::PipelineOption)options, supported));
        if(completionHandler) completionHandler(wrapped, reflection, error);
        [wrapped release];
      }];
  [real release]; [snapshot release];
}
#endif

- (NSUInteger)maxThreadgroupMemoryLength API_AVAILABLE(macos(10.13), ios(11.0))
{
  return self.real.maxThreadgroupMemoryLength;
}

- (NSUInteger)maxArgumentBufferSamplerCount API_AVAILABLE(macos(10.14), ios(12.0))
{
  return self.real.maxArgumentBufferSamplerCount;
}

- (BOOL)areProgrammableSamplePositionsSupported API_AVAILABLE(macos(10.13), ios(11.0))
{
  return GetWrapped(self)->areProgrammableSamplePositionsSupported();
}

- (void)getDefaultSamplePositions:(MTLSamplePosition *)positions
                            count:(NSUInteger)count API_AVAILABLE(macos(10.13), ios(11.0))
{
  return [self.real getDefaultSamplePositions:positions count:count];
}

- (nullable id<MTLArgumentEncoder>)newArgumentEncoderWithArguments:
    (NSArray<MTLArgumentDescriptor *> *)arguments API_AVAILABLE(macos(10.13), ios(11.0))
{
  return id<MTLArgumentEncoder>(GetWrapped(self)->newArgumentEncoderWithArguments((NS::Array *)arguments));
}

- (BOOL)supportsRasterizationRateMapWithLayerCount:(NSUInteger)layerCount
    API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4))
{
  return GetWrapped(self)->supportsRasterizationRateMapWithLayerCount(layerCount);
}

- (nullable id<MTLRasterizationRateMap>)newRasterizationRateMapWithDescriptor:
    (MTLRasterizationRateMapDescriptor *)descriptor
    API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4))
{
  return id<MTLRasterizationRateMap>(GetWrapped(self)->newRasterizationRateMap(
      (MTL::RasterizationRateMapDescriptor *)descriptor));
}

- (nullable id<MTLIndirectCommandBuffer>)
    newIndirectCommandBufferWithDescriptor:(MTLIndirectCommandBufferDescriptor *)descriptor
                           maxCommandCount:(NSUInteger)maxCount
                                   options:(MTLResourceOptions)options
    API_AVAILABLE(macos(10.14), ios(12.0))
{
  return id<MTLIndirectCommandBuffer>(GetWrapped(self)->newIndirectCommandBufferWithDescriptor(
      (MTL::IndirectCommandType)descriptor.commandTypes, descriptor.inheritPipelineState,
      descriptor.inheritBuffers, descriptor.maxVertexBufferBindCount,
      descriptor.maxFragmentBufferBindCount, maxCount, (MTL::ResourceOptions)options));
}

- (nullable id<MTLEvent>)newEvent API_AVAILABLE(macos(10.14), ios(12.0))
{
  return id<MTLEvent>(GetWrapped(self)->newEvent());
}

- (nullable id<MTLSharedEvent>)newSharedEvent API_AVAILABLE(macos(10.14), ios(12.0))
{
  return id<MTLSharedEvent>(GetWrapped(self)->newSharedEvent());
}

- (nullable id<MTLSharedEvent>)newSharedEventWithHandle:(MTLSharedEventHandle *)sharedEventHandle
    API_AVAILABLE(macos(10.14), ios(12.0))
{
  id<MTLSharedEvent> real = [self.real newSharedEventWithHandle:sharedEventHandle];
  id<MTLSharedEvent> source = MetalSharedEventHandleSource(sharedEventHandle);
  return id<MTLSharedEvent>(GetWrapped(self)->ImportSharedEventHandle(
      (MTL::SharedEvent *)real, source ? GetWrapped(source) : NULL));
}

- (uint64_t)peerGroupID API_AVAILABLE(macos(10.15))API_UNAVAILABLE(ios)
{
  return self.real.peerGroupID;
}

- (uint32_t)peerIndex API_AVAILABLE(macos(10.15))API_UNAVAILABLE(ios)
{
  return self.real.peerIndex;
}

- (uint32_t)peerCount API_AVAILABLE(macos(10.15))API_UNAVAILABLE(ios)
{
  return self.real.peerCount;
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (nullable id<MTLIOFileHandle>)newIOHandleWithURL:(NSURL *)url
                                             error:(NSError **)error
    API_DEPRECATED_WITH_REPLACEMENT("Use newIOFileHandleWithURL:error: instead", macos(13.0, 14.0),
                                    ios(16.0, 17.0))
{
  METAL_NOT_HOOKED();
  return [self.real newIOHandleWithURL:url error:error];
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (nullable id<MTLIOCommandQueue>)newIOCommandQueueWithDescriptor:(MTLIOCommandQueueDescriptor *)descriptor
                                                            error:(NSError **)error
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  METAL_NOT_HOOKED();
  return [self.real newIOCommandQueueWithDescriptor:descriptor error:error];
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (nullable id<MTLIOFileHandle>)newIOHandleWithURL:(NSURL *)url
                                 compressionMethod:(MTLIOCompressionMethod)compressionMethod
                                             error:(NSError **)error
    API_DEPRECATED_WITH_REPLACEMENT("Use newIOFileHandleWithURL:compressionMethod:error: instead",
                                    macos(13.0, 14.0), ios(16.0, 17.0))
{
  METAL_NOT_HOOKED();
  return [self.real newIOHandleWithURL:url compressionMethod:compressionMethod error:error];
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_14_0
- (nullable id<MTLIOFileHandle>)newIOFileHandleWithURL:(NSURL *)url
                                                 error:(NSError **)error
    API_AVAILABLE(macos(14.0), ios(17.0))
{
  METAL_NOT_HOOKED();
  return [self.real newIOFileHandleWithURL:url error:error];
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_14_0
- (nullable id<MTLIOFileHandle>)newIOFileHandleWithURL:(NSURL *)url
                                     compressionMethod:(MTLIOCompressionMethod)compressionMethod
                                                 error:(NSError **)error
    API_AVAILABLE(macos(14.0), ios(17.0))
{
  METAL_NOT_HOOKED();
  return [self.real newIOFileHandleWithURL:url compressionMethod:compressionMethod error:error];
}
#endif

- (MTLSize)sparseTileSizeWithTextureType:(MTLTextureType)textureType
                             pixelFormat:(MTLPixelFormat)pixelFormat
                             sampleCount:(NSUInteger)sampleCount
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  return [self.real sparseTileSizeWithTextureType:textureType
                                      pixelFormat:pixelFormat
                                      sampleCount:sampleCount];
}

- (NSUInteger)sparseTileSizeInBytes API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  return self.real.sparseTileSizeInBytes;
}

- (void)convertSparsePixelRegions:(const MTLRegion[_Nonnull])pixelRegions
                    toTileRegions:(MTLRegion[_Nonnull])tileRegions
                     withTileSize:(MTLSize)tileSize
                    alignmentMode:(MTLSparseTextureRegionAlignmentMode)mode
                       numRegions:(NSUInteger)numRegions
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  return [self.real convertSparsePixelRegions:pixelRegions
                                toTileRegions:tileRegions
                                 withTileSize:tileSize
                                alignmentMode:mode
                                   numRegions:numRegions];
}

- (void)convertSparseTileRegions:(const MTLRegion[_Nonnull])tileRegions
                  toPixelRegions:(MTLRegion[_Nonnull])pixelRegions
                    withTileSize:(MTLSize)tileSize
                      numRegions:(NSUInteger)numRegions
    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0))
{
  return [self.real convertSparseTileRegions:tileRegions
                              toPixelRegions:pixelRegions
                                withTileSize:tileSize
                                  numRegions:numRegions];
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (NSUInteger)sparseTileSizeInBytesForSparsePageSize:(MTLSparsePageSize)sparsePageSize
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  return [self.real sparseTileSizeInBytesForSparsePageSize:sparsePageSize];
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (MTLSize)sparseTileSizeWithTextureType:(MTLTextureType)textureType
                             pixelFormat:(MTLPixelFormat)pixelFormat
                             sampleCount:(NSUInteger)sampleCount
                          sparsePageSize:(MTLSparsePageSize)sparsePageSize
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  return [self.real sparseTileSizeWithTextureType:textureType
                                      pixelFormat:pixelFormat
                                      sampleCount:sampleCount
                                   sparsePageSize:sparsePageSize];
}
#endif

- (NSUInteger)maxBufferLength API_AVAILABLE(macos(10.14), ios(12.0))
{
  return self.real.maxBufferLength;
}

- (NSArray<id<MTLCounterSet>> *)counterSets API_AVAILABLE(macos(10.15), ios(14.0))
{
  return self.real.counterSets;
}

- (nullable id<MTLCounterSampleBuffer>)newCounterSampleBufferWithDescriptor:
                                           (MTLCounterSampleBufferDescriptor *)descriptor
                                                                      error:(NSError **)error
    API_AVAILABLE(macos(10.15), ios(14.0))
{
  return id<MTLCounterSampleBuffer>(GetWrapped(self)->newCounterSampleBuffer(
      (MTL::CounterSampleBufferDescriptor *)descriptor, (NS::Error **)error));
}

- (void)sampleTimestamps:(MTLTimestamp *)cpuTimestamp
            gpuTimestamp:(MTLTimestamp *)gpuTimestamp API_AVAILABLE(macos(10.15), ios(14.0))
{
  return [self.real sampleTimestamps:cpuTimestamp gpuTimestamp:gpuTimestamp];
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (id<MTLArgumentEncoder>)newArgumentEncoderWithBufferBinding:(id<MTLBufferBinding>)bufferBinding
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  return id<MTLArgumentEncoder>(GetWrapped(self)->newArgumentEncoderWithBufferBinding(
      (MTL::BufferBinding *)bufferBinding));
}
#endif

- (BOOL)supportsCounterSampling:(MTLCounterSamplingPoint)samplingPoint
    API_AVAILABLE(macos(11.0), ios(14.0))
{
  return GetWrapped(self)->supportsCounterSampling((MTL::CounterSamplingPoint)samplingPoint);
}

- (BOOL)supportsVertexAmplificationCount:(NSUInteger)count
    API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4))
{
  return GetWrapped(self)->supportsVertexAmplificationCount(count);
}

- (BOOL)supportsDynamicLibraries API_AVAILABLE(macos(11.0), ios(14.0))
{
  return GetWrapped(self)->supportsDynamicLibraries();
}

- (BOOL)supportsRenderDynamicLibraries API_AVAILABLE(macos(12.0), ios(15.0))
{
  return GetWrapped(self)->supportsRenderDynamicLibraries();
}

- (nullable id<MTLDynamicLibrary>)newDynamicLibrary:(id<MTLLibrary>)library
                                              error:(NSError **)error
    API_AVAILABLE(macos(11.0), ios(14.0))
{
  return id<MTLDynamicLibrary>(GetWrapped(self)->newDynamicLibrary(
      GetWrapped(library), (NS::Error **)error));
}

- (nullable id<MTLDynamicLibrary>)newDynamicLibraryWithURL:(NSURL *)url
                                                     error:(NSError **)error
    API_AVAILABLE(macos(11.0), ios(14.0))
{
  return id<MTLDynamicLibrary>(GetWrapped(self)->newDynamicLibraryWithURL(
      (NS::URL *)url, (NS::Error **)error));
}

- (nullable id<MTLBinaryArchive>)newBinaryArchiveWithDescriptor:(MTLBinaryArchiveDescriptor *)descriptor
                                                          error:(NSError **)error
    API_AVAILABLE(macos(11.0), ios(14.0))
{
  return id<MTLBinaryArchive>(GetWrapped(self)->newBinaryArchive(
      (MTL::BinaryArchiveDescriptor *)descriptor, (NS::Error **)error));
}

- (BOOL)supportsRaytracing API_AVAILABLE(macos(11.0), ios(14.0))
{
  return GetWrapped(self)->supportsRaytracing();
}

- (MTLAccelerationStructureSizes)accelerationStructureSizesWithDescriptor:
    (MTLAccelerationStructureDescriptor *)descriptor API_AVAILABLE(macos(11.0), ios(14.0))
{
  MTLAccelerationStructureDescriptor *native = NativePrimitiveAccelerationDescriptor(descriptor);
  if(!native) native = NativeInstanceDescriptor(descriptor);
  if(!native) METAL_NOT_HOOKED();
  MTLAccelerationStructureSizes sizes = [self.real accelerationStructureSizesWithDescriptor:native];
  [native release];
  return sizes;
}

- (nullable id<MTLAccelerationStructure>)newAccelerationStructureWithSize:(NSUInteger)size
    API_AVAILABLE(macos(11.0), ios(14.0))
{
  return id<MTLAccelerationStructure>(GetWrapped(self)->newAccelerationStructureWithSize(size));
}

- (nullable id<MTLAccelerationStructure>)newAccelerationStructureWithDescriptor:
    (MTLAccelerationStructureDescriptor *)descriptor API_AVAILABLE(macos(11.0), ios(14.0))
{
  MTLInstanceAccelerationStructureDescriptor *nativeInstance = NativeInstanceDescriptor(descriptor);
  if(nativeInstance)
  {
    WrappedMTLAccelerationStructure *wrapped =
        GetWrapped(self)->newAccelerationStructureWithDescriptor(
            (MTL::AccelerationStructureDescriptor *)nativeInstance);
    [nativeInstance release];
    return id<MTLAccelerationStructure>(wrapped);
  }
  if(![(id)descriptor isKindOfClass:[MTLPrimitiveAccelerationStructureDescriptor class]])
    METAL_NOT_HOOKED();
  MTLPrimitiveAccelerationStructureDescriptor *primitive =
      (MTLPrimitiveAccelerationStructureDescriptor *)descriptor;
  if((primitive.usage != MTLAccelerationStructureUsageNone &&
      primitive.usage != MTLAccelerationStructureUsageRefit) ||
     primitive.motionKeyframeCount > 1 || primitive.geometryDescriptors.count != 1 ||
     (![(id)primitive.geometryDescriptors[0]
           isKindOfClass:[MTLAccelerationStructureTriangleGeometryDescriptor class]] &&
      ![(id)primitive.geometryDescriptors[0]
           isKindOfClass:[MTLAccelerationStructureBoundingBoxGeometryDescriptor class]]))
    METAL_NOT_HOOKED();
  id geometry = primitive.geometryDescriptors[0];
  if([(id)geometry isKindOfClass:[MTLAccelerationStructureBoundingBoxGeometryDescriptor class]])
  {
    MTLAccelerationStructureBoundingBoxGeometryDescriptor *box = geometry;
    bool invalidBox = ![(id)box.boundingBoxBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
                      box.boundingBoxBufferOffset % 16 != 0 ||
                      box.boundingBoxBufferOffset > box.boundingBoxBuffer.length ||
                      box.boundingBoxCount == 0 || box.boundingBoxCount > 1000000 ||
                      box.boundingBoxStride < 6 * sizeof(float) ||
                      box.boundingBoxStride > 1024 * 1024 ||
                      box.boundingBoxStride % 8 != 0 ||
                      box.boundingBoxBuffer.length - box.boundingBoxBufferOffset <
                          6 * sizeof(float) ||
                      box.boundingBoxCount - 1 >
                          (box.boundingBoxBuffer.length - box.boundingBoxBufferOffset -
                           6 * sizeof(float)) / box.boundingBoxStride ||
                      box.intersectionFunctionTableOffset > 31;
    if(@available(macOS 13.0, iOS 16.0, *))
      invalidBox |= box.primitiveDataBuffer != nil;
    if(invalidBox) METAL_NOT_HOOKED();
  }
  else
  {
    MTLAccelerationStructureTriangleGeometryDescriptor *triangle = geometry;
    MTLAccelerationStructureTriangleGeometryDescriptor *defaults =
        [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    MTLAttributeFormat vertexFormat = MTLAttributeFormatFloat3;
    if(@available(macOS 13.0, iOS 16.0, *))
      vertexFormat = triangle.vertexFormat;
    else
      METAL_NOT_HOOKED();
    const NSUInteger vertexBytes = vertexFormat == MTLAttributeFormatFloat3 ? 12 :
                                   vertexFormat == MTLAttributeFormatFloat4 ? 16 : 0;
    const NSUInteger available = triangle.vertexBufferOffset <= triangle.vertexBuffer.length ?
        triangle.vertexBuffer.length - triangle.vertexBufferOffset : 0;
    if(![(id)triangle.vertexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
       triangle.vertexBufferOffset % sizeof(float) != 0 ||
       triangle.vertexBufferOffset > triangle.vertexBuffer.length ||
       !vertexBytes || triangle.vertexStride < vertexBytes ||
       triangle.vertexStride > 1024 * 1024 || triangle.vertexStride % sizeof(float) ||
       available < vertexBytes || triangle.triangleCount == 0 ||
       triangle.triangleCount > 1000000 ||
       (!triangle.indexBuffer &&
        triangle.triangleCount * 3 - 1 > (available - vertexBytes) / triangle.vertexStride) ||
       triangle.intersectionFunctionTableOffset > 31 ||
       (triangle.indexBuffer &&
        (![(id)triangle.indexBuffer isKindOfClass:[ObjCBridgeMTLBuffer class]] ||
         available < vertexBytes + 2 * triangle.vertexStride ||
         ((triangle.vertexStride == 12 && vertexFormat == MTLAttributeFormatFloat3) &&
          triangle.vertexBufferOffset % (3 * sizeof(float)) != 0) ||
         triangle.indexBufferOffset % (triangle.indexType == MTLIndexTypeUInt16 ? 2 : 4) != 0 ||
         triangle.indexBufferOffset > triangle.indexBuffer.length ||
         (triangle.indexType != MTLIndexTypeUInt16 && triangle.indexType != MTLIndexTypeUInt32) ||
         triangle.triangleCount > (triangle.indexBuffer.length - triangle.indexBufferOffset) /
             (3 * (triangle.indexType == MTLIndexTypeUInt16 ? 2 : 4)))) ||
       (primitive.usage == MTLAccelerationStructureUsageRefit &&
        ((triangle.vertexBufferOffset != 0 && !triangle.indexBuffer) ||
         (triangle.intersectionFunctionTableOffset != 0 && !triangle.indexBuffer) ||
         (triangle.opaque != defaults.opaque && !triangle.indexBuffer))))
      METAL_NOT_HOOKED();
    if(@available(macOS 13.0, iOS 16.0, *))
    {
      if(triangle.transformationMatrixBuffer || triangle.primitiveDataBuffer)
        METAL_NOT_HOOKED();
    }
  }
  MTLPrimitiveAccelerationStructureDescriptor *native =
      NativePrimitiveAccelerationDescriptor(descriptor);
  if(!native) METAL_NOT_HOOKED();
  WrappedMTLAccelerationStructure *wrapped = GetWrapped(self)->newAccelerationStructureWithDescriptor(
      (MTL::AccelerationStructureDescriptor *)native);
  [native release];
  return id<MTLAccelerationStructure>(wrapped);
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (MTLSizeAndAlign)heapAccelerationStructureSizeAndAlignWithSize:(NSUInteger)size
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  return [self.real heapAccelerationStructureSizeAndAlignWithSize:size];
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_0
- (MTLSizeAndAlign)heapAccelerationStructureSizeAndAlignWithDescriptor:
    (MTLAccelerationStructureDescriptor *)descriptor API_AVAILABLE(macos(13.0), ios(16.0))
{
  MTLAccelerationStructureDescriptor *native = NativePrimitiveAccelerationDescriptor(descriptor);
  if(!native) native = NativeInstanceDescriptor(descriptor);
  if(!native) METAL_NOT_HOOKED();
  MTLSizeAndAlign layout = [self.real heapAccelerationStructureSizeAndAlignWithDescriptor:native];
  [native release];
  return layout;
}
#endif

- (BOOL)supportsFunctionPointers API_AVAILABLE(macos(11.0), ios(14.0))
{
  return GetWrapped(self)->supportsFunctionPointers();
}

- (BOOL)supportsFunctionPointersFromRender API_AVAILABLE(macos(12.0), ios(15.0))
{
  return GetWrapped(self)->supportsFunctionPointersFromRender();
}

- (BOOL)supportsRaytracingFromRender API_AVAILABLE(macos(12.0), ios(15.0))
{
  return GetWrapped(self)->supportsRaytracingFromRender();
}

- (BOOL)supportsPrimitiveMotionBlur API_AVAILABLE(macos(11.0), ios(14.0))
{
  return GetWrapped(self)->supportsPrimitiveMotionBlur();
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_3
- (BOOL)shouldMaximizeConcurrentCompilation API_AVAILABLE(macos(13.3))API_UNAVAILABLE(ios)
{
  return GetWrapped(self)->shouldMaximizeConcurrentCompilation();
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_3
- (void)setShouldMaximizeConcurrentCompilation:(BOOL)value API_AVAILABLE(macos(13.3))
                                                   API_UNAVAILABLE(ios)
{
  // Host-side compilation scheduling does not affect captured GPU commands.
  return [self.real setShouldMaximizeConcurrentCompilation:value];
}
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_13_3
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
- (NSUInteger)maximumConcurrentCompilationTaskCount API_AVAILABLE(macos(13.3), ios(26.0))
#else
- (NSUInteger)maximumConcurrentCompilationTaskCount API_AVAILABLE(macos(13.3))API_UNAVAILABLE(ios)
#endif
{
  return GetWrapped(self)->maximumConcurrentCompilationTaskCount();
}
#endif

@end
