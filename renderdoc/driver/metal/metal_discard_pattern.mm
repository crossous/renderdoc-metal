// SPDX-License-Identifier: MIT
// Like Vulkan ApplyRPLoadDiscards/ApplyRPStoreDiscards, replace undefined attachment
// contents with the common RenderDoc pattern, in the attachment owner's command buffer.
#include "metal_device.h"
#include "metal_texture.h"
#include "metal_types.h"
#include "metal_replay.h"
#include "replay/replay_driver.h"
#include "maths/formatpacking.h"
#import <Metal/Metal.h>

static bool FillDiscard(MTL::CommandBuffer *command, MTL::Texture *texture,
                        uint64_t level, uint64_t slice, uint64_t depthPlane, uint64_t layers,
                        DiscardType type, unsigned aspect)
{
  if(!command || !texture || texture->sampleCount() != 1 ||
     texture->hazardTrackingMode() == MTL::HazardTrackingModeUntracked ||
     texture->storageMode() == MTL::StorageModeMemoryless || level >= texture->mipmapLevelCount())
    return false;
  // Replay allocations opt into tracking, including heap children. Keep the
  // native guard for unsupported external allocations that cannot be promoted.
  const auto textureType = texture->textureType();
  if(textureType != MTL::TextureType2D && textureType != MTL::TextureType2DArray &&
     textureType != MTL::TextureTypeCube && textureType != MTL::TextureTypeCubeArray &&
     textureType != MTL::TextureType3D)
    return false;
  const bool volume = textureType == MTL::TextureType3D;
  const uint64_t slices = textureType == MTL::TextureTypeCube ||
                          textureType == MTL::TextureTypeCubeArray ? texture->arrayLength() * 6 :
                          texture->arrayLength();
  const uint64_t planes = RDCMAX(1ULL, uint64_t(texture->depth()) >> level);
  if(!layers || (volume ? (slice || depthPlane >= planes || layers > planes - depthPlane) :
                          (depthPlane || slice >= slices || layers > slices - slice))) return false;
  MTL::PixelFormat format = texture->pixelFormat();
  const bool combined = format == MTL::PixelFormatDepth32Float_Stencil8 ||
                        format == MTL::PixelFormatDepth24Unorm_Stencil8;
  // D24S8 plane uploads are not available on this backend. Do not guess their layout.
  if(format == MTL::PixelFormatDepth24Unorm_Stencil8) return false;
  MTL::BlitOption options = MTL::BlitOptionNone;
  if(combined)
  {
    format = aspect == 2 ? MTL::PixelFormatStencil8 : MTL::PixelFormatDepth32Float;
    options = aspect == 2 ? MTL::BlitOptionStencilFromDepthStencil :
                            MTL::BlitOptionDepthFromDepthStencil;
  }
  uint32_t bw = 0, bh = 0, bytes = 0;
  if(format == MTL::PixelFormatStencil8) { bw = bh = bytes = 1; }
  else if(!GetTextureDataBlockShape(format, bw, bh, bytes) || bw != 1 || bh != 1) return false;
  const uint64_t width = RDCMAX(1ULL, uint64_t(texture->width()) >> level);
  const uint64_t height = RDCMAX(1ULL, uint64_t(texture->height()) >> level);
  const uint64_t pitch = AlignUp(width * bytes, uint64_t(256));
  if(pitch * height > 128ULL * 1024 * 1024) return false;
  const bytebuf pattern = GetDiscardPattern(type, MakeResourceFormat(format));
  if(pattern.size() != DiscardPatternWidth * DiscardPatternHeight * bytes) return false;
  id<MTLCommandBuffer> nativeCommand = (__bridge id<MTLCommandBuffer>)command;
  id<MTLBuffer> staging = [nativeCommand.device newBufferWithLength:pitch * height
                                                        options:MTLResourceStorageModeShared];
  if(!staging) return false;
  byte *dest = (byte *)staging.contents;
  for(uint64_t y = 0; y < height; y++)
    for(uint64_t x = 0; x < width; x++)
      memcpy(dest + y * pitch + x * bytes,
             pattern.data() + ((y % DiscardPatternHeight) * DiscardPatternWidth +
                               x % DiscardPatternWidth) * bytes, bytes);
  id<MTLBlitCommandEncoder> blit = [nativeCommand blitCommandEncoder];
  if(!blit) { [staging release]; return false; }
  for(uint64_t layer = 0; layer < layers; layer++)
    [blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:pitch
      sourceBytesPerImage:pitch * height sourceSize:MTLSizeMake(width, height, 1)
      toTexture:(__bridge id<MTLTexture>)texture destinationSlice:volume ? 0 : slice + layer
      destinationLevel:level destinationOrigin:MTLOriginMake(0, 0, volume ? depthPlane + layer : 0)
      options:(MTLBlitOption)options];
  [blit endEncoding];
  // These Metal bridges use manual reference counting. Copying the completion
  // block retains its captured Objective-C staging object, even on unretained
  // command buffers. Balance newBuffer after the command has copied the block;
  // completion/cancellation disposes the block and releases its remaining owner.
  [nativeCommand addCompletedHandler:^(id<MTLCommandBuffer>) { (void)staging.contents; }];
  [staging release];
  return true;
}

