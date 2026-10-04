// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  @autoreleasepool
  {
    const char *mode=argc>1 ? argv[1] : "serial";
    const bool parallel=!strcmp(mode,"parallel"), exportDepth=!strcmp(mode,"depth-export");
    const bool stencil=!strcmp(mode,"stencil");
    const MTLPixelFormat depthFormat=stencil ? MTLPixelFormatDepth32Float_Stencil8 : MTLPixelFormatDepth32Float;
    MTLCompareFunction compare=!strcmp(mode,"greater") ? MTLCompareFunctionGreater :
      !strcmp(mode,"equal") ? MTLCompareFunctionEqual : !strcmp(mode,"always") ? MTLCompareFunctionAlways : MTLCompareFunctionLess;
    id<MTLDevice> d=MTLCreateSystemDefaultDevice(); auto q=[d newCommandQueue]; NSError *err=nil;
    NSString *source=@"#include <metal_stdlib>\nusing namespace metal;\n"
      "vertex float4 vs(uint i [[vertex_id]], constant float4 &c [[buffer(0)]]) {"
      "const float2 p[6]={float2(-1,-1),float2(1,-1),float2(-1,1),float2(-1,1),float2(1,-1),float2(1,1)};"
      "return float4(p[i].x*c.x+c.y,p[i].y,c.z,1);}"
      "struct Seed {float4 a [[color(0)]];float4 b [[color(1)]];};"
      "fragment Seed seedfs() {return {float4(.75,.125,0,1),float4(.25,0,.75,1)};}";
    source=[source stringByAppendingString:exportDepth ?
      @"struct Out {float4 a [[color(0)]];float4 b [[color(1)]];float z [[depth(any)]];};" :
      @"struct Out {float4 a [[color(0)]];float4 b [[color(1)]];};"];
    source=[source stringByAppendingString:@"fragment Out fs(float4 p [[position]], texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {"
      "if(p.x>=24) discard_fragment(); float4 c=t.sample(s,float2(.5));"];
    source=[source stringByAppendingString:exportDepth ?
      @"return {c,float4(c.z,c.x,c.y,.25),.1f};}" : @"return {c,float4(c.z,c.x,c.y,.25)};}"];
    auto lib=[d newLibraryWithSource:source options:nil error:&err];
    auto pd=[MTLRenderPipelineDescriptor new]; pd.vertexFunction=[lib newFunctionWithName:@"vs"];
    pd.fragmentFunction=[lib newFunctionWithName:@"seedfs"];
    pd.depthAttachmentPixelFormat=depthFormat;
    if(stencil) pd.stencilAttachmentPixelFormat=depthFormat;
    for(unsigned i=0;i<2;i++) pd.colorAttachments[i].pixelFormat=MTLPixelFormatRGBA8Unorm;
    auto seedPipeline=[d newRenderPipelineStateWithDescriptor:pd error:&err];
    pd.fragmentFunction=[lib newFunctionWithName:@"fs"];
    for(unsigned i=0;i<2;i++)
    {
      auto a=pd.colorAttachments[i]; a.blendingEnabled=YES;
      a.sourceRGBBlendFactor=MTLBlendFactorSourceAlpha; a.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
      a.sourceAlphaBlendFactor=MTLBlendFactorOne; a.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
    }
    auto pipeline=[d newRenderPipelineStateWithDescriptor:pd error:&err];
    if(!seedPipeline || !pipeline) {fprintf(stderr,"pipeline %s\n",err.description.UTF8String);return 2;}
    auto ds=[MTLDepthStencilDescriptor new]; ds.depthCompareFunction=MTLCompareFunctionAlways; ds.depthWriteEnabled=YES;
    auto seedDS=[d newDepthStencilStateWithDescriptor:ds]; ds.depthCompareFunction=compare;
    if(stencil)
    {
      auto test=[MTLStencilDescriptor new]; test.stencilCompareFunction=MTLCompareFunctionEqual;
      ds.frontFaceStencil=test; ds.backFaceStencil=test;
    }
    auto drawDS=[d newDepthStencilStateWithDescriptor:ds];
    auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:32 height:16 mipmapped:NO];
    td.storageMode=MTLStorageModeShared; td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    id<MTLTexture> colors[2]={[d newTextureWithDescriptor:td],[d newTextureWithDescriptor:td]};
    colors[0].label=@"Clear original MRT 0"; colors[1].label=@"Clear original MRT 1";
    td.pixelFormat=depthFormat; td.storageMode=MTLStorageModePrivate;
    auto depth=[d newTextureWithDescriptor:td]; depth.label=@"Clear original depth";
    auto inputDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    inputDesc.storageMode=MTLStorageModeShared; inputDesc.usage=MTLTextureUsageShaderRead;
    auto input=[d newTextureWithDescriptor:inputDesc]; uint8_t texel[4]={0,255,0,128};
    [input replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:texel bytesPerRow:4];
    auto sampler=[d newSamplerStateWithDescriptor:[MTLSamplerDescriptor new]];
    uint32_t args[4]={6,1,0,0}; auto indirect=[d newBufferWithBytes:args length:sizeof(args) options:MTLResourceStorageModeShared];
    auto makePass=[&](bool cleared) {
      auto p=[MTLRenderPassDescriptor renderPassDescriptor];
      for(unsigned i=0;i<2;i++)
      {
        p.colorAttachments[i].texture=colors[i]; p.colorAttachments[i].loadAction=MTLLoadActionClear;
        p.colorAttachments[i].storeAction=MTLStoreActionStore;
        p.colorAttachments[i].clearColor=cleared ? MTLClearColorMake(.2,.3,.4,1) :
          i ? MTLClearColorMake(.6,.1,.7,.25) : MTLClearColorMake(.1,.2,.8,.75);
      }
      p.depthAttachment.texture=depth; p.depthAttachment.loadAction=MTLLoadActionClear;
      p.depthAttachment.clearDepth=.75; p.depthAttachment.storeAction=MTLStoreActionStore;
      if(stencil)
      {
        p.stencilAttachment.texture=depth; p.stencilAttachment.loadAction=MTLLoadActionClear;
        p.stencilAttachment.clearStencil=7; p.stencilAttachment.storeAction=MTLStoreActionStore;
      }
      return p;
    };
    auto seed=[&](id<MTLRenderCommandEncoder> e) {
      float cfg[4]={.5,-.5,.25,0}; [e setRenderPipelineState:seedPipeline]; [e setDepthStencilState:seedDS];
      [e setVertexBytes:cfg length:sizeof(cfg) atIndex:0];
      [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    };
    auto draw=[&](id<MTLRenderCommandEncoder> e) {
      float cfg[4]={1,0,.5,0}; [e setRenderPipelineState:pipeline]; [e setDepthStencilState:drawDS];
      if(stencil) [e setStencilReferenceValue:7];
      [e setVertexBytes:cfg length:sizeof(cfg) atIndex:0]; [e setFragmentTexture:input atIndex:0];
      [e setFragmentSamplerState:sampler atIndex:0];
      [e drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:indirect indirectBufferOffset:0];
    };
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 3;
      api->SetCaptureFilePathTemplate(path); api->StartFrameCapture(nullptr,nullptr);
    }
    auto command=[q commandBuffer]; auto pass=makePass(false);
    if(!strcmp(mode,"discard-store")) pass.depthAttachment.storeAction=MTLStoreActionDontCare;
    auto parent=parallel ? [command parallelRenderCommandEncoderWithDescriptor:pass] : nil;
    auto encoder=parent ? [parent renderCommandEncoder] : [command renderCommandEncoderWithDescriptor:pass];
    seed(encoder);
    if(parent) {[encoder endEncoding];encoder=[parent renderCommandEncoder];}
    [encoder pushDebugGroup:@"Clear selected indirect draw"]; draw(encoder); [encoder popDebugGroup];
    [encoder endEncoding]; if(parent) [parent endEncoding];
    auto layer=[CAMetalLayer layer]; layer.device=d; layer.framebufferOnly=NO;
    layer.pixelFormat=MTLPixelFormatBGRA8Unorm; layer.drawableSize=CGSizeMake(2,2);
    auto drawable=[layer nextDrawable]; auto pp=[MTLRenderPassDescriptor renderPassDescriptor];
    pp.colorAttachments[0].texture=drawable.texture; pp.colorAttachments[0].loadAction=MTLLoadActionClear;
    pp.colorAttachments[0].storeAction=MTLStoreActionStore;
    [[command renderCommandEncoderWithDescriptor:pp] endEncoding]; [command presentDrawable:drawable];
    [command commit]; [command waitUntilCompleted]; if(command.error) return 4;
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 5;
    auto dump=[&](const char *name) {
      const char *dir=getenv("RENDERDOC_METAL_CLEAR_REFERENCE_DIR"); if(!dir) return true;
      for(unsigned i=0;i<2;i++)
      {
        uint8_t pixels[32*16*4]; [colors[i] getBytes:pixels bytesPerRow:128 fromRegion:MTLRegionMake2D(0,0,32,16) mipmapLevel:0];
        std::string path=std::string(dir)+"/"+name+"-"+std::to_string(i)+".bin";
        auto f=fopen(path.c_str(),"wb"); if(!f)return false; fwrite(pixels,1,sizeof(pixels),f);fclose(f);
      }
      return true;
    };
    if(!dump("original")) return 6;
    for(const char *kind : {"draw-colour","pass-colour","draw-depth","pass-depth"})
    {
      const bool wholePass=!strncmp(kind,"pass",4), depthSelected=strstr(kind,"depth");
      auto reference=[q commandBuffer]; auto p=makePass(wholePass);
      auto e=[reference renderCommandEncoderWithDescriptor:p]; seed(e);
      if(!wholePass)
      {
        [e endEncoding]; [reference commit]; [reference waitUntilCompleted]; if(reference.error)return 7;
        reference=[q commandBuffer]; p=makePass(true);
        p.depthAttachment.loadAction=MTLLoadActionLoad;
        if(stencil) p.stencilAttachment.loadAction=MTLLoadActionLoad;
        if(depthSelected && compare!=MTLCompareFunctionEqual && compare!=MTLCompareFunctionAlways)
        {
          p.depthAttachment.loadAction=MTLLoadActionClear;
          p.depthAttachment.clearDepth=compare==MTLCompareFunctionGreater ? 0 : 1;
          if(stencil)
          {
            p.stencilAttachment.loadAction=MTLLoadActionClear;
            p.stencilAttachment.clearStencil=0;
          }
        }
        e=[reference renderCommandEncoderWithDescriptor:p];
      }
      draw(e); [e endEncoding]; [reference commit]; [reference waitUntilCompleted];
      if(reference.error || !dump(kind))return 8;
    }
    puts("PASS Native original and four Clear Before references: two MRTs, sampled fragment, discard, blending, indirect draw");
  }
  return 0;
}
