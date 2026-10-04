// SPDX-License-Identifier: MIT
// Capture MetalFX as an opaque public operation. Do not depend on private framework kernels,
// system metallib paths, or transient internal textures being serialisable.
#import <MetalFX/MetalFX.h>
#import <objc/runtime.h>
#include "metal_command_buffer.h"
#include "metal_fence.h"
#include "metal_texture.h"
#include "metal_replay.h"
#include "metal_types_bridge.h"
#include <cmath>

static IMP originalTemporal;
static id NewTemporalScaler(id self, SEL selector, id<MTLDevice> device) API_AVAILABLE(macos(13.0));

API_AVAILABLE(macos(13.0))
@interface RenderDocMetalFXSpatial : NSObject <MTLFXSpatialScaler>
{
  id<MTLFXSpatialScaler> _real;
  id<MTLTexture> _colour, _output;
  id<MTLFence> _fence;
  MTLFXSpatialScalerDescriptor *_descriptor;
}
- (id)initWithScaler:(id<MTLFXSpatialScaler>)scaler descriptor:(MTLFXSpatialScalerDescriptor *)descriptor;
@end
@implementation RenderDocMetalFXSpatial
- (id)initWithScaler:(id<MTLFXSpatialScaler>)scaler descriptor:(MTLFXSpatialScalerDescriptor *)descriptor
{
  if((self=[super init])) { _real=[scaler retain]; _descriptor=[descriptor copy]; }
  return self;
}
- (void)dealloc { [_real release]; [_descriptor release]; [_colour release]; [_output release]; [_fence release]; [super dealloc]; }
- (NSMethodSignature *)methodSignatureForSelector:(SEL)s { return [(id)_real methodSignatureForSelector:s]; }
- (BOOL)respondsToSelector:(SEL)s { return [super respondsToSelector:s] || [(id)_real respondsToSelector:s]; }
- (void)forwardInvocation:(NSInvocation *)i { [i invokeWithTarget:_real]; }
- (id<MTLTexture>)colorTexture { return _colour; }
- (void)setColorTexture:(id<MTLTexture>)value { [value retain]; [_colour release]; _colour=value; }
- (id<MTLTexture>)outputTexture { return _output; }
- (void)setOutputTexture:(id<MTLTexture>)value { [value retain]; [_output release]; _output=value; }
- (id<MTLFence>)fence { return _fence; }
- (void)setFence:(id<MTLFence>)value { [value retain]; [_fence release]; _fence=value; }
- (void)encodeToCommandBuffer:(id<MTLCommandBuffer>)buffer
{
  if(![(id)buffer isKindOfClass:[ObjCBridgeMTLCommandBuffer class]] ||
     ![(id)_colour isKindOfClass:[ObjCBridgeMTLTexture class]] ||
     ![(id)_output isKindOfClass:[ObjCBridgeMTLTexture class]] ||
     (_fence && ![(id)_fence isKindOfClass:[ObjCBridgeMTLFence class]]))
  {
    RDCERR("MetalFX spatial encode requires captured command buffer, textures and fence");
    return;
  }
  auto command=GetWrapped((ObjCBridgeMTLCommandBuffer *)buffer);
  auto colour=GetWrapped((ObjCBridgeMTLTexture *)_colour);
  auto output=GetWrapped((ObjCBridgeMTLTexture *)_output);
  auto fence=_fence ? GetWrapped((ObjCBridgeMTLFence *)_fence) : NULL;
  _real.colorTexture=(id<MTLTexture>)Unwrap(colour);
  _real.outputTexture=(id<MTLTexture>)Unwrap(output);
  _real.fence=(id<MTLFence>)Unwrap(fence);
  [_real encodeToCommandBuffer:(id<MTLCommandBuffer>)Unwrap(command)];
  rdcarray<uint64_t> p={_descriptor.inputWidth, _descriptor.inputHeight,
      _descriptor.outputWidth, _descriptor.outputHeight, (uint64_t)_descriptor.colorTextureFormat,
      (uint64_t)_descriptor.outputTextureFormat, (uint64_t)_descriptor.colorProcessingMode,
      _real.inputContentWidth, _real.inputContentHeight};
  command->CaptureMetalFXSpatial(colour,output,fence,p);
}
@end

