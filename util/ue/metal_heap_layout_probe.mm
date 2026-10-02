// SPDX-License-Identifier: MIT
// Query native allocation layouts only. Never creates heaps, resources or command queues.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
int main(int argc, char **argv)
{
  if(argc != 3) return 2;
  @autoreleasepool
  {
    NSError *error = nil;
    NSData *input = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]];
    NSDictionary *source = input ? [NSJSONSerialization JSONObjectWithData:input options:0 error:&error] : nil;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!source || !device) return 3;
    NSMutableArray *result = [NSMutableArray new];
    for(NSDictionary *entry in source[@"placements"])
    {
      MTLSizeAndAlign layout = {};
      if([entry[@"kind"] isEqualToString:@"buffer"])
      {
        NSUInteger length = [entry[@"length"] unsignedLongLongValue];
        if(!length || length > 1024ULL * 1024 * 1024) return 4;
        layout = [device heapBufferSizeAndAlignWithLength:length options:[entry[@"options"] unsignedLongLongValue]];
      }
      else
      {
        NSDictionary *fields = entry[@"descriptor"];
        MTLTextureDescriptor *descriptor = [MTLTextureDescriptor new];
        descriptor.textureType = (MTLTextureType)[fields[@"textureType"] unsignedLongLongValue];
        descriptor.pixelFormat = (MTLPixelFormat)[fields[@"pixelFormat"] unsignedLongLongValue];
        descriptor.width = [fields[@"width"] unsignedLongLongValue];
        descriptor.height = [fields[@"height"] unsignedLongLongValue];
        descriptor.depth = [fields[@"depth"] unsignedLongLongValue];
        descriptor.mipmapLevelCount = [fields[@"mipmapLevelCount"] unsignedLongLongValue];
        descriptor.sampleCount = [fields[@"sampleCount"] unsignedLongLongValue];
        descriptor.arrayLength = [fields[@"arrayLength"] unsignedLongLongValue];
        descriptor.resourceOptions = (MTLResourceOptions)[fields[@"resourceOptions"] unsignedLongLongValue];
        descriptor.cpuCacheMode = (MTLCPUCacheMode)[fields[@"cpuCacheMode"] unsignedLongLongValue];
        descriptor.storageMode = (MTLStorageMode)[fields[@"storageMode"] unsignedLongLongValue];
        descriptor.hazardTrackingMode = (MTLHazardTrackingMode)[fields[@"hazardTrackingMode"] unsignedLongLongValue];
        descriptor.usage = (MTLTextureUsage)[fields[@"usage"] unsignedLongLongValue];
        if(fields[@"allowGPUOptimizedContents"])
          descriptor.allowGPUOptimizedContents = [fields[@"allowGPUOptimizedContents"] boolValue];
        if(!descriptor.width || descriptor.width > 16384 || !descriptor.height || descriptor.height > 16384 ||
           !descriptor.depth || descriptor.depth > 2048 || !descriptor.mipmapLevelCount || descriptor.mipmapLevelCount > 16 ||
           !descriptor.arrayLength || descriptor.arrayLength > 2048 || !descriptor.sampleCount || descriptor.sampleCount > 8) return 5;
        layout = [device heapTextureSizeAndAlignWithDescriptor:descriptor];
      }
      if(!layout.size || !layout.align) return 6;
      NSMutableDictionary *row = [entry mutableCopy];
      row[@"native_size"] = @(layout.size); row[@"native_align"] = @(layout.align);
      if(entry[@"upload_rows"])
      {
        NSUInteger alignment=1;
        if([entry[@"upload_depth"] boolValue]) alignment=256;
        else if([entry[@"upload_linear"] boolValue])
          alignment=MAX(NSUInteger(1),[device minimumLinearTextureAlignmentForPixelFormat:
                      (MTLPixelFormat)[entry[@"descriptor"][@"pixelFormat"] unsignedLongLongValue]]);
        uint64_t total=0;
        for(NSDictionary *mip in entry[@"upload_rows"])
        {
          const uint64_t bytes=[mip[@"row_bytes"] unsignedLongLongValue];
          const uint64_t pitch=((bytes+alignment-1)/alignment)*alignment;
          total+=pitch*[mip[@"rows"] unsignedLongLongValue]*[mip[@"depth"] unsignedLongLongValue];
        }
        row[@"upload_staging_bytes"]=@(total);
        row[@"upload_row_alignment"]=@(alignment);
      }
      [result addObject:row];
    }
    NSDictionary *output = @{@"device":device.name, @"registry_id":@(device.registryID),
      @"recommended_max_working_set":@(device.recommendedMaxWorkingSetSize),
      @"current_allocated_size":@(device.currentAllocatedSize),
      @"heaps":source[@"heaps"], @"scope":source[@"scope"], @"placements":result,
      @"gpu_commands_submitted":@0};
    NSData *data = [NSJSONSerialization dataWithJSONObject:output options:NSJSONWritingPrettyPrinted error:&error];
    if(!data || ![data writeToFile:[NSString stringWithUTF8String:argv[2]] atomically:YES]) return 7;
    printf("Native layout queries: %lu placements; no heap/resource/command-queue creation\n", (unsigned long)result.count);
  }
  return 0;
}
