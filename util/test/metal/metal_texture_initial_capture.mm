// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include "metal_texture_initial_spec.h"
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    const bool sourced=getenv("RENDERDOC_METAL_SOURCED_TEXTURE_INITIAL")!=nullptr;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();id<MTLCommandQueue> queue=[device newCommandQueue];
    NSError *error=nil;
    NSString *shaderSource=@R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void initial_images(texture2d<float,access::read> image [[texture(0)]],
    texture2d_array<float,access::read> array [[texture(1)]],texturecube<float> cube [[texture(2)]],
    texture3d<float,access::read> volume [[texture(3)]],device uint *out [[buffer(0)]])
{
  constexpr sampler point(coord::normalized,address::clamp_to_edge,filter::nearest);
  out[0]=uint(image.read(uint2(0)).r*255.0+0.5)+uint(array.read(uint2(0),1).r*255.0+0.5)+
         uint(cube.sample(point,float3(0,-1,0)).r*255.0+0.5)+uint(volume.read(uint3(0,0,1)).r*255.0+0.5);
  out[1]=0xdeadbeef;
})MSL";
    if(sourced)shaderSource=@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Image { ulong z [[id(0)]]; texture2d<float,access::read> tex [[id(1)]]; ulong metadata [[id(2)]]; };
struct Array { ulong z [[id(0)]]; texture2d_array<float,access::read> tex [[id(1)]]; ulong metadata [[id(2)]]; };
struct Cube { ulong z [[id(0)]]; texturecube<float> tex [[id(1)]]; ulong metadata [[id(2)]]; };
struct Cubes { ulong z [[id(0)]]; texturecube_array<float> tex [[id(1)]]; ulong metadata [[id(2)]]; };
struct Volume { ulong z [[id(0)]]; texture3d<float,access::read> tex [[id(1)]]; ulong metadata [[id(2)]]; };
struct Depth { ulong z [[id(0)]]; depth2d<float> tex [[id(1)]]; ulong metadata [[id(2)]]; };
struct Stencil { ulong z [[id(0)]]; texture2d<uint,access::read> tex [[id(1)]]; ulong metadata [[id(2)]]; };
kernel void initial_images(device uint *out [[buffer(0)]],const device ulong *root [[buffer(1)]])
{
  const device ulong *table=reinterpret_cast<const device ulong *>(root[0]);
  const device Image *image=reinterpret_cast<const device Image *>(table);
  const device Array *array=reinterpret_cast<const device Array *>(table+3);
  const device Cube *cube=reinterpret_cast<const device Cube *>(table+6);
  const device Cubes *cubes=reinterpret_cast<const device Cubes *>(table+9);
  const device Volume *volume=reinterpret_cast<const device Volume *>(table+12);
  const device Depth *depth=reinterpret_cast<const device Depth *>(table+3*156);
  const device Stencil *stencil=reinterpret_cast<const device Stencil *>(table+3*158);
  constexpr sampler point(coord::normalized,address::clamp_to_edge,filter::nearest);
  out[0]=uint(image->tex.read(uint2(0)).r*255.0+0.5)+uint(array->tex.read(uint2(0),1).r*255.0+0.5)+
      uint(cube->tex.sample(point,float3(0,-1,0)).r*255.0+0.5)+uint(volume->tex.read(uint3(0,0,1)).r*255.0+0.5)+
      uint(cubes->tex.sample(point,float3(1,0,0),1).r*255.0+0.5)+uint(depth->tex.sample(point,float2(0))*256.0)+
      stencil->tex.read(uint2(0)).r;
  out[1]=image->metadata==((70ul<<32)|2ul)&&stencil->metadata==((261ul<<32)|2ul)?0xdeadbeef:0xbad;
})MSL";
    id<MTLLibrary> library=[device newLibraryWithSource:shaderSource options:nil error:&error];
    if(!library){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"initial_images"] error:&error];
    const bool shared=getenv("RENDERDOC_METAL_SHARED_TEXTURE_INITIAL")!=nullptr;
    if(sourced&&shared)return 17;
    const bool placement=getenv("RENDERDOC_METAL_TEXTURE_INITIAL_PLACEMENT")!=nullptr;
    const bool uint16=getenv("RENDERDOC_METAL_UINT16_TEXTURE_INITIAL")!=nullptr;
    if(uint16&&(sourced||shared))return 18;
    const auto specs=InitialTextureSpecs(shared,uint16);NSMutableArray<id<MTLTexture>> *textures=[NSMutableArray new];
    NSMutableArray<MTLTextureDescriptor *> *descriptors=[NSMutableArray new];
    std::vector<NSUInteger> offsets;NSUInteger heapSize=0;
    for(const auto &s:specs)
    {
      MTLTextureDescriptor *desc=[MTLTextureDescriptor new];desc.pixelFormat=(MTLPixelFormat)s.format;
      desc.textureType=(MTLTextureType)s.type;desc.width=s.width;desc.height=s.height;desc.depth=s.depth;
      desc.arrayLength=s.arrays;desc.mipmapLevelCount=s.mips;desc.storageMode=shared?MTLStorageModeShared:MTLStorageModePrivate;
      desc.usage=MTLTextureUsageShaderRead | ((s.format==70&&s.type==2)||s.format>=250?MTLTextureUsageRenderTarget:0);
      if(sourced&&s.format==260)desc.usage|=MTLTextureUsagePixelFormatView;
      [descriptors addObject:desc];
      if(placement)
      {
        MTLSizeAndAlign layout=[device heapTextureSizeAndAlignWithDescriptor:desc];
        if(!layout.size||!layout.align)return 8;
        heapSize=(heapSize+layout.align-1)/layout.align*layout.align;offsets.push_back(heapSize);heapSize+=layout.size;
      }
    }
    id<MTLHeap> heap=nil;
    if(placement)
    {
      if(heapSize>64*1024*1024)return 9;
      MTLHeapDescriptor *desc=[MTLHeapDescriptor new];desc.type=MTLHeapTypePlacement;
      desc.storageMode=shared?MTLStorageModeShared:MTLStorageModePrivate;desc.hazardTrackingMode=MTLHazardTrackingModeTracked;
      desc.size=std::max(NSUInteger(4096),(heapSize+4095)/4096*4096);heap=[device newHeapWithDescriptor:desc];
      if(!heap)return 10;
    }
    id<MTLBuffer> staging=[device newBufferWithLength:4096 options:MTLResourceStorageModeShared];
    auto upload=[&](id<MTLTexture> texture,const InitialTextureSpec &s) {
      for(unsigned slice=0;slice<InitialTextureSlices(s);slice++)for(unsigned mip=0;mip<s.mips;mip++)
      {
        if(s.format==250||s.format==252||s.format==260)
        {
          id<MTLCommandBuffer> command=[queue commandBuffer];
          MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
          pass.depthAttachment.texture=texture;pass.depthAttachment.level=mip;pass.depthAttachment.slice=slice;
          pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=MTLStoreActionStore;
          pass.depthAttachment.clearDepth=0.25+0.125*mip+0.0625*slice;
          if(s.format==260)
          {
            pass.stencilAttachment.texture=texture;pass.stencilAttachment.level=mip;pass.stencilAttachment.slice=slice;
            pass.stencilAttachment.loadAction=MTLLoadActionClear;pass.stencilAttachment.storeAction=MTLStoreActionStore;
            pass.stencilAttachment.clearStencil=17+mip+slice;
          }
          [[command renderCommandEncoderWithDescriptor:pass] endEncoding];[command commit];[command waitUntilCompleted];
          if(command.status!=MTLCommandBufferStatusCompleted)return false;
          // Establish the native clear representation independently of RenderDoc capture/replay.
          id<MTLCommandBuffer> verify=[queue commandBuffer];id<MTLBlitCommandEncoder> blit=[verify blitCommandEncoder];
          unsigned w=std::max(1u,s.width>>mip),h=std::max(1u,s.height>>mip);
          [blit copyFromTexture:texture sourceSlice:slice sourceLevel:mip sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(w,h,1) toBuffer:staging destinationOffset:0 destinationBytesPerRow:256
            destinationBytesPerImage:256*h options:s.format==260?MTLBlitOptionDepthFromDepthStencil:MTLBlitOptionNone];
          if(s.format==260)[blit copyFromTexture:texture sourceSlice:slice sourceLevel:mip sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(w,h,1) toBuffer:staging destinationOffset:2048 destinationBytesPerRow:256
            destinationBytesPerImage:256*h options:MTLBlitOptionStencilFromDepthStencil];
          [blit endEncoding];[verify commit];[verify waitUntilCompleted];
          if(verify.status!=MTLCommandBufferStatusCompleted)return false;
          unsigned pixelBytes=s.format==250?2:4;
          for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)for(unsigned c=0;c<pixelBytes;c++)
            if(((uint8_t *)staging.contents)[y*256+x*pixelBytes+c]!=InitialTextureByte(s,mip,slice,0,y,x*s.bytes+c))return false;
          if(s.format==260)for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)
            if(((uint8_t *)staging.contents)[2048+y*256+x]!=17+mip+slice)return false;
          continue;
        }
        unsigned w=std::max(1u,s.width>>mip),h=std::max(1u,s.height>>mip),d=std::max(1u,s.depth>>mip);
        unsigned rows=(h+s.block-1)/s.block,rowbytes=((w+s.block-1)/s.block)*s.bytes;
        memset(staging.contents,0xcc,4096);
        for(unsigned z=0;z<d;z++)for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<rowbytes;x++)
          ((uint8_t *)staging.contents)[(z*rows+y)*256+x]=InitialTextureByte(s,mip,slice,z,y,x);
        id<MTLCommandBuffer> command=[queue commandBuffer];id<MTLBlitCommandEncoder> blit=[command blitCommandEncoder];
        [blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:256 sourceBytesPerImage:256*rows
          sourceSize:MTLSizeMake(w,h,d) toTexture:texture destinationSlice:slice destinationLevel:mip destinationOrigin:MTLOriginMake(0,0,0)];
        [blit endEncoding];[command commit];[command waitUntilCompleted];
        if(command.status!=MTLCommandBufferStatusCompleted)return false;
      }
      return true;
    };
    for(size_t i=0;i<specs.size();i++)
    {
      const auto &s=specs[i];MTLTextureDescriptor *desc=descriptors[i];
      id<MTLTexture> texture=placement?[heap newTextureWithDescriptor:desc offset:offsets[i]]:[device newTextureWithDescriptor:desc];
      if(!texture||!upload(texture,s))return 3;
      [texture setLabel:[NSString stringWithFormat:@"initial-%u-%u",s.format,s.type]];
      [textures addObject:texture];
    }
    NSMutableArray<id<MTLTexture>> *sources=[textures mutableCopy];
    id<MTLTexture> stencilView=nil;
    id<MTLBuffer> table=nil;
    if(sourced)
    {
      if(specs.size()!=158||specs[156].format!=260||specs[156].type!=2)return 18;
      stencilView=[textures[156] newTextureViewWithPixelFormat:MTLPixelFormatX32_Stencil8
          textureType:MTLTextureType2D levels:NSMakeRange(0,3) slices:NSMakeRange(0,1)];
      if(!stencilView)return 19;[sources addObject:stencilView];
      table=[device newBufferWithLength:sources.count*24 options:MTLResourceStorageModeShared];
      if(!table)return 20;
      uint64_t *packet=(uint64_t *)table.contents;
      for(size_t i=0;i<sources.count;i++)
      {
        packet[i*3]=0;packet[i*3+1]=sources[i].gpuResourceID._impl;
        packet[i*3+2]=i<specs.size()?(uint64_t(specs[i].format)<<32)|specs[i].type:(261ULL<<32)|2;
      }
    }
    id<MTLBuffer> output=[device newBufferWithLength:8 options:MTLResourceStorageModeShared];memset(output.contents,0,8);
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 4;
      api->SetCaptureFilePathTemplate(path);
    }
    auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api)return uint32_t(0);
      RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
    };
    if(sourced&&api)
    {
      RENDERDOC_AnnotationValue coverage={};coverage.uint32=36;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage)||
         annotation(table,"metal.descriptorTable",1,0,sources.count,24))return 21;
      for(size_t i=0;i<sources.count;i++)
        if(annotation(table,"metal.descriptorSlotEvent",i*24,1,0,4)||
           annotation(table,"metal.descriptorSlotEvent",i*24,1,2,4)||
           annotation(table,"metal.descriptorSlotBinding",i*24,1,(uint64_t)(__bridge void *)sources[i],0))return 22;
    }
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    for(unsigned capture=0;capture<(api?2u:1u);capture++)
    {
      if(capture&&!upload(textures[0],specs[0]))return 5;
      if(capture)for(size_t i=0;i<specs.size();i++)if(specs[i].format>=250&&!upload(textures[i],specs[i]))return 5;
      if(api)api->StartFrameCapture(nullptr,nullptr);
      id<MTLCommandBuffer> command=[queue commandBuffer];id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];
      [compute setComputePipelineState:pipeline];
      // The first five specifications are RGBA8 2D/array/cube/cube-array/3D.
      if(sourced)
      {
        if(annotation(compute,"metal.descriptorInlineLayout",0,1,1,8)||
           annotation(compute,"metal.descriptorInlineBinding",1,0,(uint64_t)(__bridge void *)table,0))return 23;
        const uint64_t root=table.gpuAddress;[compute setBytes:&root length:8 atIndex:1];
        [compute useResource:table usage:MTLResourceUsageRead];[compute useResource:stencilView usage:MTLResourceUsageRead];
      }
      else
      {
        [compute setTexture:textures[0] atIndex:0];[compute setTexture:textures[1] atIndex:1];
        [compute setTexture:textures[2] atIndex:2];[compute setTexture:textures[4] atIndex:3];
      }
      [compute setBuffer:output offset:0 atIndex:0];
      for(id<MTLTexture> texture in textures)[compute useResource:texture usage:MTLResourceUsageRead];
      [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];
      MTLRenderPassDescriptor *overwrite=[MTLRenderPassDescriptor renderPassDescriptor];
      overwrite.colorAttachments[0].texture=textures[0];overwrite.colorAttachments[0].loadAction=MTLLoadActionClear;
      overwrite.colorAttachments[0].storeAction=MTLStoreActionStore;overwrite.colorAttachments[0].clearColor=MTLClearColorMake(192.0/255.0,0.25,0.5,1);
      [[command renderCommandEncoderWithDescriptor:overwrite] endEncoding];
      for(size_t i=0;i<specs.size();i++)if(!sourced&&specs[i].format==260&&specs[i].type==2)
      {
        MTLRenderPassDescriptor *depthOverwrite=[MTLRenderPassDescriptor renderPassDescriptor];
        depthOverwrite.depthAttachment.texture=textures[i];depthOverwrite.depthAttachment.loadAction=MTLLoadActionClear;
        depthOverwrite.depthAttachment.storeAction=MTLStoreActionStore;depthOverwrite.depthAttachment.clearDepth=0.875;
        depthOverwrite.stencilAttachment.texture=textures[i];depthOverwrite.stencilAttachment.loadAction=MTLLoadActionClear;
        depthOverwrite.stencilAttachment.storeAction=MTLStoreActionStore;depthOverwrite.stencilAttachment.clearStencil=99;
        [[command renderCommandEncoderWithDescriptor:depthOverwrite] endEncoding];
      }
      id<CAMetalDrawable> drawable=[layer nextDrawable];MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0.125,0.25,0.5,1);
      [[command renderCommandEncoderWithDescriptor:pass] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
      const uint32_t *words=(const uint32_t *)output.contents;
      if(command.status!=MTLCommandBufferStatusCompleted||words[0]!=(sourced?309u:135u)||words[1]!=0xdeadbeef)
      {fprintf(stderr,"GPU result=%u/%x expected=%u\n",words[0],words[1],sourced?309:135);return 6;}
      if(api&&!api->EndFrameCapture(nullptr,nullptr))return 7;
    }
    printf("PASS initial textures=%zu GPU=%u/DEADBEEF sourced=%d captures=%u\n",specs.size(),sourced?309:135,sourced,api?2u:0u);
  }
  return 0;
}
