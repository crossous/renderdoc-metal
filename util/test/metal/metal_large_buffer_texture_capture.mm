// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool {
    const NSUInteger width=getenv("RENDERDOC_METAL_LARGE_TEXELS")?strtoull(getenv("RENDERDOC_METAL_LARGE_TEXELS"),nullptr,10):4587520;
    if(width<3 || width>4587520)return 2;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    auto library=[device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void sample_points(texture_buffer<uint,access::read> image [[texture(0)]],device uint *output [[buffer(0)]],constant uint &width [[buffer(1)]],uint index [[thread_position_in_grid]]) {
 uint positions[3]={0,width/2,width-1};output[index]=image.read(positions[index]).r;
}
)MSL" options:nil error:&error];
    auto pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"sample_points"] error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 3;}
    auto upload=[device newBufferWithLength:width*4 options:MTLResourceStorageModeShared];
    memset(upload.contents,0xa5,upload.length);uint32_t *values=(uint32_t *)upload.contents;
    values[0]=0x12345678;values[width/2]=0x31415926;values[width-1]=0xabcdef01;
    auto parent=[device newBufferWithLength:upload.length options:MTLResourceStorageModePrivate];
    auto output=[device newBufferWithLength:12 options:MTLResourceStorageModeShared];
    auto desc=[MTLTextureDescriptor new];desc.textureType=MTLTextureTypeTextureBuffer;desc.pixelFormat=MTLPixelFormatR32Uint;
    desc.width=width;desc.height=1;desc.depth=1;desc.mipmapLevelCount=1;desc.sampleCount=1;desc.arrayLength=1;
    desc.resourceOptions=MTLResourceStorageModePrivate;desc.allowGPUOptimizedContents=NO;
    desc.usage=getenv("RENDERDOC_METAL_LARGE_READ_WRITE")?(MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite):MTLTextureUsageShaderRead;
    auto image=[parent newTextureWithDescriptor:desc offset:0 bytesPerRow:upload.length];if(!image)return 4;
    auto queue=[device newCommandQueue];auto init=[queue commandBuffer];auto copy=[init blitCommandEncoder];
    [copy copyFromBuffer:upload sourceOffset:0 toBuffer:parent destinationOffset:0 size:upload.length];[copy endEncoding];
    [init commit];[init waitUntilCompleted];if(init.error)return 5;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH")) {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 6;
      api->SetCaptureFilePathTemplate(path);
    }
    auto layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.drawableSize=CGSizeMake(2,2);
    if(api)api->StartFrameCapture(nullptr,nullptr);
    auto command=[queue commandBuffer];auto compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];
    const uint32_t count=uint32_t(width);[compute setBytes:&count length:4 atIndex:1];[compute setTexture:image atIndex:0];
    [compute setBuffer:output offset:0 atIndex:0];[compute dispatchThreads:MTLSizeMake(3,1,1) threadsPerThreadgroup:MTLSizeMake(3,1,1)];[compute endEncoding];
    auto drawable=[layer nextDrawable];if(!drawable)return 7;auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    [[command renderCommandEncoderWithDescriptor:pass] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
    auto result=(uint32_t *)output.contents;
    if(command.error||result[0]!=0x12345678||result[1]!=0x31415926||result[2]!=0xabcdef01)return 8;
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 9;
    printf("PASS Native large buffer texture: width=%lu bytes=%lu usage=%lu; only3 GPU invocations, first/middle/last correct\n",(unsigned long)width,(unsigned long)upload.length,(unsigned long)desc.usage);
  }
}