// MSAA cannot be filled by a buffer-to-texture blit. Vulkan/D3D12 use a
// fullscreen raster draw for this case, including two stencil-reference passes.
struct MetalDiscardDrawState
{
  id<MTLLibrary> library = nil;
  std::map<uint64_t, id<MTLRenderPipelineState>> pipelines;
  std::map<unsigned, id<MTLDepthStencilState>> depthStates;
  ~MetalDiscardDrawState()
  {
    for(auto &entry : pipelines) [entry.second release];
    for(auto &entry : depthStates) [entry.second release];
    [library release];
  }
};

void WrappedMTLDevice::ReleaseReplayDiscardResources()
{
  delete (MetalDiscardDrawState *)m_ReplayDiscardDrawState;
  m_ReplayDiscardDrawState = NULL;
}

bool WrappedMTLDevice::FillReplayRenderDiscard(MTL::CommandBuffer *command, MTL::Texture *texture,
    uint64_t level, uint64_t slice, uint64_t layers, DiscardType type, unsigned aspect)
{
  if(!command || !texture || texture->storageMode() == MTL::StorageModeMemoryless ||
     texture->hazardTrackingMode() == MTL::HazardTrackingModeUntracked ||
     level >= texture->mipmapLevelCount() ||
     !(texture->usage() & MTL::TextureUsageRenderTarget)) return false;
  const auto texType = texture->textureType();
  if(texType != MTL::TextureType2DMultisample && texType != MTL::TextureType2DMultisampleArray &&
     texType != MTL::TextureType2D && texType != MTL::TextureType2DArray &&
     texType != MTL::TextureTypeCube && texType != MTL::TextureTypeCubeArray)
    return false;
  const uint64_t slices = texture->arrayLength() *
      (texType == MTL::TextureTypeCube || texType == MTL::TextureTypeCubeArray ? 6ULL : 1ULL);
  if(slice >= slices || !layers || layers > slices - slice)
    return false;
  id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)command;
  id<MTLTexture> target = (__bridge id<MTLTexture>)texture;
  auto state = (MetalDiscardDrawState *)m_ReplayDiscardDrawState;
  if(!state) m_ReplayDiscardDrawState = state = new MetalDiscardDrawState;
  if(!state->library)
  {
    NSError *error = nil;
    state->library = [cb.device newLibraryWithSource:@R"(
#include <metal_stdlib>
using namespace metal;
struct Params { float4 white; uint mode; };
vertex float4 discard_vs(uint i [[vertex_id]]) {
  const float2 p[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[i],0,1);
}
bool glyph(float4 p, constant uchar *mask) {
  uint2 xy=uint2(p.xy); return mask[(xy.y%8)*64+(xy.x%64)]!=0;
}
fragment float4 discard_float(float4 p [[position]], constant uchar *mask [[buffer(0)]],
  constant Params &v [[buffer(1)]]) {return glyph(p,mask)?v.white:float4(0);}
fragment uint4 discard_uint(float4 p [[position]], constant uchar *mask [[buffer(0)]],
  constant Params &v [[buffer(1)]]) {return glyph(p,mask)?uint4(v.white):uint4(0);}
fragment int4 discard_int(float4 p [[position]], constant uchar *mask [[buffer(0)]],
  constant Params &v [[buffer(1)]]) {return glyph(p,mask)?int4(v.white):int4(0);}
struct DepthOutput { float value [[depth(any)]]; };
fragment DepthOutput discard_depth(float4 p [[position]], constant uchar *mask [[buffer(0)]],
  constant Params &v [[buffer(1)]]) {
  bool white=glyph(p,mask);
  if(v.mode<2 && white!=(v.mode==1)) discard_fragment();
  return {white?1.0:0.0};
}
)" options:nil error:&error];
    if(!state->library)
    { RDCERR("Metal discard library failed: %s", error.description.UTF8String); return false; }
  }
  const MTL::PixelFormat format = texture->pixelFormat();
  const ResourceFormat fmt = MakeResourceFormat(format);
  const uint64_t key = uint64_t(format) | (uint64_t(texture->sampleCount()) << 16) |
                       (uint64_t(aspect) << 24);
  id<MTLRenderPipelineState> pipeline = state->pipelines[key];
  if(!pipeline)
  {
    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [state->library newFunctionWithName:@"discard_vs"];
    NSString *fragment = aspect ? @"discard_depth" :
        fmt.compType == CompType::UInt ? @"discard_uint" :
        fmt.compType == CompType::SInt ? @"discard_int" : @"discard_float";
    pd.fragmentFunction = [state->library newFunctionWithName:fragment];
    pd.rasterSampleCount = texture->sampleCount();
    const bool combined = format == MTL::PixelFormatDepth32Float_Stencil8 ||
                          format == MTL::PixelFormatDepth24Unorm_Stencil8;
    if(aspect == 0) pd.colorAttachments[0].pixelFormat = target.pixelFormat;
    else
    {
      if(aspect == 1 || combined) pd.depthAttachmentPixelFormat = target.pixelFormat;
      if(aspect == 2 || combined) pd.stencilAttachmentPixelFormat = target.pixelFormat;
    }
    NSError *error = nil;
    pipeline = [cb.device newRenderPipelineStateWithDescriptor:pd error:&error];
    [pd.vertexFunction release]; [pd.fragmentFunction release]; [pd release];
    if(!pipeline)
    { RDCERR("Metal discard pipeline failed: %s", error.description.UTF8String); return false; }
    state->pipelines[key] = pipeline;
  }
  id<MTLDepthStencilState> depthState = nil;
  if(aspect)
  {
    depthState = state->depthStates[aspect];
    if(!depthState)
    {
      MTLDepthStencilDescriptor *ds = [MTLDepthStencilDescriptor new];
      ds.depthCompareFunction = MTLCompareFunctionAlways; ds.depthWriteEnabled = aspect == 1;
      if(aspect == 2)
      {
        MTLStencilDescriptor *st = [MTLStencilDescriptor new];
        st.stencilCompareFunction = MTLCompareFunctionAlways;
        st.stencilFailureOperation = st.depthFailureOperation = st.depthStencilPassOperation = MTLStencilOperationReplace;
        st.readMask = st.writeMask = 255; ds.frontFaceStencil = ds.backFaceStencil = st; [st release];
      }
      depthState = [cb.device newDepthStencilStateWithDescriptor:ds]; [ds release];
      if(!depthState) return false;
      state->depthStates[aspect] = depthState;
    }
  }
  // Use the common glyph and decode its typed white value, including packed and
  // signed-normalised formats, rather than maintaining a Metal-only pattern.
  const bytebuf mask = GetDiscardPattern(type, MakeResourceFormat(MTL::PixelFormatR8Unorm));
  if(mask.size() != DiscardPatternWidth * DiscardPatternHeight) return false;
  struct Params { FloatVector white; uint32_t mode, padding[3]; } params = {};
  params.white = FloatVector(1,1,1,1); params.mode = 2;
  if(!aspect)
  {
    uint32_t bw=0,bh=0,bytes=0;
    if(!GetTextureDataBlockShape(format,bw,bh,bytes) || bw != 1 || bh != 1) return false;
    const bytebuf typed = GetDiscardPattern(type, fmt);
    size_t white = 0;
    while(white < mask.size() && !mask[white]) white++;
    bool valid = false;
    if(typed.size() != mask.size()*bytes || white == mask.size()) return false;
    params.white = DecodeFormattedComponents(fmt, typed.data()+white*bytes, &valid);
    if(!valid) return false;
  }
  for(uint64_t layer = 0; layer < layers; layer++)
  {
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    const auto set = [&](MTLRenderPassAttachmentDescriptor *a, bool discard) {
      a.texture = target; a.level = level; a.slice = slice + layer;
      a.loadAction = discard ? MTLLoadActionDontCare : MTLLoadActionLoad;
      a.storeAction = MTLStoreActionStore;
    };
    const bool combined = format == MTL::PixelFormatDepth32Float_Stencil8 ||
                          format == MTL::PixelFormatDepth24Unorm_Stencil8;
    if(!aspect) set(pass.colorAttachments[0], true);
    else
    {
      if(aspect == 1 || combined) set(pass.depthAttachment, aspect == 1);
      if(aspect == 2 || combined) set(pass.stencilAttachment, aspect == 2);
    }
    id<MTLRenderCommandEncoder> encoder = [cb renderCommandEncoderWithDescriptor:pass];
    if(!encoder) return false;
    [encoder setRenderPipelineState:pipeline];
    if(depthState) [encoder setDepthStencilState:depthState];
    [encoder setFragmentBytes:mask.data() length:mask.size() atIndex:0];
    if(aspect == 2)
    {
      for(uint32_t v=0;v<2;v++)
      {
        params.mode = v; [encoder setStencilReferenceValue:v ? 255 : 0];
        [encoder setFragmentBytes:&params length:sizeof(params) atIndex:1];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
      }
    }
    else
    {
      [encoder setFragmentBytes:&params length:sizeof(params) atIndex:1];
      [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    }
    [encoder endEncoding];
  }
  return true;
}