static id NewSpatialScaler(id self, SEL selector, id<MTLDevice> device) API_AVAILABLE(macos(13.0));
static IMP originalSpatial;
static char replayScalersKey;
static id NewSpatialScaler(id self, SEL selector, id<MTLDevice> device)
{
  if(![(id)device isKindOfClass:[ObjCBridgeMTLDevice class]])
    return ((id(*)(id,SEL,id))originalSpatial)(self,selector,device);
  auto wrapped=GetWrapped((ObjCBridgeMTLDevice *)device);
  id<MTLFXSpatialScaler> real=((id(*)(id,SEL,id))originalSpatial)(self,selector,(id)Unwrap(wrapped));
  if(!real)return nil;
  auto proxy=[[RenderDocMetalFXSpatial alloc] initWithScaler:real descriptor:self];
  [real release]; return proxy;
}
void RegisterMetalFXHooks()
{
  // Linking the public framework loads its descriptor class before this hook is installed.
  if(@available(macOS 13.0, *))
  {
    Method method=class_getInstanceMethod([MTLFXSpatialScalerDescriptor class],@selector(newSpatialScalerWithDevice:));
    if(method && !originalSpatial) originalSpatial=method_setImplementation(method,(IMP)&NewSpatialScaler);
    Method temporal=class_getInstanceMethod([MTLFXTemporalScalerDescriptor class],@selector(newTemporalScalerWithDevice:));
    if(temporal && !originalTemporal) originalTemporal=method_setImplementation(temporal,(IMP)&NewTemporalScaler);
  }
}

