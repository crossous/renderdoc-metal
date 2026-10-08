// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

// Independent writable texture fence: original GPU write, ordering, then read.
// Shape and view change without engine identities or PSO declarations.
int main(int argc, char **argv)
{
  if(argc!=4)return 1;
  @autoreleasepool {
    const bool array=!strcmp(argv[3],"array");
    const bool integerBits=getenv("RENDERDOC_METAL_FENCE_INTEGER_BITS")!=nullptr;
    const bool variant=getenv("RENDERDOC_METAL_FENCE_INTEGER_VARIANT")!=nullptr;
    const bool constantTable=getenv("RENDERDOC_METAL_FENCE_CONSTANT_TABLE")!=nullptr;
    const bool constantVariant=getenv("RENDERDOC_METAL_FENCE_CONSTANT_VARIANT")!=nullptr;
    id<MTLDevice> d=MTLCreateSystemDefaultDevice(); NSError *error=nil;
    auto library=[d newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[2]]] error:&error];
    auto pipeline=[d newComputePipelineStateWithFunction:[library newFunctionWithName:array?@"fence_array":@"fence_2d"] error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:array?MTLPixelFormatRGBA32Float:MTLPixelFormatR32Uint width:array?27:17 height:array?19:11 mipmapped:array];
    if(integerBits){td.pixelFormat=variant?MTLPixelFormatRG32Sint:MTLPixelFormatR32Sint;
      if(variant){td.width=23;td.height=13;}}
    td.textureType=array?MTLTextureType2DArray:MTLTextureType2D;
    td.arrayLength=array?3:1;td.mipmapLevelCount=array?3:1;
    td.resourceOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
    td.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite|MTLTextureUsageRenderTarget|MTLTextureUsagePixelFormatView;
    if(getenv("RENDERDOC_METAL_FENCE_UNKNOWN_USAGE"))td.usage=MTLTextureUsageUnknown;
    const auto layout=[d heapTextureSizeAndAlignWithDescriptor:td];
    auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;
    hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=layout.size+layout.align;
    auto heap=[d newHeapWithDescriptor:hd];auto q=[d newCommandQueue];
    auto table=[d newBufferWithLength:24 options:MTLResourceStorageModeShared];
    auto output=[d newBufferWithLength:32 options:MTLResourceStorageModeShared];
    output.label=@"Fence output";
    RENDERDOC_API_1_7_0 *api=nullptr;auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(get)get(eRENDERDOC_API_Version_1_7_0,(void **)&api);
    auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t e){
      if(!api)return uint32_t(0);RENDERDOC_AnnotationValue v={};
      v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=e;
      return api->SetObjectAnnotation((__bridge void *)d,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
    };
    if(api){RENDERDOC_AnnotationValue v={};v.uint32=65;
      if(api->SetObjectAnnotation((__bridge void *)d,(__bridge void *)d,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v)||
         annotation(table,"metal.descriptorTable",1,0,1,24))return 3;
      api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);
    }
    auto parent=[heap newTextureWithDescriptor:td offset:layout.align];parent.label=@"Fence parent";
    auto view=[parent newTextureViewWithPixelFormat:td.pixelFormat textureType:td.textureType
       levels:NSMakeRange(array?1:0,1) slices:NSMakeRange(array?1:0,array?2:1)];
    view.label=@"Fence view";if(!view)return 4;
    const uint64_t packet[]={0,view.gpuResourceID._impl,0};memcpy(table.contents,packet,24);
    if(annotation(table,"metal.descriptorSlotEvent",0,1,0,5)||
       annotation(table,"metal.descriptorSlotEvent",0,1,2,5)||
       annotation(table,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)view,0))return 5;
    for(unsigned stage=0;stage<2;stage++){
      auto cb=[q commandBuffer];
      for(unsigned mip=0;mip<td.mipmapLevelCount;mip++)for(unsigned slice=0;slice<td.arrayLength;slice++){
        auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture=parent;pass.colorAttachments[0].level=mip;pass.colorAttachments[0].slice=slice;
        pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        pass.colorAttachments[0].clearColor=MTLClearColorMake(7,7,7,7);
        [[cb renderCommandEncoderWithDescriptor:pass] endEncoding];
      }
      // The second shape also verifies queue submission inheritance.
      if(array){[cb commit];cb=[q commandBuffer];}
      auto compute=[cb computeCommandEncoder];[compute setComputePipelineState:pipeline];
      if(annotation(compute,"metal.descriptorInlineLayout",0,0,1,16)||
         annotation(compute,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)table,0))return 6;
      const uint64_t root[]={table.gpuAddress,stage};[compute setBytes:root length:16 atIndex:0];
      [compute setBuffer:output offset:0 atIndex:1];[compute useResource:table usage:MTLResourceUsageRead];
      [compute useResource:view usage:MTLResourceUsageRead|MTLResourceUsageWrite];
      [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(4,1,1)];
      [compute endEncoding];[cb commit];[cb waitUntilCompleted];
      if(cb.status!=MTLCommandBufferStatusCompleted)return 7;
      for(unsigned i=0;i<4;i++){
        uint32_t expected=stage*100+i+31+(integerBits?0x80000000u:0u);
        const uint32_t key[]={11,13,17,19,23,29},blind[]={41,43,47,53,59,61,67,71,73,79,83};
        if(constantTable)expected+=(constantVariant?blind:key)[(expected^(stage*13))%(constantVariant?11:6)];
        if(((uint32_t *)output.contents)[stage*4+i]!=expected)return 8;
      }
    }
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 9;
    puts("PASS Native texture fence two stages, dynamic slice, eight GPU output words");
  }
  return 0;
}
