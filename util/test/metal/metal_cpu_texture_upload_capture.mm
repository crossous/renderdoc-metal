// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice(); NSError *error=nil;
    id<MTLLibrary> library=[device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void upload_probe(texture3d<float,access::read> volume [[texture(0)]],
                         texture2d<float,access::read> bc [[texture(1)]], device uint *out [[buffer(0)]])
{
  out[0]=uint(volume.read(uint3(0,0,0)).r*255.0+0.5)+uint(volume.read(uint3(0,0,1)).r*255.0+0.5)+uint(bc.read(uint2(0,0)).r*255.0+0.5);
  out[1]=0xdeadbeefU;
}
)MSL" options:nil error:&error];
    if(!library){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"upload_probe"] error:&error];
    MTLTextureDescriptor *v=[MTLTextureDescriptor new];v.textureType=MTLTextureType3D;
    v.pixelFormat=MTLPixelFormatRGBA8Unorm;v.width=1;v.height=1;v.depth=2;
    v.storageMode=MTLStorageModeShared;v.usage=MTLTextureUsageShaderRead;
    id<MTLTexture> volume=[device newTextureWithDescriptor:v];
    const uint8_t pixels[20]={51,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,101,0,0,255};
    [volume replaceRegion:MTLRegionMake3D(0,0,0,1,1,2) mipmapLevel:0 slice:0 withBytes:pixels bytesPerRow:8 bytesPerImage:16];
    auto b=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBC1_RGBA width:4 height:4 mipmapped:NO];
    b.storageMode=MTLStorageModeShared;b.usage=MTLTextureUsageShaderRead;
    id<MTLTexture> bc=[device newTextureWithDescriptor:b];const uint8_t block[8]={0,248,0,0,0,0,0,0};
    [bc replaceRegion:MTLRegionMake2D(0,0,4,4) mipmapLevel:0 withBytes:block bytesPerRow:8];
    id<MTLBuffer> output=[device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    if(!pipeline||!volume||!bc||!output)return 3;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 4;api->SetCaptureFilePathTemplate(path);api->StartFrameCapture(nullptr,nullptr);}
    id<MTLCommandQueue> queue=[device newCommandQueue];id<MTLCommandBuffer> command=[queue commandBuffer];
    id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];
    [compute setTexture:volume atIndex:0];[compute setTexture:bc atIndex:1];[compute setBuffer:output offset:0 atIndex:0];
    [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    id<CAMetalDrawable> drawable=[layer nextDrawable];MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    [[command renderCommandEncoderWithDescriptor:pass] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
    const uint32_t *words=(const uint32_t *)output.contents;
    if(command.error||words[0]!=407||words[1]!=0xdeadbeefU)return 5;
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 6;
    puts("PASS native padded 3D two images + BC1 block rows, GPU=407/DEADBEEF");
  }
  return 0;
}
