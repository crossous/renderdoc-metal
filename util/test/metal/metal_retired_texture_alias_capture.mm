// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice(); NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Image { ulong zero [[id(0)]]; texture2d<float,access::read> image [[id(1)]]; ulong marker [[id(2)]]; };
struct Value { device const uint *value [[id(0)]]; ulong zero [[id(1)]]; ulong marker [[id(2)]]; };
kernel void read_image(const device ulong *root [[buffer(0)]], device uint *out [[buffer(1)]])
{ auto entry = reinterpret_cast<const device Image *>(root[0]);
  out[0] = uint(entry->image.read(uint2(0)).r * 255.0 + 0.5);
  out[1] = entry->marker == 0xabcdefUL ? 0xdeadbeefU : 0xbadU; }
kernel void write_value(const device ulong *root [[buffer(0)]], device uint *out [[buffer(1)]], device uint *target [[buffer(2)]])
{ auto entry = reinterpret_cast<const device Value *>(root[0]);
  out[0] = entry->value[0]; target[0] = out[0] + 17;
  out[1] = entry->marker == 0xabcdefUL ? 0xdeadbeefU : 0xbadU; }
kernel void read_value(const device ulong *root [[buffer(0)]], device uint *out [[buffer(1)]])
{ auto entry = reinterpret_cast<const device Value *>(root[0]); out[0] = entry->value[0];
  out[1] = entry->marker == 0xabcdefUL ? 0xdeadbeefU : 0xbadU; }
)MSL" options:nil error:&error];
    id<MTLComputePipelineState> pipelines[3];
    const char *names[] = {"read_image", "write_value", "read_value"};
    for(unsigned i=0;i<3;i++)
      pipelines[i] = [device newComputePipelineStateWithFunction:
          [library newFunctionWithName:[NSString stringWithUTF8String:names[i]]] error:&error];
    if(!pipelines[0] || !pipelines[1] || !pipelines[2]) return 2;
    MTLHeapDescriptor *hd = [MTLHeapDescriptor new]; hd.type=MTLHeapTypePlacement;
    hd.storageMode=MTLStorageModePrivate; hd.hazardTrackingMode=MTLHazardTrackingModeTracked; hd.size=65536;
    id<MTLHeap> heap = [device newHeapWithDescriptor:hd];
    auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    desc.storageMode=MTLStorageModePrivate; desc.hazardTrackingMode=MTLHazardTrackingModeTracked;
    desc.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
    id<MTLTexture> old = [heap newTextureWithDescriptor:desc offset:0];
    id<MTLCommandQueue> queue = [device newCommandQueue];
    MTLCommandBufferDescriptor *cbDesc = [MTLCommandBufferDescriptor new];
    cbDesc.retainedReferences = getenv("RENDERDOC_METAL_UNRETAINED_SUBMISSIONS") == nullptr;
    auto clear = [MTLRenderPassDescriptor renderPassDescriptor]; clear.colorAttachments[0].texture=old;
    clear.colorAttachments[0].loadAction=MTLLoadActionClear; clear.colorAttachments[0].storeAction=MTLStoreActionStore;
    clear.colorAttachments[0].clearColor=MTLClearColorMake(17.0/255.0,0,0,1);
    id<MTLCommandBuffer> init = [queue commandBuffer]; [[init renderCommandEncoderWithDescriptor:clear] endEncoding];
    [init commit]; [init waitUntilCompleted]; if(init.error) return 3;
    uint32_t sourceValue=34;
    id<MTLBuffer> source = [device newBufferWithBytes:&sourceValue length:4 options:MTLResourceStorageModeShared];
    id<MTLBuffer> tables[3];
    for(unsigned i=0;i<2;i++) tables[i]=[device newBufferWithLength:24 options:MTLResourceStorageModeShared];
    tables[2]=nil;
    id<MTLBuffer> output = [device newBufferWithLength:24 options:MTLResourceStorageModeShared];
    memset(output.contents,0,24);
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path); RENDERDOC_AnnotationValue v={};v.uint32=65;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,
          "metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v)) return 5;
    }
    auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api) return uint32_t(0); RENDERDOC_AnnotationValue v={};
      v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
    };
    auto table=[&](unsigned index,id resource,bool texture) {
      uint64_t bytes[]={texture?0:((id<MTLBuffer>)resource).gpuAddress,
          texture?((id<MTLTexture>)resource).gpuResourceID._impl:0,0xabcdef};
      memcpy(tables[index].contents,bytes,24);
      return !annotation(tables[index],"metal.descriptorTable",1,0,1,24) &&
          !annotation(tables[index],"metal.descriptorSlotEvent",0,1,0,texture?4:0) &&
          !annotation(tables[index],"metal.descriptorSlotEvent",0,1,2,texture?4:0) &&
          !annotation(tables[index],"metal.descriptorSlotBinding",0,texture?1:0,(uint64_t)(__bridge void *)resource,0);
    };
    id<MTLTexture> oldSource = getenv("RENDERDOC_METAL_TEXTURE_ALIAS_VIEW") ?
        [old newTextureViewWithPixelFormat:MTLPixelFormatRGBA8Unorm textureType:MTLTextureType2D
            levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)] : old;
    if(!table(0,oldSource,true) || !table(1,source,false)) return 6;
    auto encode=[&](id<MTLCommandBuffer> command,unsigned index,id resource,id<MTLBuffer> target) {
      id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
      [encoder setComputePipelineState:pipelines[index]];
      if(annotation(encoder,"metal.descriptorInlineLayout",0,0,1,8) ||
         annotation(encoder,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)tables[index],0)) return false;
      uint64_t root=tables[index].gpuAddress; [encoder setBytes:&root length:8 atIndex:0];
      [encoder setBuffer:output offset:index*8 atIndex:1];
      if(target) [encoder setBuffer:target offset:0 atIndex:2];
      [encoder useResource:tables[index] usage:MTLResourceUsageRead];
      [encoder useResource:resource usage:MTLResourceUsageRead];
      if(index==0 && resource!=old) [encoder useResource:old usage:MTLResourceUsageRead];
      [encoder dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      [encoder endEncoding]; return true;
    };
    if(api) api->StartFrameCapture(nullptr,nullptr);
    id<MTLCommandBuffer> first=[queue commandBufferWithDescriptor:cbDesc];
    if(!encode(first,0,oldSource,nil)) return 7; [first commit];
    if(!getenv("RENDERDOC_METAL_ASYNC_TEXTURE_ALIAS")) [first waitUntilCompleted];
    if(annotation(tables[0],"metal.descriptorSlotEvent",0,1,1,4)) return 8;
    id<MTLBuffer> replacement=[heap newBufferWithLength:4
        options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:0];
    tables[2]=[device newBufferWithLength:24 options:MTLResourceStorageModeShared];
    if(!replacement || !table(2,replacement,false)) return 9;
    id<MTLCommandBuffer> middle=[queue commandBufferWithDescriptor:cbDesc];
    if(!encode(middle,1,source,replacement)) return 10; [middle commit];
    id<MTLCommandBuffer> last=[queue commandBufferWithDescriptor:cbDesc];
    if(!encode(last,2,replacement,nil)) return 11;
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    id<CAMetalDrawable> drawable=[layer nextDrawable];
    auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=drawable.texture;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    [[last renderCommandEncoderWithDescriptor:pass] endEncoding];[last presentDrawable:drawable];
    [last commit];[last waitUntilCompleted];
    const uint32_t *words=(const uint32_t *)output.contents;
    for(unsigned i=0;i<3;i++) if(words[i*2]!=(i+1)*17 || words[i*2+1]!=0xdeadbeefU) return 12;
    if(first.error || middle.error || last.error || (api&&!api->EndFrameCapture(nullptr,nullptr))) return 13;
    puts("PASS Native retired Private texture -> buffer: 17/34/51, retained logical sources, same physical heap offset");
  }
  return 0;
}
