// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv)
{
  if(argc!=3)return 1;
  @autoreleasepool {
    const bool blind=getenv("RENDERDOC_METAL_DEPTH_INDEPENDENT")!=nullptr;
    const unsigned width=blind?19:12,height=blind?13:12,mip=blind?2:1,faces=blind?1:6;
    const unsigned tableOffset=blind?48:24,samplerOffset=blind?24:0,outputOffset=blind?96:32;
    auto device=MTLCreateSystemDefaultDevice();auto queue=[device newCommandQueue];NSError *error=nil;
    auto library=[device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[2]]] error:&error];
    auto pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"depth_compare"] error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:blind?MTLPixelFormatDepth16Unorm:MTLPixelFormatDepth32Float width:width height:height mipmapped:YES];
    td.textureType=blind?MTLTextureType2D:MTLTextureTypeCube;td.mipmapLevelCount=blind?3:2;
    td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget|MTLTextureUsagePixelFormatView;
    auto parent=[device newTextureWithDescriptor:td];parent.label=@"Compare depth parent";
    auto view=[parent newTextureViewWithPixelFormat:td.pixelFormat textureType:td.textureType levels:NSMakeRange(mip,1) slices:NSMakeRange(0,faces)];view.label=@"Compare depth view";
    if(!view)return 3;
    auto sd=[MTLSamplerDescriptor new];sd.supportArgumentBuffers=YES;sd.compareFunction=MTLCompareFunctionLess;
    sd.minFilter=MTLSamplerMinMagFilterNearest;sd.magFilter=MTLSamplerMinMagFilterNearest;
    sd.label=@"Compare sampler";auto sampler=[device newSamplerStateWithDescriptor:sd];
    const uint64_t packet[]={0,view.gpuResourceID._impl,0},samplerPacket[]={sampler.gpuResourceID._impl,0,0};
    auto table=[device newBufferWithLength:tableOffset+24 options:MTLResourceStorageModeShared];table.label=@"Compare texture table";
    memcpy((char *)table.contents+tableOffset,packet,24);
    auto sampling=[device newBufferWithLength:samplerOffset+24 options:MTLResourceStorageModeShared];sampling.label=@"Compare sampler table";
    memcpy((char *)sampling.contents+samplerOffset,samplerPacket,24);
    auto output=[device newBufferWithLength:outputOffset+32 options:MTLResourceStorageModeShared];output.label=@"Compare output";
    auto clear=[&](id<MTLCommandBuffer> cb,unsigned level,double value) {
      for(unsigned face=0;face<faces;face++){
        auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.depthAttachment.texture=parent;
        pass.depthAttachment.level=level;pass.depthAttachment.slice=face;pass.depthAttachment.clearDepth=value;
        pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=MTLStoreActionStore;
        [[cb renderCommandEncoderWithDescriptor:pass] endEncoding];
      }
    };
    // Save actual Native depth encoding for each defined state before capture.
    // UNorm clear quantization is a device operation, not a CPU rounding oracle.
    const unsigned stride=blind?2:4,vw=unsigned(view.width),vh=unsigned(view.height),pitch=256;
    auto readback=[device newBufferWithLength:pitch*vh options:MTLResourceStorageModeShared];
    std::vector<unsigned char> baseline;
    auto pixels=[&](unsigned face) {
      auto cb=[queue commandBuffer];auto blit=[cb blitCommandEncoder];
      [blit copyFromTexture:view sourceSlice:face sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(vw,vh,1)
          toBuffer:readback destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:pitch*vh];
      [blit endEncoding];[cb commit];[cb waitUntilCompleted];
      return cb.status==MTLCommandBufferStatusCompleted;
    };
    for(double value:{.125,.5,.875}) {
      auto cb=[queue commandBuffer];clear(cb,mip,value);[cb commit];[cb waitUntilCompleted];
      if(cb.status!=MTLCommandBufferStatusCompleted)return 11;
      for(unsigned face=0;face<faces;face++) {
        if(!pixels(face))return 12;
        for(unsigned y=0;y<vh;y++) {
          auto row=(unsigned char *)readback.contents+y*pitch;
          baseline.insert(baseline.end(),row,row+vw*stride);
        }
      }
    }
    FILE *nativePixels=fopen((std::string(argv[1])+".native-depth.bin").c_str(),"wb");
    if(!nativePixels)return 13;
    const bool saved=fwrite(baseline.data(),1,baseline.size(),nativePixels)==baseline.size();fclose(nativePixels);
    if(!saved)return 14;
    auto warm=[queue commandBuffer];for(unsigned level=0;level<td.mipmapLevelCount;level++)clear(warm,level,.125);
    [warm commit];[warm waitUntilCompleted];if(warm.status!=MTLCommandBufferStatusCompleted)return 4;
    RENDERDOC_API_1_7_0 *api=nullptr;auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(get)get(eRENDERDOC_API_Version_1_7_0,(void **)&api);
    auto ann=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t e){
      if(!api)return 0U;RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=e;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
    };
    if(api){RENDERDOC_AnnotationValue v={};v.uint32=65;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v) ||
         ann(table,"metal.descriptorTable",1,tableOffset,1,24) || ann(sampling,"metal.descriptorTable",2,samplerOffset,1,24) ||
         ann(table,"metal.descriptorSlotEvent",tableOffset,1,0,4) || ann(table,"metal.descriptorSlotEvent",tableOffset,1,2,4) ||
         ann(table,"metal.descriptorSlotBinding",tableOffset,1,(uint64_t)(__bridge void *)view,0) ||
         ann(sampling,"metal.descriptorSlotEvent",samplerOffset,1,0,7) || ann(sampling,"metal.descriptorSlotEvent",samplerOffset,1,2,7) ||
         ann(sampling,"metal.descriptorSlotBinding",samplerOffset,2,(uint64_t)(__bridge void *)sampler,0))return 5;
      api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);
    }
    for(unsigned stage=0;stage<2;stage++){
      auto cb=[queue commandBuffer];clear(cb,mip,stage?.875:.5);
      if(blind){[cb commit];cb=[queue commandBuffer];}
      auto compute=[cb computeCommandEncoder];[compute setComputePipelineState:pipeline];
      [compute setBuffer:table offset:tableOffset atIndex:0];[compute setBuffer:sampling offset:samplerOffset atIndex:blind?6:3];
      [compute setBuffer:output offset:outputOffset atIndex:blind?9:7];[compute setBytes:&stage length:4 atIndex:2];
      [compute useResource:view usage:MTLResourceUsageRead];
      [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(4,1,1)];[compute endEncoding];
      [cb commit];[cb waitUntilCompleted];if(cb.status!=MTLCommandBufferStatusCompleted)return 6;
      for(unsigned i=0;i<4;i++)if(((uint32_t *)((char *)output.contents+outputOffset))[stage*4+i]!=(stage || !(i%2))+stage*10+i*2)return 7;
    }
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 8;
    // Compare captured execution against the independent Native clear results.
    for(unsigned face=0;face<faces;face++){
      if(!pixels(face))return 9;
      for(unsigned y=0;y<vh;y++)
        if(memcmp((char *)readback.contents+y*pitch,baseline.data()+((2*faces+face)*vh+y)*vw*stride,vw*stride))return 10;
    }
    printf("PASS Native depth comparison: two dispatches, all eight output words, %u view depth pixels\n",vw*vh*faces);
  }
}
