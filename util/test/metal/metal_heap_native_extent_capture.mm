// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
// Exercise placements in the Native rounded tail without changing the request.
int main(int argc,char **argv)
{
  if(argc!=4)return 1;
  const bool half=atoi(argv[3])!=0;
  @autoreleasepool
  {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:half?MTLPixelFormatR16Float:MTLPixelFormatR32Float
      width:half?17:3 height:half?5:362 mipmapped:NO];
    td.storageMode=MTLStorageModePrivate;td.hazardTrackingMode=MTLHazardTrackingModeTracked;
    td.usage=MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
    const auto layout=[device heapTextureSizeAndAlignWithDescriptor:td];
    auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.size=layout.align*(half?5:4);
    hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
    id<MTLHeap> heap=[device newHeapWithDescriptor:hd];
    if(!heap||!layout.size||!layout.align||heap.size<=hd.size||layout.size>heap.size-hd.size)return 2;
    id<MTLTexture> texture=[heap newTextureWithDescriptor:td offset:hd.size];
    id<MTLTexture> view=half?[texture newTextureViewWithPixelFormat:td.pixelFormat]:texture;
    const NSUInteger row=td.width*(half?2:4),pitch=256,bytes=pitch*td.height;
    id<MTLBuffer> upload=[device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    id<MTLBuffer> output=[device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    id<MTLCommandQueue> queue=[device newCommandQueue];
    if(!texture||!view||!upload||!output||!queue||texture.heapOffset!=hd.size)return 3;
    output.label=@"Native heap extent output";
    memset(upload.contents,0,bytes);memset(output.contents,0x6a,bytes);
    for(NSUInteger y=0;y<td.height;y++)for(NSUInteger x=0;x<row;x++)
      ((unsigned char *)upload.contents)[y*pitch+x]=(unsigned char)(x*7+y*13+19);
    auto initial=[queue commandBuffer];auto init=[initial blitCommandEncoder];
    [init copyFromBuffer:upload sourceOffset:0 sourceBytesPerRow:pitch sourceBytesPerImage:bytes
      sourceSize:MTLSizeMake(td.width,td.height,1) toTexture:texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
    [init endEncoding];[initial commit];[initial waitUntilCompleted];if(initial.error)return 4;
    RENDERDOC_API_1_7_0 *api=nullptr;
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(get){if(!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 5;
      RENDERDOC_AnnotationValue coverage={};coverage.uint32=65;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage))return 5;
      api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
    auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
    [blit copyFromTexture:view sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
      sourceSize:MTLSizeMake(td.width,td.height,1) toBuffer:output destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:bytes];
    [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 6;
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 7;
    auto data=(unsigned char *)output.contents;
    for(NSUInteger y=0;y<td.height;y++)for(NSUInteger x=0;x<pitch;x++)
      if(data[y*pitch+x]!=(x<row?(unsigned char)(x*7+y*13+19):0x6a))return 8;
    FILE *f=fopen(argv[2],"wbx");if(!f)return 9;bool good=fwrite(data,1,bytes,f)==bytes;fclose(f);
    printf("GPU_COMPLETED; Native output/padding MATCH; requested=%lu actual=%lu offset=%lu allocation=%lu bytes=%lu view=%d\n",hd.size,heap.size,texture.heapOffset,layout.size,bytes,half);
    return good?0:9;
  }
}