void WrappedMTLDevice::ApplyReplayLoadDiscards(MTL::CommandBuffer *command,
                                              MTL::RenderPassDescriptor *descriptor)
{
  // Vulkan uses STORE operations in partial replay passes so viewing a draw
  // does not discard its output at our artificial end boundary. Metal permits
  // encoder store setters only when the descriptor initially says Unknown.
  // Keep the captured descriptor intact; finalise its native operations at the
  // real endEncoding, or preserve them at a debugger-created partial boundary.
  for(unsigned i = 0; i < 8; i++)
    if(descriptor->colorAttachments()->object(i)->texture())
      descriptor->colorAttachments()->object(i)->setStoreAction(MTL::StoreActionUnknown);
  if(descriptor->depthAttachment()->texture())
    descriptor->depthAttachment()->setStoreAction(MTL::StoreActionUnknown);
  if(descriptor->stencilAttachment()->texture())
    descriptor->stencilAttachment()->setStoreAction(MTL::StoreActionUnknown);
  if(m_ReplayOptions.optimisation == ReplayOptimisationLevel::Fastest) return;
  const uint64_t layers = RDCMAX(1ULL, uint64_t(descriptor->renderTargetArrayLength()));
  const auto apply = [&](MTL::RenderPassAttachmentDescriptor *attachment, unsigned aspect) {
    if(attachment->texture() && attachment->loadAction() == MTL::LoadActionDontCare &&
       (attachment->texture()->sampleCount() > 1 ||
        attachment->texture()->pixelFormat() == MTL::PixelFormatDepth24Unorm_Stencil8 ?
         FillReplayRenderDiscard(command, attachment->texture(), attachment->level(), attachment->slice(),
                                 layers, DiscardType::RenderPassLoad, aspect) :
         FillDiscard(command, attachment->texture(), attachment->level(), attachment->slice(), attachment->depthPlane(),
                     layers, DiscardType::RenderPassLoad, aspect)))
      attachment->setLoadAction(MTL::LoadActionLoad);
  };
  for(unsigned i = 0; i < 8; i++) apply(descriptor->colorAttachments()->object(i), 0);
  apply(descriptor->depthAttachment(), 1);
  apply(descriptor->stencilAttachment(), 2);
}

