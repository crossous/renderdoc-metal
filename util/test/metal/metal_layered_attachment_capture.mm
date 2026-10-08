// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include "metal_layered_attachment_cases.h"
#include <dlfcn.h>
#include <cstdio>
int main(int argc, char **argv)
{
  if(argc!=3)return 1; const auto *c=FindLayeredAttachmentCase(argv[2]);if(!c)return 1;
  @autoreleasepool {
    id<MTLDevice> d=MTLCreateSystemDefaultDevice();auto q=[d newCommandQueue];
    auto td=[MTLTextureDescriptor new];td.textureType=c->volume?MTLTextureType3D:MTLTextureType2DArray;
    td.pixelFormat=MTLPixelFormat(c->format);td.width=c->width;td.height=c->height;
    td.depth=c->volume?c->depth:1;td.arrayLength=c->volume?1:c->depth;td.mipmapLevelCount=c->mips;
    td.resourceOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
    td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
    const auto layout=[d heapTextureSizeAndAlignWithDescriptor:td];auto hd=[MTLHeapDescriptor new];
    hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
    hd.size=layout.size+layout.align;auto heap=[d newHeapWithDescriptor:hd];if(!heap)return 2;
    id<MTLTexture> image=nil;RENDERDOC_API_1_7_0 *api=nullptr;
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(get)get(eRENDERDOC_API_Version_1_7_0,(void **)&api);
    auto pass=[&](unsigned level,unsigned first,unsigned layers,MTLLoadAction load,double value) {
      auto rp=[MTLRenderPassDescriptor renderPassDescriptor];rp.renderTargetArrayLength=layers;
      auto a=rp.colorAttachments[0];a.texture=image;a.level=level;
      if(c->volume)a.depthPlane=first;else a.slice=first;
      a.loadAction=load;a.storeAction=MTLStoreActionUnknown;a.clearColor=MTLClearColorMake(value,value,value,value);
      auto cb=[q commandBuffer];auto e=[cb renderCommandEncoderWithDescriptor:rp];if(!e)return false;
      [e setColorStoreAction:MTLStoreActionStore atIndex:0];[e endEncoding];[cb commit];[cb waitUntilCompleted];
      return cb.status==MTLCommandBufferStatusCompleted && !cb.error;
    };
    auto seed=[&]() {image=[heap newTextureWithDescriptor:td offset:0];image.label=@"Layered attachment";if(!image)return false;
      for(unsigned mip=0;mip<c->mips;mip++)if(!pass(mip,0,c->volume?MAX(1u,c->depth>>mip):c->depth,MTLLoadActionClear,.25))return false;return true;};
    if(!c->frame && !seed())return 3;
    if(api){RENDERDOC_AnnotationValue v={};v.uint32=65;if(api->SetObjectAnnotation((__bridge void *)d,(__bridge void *)d,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 4;
      api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
    if(c->frame && !seed())return 5;
    if(!pass(c->level,c->firstLayer,c->layers,MTLLoadActionLoad,0))return 6;
    if(!pass(c->level,c->firstLayer,c->layers,MTLLoadActionClear,.75))return 7;
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 8;
    // Compare every defined texel, including unselected mips and planes/layers.
    for(unsigned mip=0;mip<c->mips;mip++){
      unsigned width=MAX(1u,c->width>>mip),height=MAX(1u,c->height>>mip),depth=c->volume?MAX(1u,c->depth>>mip):c->depth;
      for(unsigned layer=0;layer<depth;layer++){
        auto read=[d newBufferWithLength:256*height options:MTLResourceStorageModeShared];auto cb=[q commandBuffer];auto e=[cb blitCommandEncoder];
        [e copyFromTexture:image sourceSlice:c->volume?0:layer sourceLevel:mip sourceOrigin:MTLOriginMake(0,0,c->volume?layer:0)
          sourceSize:MTLSizeMake(width,height,1) toBuffer:read destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:256*height];
        [e endEncoding];[cb commit];[cb waitUntilCompleted];if(cb.status!=MTLCommandBufferStatusCompleted)return 9;
        const bool selected=mip==c->level && layer>=c->firstLayer && layer<c->firstLayer+c->layers;
        for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)for(unsigned channel=0;channel<(c->format==55?1u:4u);channel++){
          const auto p=(const unsigned char *)read.contents+y*256+x*c->stride;
          bool good=c->format==115?((const unsigned short *)p)[channel]==(selected?0x3a00:0x3400):
            c->format==55?*(const float *)p==(selected?.75f:.25f):p[channel]==(selected?191:64);
          if(!good){fprintf(stderr,"Native mismatch mip=%u layer=%u x=%u y=%u selected=%d\n",mip,layer,x,y,selected);return 10;}
        }
      }
    }
    printf("PASS Native %s full defined texels, Load and partial layered Clear, deferred Store\n",c->name);return 0;
  }
}
