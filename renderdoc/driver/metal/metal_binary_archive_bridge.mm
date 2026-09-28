// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_binary_archive.h"
#include "metal_types_bridge.h"

bool MetalBinaryArchiveIsWrapped(MTL::BinaryArchive *archive)
{
  return [(id)archive isKindOfClass:[ObjCBridgeMTLBinaryArchive class]];
}

@implementation ObjCBridgeMTLBinaryArchive
- (id<MTLBinaryArchive>)real { return id<MTLBinaryArchive>(Unwrap(GetWrapped(self))); }
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
- (NSString *)label { return self.real.label; }
- (void)setLabel:(NSString *)label { self.real.label = label; }
- (BOOL)serializeToURL:(NSURL *)url error:(NSError **)error
{
  return [self.real serializeToURL:url error:error];
}
- (BOOL)addComputePipelineFunctionsWithDescriptor:(MTLComputePipelineDescriptor *)descriptor
                                             error:(NSError **)error
{
  return GetWrapped(self)->addComputePipelineFunctions(
      (MTL::ComputePipelineDescriptor *)descriptor, (NS::Error **)error);
}
- (BOOL)addRenderPipelineFunctionsWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor
                                            error:(NSError **)error
{
  return GetWrapped(self)->addRenderPipelineFunctions(
      (MTL::RenderPipelineDescriptor *)descriptor, (NS::Error **)error);
}
- (BOOL)addTileRenderPipelineFunctionsWithDescriptor:(MTLTileRenderPipelineDescriptor *)descriptor
                                                error:(NSError **)error
{
  if(!descriptor || !MetalTileDescriptorSupported(descriptor) ||
     descriptor.linkedFunctions.functions.count ||
     ![(id)descriptor.tileFunction isKindOfClass:[ObjCBridgeMTLFunction class]])
    return NO;
  return GetWrapped(self)->addTilePipelineFunctions((MTL::TileRenderPipelineDescriptor *)descriptor,
      GetWrapped((ObjCBridgeMTLFunction *)descriptor.tileFunction), (NS::Error **)error);
}
- (BOOL)addMeshRenderPipelineFunctionsWithDescriptor:(MTLMeshRenderPipelineDescriptor *)descriptor
                                                error:(NSError **)error
    API_AVAILABLE(macos(13.0), ios(16.0))
{
  if(!descriptor || !MetalMeshDescriptorSupported(descriptor) || descriptor.objectFunction ||
     ![(id)descriptor.meshFunction isKindOfClass:[ObjCBridgeMTLFunction class]] ||
     ![(id)descriptor.fragmentFunction isKindOfClass:[ObjCBridgeMTLFunction class]])
    return NO;
  return GetWrapped(self)->addMeshPipelineFunctions((MTL::MeshRenderPipelineDescriptor *)descriptor,
      GetWrapped((ObjCBridgeMTLFunction *)descriptor.meshFunction),
      GetWrapped((ObjCBridgeMTLFunction *)descriptor.fragmentFunction), (NS::Error **)error);
}
- (BOOL)addLibraryWithDescriptor:(MTLStitchedLibraryDescriptor *)descriptor
                           error:(NSError **)error
{
  StitchedDescriptorSnapshot snapshot;
  if(!SnapshotStitchedDescriptor(descriptor, snapshot)) return NO;
  return GetWrapped(self)->addLibrary(snapshot.function, snapshot.graphName,
                                     snapshot.functionName, (NS::Error **)error);
}
- (BOOL)addFunctionWithDescriptor:(MTLFunctionDescriptor *)descriptor
                          library:(id<MTLLibrary>)library error:(NSError **)error
{
  if(![(id)library isKindOfClass:[ObjCBridgeMTLLibrary class]]) return NO;
  return GetWrapped(self)->addFunction((MTL::FunctionDescriptor *)descriptor,
      GetWrapped((ObjCBridgeMTLLibrary *)library), (NS::Error **)error);
}
@end
