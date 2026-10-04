// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc, char **argv) {
  if(argc != 2) return 2;
  @autoreleasepool {
    auto device = MTLCreateSystemDefaultDevice(); NSError *error = nil;
    auto library = [device newLibraryWithFile:[NSString stringWithUTF8String:argv[1]] error:&error];
    id<MTLRenderPipelineState> pipelines[2];
    for(unsigned i = 0; i < 2; i++) {
      auto desc = [MTLMeshRenderPipelineDescriptor new];
      desc.meshFunction = [library newFunctionWithName:i ? @"mesh_payload" : @"mesh_only"];
      desc.fragmentFunction = [library newFunctionWithName:@"stage_fs"];
      desc.maxTotalThreadsPerMeshThreadgroup = 32;
      if(i) { desc.objectFunction = [library newFunctionWithName:@"object_main"]; desc.maxTotalThreadsPerObjectThreadgroup = 32; desc.payloadMemoryLength = 16; desc.maxTotalThreadgroupsPerMeshGrid = 1; }
      desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
      pipelines[i] = [device newRenderPipelineStateWithMeshDescriptor:desc options:MTLPipelineOptionArgumentInfo | MTLPipelineOptionBufferTypeInfo reflection:nullptr error:&error];
      if(!pipelines[i]) { fprintf(stderr,"%s\n",error.localizedDescription.UTF8String); return 3; }
    }
    auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:1 height:1 mipmapped:NO];
    desc.storageMode = MTLStorageModeShared; desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    id<MTLTexture> images[3]; const float initial[4] = {0.125,0.125,0.125,0.125};
    for(unsigned i = 0; i < 3; i++) { images[i] = [device newTextureWithDescriptor:desc]; images[i].label = [NSString stringWithFormat:@"Stage image %u",i]; [images[i] replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:initial bytesPerRow:16]; }
    uint32_t indices[2] = {0,1}; auto uniform = [device newBufferWithBytes:indices length:8 options:MTLResourceStorageModeShared];
    uint64_t entries[6] = {0,images[0].gpuResourceID._impl,0,0,images[1].gpuResourceID._impl,0};
    auto table = [device newBufferWithBytes:entries length:48 options:MTLResourceStorageModeShared];
    auto layer = [CAMetalLayer layer]; layer.device = device; layer.pixelFormat = MTLPixelFormatBGRA8Unorm; layer.drawableSize = CGSizeMake(2,2); layer.framebufferOnly = NO;
    auto queue = [device newCommandQueue]; RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH")) {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path); RENDERDOC_AnnotationValue version = {}; version.uint32 = 66;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&version)) return 5;
    }
    auto annotate = [&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api) return uint32_t(0); RENDERDOC_AnnotationValue value = {};
      value.vector.uint64[0]=a; value.vector.uint64[1]=b; value.vector.uint64[2]=c; value.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&value);
    };
    if(annotate(table,"metal.descriptorTable",1,0,2,24)) return 6;
    for(unsigned i = 0; i < 2; i++)
      if(annotate(table,"metal.descriptorSlotEvent",i*24,1,0,5) || annotate(table,"metal.descriptorSlotEvent",i*24,1,2,5) || annotate(table,"metal.descriptorSlotBinding",i*24,1,(uint64_t)(__bridge void *)images[i],0)) return 7;
    if(api) api->StartFrameCapture(nullptr,nullptr);
    auto drawable = [layer nextDrawable]; auto command = [queue commandBuffer];
    for(unsigned i = 0; i < 2; i++) {
      auto pass = [MTLRenderPassDescriptor renderPassDescriptor]; pass.colorAttachments[0].texture = drawable.texture;
      pass.colorAttachments[0].loadAction = MTLLoadActionClear; pass.colorAttachments[0].storeAction = MTLStoreActionStore;
      auto encoder = [command renderCommandEncoderWithDescriptor:pass]; [encoder setRenderPipelineState:pipelines[i]];
      auto root = [&](unsigned stage,unsigned index) {
        if(annotate(encoder,"metal.descriptorInlineLayout",stage,2,1,8) || annotate(encoder,"metal.descriptorInlineBinding",uint64_t(stage)<<32 | 2,0,(uint64_t)(__bridge void *)uniform,index*4)) return false;
        uint64_t pointer = uniform.gpuAddress + index*4;
        if(stage == 4) [encoder setMeshBytes:&pointer length:8 atIndex:2]; else [encoder setObjectBytes:&pointer length:8 atIndex:2]; return true;
      };
      [encoder setMeshBuffer:table offset:0 atIndex:0]; if(!root(4,0)) return 8;
      if(i) { [encoder setObjectBuffer:table offset:0 atIndex:0]; if(!root(3,1)) return 8; }
      [encoder useResource:uniform usage:MTLResourceUsageRead]; for(auto image : images) [encoder useResource:image usage:MTLResourceUsageRead | MTLResourceUsageWrite];
      [encoder drawMeshThreadgroups:MTLSizeMake(1,1,1) threadsPerObjectThreadgroup:MTLSizeMake(1,1,1) threadsPerMeshThreadgroup:MTLSizeMake(3,1,1)]; [encoder endEncoding];
    }
    [command presentDrawable:drawable]; [command commit]; [command waitUntilCompleted];
    if(command.status != MTLCommandBufferStatusCompleted) return 9;
    for(unsigned i = 0; i < 3; i++) { float values[4]; [images[i] getBytes:values bytesPerRow:16 fromRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0]; for(float v : values) if(v != (i == 0 ? 0.375f : i == 1 ? 0.625f : 0.125f)) return 10; }
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 11;
    puts("PASS native mesh write, object write, payload + mesh read and unrelated resident texture");
  }
}
