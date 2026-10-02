// SPDX-License-Identifier: MIT
#include "metal_render_indirect_readback.h"

MetalIndirectWriteFootprint GetMetalIndirectWriteFootprint(MTL::Resource *resource, bool texture)
{
  MetalIndirectWriteFootprint footprint;
  if(!resource)return footprint;
  if(texture)
  {
    MTL::Texture *parent=(MTL::Texture *)resource;
    for(uint32_t depth=0;depth<8;depth++)
    {
      if(parent->buffer()) { resource=parent->buffer();texture=false;break; }
      MTL::Texture *next=parent->parentTexture();
      if(!next) { resource=parent;break; }
      if(next==parent)return footprint;
      parent=next;
      if(depth==7)return footprint;
    }
  }
  footprint.owner=NS::RetainPtr(resource);
  footprint.heap=NS::RetainPtr(resource->heap());
  footprint.buffer=!texture;
  if(footprint.heap)
  {
    const uint64_t size=resource->allocatedSize();
    footprint.begin=resource->heapOffset();
    if(!size || footprint.begin>footprint.heap->size() || size>footprint.heap->size()-footprint.begin)
      return footprint;
    footprint.end=footprint.begin+size;
  }
  else if(footprint.buffer)
  {
    MTL::Buffer *buffer=(MTL::Buffer *)resource;
    footprint.begin=buffer->gpuAddress();
    if(!footprint.begin || !buffer->length() || buffer->length()>UINT64_MAX-footprint.begin)return footprint;
    footprint.end=footprint.begin+buffer->length();
  }
  footprint.valid=true;
  return footprint;
}

bool MetalIndirectSourceDisjoint(MTL::Buffer *source,
    const rdcarray<MetalIndirectWriteFootprint> &writes)
{
  const auto arguments=GetMetalIndirectWriteFootprint(source,false);
  if(!arguments.valid)return false;
  for(const auto &write:writes)
  {
    if(!write.valid || !write.owner || write.owner->device()!=source->device())return false;
    if(write.owner.get()==arguments.owner.get())return false;
    if(write.buffer)
    {
      MTL::Buffer *buffer=(MTL::Buffer *)write.owner.get();
      const uint64_t sourceVA=source->gpuAddress(), writeVA=buffer->gpuAddress();
      if(!sourceVA || !writeVA || source->length()>UINT64_MAX-sourceVA ||
         buffer->length()>UINT64_MAX-writeVA)return false;
      if(sourceVA<writeVA+buffer->length() && writeVA<sourceVA+source->length())return false;
    }
    if(write.heap && arguments.heap && write.heap.get()==arguments.heap.get())
    {
      if(write.begin<arguments.end && arguments.begin<write.end)return false;
    }
    else if(!write.heap && !arguments.heap && write.buffer &&
            write.begin<arguments.end && arguments.begin<write.end)return false;
  }
  return true;
}

bool CopyMetalRenderIndirectArguments(MTL::Device *device, MTL::CommandBuffer *command,
    MTL::Buffer *source, uint64_t offset, uint32_t wordCount,
    const rdcarray<MetalIndirectWriteFootprint> &writes, MetalIndirectReadback &result)
{
  if(!device || !command || command->device()!=device || !source || source->device()!=device ||
     (wordCount!=4 && wordCount!=5) || (offset&3) || offset>source->length() ||
     wordCount*4>source->length()-offset || !MetalIndirectSourceDisjoint(source,writes))return false;
  if(command->status()!=MTL::CommandBufferStatusNotEnqueued &&
     command->status()!=MTL::CommandBufferStatusEnqueued)return false;
  auto snapshot=NS::TransferPtr(device->newBuffer(wordCount*4,MTL::ResourceStorageModeShared));
  if(!snapshot)return false;
  auto encoder=command->blitCommandEncoder();
  if(!encoder)return false;
  encoder->copyFromBuffer(source,offset,snapshot.get(),0,wordCount*4);
  encoder->endEncoding();
  result.snapshot=snapshot;result.source=NS::RetainPtr(source);result.copyPipeline.reset();
  command->addCompletedHandler([owners=result](MTL::CommandBuffer *) {});
  return true;
}