template <typename Encoder>
static void FinaliseStores(Encoder *encoder, const RDMTL::RenderPassDescriptor &descriptor, bool partial)
{
  const auto store = [partial](const RDMTL::RenderPassAttachmentDescriptor &attachment) {
    if(partial || attachment.storeAction == MTL::StoreActionUnknown)
      return attachment.resolveTexture ? MTL::StoreActionStoreAndMultisampleResolve : MTL::StoreActionStore;
    return attachment.storeAction;
  };
  for(unsigned i = 0; i < descriptor.colorAttachments.size(); i++)
    if(descriptor.colorAttachments[i].texture)
      encoder->setColorStoreAction(store(descriptor.colorAttachments[i]), i);
  if(descriptor.depthAttachment.texture)
    encoder->setDepthStoreAction(store(descriptor.depthAttachment));
  if(descriptor.stencilAttachment.texture)
    encoder->setStencilStoreAction(store(descriptor.stencilAttachment));
}

void WrappedMTLDevice::FinaliseReplayStores(MTL::RenderCommandEncoder *encoder, bool partial)
{
  FinaliseStores(encoder, GetReplay()->GetRenderPassDescriptor(), partial);
}

void WrappedMTLDevice::FinaliseReplayStores(MTL::ParallelRenderCommandEncoder *encoder, bool partial)
{
  FinaliseStores(encoder, GetReplay()->GetRenderPassDescriptor(), partial);
}

