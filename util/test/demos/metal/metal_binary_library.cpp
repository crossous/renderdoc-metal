// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Binary_Library, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Binary Metal libraries from file, URL, segmented dispatch data, bundle and main bundle.";
  int main()
  {
    if(!Init()) return 3;
    NS::String *path = NS::Bundle::mainBundle()->pathForResource(MTLSTR("default"), MTLSTR("metallib"));
    if(!path) { TEST_WARN("Run the T48 batch script to prepare the fixture app bundle"); return 4; }
    const std::string fixture = GetEnvVar("RENDERDOC_METAL_LIBRARY_FIXTURE");
    NS::Bundle *bundle = NS::Bundle::bundle(NS::String::string(
        (fixture + "/Test.bundle").c_str(), NS::UTF8StringEncoding));
    NS::Bundle *emptyBundle = NS::Bundle::bundle(NS::String::string(
        (fixture + "/Empty.bundle").c_str(), NS::UTF8StringEncoding));
    if(!bundle || !emptyBundle) return 4;
    NS::Error *error = NULL;
    NS::String *missing = NS::String::string(
        (fixture + "/does-not-exist.metallib").c_str(), NS::UTF8StringEncoding);
    if(device->newLibrary(missing, &error) || !error) return 4;
    error = NULL;
    if(device->newLibrary(NS::URL::fileURLWithPath(missing), &error) || !error) return 4;
    error = NULL;
    if(device->newDefaultLibrary(emptyBundle, &error) || !error) return 4;
    const byte invalidBytes[16] = {};
    dispatch_data_t invalid = dispatch_data_create(invalidBytes, sizeof(invalidBytes),
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    error = NULL;
    MTL::Library *invalidLibrary = device->newLibrary(invalid, &error);
    dispatch_release(invalid);
    if(invalidLibrary || !error) return 4;
    error = NULL;
    if(device->newLibrary(MTLSTR("this is not valid MSL"), NULL, &error) || !error) return 4;

    MTL::Library *libraries[5] = {};
    libraries[0] = device->newLibrary(path, &error);
    libraries[1] = device->newLibrary(NS::URL::fileURLWithPath(path), &error);
    NS::Data *file = NS::Data::dataWithContentsOfFile(path);
    if(!file || file->length() <= 17) return 4;
    const byte *bytes = (const byte *)file->bytes();
    dispatch_queue_t workQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_data_t first = dispatch_data_create(bytes, 17, workQueue, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    dispatch_data_t second = dispatch_data_create(bytes + 17, file->length() - 17,
                                                 workQueue, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    dispatch_data_t segmented = dispatch_data_create_concat(first, second);
    libraries[2] = device->newLibrary(segmented, &error);
    dispatch_release(segmented); dispatch_release(first); dispatch_release(second);
    libraries[3] = device->newDefaultLibrary(bundle, &error);
    libraries[4] = device->newDefaultLibrary();
    MTL::ComputePipelineState *pipelines[5] = {};
    for(uint32_t i = 0; i < 5; i++)
    {
      if(!libraries[i] || libraries[i]->device() != device || libraries[i]->functionNames()->count() != 3 ||
         libraries[i]->newFunction(MTLSTR("missing_function"))) return 4;
      MTL::Function *function = libraries[i]->newFunction(MTLSTR("cs_binary"));
      if(!function) return 4;
      pipelines[i] = device->newComputePipelineState(function, &error);
      function->release();
      if(!pipelines[i]) return 4;
    }
    MTL::Function *vs = libraries[4]->newFunction(MTLSTR("vs_binary"));
    MTL::Function *fs = libraries[4]->newFunction(MTLSTR("fs_binary"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *rp = device->newRenderPipelineState(pd, &error);
    pd->release();
    MTL::Buffer *output = device->newBuffer(676, MTL::ResourceStorageModeShared);
    if(!rp || !output) return 4;
    // Pipelines and their capture records must survive releasing all source objects early.
    vs->release(); fs->release();
    for(auto library : libraries) library->release();
    const uint32_t biases[] = {29, 61, 97, 137, 173};
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
      blit->fillBuffer(output, NS::Range::Make(0, 676), 0);
      blit->endEncoding();
      MTL::ComputeCommandEncoder *enc = cb->computeCommandEncoder();
      for(uint32_t i = 0; i < 5; i++)
      {
        enc->setComputePipelineState(pipelines[i]);
        enc->setBuffer(output, i * 128, 0);
        enc->setBytes(&biases[i], 4, 1);
        enc->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(32, 1, 1));
      }
      enc->endEncoding();
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->setRenderPipelineState(rp); render->setFragmentBuffer(output, 0, 0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      EndCaptureFrame();
      if(native)
      {
        const uint32_t *values = (const uint32_t *)output->contents();
        for(uint32_t i = 0; i < 169; i++)
          if(values[i] != (i < 160 ? biases[i / 32] + i % 32 : 0)) failed = true;
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(200, 150, 1, 1), 0);
        const byte expected[] = {97, 61, 29, 255};
        if(memcmp(pixel, expected, 4)) failed = true;
      }
      pool->drain();
    }
    output->release(); rp->release();
    for(auto pipeline : pipelines) pipeline->release();
    if(failed) TEST_WARN("T48 binary library native output differs");
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
