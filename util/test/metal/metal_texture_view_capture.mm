// SPDX-License-Identifier: MIT
// Small Native values for texture inspection. No draw or descriptor result is simulated.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue = [device newCommandQueue];
    NSMutableArray<id<MTLTexture>> *srcs = [NSMutableArray new], *targets = [NSMutableArray new];
    const MTLPixelFormat formats[] = {MTLPixelFormatRGBA32Float, MTLPixelFormatRGBA8Unorm_sRGB,
                                      MTLPixelFormatR32Uint, MTLPixelFormatR32Sint};
    for(unsigned f = 0; f < 4; f++)
    {
      auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:formats[f]
                                                                     width:8
                                                                    height:4
                                                                 mipmapped:NO];
      desc.storageMode = MTLStorageModeShared;
      desc.usage = MTLTextureUsageShaderRead;
      id<MTLTexture> src = [device newTextureWithDescriptor:desc];
      desc.storageMode = MTLStorageModePrivate;
      id<MTLTexture> dst = [device newTextureWithDescriptor:desc];
      if(!src || !dst)
        return 2;
      dst.label = [NSString stringWithFormat:@"Texture inspection %u", f];
      float floats[32][4];
      uint8_t bytes[32][4];
      uint32_t uints[32];
      int32_t sints[32];
      for(unsigned i = 0; i < 32; i++)
      {
        floats[i][0] = float(i % 8) / 4.0f - 0.5f;
        floats[i][1] = float(i / 8) / 4.0f;
        floats[i][2] = 0.5f;
        floats[i][3] = 0.5f;
        bytes[i][0] = 128;
        bytes[i][1] = uint8_t(i * 8);
        bytes[i][2] = 64;
        bytes[i][3] = 128;
        uints[i] = i * 3;
        sints[i] = int(i) - 16;
      }
      floats[7][0] = NAN;
      floats[15][0] = INFINITY;
      const void *pixels = f == 0   ? (void *)floats
                           : f == 1 ? (void *)bytes
                           : f == 2 ? (void *)uints
                                    : (void *)sints;
      [src replaceRegion:MTLRegionMake2D(0, 0, 8, 4)
             mipmapLevel:0
               withBytes:pixels
             bytesPerRow:f == 0 ? 128 : 32];
      [srcs addObject:src];
      [targets addObject:dst];
    }
    // Distinct texels on each face/slice and mip catch constant-centre cube
    // sampling, wrong face orientation, and statistics over an entire volume.
    const MTLTextureType types[] = {MTLTextureType2DArray, MTLTextureTypeCube, MTLTextureType3D,
                                    MTLTextureTypeCubeArray};
    for(unsigned f = 0; f < 4; f++)
    {
      auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                                     width:8
                                                                    height:8
                                                                 mipmapped:YES];
      desc.textureType = types[f];
      desc.arrayLength = f == 0 ? 3 : f == 3 ? 2 : 1;
      desc.depth = f == 2 ? 4 : 1;
      desc.storageMode = MTLStorageModeShared;
      desc.usage = MTLTextureUsageShaderRead;
      id<MTLTexture> src = [device newTextureWithDescriptor:desc];
      desc.storageMode = MTLStorageModePrivate;
      id<MTLTexture> dst = [device newTextureWithDescriptor:desc];
      if(!src || !dst)
        return 10;
      dst.label = [NSString stringWithFormat:@"Texture inspection %u", f + 4];
      const unsigned faces = f == 1 ? 6 : f == 3 ? 12 : f == 0 ? 3 : 1;
      for(unsigned mip = 0; mip < 4; mip++)
      {
        const unsigned w = 8 >> mip, h = w, d = f == 2 ? std::max(1U, 4U >> mip) : 1;
        for(unsigned face = 0; face < faces; face++)
        {
          std::vector<float> values(w * h * d * 4);
          for(unsigned z = 0; z < d; z++)
            for(unsigned y = 0; y < h; y++)
              for(unsigned x = 0; x < w; x++)
              {
                float *p = values.data() + ((z * h + y) * w + x) * 4;
                p[0] = .04f * (float(f == 2 ? z : face) + 1) + .01f * mip;
                p[1] = float(x) / w;
                p[2] = float(y) / h;
                p[3] = 1;
              }
          [src replaceRegion:MTLRegionMake3D(0, 0, 0, w, h, d)
                 mipmapLevel:mip
                       slice:face
                   withBytes:values.data()
                 bytesPerRow:w * 16
               bytesPerImage:w * h * 16];
        }
      }
      [srcs addObject:src];
      [targets addObject:dst];
    }
    // Odd dimensions catch guessed mip scaling in the typed display path.
    auto intDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Sint
                                                                      width:7
                                                                     height:5
                                                                  mipmapped:YES];
    intDesc.storageMode = MTLStorageModeShared;
    intDesc.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> intSrc = [device newTextureWithDescriptor:intDesc];
    intDesc.storageMode = MTLStorageModePrivate;
    id<MTLTexture> intDst = [device newTextureWithDescriptor:intDesc];
    if(!intSrc || !intDst)
      return 14;
    intDst.label = @"Texture inspection 9";
    for(unsigned mip = 0; mip < intSrc.mipmapLevelCount; mip++)
    {
      unsigned w = std::max(1U, 7U >> mip), h = std::max(1U, 5U >> mip);
      std::vector<int32_t> values(w * h);
      for(unsigned y = 0; y < h; y++)
        for(unsigned x = 0; x < w; x++)
          values[y * w + x] = int32_t(x + 10 * y + 100 * mip);
      [intSrc replaceRegion:MTLRegionMake2D(0, 0, w, h)
                mipmapLevel:mip
                  withBytes:values.data()
                bytesPerRow:w * 4];
    }
    [srcs addObject:intSrc];
    [targets addObject:intDst];
    auto depthDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8
                                                           width:8
                                                          height:4
                                                       mipmapped:NO];
    depthDesc.storageMode = MTLStorageModePrivate;
    depthDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    id<MTLTexture> depth = [device newTextureWithDescriptor:depthDesc];
    if(!depth)
      return 11;
    depth.label = @"Texture inspection 8";
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0, (void **)&api))
        return 3;
      api->SetCaptureFilePathTemplate(path);
    }
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;
    layer.drawableSize = CGSizeMake(2, 2);
    if(api)
      api->StartFrameCapture(nullptr, nullptr);
    auto command = [queue commandBuffer];
    auto blit = [command blitCommandEncoder];
    for(unsigned i = 0; i < srcs.count; i++)
      for(unsigned mip = 0; mip < srcs[i].mipmapLevelCount; mip++)
        for(unsigned face = 0; face < (i == 4 ? 3 : i == 5 ? 6 : i == 7 ? 12 : 1); face++)
          [blit copyFromTexture:srcs[i]
                    sourceSlice:face
                    sourceLevel:mip
                   sourceOrigin:MTLOriginMake(0, 0, 0)
                     sourceSize:MTLSizeMake(std::max(NSUInteger(1), srcs[i].width >> mip),
                                            std::max(NSUInteger(1), srcs[i].height >> mip),
                                            std::max(NSUInteger(1), srcs[i].depth >> mip))
                      toTexture:targets[i]
               destinationSlice:face
               destinationLevel:mip
              destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
    auto depthPass = [MTLRenderPassDescriptor renderPassDescriptor];
    depthPass.depthAttachment.texture = depth;
    depthPass.depthAttachment.clearDepth = .375;
    depthPass.depthAttachment.loadAction = MTLLoadActionClear;
    depthPass.depthAttachment.storeAction = MTLStoreActionStore;
    depthPass.stencilAttachment.texture = depth;
    depthPass.stencilAttachment.clearStencil = 128;
    depthPass.stencilAttachment.loadAction = MTLLoadActionClear;
    depthPass.stencilAttachment.storeAction = MTLStoreActionStore;
    [[command renderCommandEncoderWithDescriptor:depthPass] endEncoding];
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    [[command renderCommandEncoderWithDescriptor:pass] endEncoding];
    [command presentDrawable:drawable];
    [command commit];
    [command waitUntilCompleted];
    if(command.error)
      return 4;
    if(api && !api->EndFrameCapture(nullptr, nullptr))
      return 5;
    // Native readback proves the fixture's finite/special values independently of replay.
    for(unsigned i = 0; i < targets.count; i++)
    {
      auto data = [device newBufferWithLength:32768 options:MTLResourceStorageModeShared];
      auto copy = [queue commandBuffer];
      auto encoder = [copy blitCommandEncoder];
      const unsigned face = i == 4 ? 2 : i == 5 ? 5 : i == 7 ? 11 : 0;
      [encoder copyFromTexture:targets[i]
                       sourceSlice:face
                       sourceLevel:0
                      sourceOrigin:MTLOriginMake(0, 0, 0)
                        sourceSize:MTLSizeMake(targets[i].width, targets[i].height, targets[i].depth)
                          toBuffer:data
                 destinationOffset:0
            destinationBytesPerRow:256
          destinationBytesPerImage:targets[i].height * 256];
      [encoder endEncoding];
      [copy commit];
      [copy waitUntilCompleted];
      if(copy.error)
        return 6;
      if(i == 0)
      {
        const float *p = (const float *)data.contents;
        if(p[0] != -.5f || !std::isnan(p[28]) ||
           !std::isinf(((const float *)((const char *)data.contents + 256))[28]))
          return 7;
      }
      if(i == 2 && ((uint32_t *)data.contents)[7] != 21)
        return 8;
      if(i == 3 && ((int32_t *)data.contents)[0] != -16)
        return 9;
      if(i == 8 && (((int32_t *)data.contents)[0] != 0 ||
                    ((int32_t *)((char *)data.contents + 4 * 256))[6] != 46))
        return 15;
      if(i >= 4 && i < 8)
      {
        const float *p = (const float *)data.contents;
        if(std::fabs(p[0] - (.04f * (face + 1))) > .00001f || p[7 * 4 + 1] != .875f)
          return 12;
        if(i == 6 && std::fabs(*(float *)((char *)data.contents + 3 * 8 * 256) - .16f) > .00001f)
          return 13;
      }
      printf("PASS Native inspection fixture index=%u format=%lu\n", i,
             (unsigned long)targets[i].pixelFormat);
    }
    return 0;
  }
}
