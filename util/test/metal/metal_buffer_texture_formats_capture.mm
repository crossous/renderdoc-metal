// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <dlfcn.h>
#include "metal_texture_initial_spec.h"
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();id<MTLCommandQueue> queue=[device newCommandQueue];NSError *error=nil;
    id<MTLLibrary> library=[device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void buffer_image(texture_buffer<uint,access::read> image [[texture(0)]],device uint *out [[buffer(0)]])
{out[0]=image.read(uint(0)).r;out[1]=0xdeadbeef;}
kernel void buffer_image16(texture_buffer<uint,access::read> image [[texture(0)]],device uint4 *out [[buffer(0)]],constant uint &width [[buffer(1)]],uint index [[thread_position_in_grid]])
{uint positions[3]={0,width/2,width-1};out[index]=image.read(positions[index]);}
kernel void buffer_write16(texture_buffer<uint,access::write> image [[texture(0)]],constant uint &width [[buffer(1)]],uint index [[thread_position_in_grid]])
{uint positions[3]={0,width/2,width-1};image.write(uint4(65535,60000,50000,40000)-index,positions[index]);}
kernel void native_float(texture_buffer<float,access::read> image [[texture(0)]],device float4 *out [[buffer(0)]]) {out[0]=image.read(uint(0));}
kernel void native_uint(texture_buffer<uint,access::read> image [[texture(0)]],device float4 *out [[buffer(0)]]) {out[0]=float4(image.read(uint(0)));}
kernel void native_sint(texture_buffer<int,access::read> image [[texture(0)]],device float4 *out [[buffer(0)]]) {out[0]=float4(image.read(uint(0)));}
)MSL" options:nil error:&error];
    if(!library){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"buffer_image"] error:&error];
    id<MTLComputePipelineState> nativeFloat=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"native_float"] error:&error];
    id<MTLComputePipelineState> nativeUInt=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"native_uint"] error:&error];
    id<MTLComputePipelineState> nativeSInt=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"native_sint"] error:&error];
    id<MTLComputePipelineState> read16=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"buffer_image16"] error:&error];
    id<MTLComputePipelineState> write16=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"buffer_write16"] error:&error];
    if(!pipeline||!nativeFloat||!nativeUInt||!nativeSInt||!read16||!write16)return 3;
    auto specs=BufferTextureInitialSpecs();
    const bool wide=getenv("RENDERDOC_METAL_UINT16_WIDE")!=nullptr;
    const bool readWrite=getenv("RENDERDOC_METAL_UINT16_RW")!=nullptr;
    const bool zeroOffset=getenv("RENDERDOC_METAL_UINT16_ZERO_OFFSET")!=nullptr;
    if(wide)specs.back().width=8192;NSMutableArray<id<MTLTexture>> *views=[NSMutableArray new];
    NSMutableArray<id<MTLBuffer>> *parents=[NSMutableArray new];std::vector<NSUInteger> offsets;
    id<MTLBuffer> staging=[device newBufferWithLength:wide?131072:4096 options:MTLResourceStorageModeShared];
    auto reset=[&]() {
      for(size_t i=0;i<specs.size();i++)
      {
        const auto &s=specs[i];id<MTLBuffer> buffer=parents[i];memset(staging.contents,0xcc,buffer.length);
        for(unsigned x=0;x<s.width*s.bytes;x++)((uint8_t *)staging.contents)[offsets[i]+x]=InitialTextureByte(s,0,0,0,0,x);
        id<MTLCommandBuffer> command=[queue commandBuffer];id<MTLBlitCommandEncoder> blit=[command blitCommandEncoder];
        [blit copyFromBuffer:staging sourceOffset:0 toBuffer:buffer destinationOffset:0 size:buffer.length];
        [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.status!=MTLCommandBufferStatusCompleted)return false;
      }
      return true;
    };
    for(const auto &s:specs)
    {
      MTLTextureDescriptor *desc=[MTLTextureDescriptor new];desc.textureType=MTLTextureTypeTextureBuffer;
      desc.pixelFormat=(MTLPixelFormat)s.format;desc.width=s.width;desc.height=1;desc.depth=1;desc.mipmapLevelCount=1;
      desc.arrayLength=1;desc.sampleCount=1;desc.resourceOptions=MTLResourceStorageModePrivate;
      desc.allowGPUOptimizedContents=NO;desc.usage=MTLTextureUsageShaderRead;
      const NSUInteger alignment=std::max([device minimumTextureBufferAlignmentForPixelFormat:desc.pixelFormat],
                                          [device minimumLinearTextureAlignmentForPixelFormat:desc.pixelFormat]);
      if(!alignment)return 4;const NSUInteger row=(s.width*s.bytes+alignment-1)/alignment*alignment;
      if(s.format==113&&readWrite)desc.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
      const NSUInteger offset=s.format==113&&zeroOffset?0:alignment*2,total=offset+row+alignment;
      if(total>staging.length)return 5;
      id<MTLBuffer> parent=[device newBufferWithLength:total options:MTLResourceStorageModePrivate];
      id<MTLTexture> view=[parent newTextureWithDescriptor:desc offset:offset bytesPerRow:row];
      if(!parent||!view){fprintf(stderr,"Native view creation failed format=%u\n",s.format);return 6;}
      [parents addObject:parent];[views addObject:view];offsets.push_back(offset);
    }
    if(!reset())return 7;
    id<MTLBuffer> nativePixels=[device newBufferWithLength:specs.size()*16 options:MTLResourceStorageModeShared];
    auto verifyNative=[&]() {
      id<MTLCommandBuffer> command=[queue commandBuffer];
      for(size_t i=0;i<specs.size();i++)
      {
        const auto &s=specs[i];const bool integer=s.format==53||s.format==123||s.format==103||s.format==73||s.format==23||s.format==63||s.format==113;
        id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
        [encoder setComputePipelineState:s.format==54?nativeSInt:integer?nativeUInt:nativeFloat];
        [encoder setTexture:views[i] atIndex:0];[encoder setBuffer:nativePixels offset:i*16 atIndex:0];
        [encoder dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[encoder endEncoding];
      }
      [command commit];[command waitUntilCompleted];if(command.status!=MTLCommandBufferStatusCompleted)return false;
      for(size_t i=0;i<specs.size();i++)for(unsigned c=0;c<specs[i].components;c++)
      {
        const auto &s=specs[i];const unsigned raw=s.format==113?32768+8191*c:17+23*c;
        const bool integer=s.format==53||s.format==123||s.format==103||s.format==73||s.format==23||s.format==63||s.format==113;
        const float expected=s.format==54?-17.0f:integer?float(raw):s.format==70&&c==3?1.0f:
            s.format==70||s.format==10?raw/255.0f:s.format==72?raw/127.0f:0.25f+0.125f*c;
        const float actual=((const float *)nativePixels.contents)[i*4+c];
        if(fabs(actual-expected)>0.00001f){fprintf(stderr,"Native decode mismatch format=%u component=%u actual=%g expected=%g\n",s.format,c,actual,expected);return false;}
      }
      return true;
    };
    if(!verifyNative())return 12;
    id<MTLBuffer> output=[device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    id<MTLBuffer> output16=[device newBufferWithLength:48 options:MTLResourceStorageModeShared];output16.label=@"TextureBuffer uint16 read";
    id<MTLBuffer> after16=[device newBufferWithLength:48 options:MTLResourceStorageModeShared];after16.label=@"TextureBuffer uint16 after write";
    const uint32_t overwrite=0x01020304;id<MTLBuffer> overwriteSource=[device newBufferWithBytes:&overwrite length:4 options:MTLResourceStorageModeShared];
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 8;api->SetCaptureFilePathTemplate(path);}
    for(unsigned capture=0;capture<(api?2u:1u);capture++)
    {
      if(capture&&(!reset()||!verifyNative()))return 9;memset(output.contents,0,8);memset(output16.contents,0,48);memset(after16.contents,0,48);if(api)api->StartFrameCapture(nullptr,nullptr);
      id<MTLCommandBuffer> command=[queue commandBuffer];id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];
      [compute setComputePipelineState:pipeline];[compute setTexture:views[0] atIndex:0];[compute setBuffer:output offset:0 atIndex:0];
      for(id<MTLTexture> view:views)[compute useResource:view usage:MTLResourceUsageRead];
      [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];
      const uint32_t width=specs.back().width;
      auto consume16=[&](id<MTLComputePipelineState> pso,id<MTLBuffer> destination) {
        id<MTLComputeCommandEncoder> e=[command computeCommandEncoder];[e setComputePipelineState:pso];
        [e setTexture:views.lastObject atIndex:0];[e setBytes:&width length:4 atIndex:1];
        if(destination)[e setBuffer:destination offset:0 atIndex:0];
        [e dispatchThreads:MTLSizeMake(3,1,1) threadsPerThreadgroup:MTLSizeMake(3,1,1)];[e endEncoding];
      };
      consume16(read16,output16);
      if(readWrite){consume16(write16,nil);consume16(read16,after16);}
      id<MTLBlitCommandEncoder> blit=[command blitCommandEncoder];
      [blit copyFromBuffer:overwriteSource sourceOffset:0 toBuffer:parents[0] destinationOffset:offsets[0] size:4];[blit endEncoding];
      id<CAMetalDrawable> drawable=[layer nextDrawable];MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
      pass.colorAttachments[0].clearColor=MTLClearColorMake(0.125,0.25,0.5,1);[[command renderCommandEncoderWithDescriptor:pass] endEncoding];
      [command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
      const uint32_t *words=(const uint32_t *)output.contents;if(command.status!=MTLCommandBufferStatusCompleted||words[0]!=17||words[1]!=0xdeadbeef)return 10;
      const uint32_t positions[3]={0,width/2,width-1},written[4]={65535,60000,50000,40000};
      for(unsigned i=0;i<3;i++)for(unsigned c=0;c<4;c++) {
        if(((const uint32_t *)output16.contents)[i*4+c]!=32768+8191*c+(positions[i]%101))return 13;
        if(readWrite&&((const uint32_t *)after16.contents)[i*4+c]!=written[c]-i)return 14;
      }
      if(api&&!api->EndFrameCapture(nullptr,nullptr))return 11;
    }
    printf("PASS Native TextureBuffer17 GPU=17/DEADBEEF uint16 width=%u RW=%u zeroOffset=%u high-bit RGBA first/middle/last verified captures=%u\n",specs.back().width,readWrite,zeroOffset,api?2:0);
  }
  return 0;
}
