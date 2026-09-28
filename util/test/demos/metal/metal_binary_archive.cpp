// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"
#include "metal_binary_archive_source.h"
#include <dispatch/dispatch.h>
#include <objc/message.h>

RD_TEST(Metal_Binary_Archive, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Relocated binary archive dependencies for compute and render pipelines.";

  int main()
  {
    if(!Init()) return 3;
    const bool buildArchive = !GetEnvVar("RENDERDOC_METAL_BINARY_ARCHIVE_BUILD").empty();
    const std::string path = GetEnvVar("RENDERDOC_METAL_BINARY_ARCHIVE");
    if(!buildArchive && path.empty()) return 4;
    NS::Error *error = NULL;
    MTL::BinaryArchiveDescriptor *archiveDescriptor =
        MTL::BinaryArchiveDescriptor::alloc()->init();
    if(!buildArchive)
      archiveDescriptor->setUrl(NS::URL::fileURLWithPath(
          NS::String::string(path.c_str(), NS::UTF8StringEncoding)));
    MTL::BinaryArchive *archive = device->newBinaryArchive(archiveDescriptor, &error);
    archiveDescriptor->release();
    if(!archive) { TEST_WARN("Binary archive import failed"); return 4; }

    MTL::Library *library = device->newLibrary(
        NS::String::string(kMetalBinaryArchiveSource, NS::UTF8StringEncoding), NULL, &error);
    if(!library) return 4;
    const bool addFunction = !GetEnvVar("RENDERDOC_METAL_BINARY_ARCHIVE_FUNCTION").empty();
    if(addFunction)
    {
      MTL::FunctionDescriptor *functionDescriptor = MTL::FunctionDescriptor::alloc()->init();
      functionDescriptor->setName(MTLSTR("archive_visible"));
      bool added = archive->addFunction(functionDescriptor, library, &error);
      functionDescriptor->release();
      if(!added) { TEST_WARN("Could not add visible function to archive"); return 4; }
    }
    const bool addLibrary = !GetEnvVar("RENDERDOC_METAL_BINARY_ARCHIVE_LIBRARY").empty();
    if(addLibrary)
    {
      MTL::Function *visibleBase = library->newFunction(MTLSTR("archive_visible"));
      MTL::FunctionStitchingInputNode *input = MTL::FunctionStitchingInputNode::alloc()->init(0);
      MTL::FunctionStitchingFunctionNode *output = MTL::FunctionStitchingFunctionNode::alloc()->init(
          MTLSTR("archive_visible"), NS::Array::array(input), NS::Array::array());
      MTL::FunctionStitchingGraph *graph = MTL::FunctionStitchingGraph::alloc()->init(
          MTLSTR("archive_stitched"), NS::Array::array(output), output, NS::Array::array());
      MTL::StitchedLibraryDescriptor *stitched = MTL::StitchedLibraryDescriptor::alloc()->init();
      stitched->setFunctions(NS::Array::array(visibleBase));
      stitched->setFunctionGraphs(NS::Array::array(graph));
      bool added = ((BOOL (*)(id, SEL, id, NS::Error **))objc_msgSend)(
          (id)archive, sel_registerName("addLibraryWithDescriptor:error:"),
          (id)stitched, &error) != NO;
      stitched->release(); graph->release(); output->release(); input->release();
      visibleBase->release();
      if(!added) { TEST_WARN("Could not add stitched library to archive"); return 4; }
    }
    MTL::Function *compute = library->newFunction(MTLSTR("archive_probe"));
    MTL::Function *vertex = library->newFunction(MTLSTR("archive_vs"));
    MTL::Function *fragment = library->newFunction(MTLSTR("archive_fs"));
    if(!compute || !vertex || !fragment) return 4;
    NS::Object *archiveObject = (NS::Object *)archive;
    NS::Array *archiveArray = NS::Array::array(&archiveObject, 1);
    const bool async = !GetEnvVar("RENDERDOC_METAL_BINARY_ARCHIVE_ASYNC").empty();
    dispatch_group_t group = dispatch_group_create();
    bool asyncFailed = false;

    MTL::ComputePipelineDescriptor *computeDescriptor =
        MTL::ComputePipelineDescriptor::alloc()->init();
    computeDescriptor->setComputeFunction(compute);
    if(buildArchive && !archive->addComputePipelineFunctions(computeDescriptor, &error))
    {
      TEST_WARN("Could not add compute function to archive");
      return 4;
    }
    computeDescriptor->setBinaryArchives(archiveArray);
    MTL::AutoreleasedComputePipelineReflection computeReflection = NULL;
    MTL::ComputePipelineState *computeState = NULL;
    if(async)
    {
      dispatch_group_enter(group);
      device->newComputePipelineState(computeDescriptor,
          MTL::PipelineOptionFailOnBinaryArchiveMiss,
          [&](MTL::ComputePipelineState *result, MTL::ComputePipelineReflection *, NS::Error *failure) {
            if(failure || !result) asyncFailed = true;
            if(result) computeState = result->retain();
            dispatch_group_leave(group);
          });
      if(dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC)))
        asyncFailed = true;
    }
    else
      computeState = device->newComputePipelineState(
          computeDescriptor, MTL::PipelineOptionFailOnBinaryArchiveMiss,
          &computeReflection, &error);
    computeDescriptor->release();
    if(asyncFailed || !computeState) { TEST_WARN("Compute archive miss"); return 4; }

    MTL::RenderPipelineDescriptor *renderDescriptor =
        MTL::RenderPipelineDescriptor::alloc()->init();
    renderDescriptor->setVertexFunction(vertex);
    renderDescriptor->setFragmentFunction(fragment);
    renderDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    if(buildArchive && !archive->addRenderPipelineFunctions(renderDescriptor, &error))
    {
      TEST_WARN("Could not add render functions to archive");
      return 4;
    }
    renderDescriptor->setBinaryArchives(archiveArray);
    MTL::AutoreleasedRenderPipelineReflection renderReflection = NULL;
    MTL::RenderPipelineState *renderState = NULL;
    if(async)
    {
      dispatch_group_enter(group);
      device->newRenderPipelineState(renderDescriptor,
          MTL::PipelineOptionFailOnBinaryArchiveMiss,
          [&](MTL::RenderPipelineState *result, MTL::RenderPipelineReflection *, NS::Error *failure) {
            if(failure || !result) asyncFailed = true;
            if(result) renderState = result->retain();
            dispatch_group_leave(group);
          });
      if(dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC)))
        asyncFailed = true;
    }
    else
      renderState = device->newRenderPipelineState(
          renderDescriptor, MTL::PipelineOptionFailOnBinaryArchiveMiss,
          &renderReflection, &error);
    renderDescriptor->release();
    if(asyncFailed || !renderState) { TEST_WARN("Render archive miss"); return 4; }

    MTL::Buffer *output = device->newBuffer(128, MTL::ResourceStorageModeShared);
    bool failed = false;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *command = queue->commandBuffer();
      MTL::ComputeCommandEncoder *computeEncoder = command->computeCommandEncoder();
      computeEncoder->setComputePipelineState(computeState);
      computeEncoder->setBuffer(output, 0, 0);
      computeEncoder->dispatchThreads(MTL::Size::Make(32, 1, 1), MTL::Size::Make(32, 1, 1));
      computeEncoder->endEncoding();
      MTL::RenderCommandEncoder *render = command->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->setRenderPipelineState(renderState);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding();
      command->presentDrawable(drawable);
      command->commit();
      command->waitUntilCompleted();
      EndCaptureFrame();
      if(command->error()) failed = true;
      if(native)
      {
        const uint32_t *values = (const uint32_t *)output->contents();
        for(uint32_t i = 0; i < 32; i++)
          if(values[i] != i + 17) failed = true;
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(200, 150, 1, 1), 0);
        if(pixel[0] < 76 || pixel[0] > 77 || pixel[1] < 178 || pixel[1] > 179 ||
           pixel[2] != 51 || pixel[3] != 255) failed = true;
      }
      pool->drain();
    }
    output->release();
    renderState->release();
    computeState->release();
    fragment->release();
    vertex->release();
    compute->release();
    library->release();
    archive->release();
    if(failed) TEST_WARN("Binary archive GPU output mismatch");
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
