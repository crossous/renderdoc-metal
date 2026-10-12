// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>
#include <algorithm>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc, char **argv)
{
  if(argc != 6) return 1;
  @autoreleasepool {
    auto device = MTLCreateSystemDefaultDevice(); NSError *error = nil;
    bool source = !strcmp(argv[2], "source"), half = atoi(argv[5]) != 0;
    auto library = source ? [device newLibraryWithSource:[NSString stringWithContentsOfFile:
        [NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&error]
        options:nil error:&error] : [device newLibraryWithURL:[NSURL fileURLWithPath:
        [NSString stringWithUTF8String:argv[1]]] error:&error];
    auto pipeline = [device newComputePipelineStateWithFunction:
        [library newFunctionWithName:@"direct_sampler_contract"] error:&error];
    if(!pipeline) { fprintf(stderr, "%s\n", error.description.UTF8String); return 2; }
    auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
        half ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA8Unorm width:4 height:2 mipmapped:YES];
    desc.storageMode = MTLStorageModeShared; desc.usage = MTLTextureUsageShaderRead;
    auto texture = [device newTextureWithDescriptor:desc];
    for(unsigned mip = 0; mip < 3; mip++) {
      unsigned width = std::max(1U, 4U >> mip), height = std::max(1U, 2U >> mip);
      std::vector<uint8_t> bytes(width * height * 4);
      std::vector<__fp16> halves(width * height * 4);
      for(unsigned i = 0; i < width * height * 4; i++) {
        bytes[i] = uint8_t(16 + mip * 64 + i * 3);
        halves[i] = float(mip * 16 + i) / 32.0f;
      }
      [texture replaceRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:mip
          withBytes:half ? (const void *)halves.data() : (const void *)bytes.data()
          bytesPerRow:width * (half ? 8 : 4)];
    }
    std::vector<uint32_t> seed(144, 0x6a6a6a6aU);
    auto output = [device newBufferWithBytes:seed.data() length:seed.size()*4 options:MTLResourceStorageModeShared];
    output.label = @"Direct sampler contract output";
    uint64_t packet[] = {output.gpuAddress, 0, output.length};
    auto table = [device newBufferWithBytes:packet length:24 options:MTLResourceStorageModeShared];
    auto samplerDesc = [MTLSamplerDescriptor new];
    samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDesc.mipFilter = MTLSamplerMipFilterNearest;
    auto nearest = [device newSamplerStateWithDescriptor:samplerDesc];
    samplerDesc.minFilter = samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
    auto linear = [device newSamplerStateWithDescriptor:samplerDesc];
    id<MTLBuffer> params[4];
    for(unsigned pass = 0; pass < 4; pass++) {
      uint32_t values[] = {2 + pass * 8, pass >= 2 ? 1U : 0U};
      params[pass] = [device newBufferWithBytes:values length:8 options:MTLResourceStorageModeShared];
    }
    RENDERDOC_API_1_7_0 *api = nullptr;
    auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(get && get(eRENDERDOC_API_Version_1_7_0, (void **)&api) != 1) return 3;
    if(api) {
      RENDERDOC_AnnotationValue coverage = {}; coverage.uint32 = 65;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,
          "metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage)) return 3;
      auto annotate = [&](const char *name, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
        RENDERDOC_AnnotationValue value = {}; value.vector.uint64[0]=a; value.vector.uint64[1]=b;
        value.vector.uint64[2]=c; value.vector.uint64[3]=d;
        return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)table,name,eRENDERDOC_UInt64,4,&value);
      };
      if(annotate("metal.descriptorTable",2,0,1,24) ||
         annotate("metal.descriptorSlotEvent",0,1,0,4) ||
         annotate("metal.descriptorSlotEvent",0,1,2,4) ||
         annotate("metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)output,0)) return 3;
      api->SetCaptureFilePathTemplate(argv[3]); api->StartFrameCapture(nullptr,nullptr);
    }
    auto queue = [device newCommandQueue]; auto command = [queue commandBuffer];
    auto encoder = [command computeCommandEncoder];
    [encoder setComputePipelineState:pipeline]; [encoder setTexture:texture atIndex:6];
    [encoder setBuffer:output offset:0 atIndex:9]; [encoder setBuffer:table offset:0 atIndex:30];
    for(unsigned pass = 0; pass < 4; pass++) {
      if(pass == 0) { [encoder setSamplerState:nearest atIndex:3]; [encoder setSamplerState:nil atIndex:1]; }
      if(pass == 1) { id<MTLSamplerState> states[] = {nil, linear};
        [encoder setSamplerStates:states withRange:NSMakeRange(2,2)]; }
      if(pass == 2) [encoder setSamplerState:nearest lodMinClamp:1 lodMaxClamp:1 atIndex:3];
      if(pass == 3) { id<MTLSamplerState> states[] = {linear, nil}; float low[] = {1,0}, high[] = {1,0};
        [encoder setSamplerStates:states lodMinClamps:low lodMaxClamps:high withRange:NSMakeRange(3,2)]; }
      [encoder setBuffer:params[pass] offset:0 atIndex:2];
      [encoder dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(8,1,1)];
    }
    [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
    if(command.error || command.status != MTLCommandBufferStatusCompleted) return 4;
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 5;
    if(!memcmp(output.contents, seed.data(), seed.size()*4)) return 6;
    if(memcmp(output.contents,seed.data(),32) || memcmp((char *)output.contents+544,(char *)seed.data()+544,32)) return 6;
    FILE *file = fopen(argv[4],"wbx"); if(!file) return 7;
    bool saved = fwrite(output.contents,1,576,file)==576; fclose(file);
    if(!saved) return 7;
    printf("Native direct sampler complete 576B output/padding; four binding APIs; format=%s\n",half?"RGBA16Float":"RGBA8Unorm");
  }
}