void WrappedMTLCommandBuffer::CaptureMetalFXSpatial(WrappedMTLTexture *colour,
    WrappedMTLTexture *output, WrappedMTLFence *fence, rdcarray<uint64_t> parameters)
{
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_encodeMetalFXSpatial);
    Serialise_encodeMetalFXSpatial(ser,colour,output,fence,parameters);
    auto record=GetRecord(this); record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(colour),eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(output),eFrameRef_CompleteWrite);
    if(fence) record->MarkResourceFrameReferenced(GetResID(fence),eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_encodeMetalFXSpatial(SerialiserType &ser,
    WrappedMTLTexture *colour, WrappedMTLTexture *output, WrappedMTLFence *fence,
    rdcarray<uint64_t> parameters)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer,this);
  SERIALISE_ELEMENT(colour).Important();
  SERIALISE_ELEMENT(output).Important();
  SERIALISE_ELEMENT(fence);
  SERIALISE_ELEMENT(parameters).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!CommandBuffer || !m_Device->CanEncodeReplayEvent(CommandBuffer) || !CommandBuffer->m_Real ||
       parameters.size()!=9 || !colour || !output || colour==output ||
       colour->m_Type!=eResTexture || output->m_Type!=eResTexture || !colour->m_Real || !output->m_Real ||
       colour->m_Device!=m_Device || output->m_Device!=m_Device ||
       (fence && (fence->m_Type!=eResFence || !fence->m_Real || fence->m_Device!=m_Device))) return false;
    const auto &p=parameters;
    if(!p[0] || !p[1] || !p[2] || !p[3] || p[0]>16384 || p[1]>16384 || p[2]>16384 || p[3]>16384 ||
       p[2]<p[0] || p[3]<p[1] || p[6]>2 || !p[7] || !p[8] || p[7]>p[0] || p[8]>p[1] ||
       Unwrap(colour)->width()!=p[0] || Unwrap(colour)->height()!=p[1] ||
       Unwrap(output)->width()!=p[2] || Unwrap(output)->height()!=p[3] ||
       (uint64_t)Unwrap(colour)->pixelFormat()!=p[4] || (uint64_t)Unwrap(output)->pixelFormat()!=p[5] ||
       Unwrap(output)->storageMode()!=MTL::StorageModePrivate ||
       Unwrap(colour)->textureType()!=MTL::TextureType2D || Unwrap(output)->textureType()!=MTL::TextureType2D ||
       Unwrap(colour)->sampleCount()!=1 || Unwrap(output)->sampleCount()!=1) return false;
    if(@available(macOS 13.0, *))
    {
      auto desc=[[MTLFXSpatialScalerDescriptor alloc] init];
      desc.inputWidth=p[0]; desc.inputHeight=p[1]; desc.outputWidth=p[2]; desc.outputHeight=p[3];
      desc.colorTextureFormat=(MTLPixelFormat)p[4]; desc.outputTextureFormat=(MTLPixelFormat)p[5];
      desc.colorProcessingMode=(MTLFXSpatialScalerColorProcessingMode)p[6];
      auto scaler=[desc newSpatialScalerWithDevice:(id<MTLDevice>)Unwrap(m_Device)]; [desc release];
      if(!scaler) return false;
      if(((uint64_t)Unwrap(colour)->usage() & scaler.colorTextureUsage)!=scaler.colorTextureUsage ||
         ((uint64_t)Unwrap(output)->usage() & scaler.outputTextureUsage)!=scaler.outputTextureUsage)
      { [scaler release]; return false; }
      scaler.colorTexture=(id<MTLTexture>)Unwrap(colour); scaler.outputTexture=(id<MTLTexture>)Unwrap(output);
      scaler.inputContentWidth=p[7]; scaler.inputContentHeight=p[8]; scaler.fence=(id<MTLFence>)Unwrap(fence);
      [scaler encodeToCommandBuffer:(id<MTLCommandBuffer>)Unwrap(CommandBuffer)];
      // Retain opaque internal resources even for unretained-reference command buffers,
      // and release on command-buffer destruction if partial replay never submits it.
      id nativeBuffer = (id)Unwrap(CommandBuffer);
      NSMutableArray *retained = objc_getAssociatedObject(nativeBuffer, &replayScalersKey);
      if(!retained) { retained = [NSMutableArray array]; objc_setAssociatedObject(nativeBuffer,
                         &replayScalersKey, retained, OBJC_ASSOCIATION_RETAIN_NONATOMIC); }
      [retained addObject:scaler]; [scaler release];
      m_Device->GetReplay()->ActivateEncoderContext(GetResID(CommandBuffer));
      m_Device->GetReplay()->SetMetalFXSpatial(GetResID(colour),GetResID(output),parameters);
      if(IsLoading(m_State))
      {
        AddEvent(); ActionDescription action;
        action.customName=StringFormat::Fmt("MetalFX Spatial Upscale (%llux%llu → %llux%llu)",p[0],p[1],p[2],p[3]);
        action.flags=ActionFlags::Dispatch; action.outputs[0]=GetResID(output);
        AddAction(action);
        m_Device->GetReplay()->AddUsage(GetResID(colour),ResourceUsage::MetalFXInput);
        m_Device->GetReplay()->AddUsage(GetResID(output),ResourceUsage::MetalFXOutput);
      }
    }
    else return false;
  }
  return true;
}
template bool WrappedMTLCommandBuffer::Serialise_encodeMetalFXSpatial(ReadSerialiser &,WrappedMTLTexture *,WrappedMTLTexture *,WrappedMTLFence *,rdcarray<uint64_t>);
template bool WrappedMTLCommandBuffer::Serialise_encodeMetalFXSpatial(WriteSerialiser &,WrappedMTLTexture *,WrappedMTLTexture *,WrappedMTLFence *,rdcarray<uint64_t>);

