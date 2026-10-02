// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool
  {
    const bool drawableTexture=getenv("RENDERDOC_METAL_DESCRIPTOR_DRAWABLE_TEXTURE") != nullptr;
    const bool r16Texture=getenv("RENDERDOC_METAL_DESCRIPTOR_R16_TEXTURE") != nullptr;
    const NSUInteger imageWidth=r16Texture?192:1,imageHeight=r16Texture?104:1;
    const NSUInteger imagePitch=r16Texture?512:256,imageStagingSize=imagePitch*imageHeight;
    const bool privateBuffer=getenv("RENDERDOC_METAL_DESCRIPTOR_PRIVATE_BUFFER") != nullptr;
    const NSUInteger inputOffset=privateBuffer?65536:0;
    const bool r11Cube=getenv("RENDERDOC_METAL_DESCRIPTOR_R11_CUBE_TEXTURE") != nullptr;
    const bool cubeTexture=r11Cube || getenv("RENDERDOC_METAL_DESCRIPTOR_CUBE_TEXTURE") != nullptr;
    const bool frameBufferView=getenv("RENDERDOC_METAL_DESCRIPTOR_FRAME_BUFFER_VIEW")!=nullptr;
    const bool bufferTexture=getenv("RENDERDOC_METAL_DESCRIPTOR_BUFFER_TEXTURE") != nullptr;
    const bool privateTexture=drawableTexture || r16Texture || bufferTexture || getenv("RENDERDOC_METAL_DESCRIPTOR_PRIVATE_TEXTURE") != nullptr;
    const bool privateView=getenv("RENDERDOC_METAL_DESCRIPTOR_PRIVATE_TEXTURE_VIEW") != nullptr;
    if(privateView && !privateTexture)return 16;
    const bool bgraTexture=!r11Cube && getenv("RENDERDOC_METAL_DESCRIPTOR_BGRA_TEXTURE") != nullptr;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice(); NSError *error=nil;
    NSString *shaderSource=@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct BufferEntry { device const uint *value [[id(0)]]; ulong z [[id(1)]]; ulong metadata [[id(2)]]; };
struct TextureEntry { ulong z [[id(0)]]; texture2d<float> image [[id(1)]]; ulong metadata [[id(2)]]; };
struct SamplerEntry { sampler point [[id(0)]]; ulong bias [[id(1)]]; ulong metadata [[id(2)]]; };
kernel void resources(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]])
{
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  output[0]=buffer->value[0]+uint(texture->image.sample(sampling->point,float2(0.5)).r*255.0+0.5)+17;
  output[1]=(buffer->metadata==0x1111111111111111ul && texture->metadata==0x2222222222222222ul &&
    sampling->bias==0x123456789abcdef0ul && sampling->metadata==0x3333333333333333ul &&
    root[1]==0xabcdef0123456789ul && root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul) ? 0xdeadbeefU : 0xbadU;
}
)MSL";
    if(cubeTexture)
    {
      shaderSource=[shaderSource stringByReplacingOccurrencesOfString:@"texture2d<float>" withString:@"texturecube<float>"];
      shaderSource=[shaderSource stringByReplacingOccurrencesOfString:@"float2(0.5)" withString:@"float3(1,0,0)"];
    }
    if(bufferTexture)
    {
      shaderSource=[shaderSource stringByReplacingOccurrencesOfString:@"texture2d<float>" withString:@"texture_buffer<float>"];
      shaderSource=[shaderSource stringByReplacingOccurrencesOfString:@"texture->image.sample(sampling->point,float2(0.5))" withString:@"texture->image.read(uint(0))"];
    }
    id<MTLLibrary> library=[device newLibraryWithSource:shaderSource options:nil error:&error];
    if(!library) { fprintf(stderr,"%s\n",error.localizedDescription.UTF8String); return 2; }
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"resources"] error:&error];
    const uint32_t inputValues[]={0xbadU,41U,80U};
    id<MTLBuffer> input=privateBuffer?[device newBufferWithLength:65552 options:MTLResourceStorageModePrivate]:
        [device newBufferWithBytes:inputValues length:12 options:MTLResourceStorageModeShared];
    if(privateBuffer)
    {
      id<MTLBuffer> staging=[device newBufferWithLength:65552 options:MTLResourceStorageModeShared];
      memset(staging.contents,0xcc,65552);memcpy((uint8_t *)staging.contents+inputOffset,inputValues,12);
      id<MTLCommandQueue> q=[device newCommandQueue];id<MTLCommandBuffer> c=[q commandBuffer];id<MTLBlitCommandEncoder> b=[c blitCommandEncoder];
      [b copyFromBuffer:staging sourceOffset:0 toBuffer:input destinationOffset:0 size:65552];[b endEncoding];[c commit];[c waitUntilCompleted];
      if(c.status!=MTLCommandBufferStatusCompleted)return 19;
    }
    MTLTextureDescriptor *descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(r16Texture?MTLPixelFormatR16Float:r11Cube?MTLPixelFormatRG11B10Float:bgraTexture?MTLPixelFormatBGRA8Unorm:MTLPixelFormatRGBA8Unorm) width:imageWidth height:imageHeight mipmapped:NO];
    descriptor.storageMode=privateTexture?MTLStorageModePrivate:MTLStorageModeShared; descriptor.usage=MTLTextureUsageShaderRead | (privateTexture?MTLTextureUsageRenderTarget:0) | (privateView?MTLTextureUsagePixelFormatView:0);
    if(cubeTexture)descriptor.textureType=MTLTextureTypeCube;
    id<MTLBuffer> textureParent=bufferTexture?[device newBufferWithLength:512 options:MTLResourceStorageModePrivate]:nil;
    if(bufferTexture){descriptor.textureType=MTLTextureTypeTextureBuffer;descriptor.usage=MTLTextureUsageShaderRead;
      descriptor.allowGPUOptimizedContents=NO;descriptor.resourceOptions=MTLResourceStorageModePrivate;}
    CAMetalLayer *sourceLayer=nil;id<CAMetalDrawable> sourceDrawable=nil;
    if(drawableTexture)
    {
      sourceLayer=[CAMetalLayer layer];sourceLayer.device=device;sourceLayer.pixelFormat=MTLPixelFormatBGRA8Unorm;
      sourceLayer.framebufferOnly=NO;sourceLayer.drawableSize=CGSizeMake(1,1);sourceDrawable=[sourceLayer nextDrawable];
      if(!sourceDrawable)return 20;
    }
    id<MTLTexture> image=drawableTexture?sourceDrawable.texture:bufferTexture?[textureParent newTextureWithDescriptor:descriptor offset:256 bytesPerRow:256]:[device newTextureWithDescriptor:descriptor];
    if(!image || (r11Cube&&image.pixelFormat!=MTLPixelFormatRG11B10Float))return 18;
    uint8_t pixel[]={uint8_t(bgraTexture?192:64),128,uint8_t(bgraTexture?64:192),255};
    if(r11Cube){const uint32_t packed=0x340u | (0x380u<<11) | (0x1d0u<<22);memcpy(pixel,&packed,4);}
    if(r16Texture){const uint16_t half=0x3400;memcpy(pixel,&half,2);}
    id<MTLCommandQueue> uploadQueue=privateTexture?[device newCommandQueue]:nil;
    id<MTLBuffer> initialPixels=privateTexture?[device newBufferWithLength:(bufferTexture?512:imageStagingSize) options:MTLResourceStorageModeShared]:nil;
    if(privateTexture && (!uploadQueue || !initialPixels)) return 11;
    if(bufferTexture)memset(initialPixels.contents,0xcc,512);
    if(privateTexture) memcpy((uint8_t *)initialPixels.contents+(bufferTexture?256:0),pixel,4);
    if(r16Texture)for(NSUInteger y=0;y<imageHeight;y++)for(NSUInteger x=0;x<imageWidth;x++)
      memcpy((uint8_t *)initialPixels.contents+y*imagePitch+x*2,pixel,2);
    auto resetImage=[&]() {
      if(!privateTexture) { [image replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:pixel bytesPerRow:4]; return true; }
      id<MTLCommandBuffer> upload=[uploadQueue commandBuffer];
      if(drawableTexture)
      {
        MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture=image;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
        pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        pass.colorAttachments[0].clearColor=MTLClearColorMake(64.0/255.0,128.0/255.0,192.0/255.0,1);
        [[upload renderCommandEncoderWithDescriptor:pass] endEncoding];[upload commit];[upload waitUntilCompleted];
        return upload.status==MTLCommandBufferStatusCompleted;
      }
      id<MTLBlitCommandEncoder> blit=[upload blitCommandEncoder];
      if(bufferTexture)[blit copyFromBuffer:initialPixels sourceOffset:0 toBuffer:textureParent destinationOffset:0 size:512];
      else for(unsigned slice=0;slice<(cubeTexture?6u:1u);slice++)
        [blit copyFromBuffer:initialPixels sourceOffset:0 sourceBytesPerRow:imagePitch sourceBytesPerImage:imageStagingSize
          sourceSize:MTLSizeMake(imageWidth,imageHeight,1) toTexture:image destinationSlice:slice destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
      [blit endEncoding];[upload commit];[upload waitUntilCompleted];
      return upload.status==MTLCommandBufferStatusCompleted;
    };
    if(!resetImage()) return 12;
    id<MTLTexture> mipImage=nil;
    if(privateTexture)
    {
      MTLTextureDescriptor *mips=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(bgraTexture?MTLPixelFormatBGRA8Unorm:MTLPixelFormatRGBA8Unorm) width:3 height:5 mipmapped:YES];
      mips.storageMode=MTLStorageModePrivate;mips.usage=MTLTextureUsageShaderRead;
      mipImage=[device newTextureWithDescriptor:mips];
      id<MTLBuffer> rows=[device newBufferWithLength:1280 options:MTLResourceStorageModeShared];
      if(!mipImage || !rows) return 14;
      for(unsigned mip=0;mip<3;mip++)
      {
        unsigned width=std::max(1u,3u>>mip),height=std::max(1u,5u>>mip);
        memset(rows.contents,0xcc,1280);
        for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)
        {
          uint8_t *p=(uint8_t *)rows.contents+256*y+4*x;
          p[0]=17+19*mip;p[1]=31+y;p[2]=53+x;p[3]=255;
        }
        id<MTLCommandBuffer> upload=[uploadQueue commandBuffer];id<MTLBlitCommandEncoder> blit=[upload blitCommandEncoder];
        [blit copyFromBuffer:rows sourceOffset:0 sourceBytesPerRow:256 sourceBytesPerImage:256*height
          sourceSize:MTLSizeMake(width,height,1) toTexture:mipImage destinationSlice:0 destinationLevel:mip destinationOrigin:MTLOriginMake(0,0,0)];
        [blit endEncoding];[upload commit];[upload waitUntilCompleted];
        if(upload.status!=MTLCommandBufferStatusCompleted)return 15;
      }
    }
    id<MTLTexture> sampledImage=privateView?[image newTextureViewWithPixelFormat:descriptor.pixelFormat textureType:(cubeTexture?MTLTextureTypeCube:MTLTextureType2D) levels:NSMakeRange(0,1) slices:NSMakeRange(0,cubeTexture?6:1)]:image;
    if(!sampledImage)return 17;
    MTLSamplerDescriptor *sampling=[MTLSamplerDescriptor new]; sampling.supportArgumentBuffers=YES;
    id<MTLSamplerState> point=[device newSamplerStateWithDescriptor:sampling];
    const uint64_t bufferBytes[]={input.gpuAddress+inputOffset+4,0,0x1111111111111111ULL};
    const uint64_t textureBytes[]={0,sampledImage.gpuResourceID._impl,0x2222222222222222ULL};
    const uint64_t samplerBytes[]={point.gpuResourceID._impl,0x123456789abcdef0ULL,0x3333333333333333ULL};
    id<MTLBuffer> bufferTable=[device newBufferWithBytes:bufferBytes length:24 options:MTLResourceStorageModeShared];
    id<MTLBuffer> textureTable=[device newBufferWithBytes:textureBytes length:24 options:MTLResourceStorageModeShared];
    id<MTLBuffer> samplerTable=[device newBufferWithBytes:samplerBytes length:24 options:MTLResourceStorageModeShared];
    id<MTLBuffer> output=[device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    const uint64_t tableVAs[]={bufferTable.gpuAddress,textureTable.gpuAddress,samplerTable.gpuAddress};
    if(!pipeline || !input || !image || !point || !output) return 3;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue coverage={}; coverage.uint32=frameBufferView?65:drawableTexture?37:r16Texture?35:privateBuffer?34:cubeTexture?33:bufferTexture?32:privateView?28:privateTexture?27:7;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage)) return 5;
    }
    auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api) return uint32_t(0);
      RENDERDOC_AnnotationValue v={}; v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
    };
    id<MTLBuffer> tables[]={bufferTable,textureTable,samplerTable};
    id sources[]={input,sampledImage,point};
    const uint64_t types[]={0,4,7}, kinds[]={0,1,2};
    for(unsigned i=0;i<3;i++)
      if(annotation(tables[i],"metal.descriptorTable",i==2?2:1,0,1,24) ||
         annotation(tables[i],"metal.descriptorSlotEvent",0,1,0,types[i]) ||
         annotation(tables[i],"metal.descriptorSlotEvent",0,1,2,types[i]) ||
         annotation(tables[i],"metal.descriptorSlotBinding",0,kinds[i],(uint64_t)(__bridge void *)sources[i],i==0?inputOffset+4:0)) return 6;
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    const uint8_t overwritten[]={192,64,128,255};
    id<MTLBuffer> overwriteSource=bufferTexture?[device newBufferWithBytes:overwritten length:4 options:MTLResourceStorageModeShared]:nil;
    id<MTLCommandQueue> queue=[device newCommandQueue];
    for(int capture=0;capture<(api?2:1);capture++)
    {
      if(capture && !resetImage()) return 13;
      // Register and select the presentation layer when a separate background
      // drawable layer is retained as a shader source.
      id<CAMetalDrawable> frameDrawable=drawableTexture?[layer nextDrawable]:nil;
      if(drawableTexture&&!frameDrawable)return 21;
      if(api) api->StartFrameCapture(nullptr,drawableTexture?(__bridge void *)layer:nullptr);
      if(frameBufferView)
      {
        sampledImage=[textureParent newTextureWithDescriptor:descriptor offset:256 bytesPerRow:256];
        if(!sampledImage)return 21;
        const uint64_t value[]={0,sampledImage.gpuResourceID._impl,textureBytes[2]};
        memcpy(textureTable.contents,value,24);
        if(annotation(textureTable,"metal.descriptorSlotEvent",0,1,2,4) ||
           annotation(textureTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)sampledImage,0))return 22;
        printf("FRAME_BUFFER_VIEW_%d=%llu parentVA=%llu\n",capture,(unsigned long long)sampledImage.gpuResourceID._impl,(unsigned long long)textureParent.gpuAddress);
      }
      uint64_t root[6]={};for(unsigned i=0;i<3;i++){root[i*2]=tableVAs[i];root[i*2+1]=0xabcdef0123456789ULL;}
      id<MTLCommandBuffer> command=[queue commandBuffer];[command enqueue];
      id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];
      if(annotation(compute,"metal.descriptorInlineLayout",0,0,3,16)) return 7;
      for(unsigned i=0;i<3;i++)
      {
        if(annotation(compute,"metal.descriptorInlineBinding",0,i,(uint64_t)(__bridge void *)tables[i],0))return 8;
        [compute useResource:tables[i] usage:MTLResourceUsageRead];
      }
      [compute setBytes:root length:sizeof(root) atIndex:0];[compute setBuffer:output offset:0 atIndex:1];
      if(mipImage) [compute useResource:mipImage usage:MTLResourceUsageRead];
      [compute useResource:input usage:MTLResourceUsageRead];[compute useResource:sampledImage usage:MTLResourceUsageRead];
      [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];
      if(bufferTexture)
      {
        id<MTLBlitCommandEncoder> overwrite=[command blitCommandEncoder];
        [overwrite copyFromBuffer:overwriteSource sourceOffset:0 toBuffer:textureParent destinationOffset:256 size:4];
        [overwrite endEncoding];
      }
      else if(privateTexture)
      {
        MTLRenderPassDescriptor *overwrite=[MTLRenderPassDescriptor renderPassDescriptor];
        overwrite.colorAttachments[0].texture=image;overwrite.colorAttachments[0].loadAction=MTLLoadActionClear;
        overwrite.colorAttachments[0].storeAction=MTLStoreActionStore;
        overwrite.colorAttachments[0].clearColor=MTLClearColorMake(r16Texture?0.75:192.0/255.0,0.25,0.5,1);
        [[command renderCommandEncoderWithDescriptor:overwrite] endEncoding];
      }
      id<CAMetalDrawable> drawable=frameDrawable?:[layer nextDrawable];MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0.125,0.25,0.5,1);
      [[command renderCommandEncoderWithDescriptor:pass] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
      const uint32_t *words=(const uint32_t *)output.contents;
      if(command.error || words[0]!=122 || words[1]!=0xdeadbeefU)return 9;
      if(api && !api->EndFrameCapture(nullptr,drawableTexture?(__bridge void *)layer:nullptr))return 10;
      if(frameBufferView)
      {
        sampledImage=image;memcpy(textureTable.contents,textureBytes,24);
        if(annotation(textureTable,"metal.descriptorSlotEvent",0,1,2,4) ||
           annotation(textureTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)image,0))return 23;
      }
    }
    printf("resources native PASS result=122 VA_A=%llu VA_B=%llu VA_TABLE=%llu TEX=%llu SAMP=%llu captures=%d storage=%lu drawable=%d\n",(unsigned long long)bufferBytes[0],(unsigned long long)tableVAs[1],(unsigned long long)tableVAs[0],(unsigned long long)textureBytes[1],(unsigned long long)samplerBytes[0],api?2:0,(unsigned long)image.storageMode,drawableTexture);
  }
  return 0;
}
