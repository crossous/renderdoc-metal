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
  id<MTLDevice> device=MTLCreateSystemDefaultDevice(); NSError *error=nil;
  const bool array=getenv("RENDERDOC_METAL_FRAME_FAMILY_ARRAY")!=nullptr;
  const bool wide=getenv("RENDERDOC_METAL_FRAME_FAMILY_FLOAT32")!=nullptr;
  const bool view=false;
  const bool integer=getenv("RENDERDOC_METAL_FRAME_FAMILY_UINT")!=nullptr;
  const bool unorm8=getenv("RENDERDOC_METAL_FRAME_FAMILY_UNORM8")!=nullptr;
  const bool packed10=getenv("RENDERDOC_METAL_FRAME_FAMILY_PACKED10")!=nullptr;
  const bool atomic=getenv("RENDERDOC_METAL_FRAME_FAMILY_ATOMIC")!=nullptr;
  const bool work=getenv("RENDERDOC_METAL_FRAME_FAMILY_WORK")!=nullptr;
  const unsigned dispatchCount=getenv("RENDERDOC_METAL_FRAME_FAMILY_DISPATCHES")?unsigned(strtoul(getenv("RENDERDOC_METAL_FRAME_FAMILY_DISPATCHES"),nullptr,10)):3;
  if(dispatchCount<3||dispatchCount>128)return 18;
  const bool extra=integer||unorm8||packed10;
  if(unsigned(integer)+unsigned(unorm8)+unsigned(packed10)>1 || (atomic&&!integer))return 16;
  const bool actual=getenv("RENDERDOC_METAL_FRAME_FAMILY_ACTUAL")!=nullptr;
  const bool twoD=getenv("RENDERDOC_METAL_FRAME_FAMILY_2D")!=nullptr;
  if(twoD && array)return 15;
  const NSUInteger width=integer?(twoD?128:1):unorm8?512:packed10?1:actual?64:3;
  const NSUInteger height=integer||packed10?1:unorm8?512:actual?64:5;
  const NSUInteger layers=twoD||packed10?1:integer?3:actual?64:2;
  NSString *source=@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct BufferEntry { const device uint *value [[id(0)]]; ulong zero [[id(1)]]; ulong metadata [[id(2)]]; };