// Temporal state belongs to the public scaler instance, across command buffers.
API_AVAILABLE(macos(13.0))
@interface RenderDocMetalFXTemporal : NSObject <MTLFXTemporalScaler>
{
  id<MTLFXTemporalScaler> _real;
  id<MTLTexture> _textures[6];
  id<MTLFence> _fence;
  MTLFXTemporalScalerDescriptor *_descriptor;
  ResourceId _scaler;
}
- (id)initWithScaler:(id<MTLFXTemporalScaler>)scaler descriptor:(MTLFXTemporalScalerDescriptor *)descriptor;
@end
@implementation RenderDocMetalFXTemporal
- (id)initWithScaler:(id<MTLFXTemporalScaler>)scaler descriptor:(MTLFXTemporalScalerDescriptor *)descriptor
{
  if((self=[super init]))
  {
    _real=[scaler retain]; _descriptor=[descriptor copy];
    _scaler=ResourceIDGen::GetNewUniqueID();
  }
  return self;
}
- (void)dealloc
{
  for(id texture : _textures) [texture release];
  [_real release]; [_descriptor release]; [_fence release]; [super dealloc];
}
- (NSMethodSignature *)methodSignatureForSelector:(SEL)s { return [(id)_real methodSignatureForSelector:s]; }
- (BOOL)respondsToSelector:(SEL)s { return [super respondsToSelector:s] || [(id)_real respondsToSelector:s]; }
- (void)forwardInvocation:(NSInvocation *)i { [i invokeWithTarget:_real]; }
#define FX_TEXTURE_PROPERTY(getter, setter, slot) \
- (id<MTLTexture>)getter { return _textures[slot]; } \
- (void)setter:(id<MTLTexture>)value { [value retain]; [_textures[slot] release]; _textures[slot]=value; }
FX_TEXTURE_PROPERTY(colorTexture, setColorTexture, 0)
FX_TEXTURE_PROPERTY(depthTexture, setDepthTexture, 1)
FX_TEXTURE_PROPERTY(motionTexture, setMotionTexture, 2)
FX_TEXTURE_PROPERTY(exposureTexture, setExposureTexture, 3)
FX_TEXTURE_PROPERTY(reactiveMaskTexture, setReactiveMaskTexture, 4)
FX_TEXTURE_PROPERTY(outputTexture, setOutputTexture, 5)
#undef FX_TEXTURE_PROPERTY
- (id<MTLFence>)fence { return _fence; }
- (void)setFence:(id<MTLFence>)value { [value retain]; [_fence release]; _fence=value; }
- (void)encodeToCommandBuffer:(id<MTLCommandBuffer>)buffer
{
  if(![(id)buffer isKindOfClass:[ObjCBridgeMTLCommandBuffer class]] ||
     (_fence && ![(id)_fence isKindOfClass:[ObjCBridgeMTLFence class]]))
  { RDCERR("MetalFX temporal encode requires a captured command buffer and fence"); return; }
  rdcarray<WrappedMTLTexture *> textures;
  for(unsigned i=0;i<6;i++)
  {
    if((i<3 || i==5) && !_textures[i])
    { RDCERR("Missing MetalFX temporal input/output"); return; }
    if(_textures[i] && ![(id)_textures[i] isKindOfClass:[ObjCBridgeMTLTexture class]])
    { RDCERR("MetalFX temporal encode requires captured textures"); return; }
    textures.push_back(_textures[i] ? GetWrapped((ObjCBridgeMTLTexture *)_textures[i]) : NULL);
  }
  auto command=GetWrapped((ObjCBridgeMTLCommandBuffer *)buffer);
  auto fence=_fence ? GetWrapped((ObjCBridgeMTLFence *)_fence) : NULL;
  _real.colorTexture=(id<MTLTexture>)Unwrap(textures[0]);
  _real.depthTexture=(id<MTLTexture>)Unwrap(textures[1]);
  _real.motionTexture=(id<MTLTexture>)Unwrap(textures[2]);
  _real.exposureTexture=(id<MTLTexture>)Unwrap(textures[3]);
  _real.outputTexture=(id<MTLTexture>)Unwrap(textures[5]);
  _real.fence=(id<MTLFence>)Unwrap(fence);
  uint64_t reactive=0, reactiveFormat=0;
  if(@available(macOS 14.4, *))
  {
    reactive=_descriptor.reactiveMaskTextureEnabled;
    reactiveFormat=_descriptor.reactiveMaskTextureFormat;
    _real.reactiveMaskTexture=(id<MTLTexture>)Unwrap(textures[4]);
  }
  rdcarray<uint64_t> p={_descriptor.inputWidth, _descriptor.inputHeight,
      _descriptor.outputWidth, _descriptor.outputHeight, (uint64_t)_descriptor.colorTextureFormat,
      (uint64_t)_descriptor.depthTextureFormat, (uint64_t)_descriptor.motionTextureFormat,
      (uint64_t)_descriptor.outputTextureFormat, (uint64_t)_descriptor.autoExposureEnabled,
      (uint64_t)_descriptor.inputContentPropertiesEnabled, _real.inputContentWidth,
      _real.inputContentHeight, (uint64_t)_real.reset, (uint64_t)_real.depthReversed,
      reactive, reactiveFormat, (uint64_t)_descriptor.requiresSynchronousInitialization};
  rdcarray<float> values={_real.jitterOffsetX, _real.jitterOffsetY,
      _real.motionVectorScaleX, _real.motionVectorScaleY, _real.preExposure,
      _descriptor.inputContentMinScale, _descriptor.inputContentMaxScale};
  [_real encodeToCommandBuffer:(id<MTLCommandBuffer>)Unwrap(command)];
  auto output=textures.back(); textures.pop_back();
  // Auto exposure and disabled reactive masks are not reads of the supplied textures.
  if(p[8]) textures[3]=NULL;
  if(!p[14]) textures[4]=NULL;
  command->CaptureMetalFXTemporal(_scaler,textures,output,fence,p,values);
}
@end
static id NewTemporalScaler(id self, SEL selector, id<MTLDevice> device)
{
  if(![(id)device isKindOfClass:[ObjCBridgeMTLDevice class]])
    return ((id(*)(id,SEL,id))originalTemporal)(self,selector,device);
  auto wrapped=GetWrapped((ObjCBridgeMTLDevice *)device);
  id<MTLFXTemporalScaler> real=((id(*)(id,SEL,id))originalTemporal)(self,selector,(id)Unwrap(wrapped));
  if(!real) return nil;
  auto proxy=[[RenderDocMetalFXTemporal alloc] initWithScaler:real descriptor:self];
  [real release]; return proxy;
}

