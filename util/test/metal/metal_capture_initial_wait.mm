// SPDX-License-Identifier: MIT
// Tiny pre-capture writes on two queues, a CPU completion callback requiring the
// capture lock, and an enqueued reservation which is committed only after start.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(!get || !get(eRENDERDOC_API_Version_1_7_0, (void **)&api)) return 2;
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@"#include <metal_stdlib>\nusing namespace metal;\nkernel void fill(device uint *out [[buffer(0)]], constant uint &v [[buffer(1)]]) { out[0]=v; }" options:nil error:&error];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"fill"] error:&error];
    id<MTLCommandQueue> queueA = [device newCommandQueue], queueB = [device newCommandQueue], reserveQueue = [device newCommandQueue];
    if(getenv("RENDERDOC_METAL_TEST_BLOCKED_RESERVATION"))
    {
      id<MTLCommandBuffer> reservation = [queueA commandBuffer]; [reservation enqueue];
      id<MTLCommandBuffer> blocked = [queueA commandBuffer]; [blocked commit];
      auto started = std::chrono::steady_clock::now();
      api->StartFrameCapture(nullptr, nullptr);
      const bool ended = api->EndFrameCapture(nullptr, nullptr);
      auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started).count();
      if(ended || wait > 50) return 8;
      [reservation commit]; [reservation waitUntilCompleted]; [blocked waitUntilCompleted];
      if(reservation.error || blocked.error) return 9;
      printf("PASS blocked reservation: start refused without waiting, recovered after commit\n");
    }
    id<MTLBuffer> a = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
    id<MTLBuffer> b = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
    id<MTLSharedEvent> event = [device newSharedEvent];
    id<MTLSharedEvent> gate = [device newSharedEvent];
    if(!pipeline || !a || !b || !event || !gate) return 3;
    *(uint32_t *)a.contents = *(uint32_t *)b.contents = 0;
    id<MTLCommandBuffer> reserved = [reserveQueue commandBuffer]; [reserved enqueue];
    id<MTLCommandBuffer> first = [queueA commandBuffer], second = [queueB commandBuffer];
    [first encodeWaitForEvent:event value:1];
    [second encodeWaitForEvent:gate value:1];
    auto encode = [&](id<MTLCommandBuffer> command, id<MTLBuffer> buffer, uint32_t value)
    {
      id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
      [compute setComputePipelineState:pipeline]; [compute setBuffer:buffer offset:0 atIndex:0];
      [compute setBytes:&value length:4 atIndex:1];
      [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      [compute endEncoding];
    };
    encode(first,a,111); encode(second,b,222);
    [second encodeSignalEvent:event value:1];
    std::atomic<bool> handlerDone{false};
    auto *done = &handlerDone;
    [first addCompletedHandler:^(id<MTLCommandBuffer>)
    {
      api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
          "metal.capturePresented", eRENDERDOC_Empty, 0, nullptr);
      done->store(true);
    }];
    [first commit]; [second commit];
    std::thread signal([&]() { std::this_thread::sleep_for(std::chrono::milliseconds(100)); gate.signaledValue = 1; });
    api->SetCaptureFilePathTemplate(getenv("RENDERDOC_METAL_CAPTURE_PATH"));
    auto begin = std::chrono::steady_clock::now();
    api->StartFrameCapture(nullptr,nullptr);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
    const uint32_t valueA = *(uint32_t *)a.contents, valueB = *(uint32_t *)b.contents;
    signal.join();
    [first waitUntilCompleted]; [second waitUntilCompleted];
    if(valueA != 111 || valueB != 222 || !handlerDone.load() || elapsed < 50 || first.error || second.error) return 4;
    [reserved commit]; [reserved waitUntilCompleted];
    CAMetalLayer *layer = [CAMetalLayer layer]; layer.device=device;
    layer.pixelFormat=MTLPixelFormatBGRA8Unorm; layer.framebufferOnly=NO; layer.drawableSize=CGSizeMake(2,2);
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=drawable.texture;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear; pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    id<MTLCommandBuffer> command = [queueA commandBuffer]; [command enqueue];
    id<MTLRenderCommandEncoder> render = [command renderCommandEncoderWithDescriptor:pass];
    [render useResource:a usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
    [render useResource:b usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
    [render endEncoding]; [command presentDrawable:drawable];
    std::atomic<bool> allowCompletion{false}, completionCommitted{false};
    auto *allow = &allowCompletion;
    auto *committed = &completionCommitted;
    [command addCompletedHandler:^(id<MTLCommandBuffer>) {
      while(!allow->load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
      id<MTLCommandBuffer> callbackWork = [queueB commandBuffer];
      [callbackWork enqueue]; [callbackWork commit]; [callbackWork waitUntilCompleted];
      committed->store(callbackWork.error == nil);
    }];
    [command commit];
    std::thread releaseCompletion([&]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(100)); allowCompletion.store(true);
    });
    const bool ended = api->EndFrameCapture(nullptr,nullptr);
    releaseCompletion.join(); [command waitUntilCompleted];
    if(command.error || !ended || !completionCommitted.load()) return 5;
    puts("PASS capture end: completion callback can enqueue and commit while End waits");
    if(getenv("RENDERDOC_METAL_TEST_BLOCKED_RESERVATION"))
    {
      api->StartFrameCapture(nullptr, nullptr);
      id<MTLCommandBuffer> reservation = [queueB commandBuffer]; [reservation enqueue];
      id<MTLCommandBuffer> blocked = [queueB commandBuffer]; [blocked commit];
      drawable = [layer nextDrawable]; pass.colorAttachments[0].texture = drawable.texture;
      command = [queueA commandBuffer];
      render = [command renderCommandEncoderWithDescriptor:pass]; [render endEncoding];
      [command presentDrawable:drawable]; [command commit]; [command waitUntilCompleted];
      auto started = std::chrono::steady_clock::now();
      const bool saved = api->EndFrameCapture(nullptr, nullptr);
      auto endWait = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started).count();
      if(saved || endWait > 50) return 10;
      [reservation commit]; [reservation waitUntilCompleted]; [blocked waitUntilCompleted];
      if(reservation.error || blocked.error || command.error) return 11;
      puts("PASS incomplete frame: blocked committed work refused at end without waiting");
    }
    // Present must trigger automatic Start/End outside the commit cutoff lock.
    api->TriggerCapture();
    for(int frame = 0; frame < 4 && api->GetNumCaptures() < 2; ++frame)
    {
      drawable = [layer nextDrawable];
      pass.colorAttachments[0].texture = drawable.texture;
      command = [queueA commandBuffer]; [command enqueue];
      render = [command renderCommandEncoderWithDescriptor:pass];
      [render endEncoding]; [command presentDrawable:drawable];
      [command commit]; [command waitUntilCompleted];
      if(command.error) return 6;
    }
    if(api->GetNumCaptures() != 2) return 7;
    printf("PASS initial GPU wait: queues=2 A=111 B=222 start_ms=%lld callback=complete uncommitted_reservation=not_waited automatic_capture=complete\n", (long long)elapsed);
  }
  return 0;
}
