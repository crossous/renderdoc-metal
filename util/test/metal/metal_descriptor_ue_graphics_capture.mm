// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv)
{
  if(argc!=2)return 24;
  @autoreleasepool
  {
    const unsigned updates=getenv("RENDERDOC_METAL_UE_BATCH_UPDATES")?unsigned(strtoul(getenv("RENDERDOC_METAL_UE_BATCH_UPDATES"),nullptr,10)):1;
    if(!updates||updates>256)return 29;
    const NSUInteger textureBytesCount=updates*24;
    const NSUInteger textureMember=(updates-1)*24;
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
    root[1]==0xabcdef0123456789ul && root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul) ? 0xdeadbeefU : 0xbadU;
}
struct VSOut { float4 position [[position]]; float value; };
vertex VSOut resource_vertex(uint index [[vertex_id]], const device ulong *root [[buffer(0)]])
{
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  float2 positions[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
  VSOut out;out.position=float4(positions[index],0,1);
  out.value=(buffer->metadata==0x1111111111111111ul && root[1]==0xabcdef0123456789ul) ? float(buffer->value[0]) : 0.0;
  return out;
}
fragment float4 resource_fragment(VSOut in [[stage_in]], const device ulong *root [[buffer(0)]])
{
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  const bool ordinary=texture->metadata==0x2222222222222222ul && sampling->bias==0x123456789abcdef0ul &&
      sampling->metadata==0x3333333333333333ul && root[1]==0xabcdef0123456789ul &&
      root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul;
  return ordinary ? float4(texture->image.sample(sampling->point,float2(0.5)).r+(in.value+17.0)/255.0,64.0/255.0,128.0/255.0,1) : float4(1,0,1,1);
}
struct CopyRoot { device const ulong *source [[id(0)]]; device ulong *destination [[id(1)]]; };
kernel void copy_resource(const device CopyRoot &root [[buffer(0)]], device uint *output [[buffer(1)]])
{ for(uint i=0;i<3;i++)root.destination[i]=root.source[i]; output[1]=0xdeadbeefU; }
)MSL" options:nil error:&error];
    if(!library) { fprintf(stderr,"%s\n",error.localizedDescription.UTF8String); return 2; }
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"resources"] error:&error];
    id<MTLComputePipelineState> copyPipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"copy_resource"] error:&error];
    MTLRenderPipelineDescriptor *renderDescriptor=[MTLRenderPipelineDescriptor new];
    renderDescriptor.vertexFunction=[library newFunctionWithName:@"resource_vertex"];
    renderDescriptor.fragmentFunction=[library newFunctionWithName:@"resource_fragment"];
    renderDescriptor.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;
    id<MTLRenderPipelineState> renderPipeline=[device newRenderPipelineStateWithDescriptor:renderDescriptor error:&error];
    if(!renderPipeline){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 20;}
    NSString *assetFolder=[NSString stringWithUTF8String:argv[1]];
    NSData *ueCode=[NSData dataWithContentsOfFile:[assetFolder stringByAppendingPathComponent:@"shader.metallib"]];
    NSData *ueSamplers=[NSData dataWithContentsOfFile:[assetFolder stringByAppendingPathComponent:@"samplers.bin"]];
    NSDictionary *ueManifest=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[assetFolder stringByAppendingPathComponent:@"manifest.json"]] options:0 error:nil];
    if(!ueCode || !ueSamplers || !ueManifest)return 25;
    dispatch_data_t ueBinary=dispatch_data_create(ueCode.bytes,ueCode.length,dispatch_get_main_queue(),DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    id<MTLLibrary> ueLibrary=[device newLibraryWithData:ueBinary error:&error];
    MTLComputePipelineDescriptor *ueDescriptor=[MTLComputePipelineDescriptor new];
    ueDescriptor.computeFunction=[ueLibrary newFunctionWithName:ueManifest[@"function"]];
    ueDescriptor.maxTotalThreadsPerThreadgroup=1;ueDescriptor.buffers[0].mutability=MTLMutabilityImmutable;
    copyPipeline=[device newComputePipelineStateWithDescriptor:ueDescriptor options:MTLPipelineOptionBindingInfo|MTLPipelineOptionBufferTypeInfo reflection:nil error:&error];
    if(!copyPipeline){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 26;}
    const uint32_t inputValues[]={0xbadU,41U,80U};
    id<MTLBuffer> input=[device newBufferWithBytes:inputValues length:12 options:MTLResourceStorageModeShared];
    MTLTextureDescriptor *descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    descriptor.storageMode=MTLStorageModeShared; descriptor.usage=MTLTextureUsageShaderRead;
    id<MTLTexture> image=[device newTextureWithDescriptor:descriptor];
    const uint8_t pixel[]={64,128,192,255};
    [image replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:pixel bytesPerRow:4];
    id<MTLTexture> other=[device newTextureWithDescriptor:descriptor];
    const uint8_t otherPixel[]={128,128,192,255};
    [other replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:otherPixel bytesPerRow:4];
    const uint64_t otherID=other.gpuResourceID._impl;
    MTLSamplerDescriptor *sampling=[MTLSamplerDescriptor new]; sampling.supportArgumentBuffers=YES;
    id<MTLSamplerState> point=[device newSamplerStateWithDescriptor:sampling];
    const uint64_t bufferBytes[]={input.gpuAddress+4,0,0x1111111111111111ULL};
    const uint64_t textureBytes[]={0,image.gpuResourceID._impl,0x2222222222222222ULL};
    const uint64_t samplerBytes[]={point.gpuResourceID._impl,0x123456789abcdef0ULL,0x3333333333333333ULL};
    id<MTLBuffer> bufferTable=[device newBufferWithBytes:bufferBytes length:24 options:MTLResourceStorageModeShared];
    id<MTLBuffer> textureTable=[device newBufferWithLength:textureBytesCount options:MTLResourceStorageModeShared];
    for(unsigned i=0;i<updates;i++)memcpy((char *)textureTable.contents+i*24,textureBytes,24);
    id<MTLBuffer> samplerTable=[device newBufferWithBytes:samplerBytes length:24 options:MTLResourceStorageModeShared];
    uint64_t payloadBytes[]={0,otherID,0x2222222222222222ULL};
    id<MTLBuffer> payload=[device newBufferWithLength:textureBytesCount options:MTLResourceStorageModeShared];
    for(unsigned i=0;i<updates;i++)memcpy((char *)payload.contents+i*24,payloadBytes,24);
    const uint32_t ueUniform[]={updates,1,0,2};
    id<MTLBuffer> ueConstants=[device newBufferWithBytes:ueUniform length:16 options:MTLResourceStorageModeShared];
    id<MTLBuffer> ueIndices=[device newBufferWithLength:updates*4 options:MTLResourceStorageModeShared];
    for(unsigned i=0;i<updates;i++)((uint32_t *)ueIndices.contents)[i]=i;
    id<MTLBuffer> ueSampling=[device newBufferWithBytes:ueSamplers.bytes length:ueSamplers.length options:MTLResourceStorageModeShared];
    const uint64_t uePackets[]={payload.gpuAddress,0,textureBytesCount,ueIndices.gpuAddress,0,updates*4,textureTable.gpuAddress,0,textureBytesCount};
    id<MTLBuffer> ueTable=[device newBufferWithBytes:uePackets length:72 options:MTLResourceStorageModeShared];
    textureTable.label=@"Resource texture destination";payload.label=@"Resource texture payload";
    id<MTLBuffer> output=[device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    const uint64_t tableVAs[]={bufferTable.gpuAddress,textureTable.gpuAddress+textureMember,samplerTable.gpuAddress};
    if(!pipeline || !input || !image || !point || !output) return 3;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue coverage={}; coverage.uint32=updates>1?45:9;
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
    for(unsigned i=0;i<3;i++) {
      const unsigned count=i==1?updates:1;
      if(annotation(tables[i],"metal.descriptorTable",i==2?2:1,0,count,24))return 6;
      for(unsigned slot=0;slot<count;slot++)
       if(annotation(tables[i],"metal.descriptorSlotEvent",slot*24,1,0,types[i]) ||
          annotation(tables[i],"metal.descriptorSlotEvent",slot*24,1,2,types[i]) ||
          annotation(tables[i],"metal.descriptorSlotBinding",slot*24,kinds[i],(uint64_t)(__bridge void *)sources[i],i==0?4:0)) return 6;
    }
    if(annotation(payload,"metal.descriptorTable",1,0,updates,24))return 11;
    for(unsigned slot=0;slot<updates;slot++)if(annotation(payload,"metal.descriptorSlotEvent",slot*24,1,0,4)) return 11;
    if(api)
    {
      RENDERDOC_AnnotationValue gpu={};gpu.uint32=1;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)textureTable,"metal.descriptorGPUWrites",eRENDERDOC_UInt32,0,&gpu))return 12;
    }
    if(annotation(ueTable,"metal.descriptorTable",1,0,3,24))return 27;
    id<MTLBuffer> ueSources[]={payload,ueIndices,textureTable};
    for(unsigned i=0;i<3;i++)
      if(annotation(ueTable,"metal.descriptorSlotEvent",i*24,1,0,i==2?1:0) ||
         annotation(ueTable,"metal.descriptorSlotEvent",i*24,1,2,i==2?1:0) ||
         annotation(ueTable,"metal.descriptorSlotBinding",i*24,0,(uint64_t)(__bridge void *)ueSources[i],0))return 28;
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    id<MTLCommandQueue> queue=[device newCommandQueue];
    for(int capture=0;capture<(api?2:1);capture++)
    {
      id<MTLTexture> nextImage=capture==0?other:image;
      payloadBytes[1]=capture==0?otherID:textureBytes[1];
      for(unsigned slot=0;slot<updates;slot++) {
       memcpy((char *)payload.contents+slot*24,payloadBytes,24);
       if(annotation(payload,"metal.descriptorSlotEvent",slot*24,1,2,4) ||
          annotation(payload,"metal.descriptorSlotBinding",slot*24,1,(uint64_t)(__bridge void *)nextImage,0))return 13;
      }
      if(api) api->StartFrameCapture(nullptr,nullptr);
      uint64_t root[6]={};for(unsigned i=0;i<3;i++){root[i*2]=tableVAs[i];root[i*2+1]=0xabcdef0123456789ULL;}
      id<MTLCommandBuffer> command=[queue commandBuffer];[command enqueue];
      auto consume=[&]() {
        id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];
        if(annotation(compute,"metal.descriptorInlineLayout",0,0,3,16))return false;
        for(unsigned i=0;i<3;i++)
        {
          if(annotation(compute,"metal.descriptorInlineBinding",0,i,(uint64_t)(__bridge void *)tables[i],i==1?textureMember:0))return false;
          [compute useResource:tables[i] usage:MTLResourceUsageRead];
        }
        [compute setBytes:root length:sizeof(root) atIndex:0];[compute setBuffer:output offset:0 atIndex:1];
        [compute useResource:input usage:MTLResourceUsageRead];[compute useResource:image usage:MTLResourceUsageRead];
        [compute useResource:other usage:MTLResourceUsageRead];
        [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];return true;
      };
      if(!consume())return 14;
      id<MTLComputeCommandEncoder> producer=[command computeCommandEncoder];[producer setComputePipelineState:copyPipeline];
      const uint64_t copyRoot[]={ueConstants.gpuAddress,ueSampling.gpuAddress};
      if(annotation(producer,"metal.descriptorInlineLayout",0,2,2,8) ||
         annotation(producer,"metal.descriptorInlineBinding",2,0,(uint64_t)(__bridge void *)ueConstants,0) ||
         annotation(producer,"metal.descriptorInlineBinding",2,1,(uint64_t)(__bridge void *)ueSampling,0))return 15;
      [producer setBuffer:ueTable offset:0 atIndex:0];[producer setBytes:copyRoot length:16 atIndex:2];
      for(id<MTLBuffer> source in @[payload,ueIndices,ueConstants,ueSampling])[producer useResource:source usage:MTLResourceUsageRead];
      [producer useResource:textureTable usage:MTLResourceUsageRead|MTLResourceUsageWrite];
      [producer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      for(unsigned slot=0;slot<updates;slot++) {
      if(annotation(textureTable,"metal.descriptorSlotProducer",slot*24,(uint64_t)(__bridge void *)producer,(uint64_t)(__bridge void *)payload,slot*24))return 16;
      if(api)
      {
        RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=slot*24;memcpy(&value.vector.uint64[1],payloadBytes,24);
        if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)textureTable,"metal.descriptorSlotGPUValue",eRENDERDOC_UInt64,4,&value))return 17;
      }
      if(annotation(textureTable,"metal.descriptorSlotBinding",slot*24,1,(uint64_t)(__bridge void *)nextImage,0))return 18;
      }
      [producer endEncoding];if(!consume())return 19;
      id<CAMetalDrawable> drawable=[layer nextDrawable];MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0.125,0.25,0.5,1);
      id<MTLRenderCommandEncoder> render=[command renderCommandEncoderWithDescriptor:pass];
      [render setRenderPipelineState:renderPipeline];
      for(unsigned stage=1;stage<=2;stage++)
      {
        if(annotation(render,"metal.descriptorInlineLayout",stage,0,3,16)){[render endEncoding];return 21;}
        for(unsigned i=0;i<3;i++)
          if(annotation(render,"metal.descriptorInlineBinding",uint64_t(stage)<<32,i,(uint64_t)(__bridge void *)tables[i],i==1?textureMember:0)){[render endEncoding];return 22;}
      }
      [render setVertexBytes:root length:sizeof(root) atIndex:0];
      [render setFragmentBytes:root length:sizeof(root) atIndex:0];
      for(id<MTLBuffer> table:tables)[render useResource:table usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
      [render useResource:input usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
      [render useResource:image usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      [render useResource:other usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      [render drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[render endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
      const uint32_t *words=(const uint32_t *)output.contents;
      if(command.error || words[0]!=(capture==0?186U:122U) || words[1]!=0xdeadbeefU)return 9;
      uint8_t pixels[16]={};[drawable.texture getBytes:pixels bytesPerRow:8 fromRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0];
      for(unsigned i=0;i<16;i+=4)if(pixels[i]!=128 || pixels[i+1]!=64 || pixels[i+2]!=(capture==0?186:122) || pixels[i+3]!=255)return 23;
      if(api && !api->EndFrameCapture(nullptr,nullptr))return 10;
    }
    printf("actual UE shader-to-graphics native PASS results=122/186/122 VA_A=%llu VA_B=%llu VA_TABLE=%llu TEX=%llu SAMP=%llu TEX_B=%llu captures=%d\n",(unsigned long long)bufferBytes[0],(unsigned long long)tableVAs[1],(unsigned long long)tableVAs[0],(unsigned long long)textureBytes[1],(unsigned long long)samplerBytes[0],(unsigned long long)otherID,api?2:0);
  }
  return 0;
}
