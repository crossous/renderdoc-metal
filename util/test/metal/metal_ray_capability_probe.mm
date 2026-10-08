// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
int main(int argc, char **argv)
{
  if(argc!=1 && argc!=3) return 1;
  @autoreleasepool
  {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    if(!device) return 2;
    const bool ray=device.supportsRaytracing, render=device.supportsRaytracingFromRender;
    printf("compute=%u render=%u\n",unsigned(ray),unsigned(render));
    if(argc==1) return 0;
    return ray==bool(atoi(argv[1])) && render==bool(atoi(argv[2]))?0:3;
  }
}
