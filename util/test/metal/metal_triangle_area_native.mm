// SPDX-License-Identifier: MIT
// Component proof: raster barycentric derivatives must equal Vulkan's original
// projected triangle area, including clipping and unequal clip-space w.
// No RenderDoc feature is advertised by this standalone Native test.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <dlfcn.h>
#import <QuartzCore/CAMetalLayer.h>
#include "renderdoc/api/app/renderdoc_app.h"
struct Position {float x,y,z,w;};
int main(int argc, char **argv)
{
  @autoreleasepool
  {
    constexpr unsigned width=64,height=32;
    id<MTLDevice> dev=MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue=[dev newCommandQueue];NSError *error=nil;
    NSString *msl=@"#include <metal_stdlib>\nusing namespace metal;\n"
      "vertex float4 vs(uint i [[vertex_id]],constant float4 *p [[buffer(0)]]) {return p[i];}"
      "fragment float4 area(float3 b [[barycentric_coord,center_no_perspective]]) {"
      "float2 x=dfdx(b.xy),y=dfdy(b.xy);"
      "float a=max(.5f/abs(x.x*y.y-x.y*y.x),.001f);return float4(a,a,a,1);} ";
    id<MTLLibrary> lib=[dev newLibraryWithSource:msl options:nil error:&error];
    if(!lib) {fprintf(stderr,"MSL: %s\n",error.description.UTF8String);return 2;}
    auto desc=[MTLRenderPipelineDescriptor new];
    desc.vertexFunction=[lib newFunctionWithName:@"vs"];desc.fragmentFunction=[lib newFunctionWithName:@"area"];
    desc.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA32Float;
    auto pipeline=[dev newRenderPipelineStateWithDescriptor:desc error:&error];
    if(!pipeline) {fprintf(stderr,"PSO: %s\n",error.description.UTF8String);return 3;}
    auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:width height:height mipmapped:NO];
    td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageRenderTarget;
    auto target=[dev newTextureWithDescriptor:td];target.label=@"Triangle original colour";
    RENDERDOC_API_1_7_0 *api=nullptr;
    auto layer=[CAMetalLayer layer];layer.device=dev;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 7;
      api->SetCaptureFilePathTemplate(path);
    }
    const unsigned first=argc>1?unsigned(strtoul(argv[1],nullptr,10)):0;
    if(first>10) return 8;
    const unsigned end=argc>1?first+1:11;
    for(unsigned mode=first;mode<end;mode++)
    {
      double xy[3][2]={{4,4},{20,4},{4,20}};
      if(mode==1) {xy[0][0]=2;xy[0][1]=2;xy[1][0]=4;xy[1][1]=2;xy[2][0]=2;xy[2][1]=4;}
      if(mode>=2 && mode<=6) {xy[0][0]=-8;xy[0][1]=4;xy[1][0]=24;xy[1][1]=4;xy[2][0]=-8;xy[2][1]=20;}
      Position positions[6] = {};
      for(unsigned i=0;i<3;i++)
      {
        float w=mode==3 || mode==4 ? (i==0?.5f:(i==1?2.f:4.f)) : 1.f;
        positions[i]={float(xy[i][0]*2/width-1)*w,float(1-xy[i][1]*2/height)*w,.5f*w,w};
      }
      if(mode==4) positions[0].z=-.25f;
      if(mode==9) positions[3]={float(20.*2/width-1),float(1-20.*2/height),.5f,1};
      if(mode==10)
      {
        positions[0].z=positions[1].z=positions[2].z=.2f;
        positions[3]={float(4.*2/width-1),float(1-4.*2/height),.8f,1};
        positions[4]={float(36.*2/width-1),float(1-4.*2/height),.8f,1};
        positions[5]={float(4.*2/width-1),float(1-36.*2/height),.8f,1};
      }
      double area=.5*std::fabs((xy[1][0]-xy[0][0])*(xy[2][1]-xy[0][1])-(xy[1][1]-xy[0][1])*(xy[2][0]-xy[0][0]));
      if(mode==5) area*=.25;
      if(api) api->StartFrameCapture(nullptr,nullptr);
      auto command=[queue commandBuffer];auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,0);pass.colorAttachments[0].storeAction=MTLStoreActionStore;
      id<MTLDepthStencilState> depthState=nil;
      id<MTLRenderPipelineState> selected=pipeline;
      if(mode==10)
      {
        auto depthDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:width height:height mipmapped:NO];
        depthDesc.storageMode=MTLStorageModePrivate;depthDesc.usage=MTLTextureUsageRenderTarget;
        pass.depthAttachment.texture=[dev newTextureWithDescriptor:depthDesc];
        pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.clearDepth=1;
        pass.depthAttachment.storeAction=MTLStoreActionStore;
        desc.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;
        selected=[dev newRenderPipelineStateWithDescriptor:desc error:&error];
        desc.depthAttachmentPixelFormat=MTLPixelFormatInvalid;
        auto ds=[MTLDepthStencilDescriptor new];ds.depthCompareFunction=MTLCompareFunctionLess;ds.depthWriteEnabled=YES;
        depthState=[dev newDepthStencilStateWithDescriptor:ds];
        if(!selected || !depthState) return 10;
      }
      id<MTLParallelRenderCommandEncoder> parent=mode==8?[command parallelRenderCommandEncoderWithDescriptor:pass]:nil;
      auto e=parent?[parent renderCommandEncoder]:[command renderCommandEncoderWithDescriptor:pass];
      [e setRenderPipelineState:selected];
      if(depthState) [e setDepthStencilState:depthState];
      [e setVertexBytes:positions length:sizeof(positions) atIndex:0];
      if(mode==5) [e setViewport:MTLViewport{8,4,32,16,0,1}];
      if(mode==6) [e setScissorRect:MTLScissorRect{2,5,16,10}];
      [e drawPrimitives:mode==9?MTLPrimitiveTypeTriangleStrip:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:mode==9?4:(mode==10?6:3)];
      if(mode==7)
      {
        const Position second[3]={{float(2.*2/width-1),float(1-2.*2/height),.5f,1},
          {float(4.*2/width-1),float(1-2.*2/height),.5f,1},
          {float(2.*2/width-1),float(1-4.*2/height),.5f,1}};
        [e setVertexBytes:second length:sizeof(second) atIndex:0];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
      }
      [e endEncoding];
      if(parent) [parent endEncoding];
      if(api)
      {
        auto drawable=[layer nextDrawable];auto pp=[MTLRenderPassDescriptor renderPassDescriptor];
        pp.colorAttachments[0].texture=drawable.texture;pp.colorAttachments[0].loadAction=MTLLoadActionClear;
        pp.colorAttachments[0].storeAction=MTLStoreActionStore;
        [[command renderCommandEncoderWithDescriptor:pp] endEncoding];[command presentDrawable:drawable];
      }
      [command commit];[command waitUntilCompleted];
      if(api && !api->EndFrameCapture(nullptr,nullptr)) return 9;
      if(command.error) {fprintf(stderr,"GPU: %s\n",command.error.description.UTF8String);return 4;}
      float pixels[width*height*4];[target getBytes:pixels bytesPerRow:width*16 fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
      unsigned coverage=0;float minArea=INFINITY,maxArea=0;
      for(unsigned i=0;i<width*height;i++) if(pixels[i*4+3])
      {
        coverage++;minArea=std::fmin(minArea,pixels[i*4]);maxArea=std::fmax(maxArea,pixels[i*4]);
        double expected=mode==7 && i/width==2 && i%width==2?2:area;
        // Pixel centres on the descending edge are excluded by top-left fill.
        if(mode==10 && i%width+i/width>=23) expected=512;
        if(!std::isfinite(pixels[i*4]) || std::fabs(pixels[i*4]-expected)>expected*.001+1e-4 ||
           pixels[i*4]!=pixels[i*4+1] || pixels[i*4]!=pixels[i*4+2] || pixels[i*4+3]!=1)
        {fprintf(stderr,"Triangle area mismatch case=%u pixel=%u actual=%g expected=%g\n",mode,i,pixels[i*4],area);return 5;}
      }
      if(!coverage) return 6;
      printf("PASS Native projected area mode=%u coverage=%u area=%g min=%g max=%g\n",mode,coverage,area,minArea,maxArea);
    }
  }
  return 0;
}
