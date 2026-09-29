// SPDX-License-Identifier: MIT
// Probe native Metal placement texture layouts without allocating heaps or textures.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstdio>

int main(int argc, char **argv)
{
  if(argc != 2) return 2;
  FILE *input = fopen(argv[1], "r");
  if(!input) return 2;
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if(!device) return 3;
  unsigned long long values[15];
  unsigned count = 0, unavailable = 0, created = 0;
  bool typeCreated[10] = {};
  while(true)
  {
    int scanned = fscanf(input,
                         "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                         &values[0], &values[1], &values[2], &values[3], &values[4],
                         &values[5], &values[6], &values[7], &values[8], &values[9],
                         &values[10], &values[11], &values[12], &values[13], &values[14]);
    if(scanned == EOF) break;
    if(scanned != 15) { fclose(input); return 4; }
    @autoreleasepool
    {
      MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
      desc.textureType = (MTLTextureType)values[2];
      desc.pixelFormat = (MTLPixelFormat)values[3];
      desc.width = values[4];
      desc.height = values[5];
      desc.depth = values[6];
      desc.mipmapLevelCount = values[7];
      desc.sampleCount = values[8];
      desc.arrayLength = values[9];
      desc.resourceOptions = (MTLResourceOptions)values[10];
      desc.cpuCacheMode = (MTLCPUCacheMode)values[11];
      desc.storageMode = (MTLStorageMode)values[12];
      desc.hazardTrackingMode = (MTLHazardTrackingMode)values[13];
      desc.usage = (MTLTextureUsage)values[14];
      MTLSizeAndAlign layout = [device heapTextureSizeAndAlignWithDescriptor:desc];
      printf("%llu %llu %llu %llu\n", values[0], values[1],
             (unsigned long long)layout.size, (unsigned long long)layout.align);
      if(!layout.size || !layout.align) unavailable++;
      const NSUInteger type = (NSUInteger)desc.textureType;
      if(layout.size && layout.align && type < 10 && !typeCreated[type] &&
         layout.size < 1024 * 1024 && desc.resourceOptions ==
             (MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeTracked))
      {
        MTLHeapDescriptor *heapDesc = [[MTLHeapDescriptor alloc] init];
        heapDesc.size = layout.size + 2 * layout.align;
        heapDesc.storageMode = MTLStorageModePrivate;
        heapDesc.hazardTrackingMode = MTLHazardTrackingModeTracked;
        heapDesc.type = MTLHeapTypePlacement;
        id<MTLHeap> heap = [device newHeapWithDescriptor:heapDesc];
        id<MTLTexture> texture = heap ? [heap newTextureWithDescriptor:desc offset:layout.align] : nil;
        if(!texture || texture.heapOffset != layout.align)
        {
          fprintf(stderr, "native placement creation failed: chunk %llu type %llu\n",
                  values[0], values[2]);
          [texture release]; [heap release]; [heapDesc release]; [desc release];
          fclose(input);
          return 6;
        }
        typeCreated[type] = true;
        created++;
        [texture release]; [heap release]; [heapDesc release];
      }
      count++;
      [desc release];
    }
  }
  fclose(input);
  fprintf(stderr, "native descriptor layouts: %u total, %u unavailable, %u placement types created\n",
          count, unavailable, created);
  return unavailable || created != 4 ? 5 : 0;
}
