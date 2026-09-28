/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "metal_test.h"

RD_TEST(Metal_Command_Creation_Variants, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Exercises limited queues and command-buffer/compute-encoder creation variants.";

  int main()
  {
    if(!Init())
      return 3;

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;
kernel void write_value(device uint *output [[buffer(0)]],
                        uint index [[thread_position_in_grid]])
{
  output[index] = 17;
}
)EOSHADER";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    MTL::Function *function = library ? library->newFunction(MTLSTR("write_value")) : NULL;
    MTL::ComputePipelineState *pipeline =
        function ? device->newComputePipelineState(function, &error) : NULL;
    MTL::CommandQueue *limitedQueue = device->newCommandQueue(4);
    const uint32_t expected[] = {17, 17};
    MTL::Buffer *output = device->newBuffer(16, MTL::ResourceStorageModeShared);
    if(!library || !function || !pipeline || !limitedQueue || !output)
    {
      TEST_WARN("Failed to create T35 Metal resources: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }

    bool validationFailed = false;
    const bool validateNative = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      memset(output->contents(), 0, 16);
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable)
      {
        pool->drain();
        continue;
      }

      MTL::CommandBuffer *first = limitedQueue->commandBufferWithUnretainedReferences();
      MTL::ComputeCommandEncoder *concurrent =
          first->computeCommandEncoder(MTL::DispatchTypeConcurrent);
      concurrent->setComputePipelineState(pipeline);
      concurrent->setBuffer(output, 0, 0);
      concurrent->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(1, 1, 1));
      concurrent->endEncoding();
      first->commit();
      first->waitUntilScheduled();

      MTL::CommandBufferDescriptor *commandDescriptor =
          MTL::CommandBufferDescriptor::alloc()->init();
      commandDescriptor->setRetainedReferences(false);
      commandDescriptor->setErrorOptions(MTL::CommandBufferErrorOptionNone);
      MTL::CommandBuffer *second = limitedQueue->commandBuffer(commandDescriptor);
      commandDescriptor->release();
      MTL::ComputePassDescriptor *computeDescriptor =
          MTL::ComputePassDescriptor::computePassDescriptor();
      computeDescriptor->setDispatchType(MTL::DispatchTypeSerial);
      MTL::ComputeCommandEncoder *serial = second->computeCommandEncoder(computeDescriptor);
      serial->setComputePipelineState(pipeline);
      serial->setBuffer(output, sizeof(uint32_t), 0);
      serial->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(1, 1, 1));
      serial->endEncoding();
      MTL::RenderPassDescriptor *pass =
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.07, 0.16, 0.31, 1.0));
      MTL::RenderCommandEncoder *render = second->renderCommandEncoder(pass);
      render->endEncoding();
      second->presentDrawable(drawable);
      second->commit();
      second->waitUntilCompleted();
      EndCaptureFrame();

      if(validateNative && memcmp(output->contents(), expected, sizeof(expected)) != 0)
      {
        const uint32_t *actual = (const uint32_t *)output->contents();
        TEST_WARN("T35 native output differs: %u %u", actual[0], actual[1]);
        validationFailed = true;
      }
      pool->drain();
    }

    output->release();
    limitedQueue->release();
    pipeline->release();
    function->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
