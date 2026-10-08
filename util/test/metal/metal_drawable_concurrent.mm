// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <atomic>
#include <thread>
#include <vector>
#include <cstdio>
int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    constexpr unsigned count = 8;
    CAMetalLayer *layers[count] = {};
    for(unsigned i = 0; i < count; ++i)
    {
      layers[i] = [CAMetalLayer new]; layers[i].device = device;
      layers[i].pixelFormat = MTLPixelFormatBGRA8Unorm;
      layers[i].drawableSize = CGSizeMake(64 + 8 * i, 48 + 4 * i);
    }
    std::atomic<unsigned> ready{0}, failed{0}; std::atomic<bool> start{false};
    std::vector<std::thread> workers;
    for(unsigned i = 0; i < count; ++i)
      workers.emplace_back([&, i] {
        @autoreleasepool
        {
          ++ready; while(!start.load()) std::this_thread::yield();
          id<CAMetalDrawable> drawable = [layers[i] nextDrawable];
          if(!drawable) { ++failed; return; }
          id<MTLTexture> texture = drawable.texture;
          if(!texture || texture.width != 64 + 8 * i || texture.height != 48 + 4 * i) ++failed;
          [drawable present];
          // Presentation removes the wrapped lookup. The hook must now call
          // the real original rather than recurse into itself.
          if(!drawable.texture || drawable.texture.width != 64 + 8 * i) ++failed;
        }
      });
    while(ready.load() != count) std::this_thread::yield(); start.store(true);
    for(auto &worker : workers) worker.join();
    for(auto layer : layers) [layer release];
    if(failed.load()) return 2;
    puts("PASS eight simultaneous drawable acquisitions, varied extents, texture and post-present original fallback");
  }
  return 0;
}
