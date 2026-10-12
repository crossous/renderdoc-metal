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
  @autoreleasepool
  {
    const bool dynamic=getenv("RENDERDOC_METAL_DYNAMIC_TEXTURE")!=nullptr;
    const unsigned selected=getenv("RENDERDOC_METAL_DYNAMIC_TEXTURE_SECOND_ROW")?1:0;
    const bool samplerVariant=getenv("RENDERDOC_METAL_DYNAMIC_SAMPLER")!=nullptr;
    const bool uintVariant=getenv("RENDERDOC_METAL_DYNAMIC_TEXTURE_UINT")!=nullptr;
    const bool viewVariant=getenv("RENDERDOC_METAL_DYNAMIC_TEXTURE_VIEW")!=nullptr;
    const bool unusedBirth=getenv("RENDERDOC_METAL_UNUSED_TEXTURE_BIRTH")!=nullptr;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice(); NSError *error=nil;
    id<MTLLibrary> library=[device newLibraryWithSource:@R"MSL(
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
    (root[1]==0xabcdef0123456789ul || root[1]==0x8000000000000000ul || root[1]==0x4000000000000000ul) && root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul) ? 0xdeadbeefU : 0xbadU;
}
kernel void resources_dynamic(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]])
{
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  texture+=clz(root[1]);
  sampling+=clz(root[3]);
  output[0]=buffer->value[0]+uint(texture->image.sample(sampling->point,float2(0.5)).r*255.0+0.5)+17;
  output[1]=(buffer->metadata==0x1111111111111111ul && texture->metadata==0x2222222222222222ul &&
    sampling->bias==0x123456789abcdef0ul && sampling->metadata==0x3333333333333333ul &&
    (root[1]==0xabcdef0123456789ul || root[1]==0x8000000000000000ul || root[1]==0x4000000000000000ul) && (root[3]==0xabcdef0123456789ul || root[3]==0x8000000000000000ul || root[3]==0x4000000000000000ul) && root[5]==0xabcdef0123456789ul) ? 0xdeadbeefU : 0xbadU;
}
struct UintTextureEntry { ulong z [[id(0)]]; texture2d<uint,access::read> image [[id(1)]]; ulong metadata [[id(2)]]; };
kernel void resources_uint(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]])
{
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  const device UintTextureEntry *textures=reinterpret_cast<const device UintTextureEntry *>(root[2]);
  const device UintTextureEntry &selected=textures[clz(root[1])];
  output[0]=buffer->value[0]+selected.image.read(uint2(0)).r+17;
  output[1]=(buffer->metadata==0x1111111111111111ul && selected.metadata==0x2222222222222222ul &&
    root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul)?0xdeadbeefU:0xbadU;
}
struct CopyRoot { device const ulong *source [[id(0)]]; device ulong *destination [[id(1)]]; };
kernel void copy_resource(const device CopyRoot &root [[buffer(0)]], device uint *output [[buffer(1)]])
{ for(uint i=0;i<3;i++)root.destination[i]=root.source[i]; output[1]=0xdeadbeefU; }
)MSL" options:nil error:&error];
    if(const char *binary=getenv("RENDERDOC_METAL_TEST_METALLIB"))
      library=[device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:binary]] error:&error];
    if(!library) { fprintf(stderr,"%s\n",error.localizedDescription.UTF8String); return 2; }
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:uintVariant?@"resources_uint":dynamic?@"resources_dynamic":@"resources"] error:&error];
    id<MTLComputePipelineState> copyPipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"copy_resource"] error:&error];
    const uint32_t inputValues[]={0xbadU,41U,80U};
    id<MTLBuffer> input=[device newBufferWithBytes:inputValues length:12 options:MTLResourceStorageModeShared];
    MTLTextureDescriptor *descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    if(samplerVariant){descriptor.width=2;descriptor.height=1;}
    if(viewVariant){descriptor.pixelFormat=MTLPixelFormatBGRA8Unorm;descriptor.width=3;descriptor.height=2;}
    descriptor.storageMode=MTLStorageModeShared; descriptor.usage=MTLTextureUsageShaderRead|(viewVariant?MTLTextureUsagePixelFormatView:0);
    id<MTLTexture> image=[device newTextureWithDescriptor:descriptor];
    id<MTLTexture> imageParent=image;
    MTLTextureDescriptor *otherDescriptor=[descriptor copy];
    if(uintVariant)otherDescriptor.pixelFormat=MTLPixelFormatR32Uint;
    id<MTLTexture> otherParent=[device newTextureWithDescriptor:otherDescriptor];
    uint8_t pixels[24],otherPixels[24];
    for(unsigned i=0;i<6;i++)
    {
      const uint8_t first[]={uint8_t(viewVariant?192:64),128,uint8_t(viewVariant?64:192),255};
      const uint8_t second[]={uint8_t(viewVariant?192:128),128,uint8_t(viewVariant?128:192),255};
      memcpy(pixels+i*4,first,4);memcpy(otherPixels+i*4,second,4);
    }
    if(uintVariant)for(unsigned i=0;i<6;i++){const uint32_t n=128;memcpy(otherPixels+i*4,&n,4);}
    if(samplerVariant){pixels[4]=128;otherPixels[4]=192;}
    [imageParent replaceRegion:MTLRegionMake2D(0,0,descriptor.width,descriptor.height) mipmapLevel:0 withBytes:pixels bytesPerRow:descriptor.width*4];
    [otherParent replaceRegion:MTLRegionMake2D(0,0,descriptor.width,descriptor.height) mipmapLevel:0 withBytes:otherPixels bytesPerRow:descriptor.width*4];
    if(viewVariant)image=[imageParent newTextureViewWithPixelFormat:descriptor.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)];
    id<MTLTexture> other=viewVariant?[otherParent newTextureViewWithPixelFormat:otherDescriptor.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)]:otherParent;
    const uint64_t otherID=other.gpuResourceID._impl;
    MTLSamplerDescriptor *sampling=[MTLSamplerDescriptor new]; sampling.supportArgumentBuffers=YES;
    id<MTLSamplerState> point=[device newSamplerStateWithDescriptor:sampling];
    sampling.minFilter=MTLSamplerMinMagFilterLinear;sampling.magFilter=MTLSamplerMinMagFilterLinear;
    id<MTLSamplerState> linear=samplerVariant?[device newSamplerStateWithDescriptor:sampling]:point;
    const uint64_t bufferBytes[]={input.gpuAddress+4,0,0x1111111111111111ULL};
    const uint64_t textureBytes[]={0,image.gpuResourceID._impl,0x2222222222222222ULL};
    const uint64_t samplerBytes[]={point.gpuResourceID._impl,0x123456789abcdef0ULL,0x3333333333333333ULL};
    id<MTLBuffer> bufferTable=[device newBufferWithBytes:bufferBytes length:24 options:MTLResourceStorageModeShared];
    uint64_t textureRows[6];memcpy(textureRows,textureBytes,24);memcpy(textureRows+3,textureBytes,24);
    if(dynamic)textureRows[4]=otherID;
    id<MTLBuffer> textureTable=[device newBufferWithBytes:textureRows length:dynamic?48:24 options:MTLResourceStorageModeShared];
    uint64_t samplerRows[6];memcpy(samplerRows,samplerBytes,24);memcpy(samplerRows+3,samplerBytes,24);samplerRows[3]=linear.gpuResourceID._impl;
    id<MTLBuffer> samplerTable=[device newBufferWithBytes:samplerRows length:samplerVariant?48:24 options:MTLResourceStorageModeShared];
    uint64_t payloadBytes[]={0,otherID,0x2222222222222222ULL};
    id<MTLBuffer> payload=[device newBufferWithBytes:payloadBytes length:24 options:MTLResourceStorageModeShared];
    textureTable.label=@"Resource texture destination";payload.label=@"Resource texture payload";
    id<MTLBuffer> output=[device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    const uint64_t tableVAs[]={bufferTable.gpuAddress,textureTable.gpuAddress,samplerTable.gpuAddress};
    if(!pipeline || !input || !image || !point || !output) return 3;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue coverage={}; coverage.uint32=dynamic?65:7;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage)) return 5;
    }
    auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api) return uint32_t(0);
      RENDERDOC_AnnotationValue v={}; v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
    };
    id<MTLBuffer> tables[]={bufferTable,textureTable,samplerTable};
    id sources[]={input,image,point};
    const uint64_t types[]={0,4,7}, kinds[]={0,1,2};
    for(unsigned i=0;i<3;i++)
      if(annotation(tables[i],"metal.descriptorTable",i==2?2:1,0,(dynamic && i==1) || (samplerVariant && i==2)?2:1,24) ||
         annotation(tables[i],"metal.descriptorSlotEvent",0,1,0,types[i]) ||
         annotation(tables[i],"metal.descriptorSlotEvent",0,1,2,types[i]) ||
         annotation(tables[i],"metal.descriptorSlotBinding",0,kinds[i],(uint64_t)(__bridge void *)sources[i],i==0?4:0)) return 6;
    if(dynamic && (annotation(textureTable,"metal.descriptorSlotEvent",24,2,0,4) ||
       annotation(textureTable,"metal.descriptorSlotEvent",24,2,2,4) ||
       annotation(textureTable,"metal.descriptorSlotBinding",24,1,(uint64_t)(__bridge void *)other,0)))return 20;
    if(samplerVariant && (annotation(samplerTable,"metal.descriptorSlotEvent",24,2,0,7) ||
       annotation(samplerTable,"metal.descriptorSlotEvent",24,2,2,7) ||
       annotation(samplerTable,"metal.descriptorSlotBinding",24,2,(uint64_t)(__bridge void *)linear,0)))return 22;
    if(annotation(payload,"metal.descriptorTable",1,0,1,24) ||
       annotation(payload,"metal.descriptorSlotEvent",0,1,0,4)) return 11;
    if(api && !dynamic)
    {
      RENDERDOC_AnnotationValue gpu={};gpu.uint32=1;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)textureTable,"metal.descriptorGPUWrites",eRENDERDOC_UInt32,0,&gpu))return 12;
    }
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    id<MTLCommandQueue> queue=[device newCommandQueue];
    for(int capture=0;capture<(api?2:1);capture++)
    {
      id<MTLTexture> nextImage=capture==0?other:image;
      payloadBytes[1]=capture==0?otherID:textureBytes[1];
      memcpy(payload.contents,payloadBytes,24);
      if(annotation(payload,"metal.descriptorSlotEvent",0,1,2,4) ||
         annotation(payload,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 13;
      if(uintVariant && capture)
      {for(unsigned i=0;i<6;i++){const uint32_t n=64;memcpy(otherPixels+i*4,&n,4);}
       [otherParent replaceRegion:MTLRegionMake2D(0,0,descriptor.width,descriptor.height) mipmapLevel:0 withBytes:otherPixels bytesPerRow:descriptor.width*4];}
      // A live typed row is not necessarily read by this invocation. Create
      // its separate Private backing in-frame, leave its pixels untouched,
      // and select the other, fully uploaded row with the original shader.
      id<MTLHeap> birthHeap=nil;
      id<MTLTexture> birthTexture=nil;
      MTLTextureDescriptor *birthDescriptor=nil;
      if(unusedBirth)
      {
        if(!dynamic || samplerVariant || uintVariant)return 23;
        // The previous frame's temporary object has retired. Establish this
        // frame's real initial packet before replacing it with the new birth.
        textureRows[4]=otherID;memcpy(textureTable.contents,textureRows,48);
        if(annotation(textureTable,"metal.descriptorSlotEvent",24,2,2,4) ||
           annotation(textureTable,"metal.descriptorSlotBinding",24,1,(uint64_t)(__bridge void *)other,0))return 26;
        birthDescriptor=[descriptor copy];
        birthDescriptor.storageMode=MTLStorageModePrivate;
        birthDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;
        const MTLSizeAndAlign layout=[device heapTextureSizeAndAlignWithDescriptor:birthDescriptor];
        MTLHeapDescriptor *heapDescriptor=[MTLHeapDescriptor new];
        heapDescriptor.type=MTLHeapTypePlacement;heapDescriptor.storageMode=MTLStorageModePrivate;
        heapDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;
        heapDescriptor.size=layout.size+layout.align;
        birthHeap=[device newHeapWithDescriptor:heapDescriptor];
      }
      if(api) api->StartFrameCapture(nullptr,nullptr);
      if(unusedBirth)
      {
        birthTexture=[birthHeap newTextureWithDescriptor:birthDescriptor offset:0];
        if(!birthHeap || !birthTexture)return 24;
        textureRows[4]=birthTexture.gpuResourceID._impl;
        memcpy(textureTable.contents,textureRows,48);
        if(annotation(textureTable,"metal.descriptorSlotEvent",24,2,2,4) ||
           annotation(textureTable,"metal.descriptorSlotBinding",24,1,(uint64_t)(__bridge void *)birthTexture,0))return 25;
      }
      uint64_t root[6]={};for(unsigned i=0;i<3;i++){root[i*2]=tableVAs[i];root[i*2+1]=0xabcdef0123456789ULL;}
      if(dynamic)root[1]=0x8000000000000000ULL>>(selected^unsigned(uintVariant?0:capture));
      if(unusedBirth)root[1]=0x8000000000000000ULL;
      if(samplerVariant)root[3]=0x8000000000000000ULL>>capture;
      id<MTLCommandBuffer> command=[queue commandBuffer];[command enqueue];
      auto consume=[&]() {
        id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];
        if(annotation(compute,"metal.descriptorInlineLayout",0,0,3,16))return false;
        for(unsigned i=0;i<3;i++)
        {
          if(annotation(compute,"metal.descriptorInlineBinding",0,i,(uint64_t)(__bridge void *)tables[i],0))return false;
          [compute useResource:tables[i] usage:MTLResourceUsageRead];
        }
        [compute setBytes:root length:sizeof(root) atIndex:0];[compute setBuffer:output offset:0 atIndex:1];
        [compute useResource:input usage:MTLResourceUsageRead];[compute useResource:image usage:MTLResourceUsageRead];
        [compute useResource:other usage:MTLResourceUsageRead];
        if(birthTexture)[compute useResource:birthTexture usage:MTLResourceUsageRead];
        [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];return true;
      };
      if(!consume())return 14;
      if(!dynamic)
      {
      id<MTLComputeCommandEncoder> producer=[command computeCommandEncoder];[producer setComputePipelineState:copyPipeline];
      const uint64_t copyRoot[]={payload.gpuAddress,textureTable.gpuAddress+(dynamic?selected*24:0)};
      if(annotation(producer,"metal.descriptorInlineLayout",0,0,2,8) ||
         annotation(producer,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)payload,0) ||
         annotation(producer,"metal.descriptorInlineBinding",0,1,(uint64_t)(__bridge void *)textureTable,dynamic?selected*24:0))return 15;
      [producer setBytes:copyRoot length:16 atIndex:0];[producer setBuffer:output offset:0 atIndex:1];[producer useResource:payload usage:MTLResourceUsageRead];
      [producer useResource:textureTable usage:MTLResourceUsageWrite];
      [producer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      if(annotation(textureTable,"metal.descriptorSlotProducer",dynamic?selected*24:0,(uint64_t)(__bridge void *)producer,(uint64_t)(__bridge void *)payload,0))return 16;
      if(api)
      {
        RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=dynamic?selected*24:0;memcpy(&value.vector.uint64[1],payloadBytes,24);
        if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)textureTable,"metal.descriptorSlotGPUValue",eRENDERDOC_UInt64,4,&value))return 17;
      }
      if(annotation(textureTable,"metal.descriptorSlotBinding",dynamic?selected*24:0,1,(uint64_t)(__bridge void *)nextImage,0))return 18;
      [producer endEncoding];if(!consume())return 19;
      }
      id<CAMetalDrawable> drawable=[layer nextDrawable];MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0.125,0.25,0.5,1);
      [[command renderCommandEncoderWithDescriptor:pass] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
      const uint32_t *words=(const uint32_t *)output.contents;
      if(command.error || words[0]!=(unusedBirth?122U:samplerVariant?(capture?218U:186U):dynamic?(uintVariant?(capture?122U:186U):((selected^unsigned(capture))?186U:122U)):(capture==0?186U:122U)) || words[1]!=0xdeadbeefU)return 9;
      printf("Native capture=%d full_output=%u/%u texture_rows=%u sampler_rows=%u\n",capture,words[0],words[1],dynamic?2:1,samplerVariant?2:1);
      if(const char *oracle=getenv("RENDERDOC_METAL_TEST_ORACLE"))
      {char path[2048];snprintf(path,sizeof(path),"%s.%d.bin",oracle,capture);FILE *f=fopen(path,"wbx");if(!f || fwrite(output.contents,1,8,f)!=8 || fclose(f))return 21;}
      if(api && !api->EndFrameCapture(nullptr,nullptr))return 10;
    }
    printf("resources native PASS VA_A=%llu VA_B=%llu VA_TABLE=%llu TEX=%llu SAMP=%llu TEX_B=%llu captures=%d\n",(unsigned long long)bufferBytes[0],(unsigned long long)tableVAs[1],(unsigned long long)tableVAs[0],(unsigned long long)textureBytes[1],(unsigned long long)samplerBytes[0],(unsigned long long)otherID,api?2:0);
  }
  return 0;
}
