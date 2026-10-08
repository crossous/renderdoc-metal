// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  if(argc != 3) return 2;
  @autoreleasepool {
    const unsigned variant = unsigned(atoi(argv[1]));
    if(variant > 4) return 2;
    const bool array = variant == 1 || variant == 3 || variant == 4;
    const unsigned width = array ? 37 : 80, height = array ? 19 : 48;
    const unsigned mips = array ? 3 : 1, slices = array ? 2 : 1, bpp = array ? 8 : 4;
    auto device = MTLCreateSystemDefaultDevice(); auto queue = [device newCommandQueue];
    auto desc = [MTLTextureDescriptor new];
    desc.textureType = array ? MTLTextureType2DArray : MTLTextureType2D;
    desc.pixelFormat = array ? MTLPixelFormatRGBA16Float : variant == 2 ? MTLPixelFormatRGB10A2Unorm : MTLPixelFormatBGRA8Unorm;
    desc.width = width; desc.height = height; desc.arrayLength = slices; desc.mipmapLevelCount = mips;
    desc.storageMode = MTLStorageModePrivate; desc.usage = MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView;
    auto source = [device newTextureWithDescriptor:desc]; if(!source) return 3;
    source.label = @"native-background-defined-source";
    std::vector<uint8_t> expected;
    auto upload = [queue commandBuffer]; auto blit = [upload blitCommandEncoder];
    for(unsigned slice = 0; slice < slices; ++slice) for(unsigned mip = 0; mip < mips; ++mip)
    {
      const unsigned w = MAX(1U, width >> mip), h = MAX(1U, height >> mip);
      const unsigned row = (w * bpp + 255) & ~255U, offset = array ? 512 : 256;
      auto staging = [device newBufferWithLength:offset + row * h options:MTLResourceStorageModeShared];
      memset(staging.contents, 0xcc, staging.length);
      for(unsigned y = 0; y < h; ++y) for(unsigned x = 0; x < w * bpp; ++x)
      {
        // Only integer patterns/finite half bits are copied, never shader arithmetic.
        uint8_t value = array ? ((x & 1) ? 0x38 : uint8_t((x + y + slice * 7 + mip * 11) & 63)) : uint8_t(x + y * 3 + 17);
        ((uint8_t *)staging.contents)[offset + y * row + x] = value; expected.push_back(value);
      }
      [blit copyFromBuffer:staging sourceOffset:offset sourceBytesPerRow:row sourceBytesPerImage:row*h sourceSize:MTLSizeMake(w,h,1) toTexture:source destinationSlice:slice destinationLevel:mip destinationOrigin:MTLOriginMake(0,0,0)];
    }
    [blit endEncoding]; [upload commit]; [upload waitUntilCompleted]; if(upload.error) return 4;
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0, (void **)&api)) return 5;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue protocol = {}; protocol.uint32 = 65;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device, "metal.descriptorCoverage", eRENDERDOC_UInt32, 0, &protocol)) return 6;
    }
    auto layer = [CAMetalLayer layer]; layer.device = device; layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO; layer.drawableSize = CGSizeMake(2,2);
    if(api) api->StartFrameCapture(nullptr,nullptr);
    desc.storageMode = MTLStorageModeManaged; desc.hazardTrackingMode = MTLHazardTrackingModeUntracked;
    auto destination = [device newTextureWithDescriptor:desc]; if(!destination) return 7;
    destination.label = @"native-frame-copy-destination";
    auto commands = [queue commandBuffer]; auto copy = [commands blitCommandEncoder];
    if(variant == 3) [copy copyFromTexture:source toTexture:destination];
    else if(variant == 4) [copy copyFromTexture:source sourceSlice:0 sourceLevel:0 toTexture:destination destinationSlice:0 destinationLevel:0 sliceCount:slices levelCount:mips];
    else for(unsigned slice=0;slice<slices;++slice) for(unsigned mip=0;mip<mips;++mip)
      [copy copyFromTexture:source sourceSlice:slice sourceLevel:mip sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(MAX(1U,width>>mip),MAX(1U,height>>mip),1) toTexture:destination destinationSlice:slice destinationLevel:mip destinationOrigin:MTLOriginMake(0,0,0)];
    [copy synchronizeResource:destination]; [copy endEncoding];
    auto drawable=[layer nextDrawable];if(!drawable)return 8;
    auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=drawable.texture;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    [[commands renderCommandEncoderWithDescriptor:pass] endEncoding];[commands presentDrawable:drawable];
    if(variant >= 2) {
      // Legal host synchronization without a waitUntilCompleted API chunk.
      dispatch_semaphore_t done=dispatch_semaphore_create(0);
      [commands addCompletedHandler:^(id<MTLCommandBuffer> cb) { dispatch_semaphore_signal(done); }];
      [commands commit];
      if(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,10*NSEC_PER_SEC)))return 9;
    } else { [commands commit];[commands waitUntilCompleted]; }
    if(commands.error)return 9;
    std::vector<uint8_t> actual;
    for(unsigned slice=0;slice<slices;++slice) for(unsigned mip=0;mip<mips;++mip)
    {
      unsigned w=MAX(1U,width>>mip),h=MAX(1U,height>>mip);std::vector<uint8_t> data(w*h*bpp);
      [destination getBytes:data.data() bytesPerRow:w*bpp bytesPerImage:w*h*bpp fromRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:mip slice:slice];actual.insert(actual.end(),data.begin(),data.end());
    }
    if(actual!=expected)return 10;
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 11;
    FILE *out=fopen(argv[2],"wb");if(!out)return 12;
    bool saved=fwrite(actual.data(),1,actual.size(),out)==actual.size();fclose(out);if(!saved)return 13;
    printf("NATIVE OUTPUT MATCH variant=%u dimensions=%ux%u mips=%u slices=%u bytes=%zu\n",variant,width,height,mips,slices,actual.size());return 0;
  }
}