void WrappedMTLCommandBuffer::CaptureMetalFXTemporal(ResourceId scaler,
    rdcarray<WrappedMTLTexture *> inputs, WrappedMTLTexture *output, WrappedMTLFence *fence,
    rdcarray<uint64_t> parameters, rdcarray<float> values)
{
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_encodeMetalFXTemporal);
    Serialise_encodeMetalFXTemporal(ser,scaler,inputs,output,fence,parameters,values);
    auto record=GetRecord(this); record->AddChunk(scope.Get());
    for(auto texture : inputs)
      if(texture) record->MarkResourceFrameReferenced(GetResID(texture),eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(output),eFrameRef_CompleteWrite);
    if(fence) record->MarkResourceFrameReferenced(GetResID(fence),eFrameRef_Read);
  }
}

void MetalReplay::ResetMetalFXTemporal()
{
  for(auto &entry : m_TemporalScalers) [(id)entry.second.object release];
  m_TemporalScalers.clear();
}

bool MetalReplay::EncodeMetalFXTemporal(WrappedMTLCommandBuffer *command, ResourceId scalerID,
    const rdcarray<WrappedMTLTexture *> &inputs, WrappedMTLTexture *output, WrappedMTLFence *fence,
    const rdcarray<uint64_t> &p, const rdcarray<float> &v)
{
  if(!command || !m_pDriver->CanEncodeReplayEvent(command) || !command->m_Real ||
     scalerID==ResourceId() || p.size()!=17 || v.size()!=7 || inputs.size()!=5 || !output ||
     output->m_Type!=eResTexture || !output->m_Real || output->m_Device!=m_pDriver ||
     (fence && (fence->m_Type!=eResFence || !fence->m_Real || fence->m_Device!=m_pDriver))) return false;
  for(unsigned i=0;i<5;i++)
  {
    auto t=inputs[i];
    if((i<3 || (i==4 && p[14])) && !t) return false;
    if(t && (t==output || t->m_Type!=eResTexture || !t->m_Real || t->m_Device!=m_pDriver ||
             Unwrap(t)->textureType()!=MTL::TextureType2D || Unwrap(t)->sampleCount()!=1)) return false;
  }
  for(float value : v) if(!std::isfinite(value)) return false;
  for(unsigned i : {8U,9U,12U,13U,14U,16U}) if(p[i]>1) return false;
  if(!p[0] || !p[1] || !p[2] || !p[3] || p[0]>16384 || p[1]>16384 || p[2]>16384 || p[3]>16384 ||
     !p[10] || !p[11] || p[10]>p[0] || p[11]>p[1] || v[4]<=0 ||
     (p[9] && (v[5]<=0 || v[6]<v[5])) || (p[8] && inputs[3]) || (!p[14] && inputs[4]) ||
     Unwrap(output)->textureType()!=MTL::TextureType2D || Unwrap(output)->sampleCount()!=1 ||
     Unwrap(output)->storageMode()!=MTL::StorageModePrivate ||
     Unwrap(output)->width()!=p[2] || Unwrap(output)->height()!=p[3] ||
     uint64_t(Unwrap(output)->pixelFormat())!=p[7]) return false;
  for(unsigned i=0;i<3;i++)
    if(Unwrap(inputs[i])->width()!=p[0] || Unwrap(inputs[i])->height()!=p[1] ||
       uint64_t(Unwrap(inputs[i])->pixelFormat())!=p[4+i]) return false;
  if(inputs[3] && (Unwrap(inputs[3])->width()!=1 || Unwrap(inputs[3])->height()!=1 ||
                  Unwrap(inputs[3])->pixelFormat()!=MTL::PixelFormatR16Float)) return false;
  if(inputs[4] && (Unwrap(inputs[4])->width()!=p[0] || Unwrap(inputs[4])->height()!=p[1] ||
                  uint64_t(Unwrap(inputs[4])->pixelFormat())!=p[15])) return false;
  if(@available(macOS 13.0, *))
  {
    if(![MTLFXTemporalScalerDescriptor supportsDevice:(id<MTLDevice>)Unwrap(m_pDriver)]) return false;
    auto &entry=m_TemporalScalers[scalerID];
    const bool first=entry.object==NULL;
    if(first)
    {
      // Public scaler IDs must not alias an ordinary captured Metal object.
      if(m_pDriver->GetResourceManager()->HasResource(scalerID)) return false;
      auto desc=[MTLFXTemporalScalerDescriptor new];
      desc.inputWidth=p[0]; desc.inputHeight=p[1]; desc.outputWidth=p[2]; desc.outputHeight=p[3];
      desc.colorTextureFormat=(MTLPixelFormat)p[4]; desc.depthTextureFormat=(MTLPixelFormat)p[5];
      desc.motionTextureFormat=(MTLPixelFormat)p[6]; desc.outputTextureFormat=(MTLPixelFormat)p[7];
      desc.autoExposureEnabled=p[8]; desc.inputContentPropertiesEnabled=p[9];
      desc.inputContentMinScale=v[5]; desc.inputContentMaxScale=v[6];
      desc.requiresSynchronousInitialization=p[16];
      if(@available(macOS 14.4, *))
      { desc.reactiveMaskTextureEnabled=p[14]; desc.reactiveMaskTextureFormat=(MTLPixelFormat)p[15]; }
      else if(p[14]) { [desc release]; return false; }
      entry.object=[desc newTemporalScalerWithDevice:(id<MTLDevice>)Unwrap(m_pDriver)]; [desc release];
      if(!entry.object) { m_TemporalScalers.erase(scalerID); return false; }
      entry.parameters=p; entry.values=v;
      entry.historyUnavailable=!p[12];
      if(IsLoading(m_pDriver->GetStateRef()))
        m_pDriver->AddResource(scalerID,ResourceType::StateObject,"MetalFX Temporal Scaler");
    }
    else
    {
      for(unsigned i : {0U,1U,2U,3U,4U,5U,6U,7U,8U,9U,14U,15U,16U})
        if(entry.parameters[i]!=p[i]) return false;
      if(entry.values[5]!=v[5] || entry.values[6]!=v[6]) return false;
    }
    id<MTLFXTemporalScaler> scaler=(id<MTLFXTemporalScaler>)entry.object;
    uint64_t usage[]={scaler.colorTextureUsage,scaler.depthTextureUsage,scaler.motionTextureUsage,
                      MTLTextureUsageShaderRead,0};
    if(@available(macOS 14.4, *)) usage[4]=scaler.reactiveTextureUsage;
    for(unsigned i=0;i<5;i++)
      if(inputs[i] && (uint64_t(Unwrap(inputs[i])->usage())&usage[i])!=usage[i]) return false;
    if((uint64_t(Unwrap(output)->usage())&scaler.outputTextureUsage)!=scaler.outputTextureUsage) return false;
    scaler.colorTexture=(id<MTLTexture>)Unwrap(inputs[0]); scaler.depthTexture=(id<MTLTexture>)Unwrap(inputs[1]);
    scaler.motionTexture=(id<MTLTexture>)Unwrap(inputs[2]); scaler.exposureTexture=(id<MTLTexture>)Unwrap(inputs[3]);
    if(@available(macOS 14.4, *)) scaler.reactiveMaskTexture=(id<MTLTexture>)Unwrap(inputs[4]);
    scaler.outputTexture=(id<MTLTexture>)Unwrap(output); scaler.fence=(id<MTLFence>)Unwrap(fence);
    scaler.inputContentWidth=p[10]; scaler.inputContentHeight=p[11];
    scaler.jitterOffsetX=v[0]; scaler.jitterOffsetY=v[1];
    scaler.motionVectorScaleX=v[2]; scaler.motionVectorScaleY=v[3]; scaler.preExposure=v[4];
    // Private MetalFX history has no public export. Make missing pre-capture history explicit.
    scaler.reset=p[12] || first; scaler.depthReversed=p[13];
    if(p[12]) entry.historyUnavailable=false;
    [scaler encodeToCommandBuffer:(id<MTLCommandBuffer>)Unwrap(command)];
    id nativeBuffer=(id)Unwrap(command);
    NSMutableArray *retained=objc_getAssociatedObject(nativeBuffer,&replayScalersKey);
    if(!retained) { retained=[NSMutableArray array]; objc_setAssociatedObject(nativeBuffer,
                         &replayScalersKey,retained,OBJC_ASSOCIATION_RETAIN_NONATOMIC); }
    [retained addObject:scaler];
    ActivateEncoderContext(GetResID(command));
    m_CurrentPipelineState=MetalPipe::State(); m_CurrentGraphicsInlineData.clear();
    m_CurrentPipelineState.metalFXTemporal=p;
    m_CurrentPipelineState.metalFXTemporalFloats=v;
    for(auto input : inputs) m_CurrentPipelineState.metalFXTemporalInputs.push_back(GetResID(input));
    m_CurrentPipelineState.metalFXInput=GetResID(inputs[0]);
    m_CurrentPipelineState.metalFXOutput=GetResID(output);
    m_CurrentPipelineState.metalFXScaler=scalerID;
    m_CurrentPipelineState.metalFXHistoryUnavailable=entry.historyUnavailable;
    return true;
  }
  return false;
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_encodeMetalFXTemporal(SerialiserType &ser,
    ResourceId scaler, rdcarray<WrappedMTLTexture *> inputs, WrappedMTLTexture *output,
    WrappedMTLFence *fence, rdcarray<uint64_t> parameters, rdcarray<float> values)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer,this);
  SERIALISE_ELEMENT(scaler).Important();
  SERIALISE_ELEMENT(inputs).Important();
  SERIALISE_ELEMENT(output).Important();
  SERIALISE_ELEMENT(fence);
  SERIALISE_ELEMENT(parameters).Important();
  SERIALISE_ELEMENT(values).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!m_Device->GetReplay()->EncodeMetalFXTemporal(CommandBuffer,scaler,inputs,output,fence,parameters,values))
      return false;
    if(IsLoading(m_State))
    {
      AddEvent(); ActionDescription action;
      action.customName=StringFormat::Fmt("MetalFX Temporal Upscale (%llux%llu → %llux%llu)",
          parameters[0],parameters[1],parameters[2],parameters[3]);
      action.flags=ActionFlags::Dispatch; action.outputs[0]=GetResID(output); AddAction(action);
      const ResourceUsage roles[]={ResourceUsage::MetalFXInput, ResourceUsage::MetalFXDepthInput,
          ResourceUsage::MetalFXMotionInput, ResourceUsage::MetalFXExposureInput, ResourceUsage::MetalFXReactiveInput};
      for(unsigned i=0;i<inputs.size();i++)
        if(inputs[i]) m_Device->GetReplay()->AddUsage(GetResID(inputs[i]),roles[i]);
      m_Device->GetReplay()->AddUsage(GetResID(output),ResourceUsage::MetalFXOutput);
    }
  }
  return true;
}
template bool WrappedMTLCommandBuffer::Serialise_encodeMetalFXTemporal(ReadSerialiser &,ResourceId,rdcarray<WrappedMTLTexture *>,WrappedMTLTexture *,WrappedMTLFence *,rdcarray<uint64_t>,rdcarray<float>);
template bool WrappedMTLCommandBuffer::Serialise_encodeMetalFXTemporal(WriteSerialiser &,ResourceId,rdcarray<WrappedMTLTexture *>,WrappedMTLTexture *,WrappedMTLFence *,rdcarray<uint64_t>,rdcarray<float>);
