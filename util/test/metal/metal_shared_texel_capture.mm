// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>
#include <algorithm>
#include <string>
#include <utility>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv)
{
  if(argc!=7)return 1;
  const bool source=!strcmp(argv[2],"source"),volume=atoi(argv[5])!=0;
  const unsigned offset=atoi(argv[6]);if(!offset || offset>128)return 1;
  const bool activeTable=getenv("RENDERDOC_METAL_TEST_ACTIVE_TABLE")!=nullptr;
  const bool groups=getenv("RENDERDOC_METAL_TEST_GROUPS")!=nullptr;
  const unsigned width=volume?(groups?8:5):(groups?64:67),height=volume?(groups?6:7):1,
      depth=volume?(groups?4:3):1,count=width*height*depth;
  const unsigned capacity=512;
  const bool dynamic=getenv("RENDERDOC_METAL_TEST_DYNAMIC_MEMORY")!=nullptr;
  @autoreleasepool
  {
    auto device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    if(![device supportsFamily:MTLGPUFamilyApple4] && ![device supportsFamily:MTLGPUFamilyMac2])return 2;
    auto library=source?[device newLibraryWithSource:[NSString stringWithContentsOfFile:
        [NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&error] options:nil error:&error]:
        [device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[1]]] error:&error];
    auto pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:
        activeTable?(volume?@"linear_uint_table":@"linear_half_table"):(volume?@"linear_uint":@"linear_half")] error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.description.UTF8String);return 3;}
    std::vector<uint32_t> input(capacity),expected(capacity+offset+16,0x6a6a6a6aU);
    for(unsigned i=0;i<capacity;i++)input[i]=i*23+7;
    const uint32_t parameters[]={width,height,depth,offset};
    auto inputs=[device newBufferWithBytes:input.data() length:input.size()*4 options:MTLResourceStorageModeShared];
    auto outputs=[device newBufferWithBytes:expected.data() length:expected.size()*4 options:MTLResourceStorageModeShared];
    auto params=[device newBufferWithBytes:parameters length:sizeof(parameters) options:MTLResourceStorageModeShared];
    outputs.label=@"Boundary QA output";
    const uint64_t packet[]={inputs.gpuAddress,0,inputs.length};
    auto table=[device newBufferWithBytes:packet length:sizeof(packet) options:MTLResourceStorageModeShared];
    std::vector<uint32_t> pixels2D(35),pixels3D(30);const unsigned texelWidth=volume?13:17, texelOffset=volume?512:256;
    std::vector<uint32_t> texels(texelWidth);
    std::vector<__fp16> halfTexels(texelWidth);
    for(unsigned i=0;i<35;i++)pixels2D[i]=0x12340000+i*7919;
    for(unsigned i=0;i<30;i++)pixels3D[i]=0x56780000+i*131;
    for(unsigned i=0;i<texelWidth;i++){texels[i]=i*29+5;halfTexels[i]=float(texels[i])/32.0f;}
    auto d2=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Uint width:5 height:7 mipmapped:NO];
    d2.storageMode=MTLStorageModeShared;d2.usage=MTLTextureUsageShaderRead;
    auto image2D=[device newTextureWithDescriptor:d2];image2D.label=@"Boundary QA 2D";
    [image2D replaceRegion:MTLRegionMake2D(0,0,5,7) mipmapLevel:0 withBytes:pixels2D.data() bytesPerRow:20];
    auto d3=[MTLTextureDescriptor new];d3.textureType=MTLTextureType3D;d3.pixelFormat=MTLPixelFormatR32Uint;
    d3.width=3;d3.height=5;d3.depth=2;d3.storageMode=MTLStorageModeShared;d3.usage=MTLTextureUsageShaderRead;
    auto image3D=[device newTextureWithDescriptor:d3];image3D.label=@"Boundary QA 3D";
    [image3D replaceRegion:MTLRegionMake3D(0,0,0,3,5,2) mipmapLevel:0 slice:0 withBytes:pixels3D.data() bytesPerRow:12 bytesPerImage:60];
    auto parent=[device newBufferWithLength:1024 options:MTLResourceStorageModeShared];parent.label=@"Boundary QA texel parent";
    memset(parent.contents,0xcc,1024);memcpy((char *)parent.contents+texelOffset,volume?(const void*)texels.data():(const void*)halfTexels.data(),texelWidth*(volume?4:2));
    auto td=[MTLTextureDescriptor new];td.textureType=MTLTextureType2D;td.pixelFormat=volume?MTLPixelFormatR32Uint:MTLPixelFormatR16Float;
    td.width=texelWidth;td.height=1;td.depth=1;td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;
    auto view=[parent newTextureWithDescriptor:td offset:texelOffset bytesPerRow:256];view.label=@"Boundary QA texel";
    if(!image2D||!image3D||!view)return 11;
    for(auto pair: {std::make_pair("2D",std::make_pair((const void *)pixels2D.data(),size_t(140))),
                   std::make_pair("3D",std::make_pair((const void *)pixels3D.data(),size_t(120))),
                   std::make_pair("texel",std::make_pair(volume?(const void *)texels.data():(const void *)halfTexels.data(),size_t(texelWidth*(volume?4:2))))}) {
      std::string path=std::string(argv[4])+"."+pair.first;FILE *f=fopen(path.c_str(),"wbx");if(!f)return 12;
      bool good=fwrite(pair.second.first,1,pair.second.second,f)==pair.second.second;fclose(f);if(!good)return 12;
    }
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");RENDERDOC_API_1_7_0 *api=nullptr;
    if(get && get(eRENDERDOC_API_Version_1_7_0,(void **)&api)!=1)return 4;
    if(api)
    {
      RENDERDOC_AnnotationValue coverage={};coverage.uint32=65;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage))return 4;
      auto annotate=[&](const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
        RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=a;value.vector.uint64[1]=b;
        value.vector.uint64[2]=c;value.vector.uint64[3]=d;
        return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)table,key,eRENDERDOC_UInt64,4,&value);
      };
      if(annotate("metal.descriptorTable",2,0,1,24) || annotate("metal.descriptorSlotEvent",0,1,0,4) ||
         annotate("metal.descriptorSlotEvent",0,1,2,4) ||
         annotate("metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)inputs,0))return 4;
      api->SetCaptureFilePathTemplate(argv[3]);api->StartFrameCapture(nullptr,nullptr);
    }
    auto queue=[device newCommandQueue];auto command=[queue commandBuffer];
    const unsigned encoders=getenv("RENDERDOC_METAL_TEST_TWO_ENCODERS")?2:1;
    for(unsigned pass=0;pass<encoders;pass++)
    {
    auto encoder=[command computeCommandEncoder];
    [encoder useResource:inputs usage:MTLResourceUsageRead];
    id<MTLResource> observed[]={image2D,image3D,view,parent}; [encoder useResources:observed count:4 usage:MTLResourceUsageRead];
    [encoder setComputePipelineState:pipeline];[encoder setBuffer:inputs offset:0 atIndex:4];
    [encoder setBuffer:outputs offset:0 atIndex:9];[encoder setBuffer:params offset:0 atIndex:2];[encoder setTexture:view atIndex:6];
    [encoder setBuffer:table offset:0 atIndex:30];
    if(dynamic)
    {
      [encoder setThreadgroupMemoryLength:256 atIndex:3];
      [encoder setThreadgroupMemoryLength:128 atIndex:3];
      [encoder setThreadgroupMemoryLength:128 atIndex:7];
    }
    const MTLSize localSize=volume?MTLSizeMake(4,3,2):MTLSizeMake(32,1,1);
    if(groups)[encoder dispatchThreadgroups:MTLSizeMake(width/localSize.width,
        height/localSize.height,depth/localSize.depth) threadsPerThreadgroup:localSize];
    else [encoder dispatchThreads:MTLSizeMake(width,height,depth) threadsPerThreadgroup:localSize];
    [encoder endEncoding];
    }
    [command commit];[command waitUntilCompleted];
    if(command.status!=MTLCommandBufferStatusCompleted || command.error)return 5;
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 6;
    for(unsigned z=0;z<depth;z++)for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)
    {
      const unsigned i=x+width*(y+height*z);
      if(!dynamic){expected[offset+i]=input[i]*7+i*11+101+texels[i%texelWidth];continue;}
      const unsigned gx=volume?4:32,gy=volume?3:1,gz=volume?2:1;
      const unsigned bx=x/gx*gx,by=y/gy*gy,bz=z/gz*gz;
      const unsigned ax=std::min(gx,width-bx),ay=std::min(gy,height-by),az=std::min(gz,depth-bz);
      const unsigned lane=(x-bx)+ax*((y-by)+ay*(z-bz));
      const unsigned next=(lane+1)%(ax*ay*az);
      const unsigned neighbour=(bx+next%ax)+width*((by+next/ax%ay)+height*(bz+next/(ax*ay)));
      const unsigned base=bx+width*(by+height*bz);
      expected[offset+i]=input[neighbour]*7+i*11+input[i]+101+input[base]%13;
    }
    if(memcmp(outputs.contents,expected.data(),expected.size()*4))return 7;
    FILE *raw=fopen(argv[4],"wb");if(!raw)return 8;
    bool saved=fwrite(outputs.contents,4,expected.size(),raw)==expected.size();fclose(raw);if(!saved)return 8;
    printf("Native linear texel full output/padding MATCH: grid=%u,%u,%u; nonuniform edge; source=%d; dynamic_memory=%d static_memory=%lu max_memory=%lu; device=%s\n",width,height,depth,source,dynamic,(unsigned long)pipeline.staticThreadgroupMemoryLength,(unsigned long)device.maxThreadgroupMemoryLength,device.name.UTF8String);
  }
}