void WrappedMTLDevice::ApplyReplayStoreDiscards(MTL::CommandBuffer *command,
                                               const RDMTL::RenderPassDescriptor &descriptor)
{
  if(m_ReplayOptions.optimisation == ReplayOptimisationLevel::Fastest) return;
  const uint64_t layers = RDCMAX(1ULL, uint64_t(descriptor.renderTargetArrayLength));
  const auto apply = [&](const RDMTL::RenderPassAttachmentDescriptor &attachment, unsigned aspect) {
    if(attachment.texture && (attachment.storeAction == MTL::StoreActionDontCare ||
                             attachment.storeAction == MTL::StoreActionMultisampleResolve))
    {
      MTL::Texture *texture = Unwrap(attachment.texture);
      if(texture->sampleCount() > 1 || texture->pixelFormat() == MTL::PixelFormatDepth24Unorm_Stencil8)
        FillReplayRenderDiscard(command, texture, attachment.level, attachment.slice,
                                layers, DiscardType::RenderPassStore, aspect);
      else
        FillDiscard(command, texture, attachment.level, attachment.slice, attachment.depthPlane,
                    layers, DiscardType::RenderPassStore, aspect);
    }
  };
  for(const auto &attachment : descriptor.colorAttachments) apply(attachment, 0);
  apply(descriptor.depthAttachment, 1);
  apply(descriptor.stencilAttachment, 2);
}
