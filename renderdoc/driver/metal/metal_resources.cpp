/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2026 Baldur Karlsson
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

#include "metal_resources.h"
#include "metal_blit_command_encoder.h"
#include "metal_argument_encoder.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_command_queue.h"
#include "metal_compute_command_encoder.h"
#include "metal_compute_pipeline_state.h"
#include "metal_device.h"
#include "metal_depth_stencil_state.h"
#include "metal_function.h"
#include "metal_library.h"
#include "metal_render_command_encoder.h"
#include "metal_parallel_render_command_encoder.h"
#include "metal_render_pipeline_state.h"
#include "metal_sampler_state.h"
#include "metal_fence.h"
#include "metal_event.h"
#include "metal_heap.h"
#include "metal_rate_map.h"
#include "metal_counter_sample_buffer.h"
#include "metal_dynamic_library.h"
#include "metal_binary_archive.h"
#include "metal_visible_function_table.h"
#include "metal_acceleration_structure.h"
#include "metal_acceleration_structure_command_encoder.h"
#include "metal_indirect_command_buffer.h"
#include "metal_texture.h"

ResourceId GetResID(WrappedMTLObject *obj)
{
  if(obj == NULL)
    return ResourceId();

  return obj->m_ID;
}

#define IMPLEMENT_WRAPPED_TYPE_HELPERS(CPPTYPE)  \
  MTL::CPPTYPE *Unwrap(WrappedMTL##CPPTYPE *obj) \
  {                                              \
    return Unwrap<MTL::CPPTYPE *>(obj);          \
  }
METALCPP_WRAPPED_PROTOCOLS(IMPLEMENT_WRAPPED_TYPE_HELPERS)
#undef IMPLEMENT_WRAPPED_TYPE_HELPERS

MetalResourceManager *WrappedMTLObject::GetResourceManager()
{
  return m_Device->GetResourceManager();
}

bool MetalResourceRecord::MarkResourceFrameReferenced(ResourceId id, FrameRefType type)
{
  // Native Metal encoders retain immutable state even when the application's
  // proxy goes out of scope before submission. Preserve its creation record
  // with the standard ResourceRecord parent ownership used by other drivers.
  // Keeping a descriptor record does not retain writable GPU resources.
  MetalResourceRecord *state = NULL;
  if(id != ResourceId() && m_Resource)
    state = m_Resource->GetResourceManager()->GetResourceRecord(id);
  LockChunks();
  const bool added = ResourceRecord::MarkResourceFrameReferenced(id, type);
  // A texture view is an API object, not independent storage. Its creation
  // parent record alone retains the factory but does not reference the parent's
  // initial contents. As Vulkan image descriptors refer to their base image,
  // record the real backing when an encoder uses a view. A subresource view
  // write cannot establish a full overwrite of the parent allocation.
  if(state && state->m_Type == eResTexture && state->textureParent != ResourceId())
    ResourceRecord::MarkResourceFrameReferenced(state->textureParent, eFrameRef_ReadBeforeWrite);
  if(state && (state->m_Type == eResDepthStencilState || state->m_Type == eResSamplerState))
    AddParent(state);
  UnlockChunks();
  return added;
}

MetalResourceRecord::~MetalResourceRecord()
{
  if(m_Type == eResCommandBuffer)
    SAFE_DELETE(cmdInfo);
  else if(m_Type == eResBuffer)
    SAFE_DELETE(bufInfo);
}

void MetalResourceRecord::DiscardBackgroundBufferMarkers()
{
  // removeAllDebugMarkers supersedes earlier background annotations, not buffer creation/data.
  LockChunks();
  for(size_t i = m_Chunks.size(); i > 0; --i)
  {
    const MetalChunk type = m_Chunks[i - 1].chunk->GetChunkType<MetalChunk>();
    if(type == MetalChunk::MTLBuffer_addDebugMarker ||
       type == MetalChunk::MTLBuffer_removeAllDebugMarkers)
    {
      m_Chunks[i - 1].chunk->Delete(m_Chunks[i - 1].fromAllocator != 0);
      m_Chunks.erase(i - 1);
    }
  }
  UnlockChunks();
}

void WrappedMTLObject::AddEvent()
{
  m_Device->AddEvent();
}

void WrappedMTLObject::AddAction(const ActionDescription &a)
{
  m_Device->AddAction(a);
}

bool MetalResourceRecord::HasOnlyASInitialCommands()
{
  LockChunks();
  bool valid = true;
  for(const StoredChunk &stored : m_Chunks)
  {
    const MetalChunk chunk = stored.chunk->GetChunkType<MetalChunk>();
    if(ToStr(chunk).beginsWith("MTLAccelerationStructureCommandEncoder::")) continue;
    if(chunk == MetalChunk::MTLCommandQueue_commandBuffer ||
       chunk == MetalChunk::MTLCommandQueue_commandBufferWithDescriptor ||
       chunk == MetalChunk::MTLCommandQueue_commandBufferWithUnretainedReferences ||
       chunk == MetalChunk::MTLCommandBuffer_accelerationStructureCommandEncoder ||
       chunk == MetalChunk::MTLCommandBuffer_accelerationStructureCommandEncoderWithDescriptor ||
       chunk == MetalChunk::MTLCommandBuffer_pushDebugGroup ||
       chunk == MetalChunk::MTLCommandBuffer_popDebugGroup ||
       chunk == MetalChunk::MTLBuffer_InternalModifyCPUContents ||
       chunk == MetalChunk::MTLCommandBuffer_enqueue) continue;
    valid = false;
    if(getenv("RENDERDOC_METAL_TRACE_INITIAL_AS"))
      fprintf(stderr, "Metal AS initial input excludes command %s\n", ToStr(chunk).c_str());
    break;
  }
  UnlockChunks();
  return valid;
}

void MetalResourceRecord::MarkASInitialReferences(WrappedMTLAccelerationStructure *structure)
{
  if(!structure) return;
  MarkResourceFrameReferenced(GetResID(structure), eFrameRef_Read);
  if(structure->m_LastCompactedWriteCommandBuffer != ResourceId() &&
     structure->m_LastCompactedWriteCommandBuffer == structure->m_LastBuildCommandBuffer)
    MarkResourceFrameReferenced(structure->m_LastCompactedSizeBuffer, eFrameRef_Read);
  const auto build = structure->m_CapturedInitialBuild;
  if(!build) return;
  MarkResourceFrameReferenced(build->source, eFrameRef_Read);
  MarkResourceFrameReferenced(build->indexSource, eFrameRef_Read);
  // Initial TLAS support permits primitive children only; keep this walk bounded.
  for(ResourceId child : build->children)
  {
    MarkResourceFrameReferenced(child, eFrameRef_Read);
    auto object = structure->GetResourceManager()->GetResource(child, true);
    if(object && object->m_Type == eResAccelerationStructure)
    {
      const auto input = ((WrappedMTLAccelerationStructure *)object)->m_CapturedInitialBuild;
      if(input)
      {
        MarkResourceFrameReferenced(input->source, eFrameRef_Read);
        MarkResourceFrameReferenced(input->indexSource, eFrameRef_Read);
      }
    }
  }
}