struct Entry { ulong zero [[id(0)]]; texture3d<float,access::read_write> image [[id(1)]]; ulong metadata [[id(2)]]; };
kernel void read_color(const device ulong *root [[buffer(0)]], device uint *result [[buffer(1)]]) {
 const device Entry *table=reinterpret_cast<const device Entry *>(root[0]);
 float4 c=table[0].image.read(uint3(2,4,1));
 result[0]=uint(c.r*256); result[1]=uint(c.g*256); result[2]=uint(c.b*256);
 const device BufferEntry *input=reinterpret_cast<const device BufferEntry *>(root[2]);
 result[3]=table[0].metadata==0x4141414141414141ul&&input[0].value[0]==13&&input[0].metadata==0x5151515151515151ul?0xdeadbeef:0xbad;
}
kernel void write_color(const device ulong *root [[buffer(0)]]) {
 const device Entry *table=reinterpret_cast<const device Entry *>(root[0]);
 const float4 value=root[1]?float4(0.75,0.25,0.5,1):float4(0.25,0.5,0.75,1);
 for(uint z=0;z<2;z++)for(uint y=0;y<5;y++)for(uint x=0;x<3;x++)
  table[0].image.write(value,uint3(x,y,z));
}
)MSL";
  if(array){source=[source stringByReplacingOccurrencesOfString:@"texture3d<float,access::read_write>" withString:@"texture2d_array<float,access::read_write>"];
   source=[source stringByReplacingOccurrencesOfString:@".read(uint3(2,4,1))" withString:@".read(uint2(2,4),1)"];
   source=[source stringByReplacingOccurrencesOfString:@".write(value,uint3(x,y,z))" withString:@".write(value,uint2(x,y),z)"];}

  if(twoD){source=[source stringByReplacingOccurrencesOfString:@"texture3d<float,access::read_write>" withString:@"texture2d<float,access::read_write>"];
   source=[source stringByReplacingOccurrencesOfString:@".read(uint3(2,4,1))" withString:@".read(uint2(2,4))"];
   source=[source stringByReplacingOccurrencesOfString:@".write(value,uint3(x,y,z))" withString:@".write(value,uint2(x,y))"];}
  if(integer){source=[source stringByReplacingOccurrencesOfString:@"<float,access::read_write>" withString:@"<uint,access::read_write>"];
   source=[source stringByReplacingOccurrencesOfString:@"float4 c=" withString:@"uint4 c="];
   source=[source stringByReplacingOccurrencesOfString:@"const float4 value=root[1]?float4(0.75,0.25,0.5,1):float4(0.25,0.5,0.75,1);" withString:@"const uint4 value=root[1]?uint4(192,64,128,1):uint4(64,128,192,1);"];
   source=[source stringByReplacingOccurrencesOfString:@"uint(c.r*256)" withString:@"c.r"];
   source=[source stringByReplacingOccurrencesOfString:@"uint(c.g*256)" withString:@"c.g"];
   source=[source stringByReplacingOccurrencesOfString:@"uint(c.b*256)" withString:@"c.b"];}
  else {source=[source stringByReplacingOccurrencesOfString:@"uint(c.r*256)" withString:@"uint(c.r*256+0.5)"];
   source=[source stringByReplacingOccurrencesOfString:@"uint(c.g*256)" withString:@"uint(c.g*256+0.5)"];
   source=[source stringByReplacingOccurrencesOfString:@"uint(c.b*256)" withString:@"uint(c.b*256+0.5)"];}
  if(atomic)source=[source stringByReplacingOccurrencesOfString:@"table[0].image.write(value,uint2(x,y));"
    withString:@"{ table[0].image.atomic_exchange(uint2(x,y),uint4(0)); table[0].image.atomic_fetch_add(uint2(x,y),value); }"];
  source=[source stringByReplacingOccurrencesOfString:@".read(uint2(2,4),1)" withString:[NSString stringWithFormat:@".read(uint2(%lu,%lu),%lu)",(unsigned long)width-1,(unsigned long)height-1,(unsigned long)layers-1]];
  source=[source stringByReplacingOccurrencesOfString:@"uint2(2,4)" withString:[NSString stringWithFormat:@"uint2(%lu,%lu)",(unsigned long)width-1,(unsigned long)height-1]];
  source=[source stringByReplacingOccurrencesOfString:@"uint3(2,4,1)" withString:[NSString stringWithFormat:@"uint3(%lu,%lu,%lu)",(unsigned long)width-1,(unsigned long)height-1,(unsigned long)layers-1]];
  source=[source stringByReplacingOccurrencesOfString:@"for(uint z=0;z<2;z++)for(uint y=0;y<5;y++)for(uint x=0;x<3;x++)"
    withString:[NSString stringWithFormat:@"for(uint z=0;z<%lu;z++)for(uint y=0;y<%lu;y++)for(uint x=0;x<%lu;x++)",(unsigned long)layers,(unsigned long)height,(unsigned long)width]];
  if(work) {
   source=[source stringByReplacingOccurrencesOfString:@"kernel void write_color(const device ulong *root [[buffer(0)]])"
     withString:@"kernel void write_color(const device ulong *root [[buffer(0)]],uint3 position [[thread_position_in_grid]])"];
   source=[source stringByReplacingOccurrencesOfString:[NSString stringWithFormat:@"for(uint z=0;z<%lu;z++)for(uint y=0;y<%lu;y++)for(uint x=0;x<%lu;x++)",(unsigned long)layers,(unsigned long)height,(unsigned long)width]
     withString:[NSString stringWithFormat:@"const uint x=position.x,y=position.y,z=position.z; if(x>=%lu||y>=%lu||z>=%lu)return;",(unsigned long)width,(unsigned long)height,(unsigned long)layers]];
  }
  id<MTLLibrary> library=[device newLibraryWithSource:source options:nil error:&error];
  id<MTLComputePipelineState> pipeline=library?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"read_color"] error:&error]:nil;
  id<MTLComputePipelineState> writer=library?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"write_color"] error:&error]:nil;
  if(!pipeline||!writer){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
  auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:integer?MTLPixelFormatR32Uint:unorm8?MTLPixelFormatR8Unorm:packed10?MTLPixelFormatRGB10A2Unorm:array?MTLPixelFormatR32Float:wide?MTLPixelFormatRGBA32Float:MTLPixelFormatRGBA16Float width:width height:height mipmapped:NO];
  d.textureType=twoD?MTLTextureType2D:array?MTLTextureType2DArray:MTLTextureType3D;d.arrayLength=array?layers:1;d.depth=array||twoD?1:layers;
  d.resourceOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
  d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite|((!array&&!wide&&!integer&&!packed10)?MTLTextureUsageRenderTarget:0);
  if(atomic) {if(@available(macOS 14.0,*))d.usage|=MTLTextureUsageShaderAtomic;else return 17;}
  MTLTextureDescriptor *rw=[d copy];rw.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
  const auto layout=[device heapTextureSizeAndAlignWithDescriptor:d];
  const auto rwLayout=[device heapTextureSizeAndAlignWithDescriptor:rw];
  const NSUInteger rwOffset=rwLayout.align?(layout.size+rwLayout.align-1)/rwLayout.align*rwLayout.align:0;
  const NSUInteger stride=(rwOffset+rwLayout.size+layout.align-1)/layout.align*layout.align;
  if(!layout.size||!layout.align||!rwLayout.size||!rwLayout.align||stride*2>(actual||twoD?16U*1024*1024:1024*1024))return 3;
  auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;
  hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=MAX(NSUInteger(4096),stride*2);
  id<MTLHeap> heap=[device newHeapWithDescriptor:hd];id<MTLCommandQueue> queue=[device newCommandQueue];
  const uint64_t zero[3]={};id<MTLBuffer> table=[device newBufferWithBytes:zero length:24 options:MTLResourceStorageModeShared];
  id<MTLBuffer> output=[device newBufferWithLength:16 options:MTLResourceStorageModeShared];
  table.label=@"Frame color table";output.label=@"Frame color output";
  const uint32_t value=13;id<MTLBuffer> input=[device newBufferWithBytes:&value length:4 options:MTLResourceStorageModeShared];
  const uint64_t inputPacket[]={input.gpuAddress,0,0x5151515151515151ULL};
  id<MTLBuffer> inputTable=[device newBufferWithBytes:inputPacket length:24 options:MTLResourceStorageModeShared];
  CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
  layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
  RENDERDOC_API_1_7_0 *api=nullptr;
  if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH")) {
   auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
   if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 4;
   api->SetCaptureFilePathTemplate(path);RENDERDOC_AnnotationValue v={};v.uint32=work||dispatchCount>4?46:extra?44:actual||twoD?43:39;
   if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 5;
  }
  auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t e) {
   if(!api)return uint32_t(0);RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=e;
   return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
  };
  if(annotation(table,"metal.descriptorTable",1,0,1,24)||annotation(inputTable,"metal.descriptorTable",1,0,1,24)||
     annotation(inputTable,"metal.descriptorSlotEvent",0,1,0,0)||annotation(inputTable,"metal.descriptorSlotEvent",0,1,2,0)||
     annotation(inputTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)input,0))return 6;
  uint64_t capturedIDs[2]={};
  for(unsigned capture=0;capture<(api?2u:1u);capture++) {
   memset(output.contents,0,16);memset(table.contents,0,24);
   if(api)api->StartFrameCapture(nullptr,nullptr);
   id<MTLTexture> image=[heap newTextureWithDescriptor:d offset:capture*stride];if(!image)return 7;
   // Exact UE usage (read/write, no render target) also has a legal CPU birth.
   id<MTLTexture> rwImage=[heap newTextureWithDescriptor:rw offset:capture*stride+rwOffset];if(!rwImage)return 8;
   id<MTLTexture> sampled=view?[image newTextureViewWithPixelFormat:d.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)]:image;
   if(!sampled)return 9;capturedIDs[capture]=sampled.gpuResourceID._impl;
   uint64_t packet[3]={0,capturedIDs[capture],0x4141414141414141ULL};memcpy(table.contents,packet,24);
   if(annotation(table,"metal.descriptorSlotEvent",0,capture+1,0,5)||annotation(table,"metal.descriptorSlotEvent",0,capture+1,2,5)||
      annotation(table,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)sampled,0))return 10;
   id<MTLCommandBuffer> command=[queue commandBuffer];
   auto encode=[&](id<MTLComputePipelineState> state,uint64_t phase,bool read) {
    id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:state];
    if(annotation(compute,"metal.descriptorInlineLayout",0,0,2,16)||
       annotation(compute,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)table,0)||
       annotation(compute,"metal.descriptorInlineBinding",0,1,(uint64_t)(__bridge void *)inputTable,0))return false;
    const uint64_t root[]={table.gpuAddress,phase,inputTable.gpuAddress,0};[compute setBytes:root length:32 atIndex:0];
    if(read)[compute setBuffer:output offset:0 atIndex:1];
    [compute useResource:input usage:MTLResourceUsageRead];[compute useResource:inputTable usage:MTLResourceUsageRead];
    [compute useResource:table usage:MTLResourceUsageRead];[compute useResource:sampled usage:read?MTLResourceUsageRead:MTLResourceUsageWrite];
    const NSUInteger tx=twoD?8:4,ty=twoD?(integer?1:8):4,tz=twoD?1:4;
    const MTLSize threads=work&&!read?MTLSizeMake(tx,ty,tz):MTLSizeMake(1,1,1);
    const MTLSize groups=work&&!read?MTLSizeMake((width+tx-1)/tx,(height+ty-1)/ty,(layers+tz-1)/tz):MTLSizeMake(1,1,1);
    [compute dispatchThreadgroups:groups threadsPerThreadgroup:threads];[compute endEncoding];return true;
   };
   if(!encode(writer,0,false))return 14;
   for(unsigned dispatch=1;dispatch+1<dispatchCount;dispatch++)if(!encode(pipeline,0,true))return 14;
   if(!encode(writer,1,false))return 14;
   id<CAMetalDrawable> drawable=[layer nextDrawable];auto present=[MTLRenderPassDescriptor renderPassDescriptor];
   present.colorAttachments[0].texture=drawable.texture;present.colorAttachments[0].loadAction=MTLLoadActionClear;present.colorAttachments[0].storeAction=MTLStoreActionStore;
   [[command renderCommandEncoderWithDescriptor:present] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
   const uint32_t *result=(const uint32_t *)output.contents;
   if(command.status!=MTLCommandBufferStatusCompleted||result[0]!=64||result[1]!=((array||integer||unorm8)?0:128)||result[2]!=((array||integer||unorm8)?0:192)||result[3]!=0xdeadbeef)return 11;
   if(api&&!api->EndFrameCapture(nullptr,nullptr))return 12;
   printf("Native frame family PASS capture=%u gpu=%u/%u/%u id=%llu\n",capture,result[0],result[1],result[2],(unsigned long long)capturedIDs[capture]);
   if(annotation(table,"metal.descriptorSlotEvent",0,capture+1,1,5))return 13;
   memset(table.contents,0,24);sampled=nil;image=nil;rwImage=nil;
  }
  printf("FRAME_IDS=%llu,%llu captures=%u\n",(unsigned long long)capturedIDs[0],(unsigned long long)capturedIDs[1],api?2:1);
 }
 return 0;
}
