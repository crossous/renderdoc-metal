// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include "renderdoc/api/app/renderdoc_app.h"

// Small real GPU consumers read every sample, rather than inspecting undefined
// native DontCare bytes or relying on an averaged resolve to prove sample coverage.
int main()
{
  @autoreleasepool {
    id<MTLDevice> d=MTLCreateSystemDefaultDevice(); NSError *error=nil;
    printf("Device=%s D24S8=%u MSAA2=%u MSAA4=%u MSAA8=%u\n",d.name.UTF8String,
      d.depth24Stencil8PixelFormatSupported,[d supportsTextureSampleCount:2],
      [d supportsTextureSampleCount:4],[d supportsTextureSampleCount:8]);
    const bool combinedResolveProbe=getenv("RENDERDOC_METAL_PROBE_NATIVE_COMBINED_RESOLVE")!=nullptr;
    const bool storePlanes=getenv("RENDERDOC_METAL_PROBE_STORE_PLANES")!=nullptr;
    auto lib=[d newLibraryWithSource:@R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs(uint i [[vertex_id]]) {
  float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[i],0,1);
}
fragment float4 fs(){return float4(.25,.5,.75,1);}
fragment float4 fs_probe(){return float4(1);}
kernel void read_float(texture2d_ms<float,access::read> t [[texture(0)]],
  device float4 *out [[buffer(0)]],uint2 p [[thread_position_in_grid]]) {
  if(p.x>=t.get_width()||p.y>=t.get_height())return;
  for(uint s=0;s<t.get_num_samples();s++)out[(p.y*t.get_width()+p.x)*t.get_num_samples()+s]=t.read(p,s);
}
kernel void read_uint(texture2d_ms<uint,access::read> t [[texture(0)]],
  device float4 *out [[buffer(0)]],uint2 p [[thread_position_in_grid]]) {
  if(p.x>=t.get_width()||p.y>=t.get_height())return;
  for(uint s=0;s<t.get_num_samples();s++)out[(p.y*t.get_width()+p.x)*t.get_num_samples()+s]=float4(t.read(p,s));
}
kernel void read_depth(depth2d_ms<float,access::read> t [[texture(0)]],
  device float4 *out [[buffer(0)]],uint2 p [[thread_position_in_grid]]) {
  if(p.x>=t.get_width()||p.y>=t.get_height())return;
  for(uint s=0;s<t.get_num_samples();s++)out[(p.y*t.get_width()+p.x)*t.get_num_samples()+s]=float4(t.read(p,s));
}
kernel void read_array(texture2d_ms_array<float,access::read> t [[texture(0)]],
  device float4 *out [[buffer(0)]],uint2 p [[thread_position_in_grid]]) {
  if(p.x>=t.get_width()||p.y>=t.get_height())return;
  for(uint s=0;s<t.get_num_samples();s++)out[(p.y*t.get_width()+p.x)*t.get_num_samples()+s]=t.read(p,1,s);
}
)" options:nil error:&error];
    if(!lib){fprintf(stderr,"MSL: %s\n",error.description.UTF8String);return 2;}
    auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[lib newFunctionWithName:@"vs"];
    pd.fragmentFunction=[lib newFunctionWithName:@"fs"];pd.rasterSampleCount=4;
    pd.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
    auto drawPSO=[d newRenderPipelineStateWithDescriptor:pd error:&error];if(!drawPSO)return 3;
    pd.fragmentFunction=[lib newFunctionWithName:@"fs_probe"];
    pd.depthAttachmentPixelFormat=pd.stencilAttachmentPixelFormat=MTLPixelFormatDepth32Float_Stencil8;
    auto stencilPSO=[d newRenderPipelineStateWithDescriptor:pd error:&error];if(!stencilPSO)return 11;
    auto ds=[MTLDepthStencilDescriptor new];ds.depthCompareFunction=MTLCompareFunctionAlways;
    auto st=[MTLStencilDescriptor new];st.stencilCompareFunction=MTLCompareFunctionEqual;st.writeMask=0;
    ds.frontFaceStencil=ds.backFaceStencil=st;auto stencilState=[d newDepthStencilStateWithDescriptor:ds];
    NSMutableArray *pipelines=[NSMutableArray new];
    for(NSString *name in @[@"read_float",@"read_uint",@"read_depth",@"read_array"]){
      auto p=[d newComputePipelineStateWithFunction:[lib newFunctionWithName:name] error:&error];
      if(!p){fprintf(stderr,"PSO: %s\n",error.description.UTF8String);return 4;}[pipelines addObject:p];
    }
    constexpr unsigned count=15,w=70,h=18;
    id<MTLTexture> textures[count],resolves[count];id<MTLBuffer> outputs[count];
    const MTLPixelFormat fmts[count]={MTLPixelFormatRGBA8Unorm,MTLPixelFormatR32Float,
      MTLPixelFormatR32Uint,MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,
      MTLPixelFormatRGBA8Unorm,MTLPixelFormatDepth32Float,MTLPixelFormatDepth32Float_Stencil8,
      MTLPixelFormatDepth32Float_Stencil8,MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,
      MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm};
    NSMutableArray *heaps=[NSMutableArray new];
    for(unsigned i=0;i<count;i++){
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmts[i] width:w height:h mipmapped:NO];
      td.textureType=i==10?MTLTextureType2DMultisampleArray:MTLTextureType2DMultisample;
      td.sampleCount=i==10?2:i==14&&[d supportsTextureSampleCount:8]?8:4;td.arrayLength=i==10?2:1;
      td.storageMode=i==11?MTLStorageModeMemoryless:MTLStorageModePrivate;
      td.usage=MTLTextureUsageRenderTarget|(i==11?0:MTLTextureUsageShaderRead);
      if(i==12)td.hazardTrackingMode=MTLHazardTrackingModeUntracked;
      if(i>=13){auto hd=[MTLHeapDescriptor new];hd.storageMode=MTLStorageModePrivate;
        hd.type=i==13?MTLHeapTypeAutomatic:MTLHeapTypePlacement;
        hd.hazardTrackingMode=i==13?MTLHazardTrackingModeUntracked:MTLHazardTrackingModeDefault;
        auto sa=[d heapTextureSizeAndAlignWithDescriptor:td];hd.size=MAX(4096,((sa.size+sa.align-1)/sa.align)*sa.align);
        auto heap=[d newHeapWithDescriptor:hd];if(!heap)return 12;[heaps addObject:heap];
        textures[i]=i==13?[heap newTextureWithDescriptor:td]:[heap newTextureWithDescriptor:td offset:0];
      }else textures[i]=[d newTextureWithDescriptor:td];if(!textures[i])return 5;
      textures[i].label=[NSString stringWithFormat:@"MSAA discard source %u",i];
      td.textureType=MTLTextureType2D;td.sampleCount=1;td.arrayLength=1;
      td.storageMode=MTLStorageModePrivate;td.hazardTrackingMode=MTLHazardTrackingModeTracked;
      td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
      resolves[i]=[d newTextureWithDescriptor:td];if(!resolves[i])return 6;
      resolves[i].label=[NSString stringWithFormat:@"MSAA discard resolve %u",i];
      outputs[i]=[d newBufferWithLength:w*h*textures[i].sampleCount*16 options:MTLResourceStorageModeShared];
      outputs[i].label=[NSString stringWithFormat:@"MSAA discard samples %u",i];
      memset(outputs[i].contents,0,outputs[i].length);
    }
    auto fence=[d newFence];auto q=[d newCommandQueue];
    id<MTLTexture> stencilTargets[2];id<MTLBuffer> stencilOutputs[2];
    for(unsigned i=0;i<2;i++){
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:w height:h mipmapped:NO];
      td.textureType=MTLTextureType2DMultisample;td.sampleCount=4;td.storageMode=MTLStorageModePrivate;
      td.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;stencilTargets[i]=[d newTextureWithDescriptor:td];
      stencilOutputs[i]=[d newBufferWithLength:w*h*4*16 options:MTLResourceStorageModeShared];
      memset(stencilOutputs[i].contents,0,stencilOutputs[i].length);
      stencilOutputs[i].label=[NSString stringWithFormat:@"MSAA discard stencil %u",i+7];
    }
    auto layer=[CAMetalLayer layer];layer.device=d;layer.framebufferOnly=NO;
    layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.drawableSize=CGSizeMake(2,2);
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(auto path=getenv("RENDERDOC_METAL_CAPTURE_PATH")){
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 7;
      api->SetCaptureFilePathTemplate(path);api->StartFrameCapture(nullptr,nullptr);
    }
    auto cb=[q commandBufferWithUnretainedReferences];
    for(unsigned i=0;i<count;i++){
      auto p=[MTLRenderPassDescriptor renderPassDescriptor];
      if(i>=6&&i<=8){
        if(i==7&&combinedResolveProbe&&!api){auto init=[MTLRenderPassDescriptor renderPassDescriptor];
          init.depthAttachment.texture=textures[i];init.depthAttachment.loadAction=MTLLoadActionClear;
          init.depthAttachment.clearDepth=0;init.depthAttachment.storeAction=MTLStoreActionStore;
          init.stencilAttachment.texture=textures[i];init.stencilAttachment.loadAction=MTLLoadActionClear;
          init.stencilAttachment.storeAction=MTLStoreActionStore;[[cb renderCommandEncoderWithDescriptor:init] endEncoding];}
        p.depthAttachment.texture=textures[i];p.depthAttachment.clearDepth=.25;
        p.depthAttachment.loadAction=i==8?MTLLoadActionClear:MTLLoadActionDontCare;
        if(i==7&&combinedResolveProbe&&!api)p.depthAttachment.loadAction=MTLLoadActionLoad;
        p.depthAttachment.storeAction=i==6||combinedResolveProbe?MTLStoreActionStoreAndMultisampleResolve:MTLStoreActionStore;
        if(i==6||combinedResolveProbe)p.depthAttachment.resolveTexture=resolves[i];
        p.depthAttachment.depthResolveFilter=MTLMultisampleDepthResolveFilterSample0;
        if(i>=7){p.stencilAttachment.texture=textures[i];p.stencilAttachment.clearStencil=37;
          p.stencilAttachment.loadAction=i==7?MTLLoadActionClear:MTLLoadActionDontCare;
          p.stencilAttachment.storeAction=combinedResolveProbe?MTLStoreActionStoreAndMultisampleResolve:MTLStoreActionStore;
          if(combinedResolveProbe)p.stencilAttachment.resolveTexture=resolves[i];
          p.stencilAttachment.stencilResolveFilter=MTLMultisampleStencilResolveFilterSample0;}
        if(storePlanes&&i>=7){
          p.depthAttachment.loadAction=p.stencilAttachment.loadAction=MTLLoadActionClear;
          p.depthAttachment.storeAction=i==7?MTLStoreActionDontCare:MTLStoreActionStore;
          p.stencilAttachment.storeAction=i==8?MTLStoreActionDontCare:MTLStoreActionStore;
          p.depthAttachment.resolveTexture=p.stencilAttachment.resolveTexture=nil;
        }
      }else{
        auto a=p.colorAttachments[0];a.texture=textures[i];a.slice=i==10?1:0;
        a.clearColor=MTLClearColorMake(.25,.5,.75,1);
        a.loadAction=(i>=3&&i<=5)||i==9?MTLLoadActionClear:MTLLoadActionDontCare;
        a.storeAction=i==3?MTLStoreActionDontCare:i==4||i==11?MTLStoreActionMultisampleResolve:
          i==5?MTLStoreActionStoreAndMultisampleResolve:i==9?MTLStoreActionUnknown:MTLStoreActionStore;
        if(i==4||i==5||i==11)a.resolveTexture=resolves[i];
      }
      if(i==9){auto parent=[cb parallelRenderCommandEncoderWithDescriptor:p];
        [[parent renderCommandEncoder] endEncoding];[parent setColorStoreAction:MTLStoreActionDontCare atIndex:0];[parent endEncoding];
      }else{
        auto e=[cb renderCommandEncoderWithDescriptor:p];
        if(i==0){[e setRenderPipelineState:drawPSO];[e setScissorRect:MTLScissorRect{0,0,4,4}];
          [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];}
        if(i>=12)[e updateFence:fence afterStages:MTLRenderStageFragment];
        [e endEncoding];
      }
      if(i==7||i==8){
        auto probe=[MTLRenderPassDescriptor renderPassDescriptor];
        probe.colorAttachments[0].texture=stencilTargets[i-7];probe.colorAttachments[0].loadAction=MTLLoadActionClear;
        probe.colorAttachments[0].storeAction=MTLStoreActionStore;
        probe.depthAttachment.texture=probe.stencilAttachment.texture=textures[i];
        probe.depthAttachment.loadAction=probe.stencilAttachment.loadAction=MTLLoadActionLoad;
        probe.depthAttachment.storeAction=probe.stencilAttachment.storeAction=MTLStoreActionStore;
        auto e=[cb renderCommandEncoderWithDescriptor:probe];[e setRenderPipelineState:stencilPSO];
        [e setDepthStencilState:stencilState];[e setStencilReferenceValue:i==7?37:255];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[e endEncoding];
        auto c=[cb computeCommandEncoder];[c setComputePipelineState:pipelines[0]];
        [c setTexture:stencilTargets[i-7] atIndex:0];[c setBuffer:stencilOutputs[i-7] offset:0 atIndex:0];
        [c dispatchThreadgroups:MTLSizeMake((w+7)/8,(h+7)/8,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];[c endEncoding];
      }
      if(i==11)continue; // native memoryless data can only be observed through its resolve
      auto e=[cb computeCommandEncoder];if(i>=12)[e waitForFence:fence];
      [e setComputePipelineState:pipelines[i==2?1:i>=6&&i<=8?2:i==10?3:0]];
      [e setTexture:textures[i] atIndex:0];[e setBuffer:outputs[i] offset:0 atIndex:0];
      [e dispatchThreadgroups:MTLSizeMake((w+7)/8,(h+7)/8,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];[e endEncoding];
    }
    auto draw=[layer nextDrawable];auto p=[MTLRenderPassDescriptor renderPassDescriptor];
    p.colorAttachments[0].texture=draw.texture;p.colorAttachments[0].loadAction=MTLLoadActionClear;
    p.colorAttachments[0].storeAction=MTLStoreActionStore;[[cb renderCommandEncoderWithDescriptor:p] endEncoding];
    [cb presentDrawable:draw];[cb commit];[cb waitUntilCompleted];
    if(cb.error){fprintf(stderr,"%s\n",cb.error.description.UTF8String);return 8;}
    if(!api&&combinedResolveProbe){auto inspect=[q commandBuffer];auto blit=[inspect blitCommandEncoder];
    auto resolvedDepth=[d newBufferWithLength:512*h*2 options:MTLResourceStorageModeShared];
    for(unsigned i=7;i<=8;i++)[blit copyFromTexture:resolves[i] sourceSlice:0 sourceLevel:0
      sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(w,h,1) toBuffer:resolvedDepth
      destinationOffset:(i-7)*512*h destinationBytesPerRow:512 destinationBytesPerImage:512*h
      options:MTLBlitOptionDepthFromDepthStencil];
    [blit endEncoding];[inspect commit];[inspect waitUntilCompleted];
    float z7,z8;memcpy(&z7,resolvedDepth.contents,4);memcpy(&z8,(char *)resolvedDepth.contents+512*h,4);
    printf("Native combined resolve depth first pixels: discarded=%g clear=%g\n",z7,z8);
    if(inspect.error||z8!=.25f)return 10;}
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 9;
    puts("PASS Native MSAA colour/depth/array, resolve/store, independent stencil, memoryless and untracked fenced passes");
  }
}
