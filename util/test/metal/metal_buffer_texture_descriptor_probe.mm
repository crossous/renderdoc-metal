// SPDX-License-Identifier: MIT
// Probe the UE texture-buffer formats with native Metal Validation on small representatives.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
  if(argc != 2 && argc != 3) return 2;
  char *end = nullptr;
  unsigned expected = 9;
  if(argc == 3)
  {
    unsigned long value = strtoul(argv[2], &end, 10);
    if(!argv[2][0] || *end || value == 0 || value > 512) return 2;
    expected = (unsigned)value;
  }
  FILE *input = fopen(argv[1], "r");
  if(!input) return 2;
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if(!device) return 3;
  unsigned count = 0, created = 0;
  bool seen[512] = {};
  while(true)
  {
    unsigned long long v[17]; char optimized[8];
    int n = fscanf(input, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %7s",
                   &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
                   &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16], optimized);
    if(n == EOF) break;
    if(n != 18) { fclose(input); return 4; }
    MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType = (MTLTextureType)v[4]; desc.pixelFormat = (MTLPixelFormat)v[5];
    desc.width = v[6]; desc.height = v[7]; desc.depth = v[8];
    desc.mipmapLevelCount = v[9]; desc.sampleCount = v[10]; desc.arrayLength = v[11];
    desc.resourceOptions = (MTLResourceOptions)v[12]; desc.cpuCacheMode = (MTLCPUCacheMode)v[13];
    desc.storageMode = (MTLStorageMode)v[14]; desc.hazardTrackingMode = (MTLHazardTrackingMode)v[15];
    desc.usage = (MTLTextureUsage)v[16]; desc.allowGPUOptimizedContents = false;
    NSUInteger linear = [device minimumLinearTextureAlignmentForPixelFormat:desc.pixelFormat];
    NSUInteger buffer = [device minimumTextureBufferAlignmentForPixelFormat:desc.pixelFormat];
    printf("%llu %llu %llu %llu\n", v[0], v[3], (unsigned long long)linear,
           (unsigned long long)buffer);
    NSUInteger format = (NSUInteger)desc.pixelFormat;
    if(format < 512 && !seen[format])
    {
      MTLTextureDescriptor *small = [desc copy];
      small.width = 64;
      NSUInteger row = ((64 * (v[3] / v[6]) + linear - 1) / linear) * linear;
      id<MTLBuffer> backing = [device newBufferWithLength:row options:MTLResourceStorageModePrivate];
      id<MTLTexture> view = backing ? [backing newTextureWithDescriptor:small offset:0 bytesPerRow:row] : nil;
      if(!view)
      {
        fprintf(stderr, "native texture-buffer creation failed: chunk %llu format %llu\n", v[0], v[5]);
        [backing release]; [small release]; [desc release]; fclose(input); return 5;
      }
      seen[format] = true; created++;
      [view release]; [backing release]; [small release];
    }
    [desc release]; count++;
  }
  fclose(input);
  fprintf(stderr, "native texture-buffer descriptors: %u total, %u formats created\n",count,created);
  return created == expected ? 0 : 6;
}
