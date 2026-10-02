// SPDX-License-Identifier: MIT
#include "renderdoc/driver/metal/metal_render_indirect_readback.h"
#include <cstdio>
#include <cstring>
REPLAY_PROGRAM_MARKER()
int main()
{
  auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
  auto device=NS::TransferPtr(MTL::CreateSystemDefaultDevice());
  auto queue=NS::TransferPtr(device->newCommandQueue());
  NS::Error *error=NULL;
  const char *code=R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 triangle(uint index [[vertex_id]]) {
  float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[index%3],0,1);
}
fragment float4 color(){return float4(1,0.25,0.5,1);}
)MSL";
  auto library=NS::TransferPtr(device->newLibrary(NS::String::string(code,NS::UTF8StringEncoding),NULL,&error));
  if(!library)return 2;
  auto vertex=NS::TransferPtr(library->newFunction(NS::String::string("triangle",NS::UTF8StringEncoding)));
  auto fragment=NS::TransferPtr(library->newFunction(NS::String::string("color",NS::UTF8StringEncoding)));
  auto pd=NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
  pd->setVertexFunction(vertex.get());pd->setFragmentFunction(fragment.get());
  pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
  auto pipeline=NS::TransferPtr(device->newRenderPipelineState(pd.get(),&error));
  if(!pipeline)return 3;
  const auto options=(MTL::ResourceOptions)(MTL::ResourceStorageModePrivate|MTL::ResourceHazardTrackingModeTracked);
  auto td=NS::TransferPtr(MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA8Unorm,2,2,false)->copy());
  td->setStorageMode(MTL::StorageModePrivate);td->setHazardTrackingMode(MTL::HazardTrackingModeTracked);
  td->setUsage((MTL::TextureUsage)(MTL::TextureUsageRenderTarget|MTL::TextureUsagePixelFormatView));
  auto textureLayout=device->heapTextureSizeAndAlign(td.get());
  const uint64_t textureBufferAlignment=device->minimumTextureBufferAlignmentForPixelFormat(MTL::PixelFormatR32Uint);
  const uint64_t linearAlignment=device->minimumLinearTextureAlignmentForPixelFormat(MTL::PixelFormatR32Uint);
  const uint64_t bufferRow=textureBufferAlignment>linearAlignment?textureBufferAlignment:linearAlignment;
  if(!bufferRow || bufferRow>4096)return 15;
  const uint64_t argumentLength=bufferRow>96?bufferRow:96;
  auto argumentLayout=device->heapBufferSizeAndAlign(argumentLength,options);
  if(!textureLayout.align || !argumentLayout.size)return 4;
  const uint64_t textureOffset=(argumentLayout.size+textureLayout.align-1)/textureLayout.align*textureLayout.align;
  auto hd=NS::TransferPtr(MTL::HeapDescriptor::alloc()->init());
  hd->setType(MTL::HeapTypePlacement);hd->setStorageMode(MTL::StorageModePrivate);
  hd->setHazardTrackingMode(MTL::HazardTrackingModeTracked);hd->setSize(textureOffset+textureLayout.size);
  for(unsigned unretained=0;unretained<2;unretained++)for(unsigned parallel=0;parallel<2;parallel++)for(unsigned indexed=0;indexed<2;indexed++)
  {
    auto heap=NS::TransferPtr(device->newHeap(hd.get()));
    auto arguments=NS::TransferPtr(heap->newBuffer(argumentLength,options,0));
    auto texture=NS::TransferPtr(heap->newTexture(td.get(),textureOffset));
    auto upload=NS::TransferPtr(device->newBuffer(96,MTL::ResourceStorageModeShared));
    auto finalArgs=NS::TransferPtr(device->newBuffer(96,MTL::ResourceStorageModeShared));
    auto pixels=NS::TransferPtr(device->newBuffer(16,MTL::ResourceStorageModeShared));
    const uint16_t indices[6]={0,1,2,0,1,2};
    auto index=NS::TransferPtr(device->newBuffer(indices,sizeof(indices),MTL::ResourceStorageModeShared));
    if(!arguments||!texture||!upload||!pixels||!finalArgs||!index)return 5;
    if(!unretained && !parallel && !indexed)
      printf("Native placement heap state immediately after creation: argumentsAliasable=%u textureAliasable=%u\n",
          unsigned(arguments->isAliasable()),unsigned(texture->isAliasable()));
    auto textureView=NS::TransferPtr(texture->newTextureView(MTL::PixelFormatRGBA8Unorm));
    auto bufferDesc=NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    bufferDesc->setTextureType(MTL::TextureTypeTextureBuffer);bufferDesc->setPixelFormat(MTL::PixelFormatR32Uint);
    bufferDesc->setWidth(1);bufferDesc->setHeight(1);bufferDesc->setDepth(1);
    bufferDesc->setResourceOptions(options);bufferDesc->setAllowGPUOptimizedContents(false);
    bufferDesc->setUsage(MTL::TextureUsageShaderRead);
    auto bufferView=NS::TransferPtr(arguments->newTexture(bufferDesc.get(),0,bufferRow));
    if(!textureView || !bufferView)return 16;
    const uint32_t count=indexed?5:4;
    memset(upload->contents(),0,96);
    uint32_t *data=(uint32_t *)upload->contents();
    data[4]=3;data[5]=1;data[16]=6;data[17]=1;
    if(indexed) {data[8]=2;data[20]=2;} else {data[7]=2;data[19]=2;}
    rdcarray<MetalIndirectWriteFootprint> writes={GetMetalIndirectWriteFootprint(texture.get(),true)};
    if(!MetalIndirectSourceDisjoint(arguments.get(),writes)) {
      const auto args=GetMetalIndirectWriteFootprint(arguments.get(),false);
      fprintf(stderr,"Native footprint mismatch: args valid=%u heap=%p begin=%llu end=%llu texture valid=%u heap=%p begin=%llu end=%llu plannedOffset=%llu\n",
          unsigned(args.valid),args.heap.get(),(unsigned long long)args.begin,(unsigned long long)args.end,
          unsigned(writes[0].valid),writes[0].heap.get(),(unsigned long long)writes[0].begin,(unsigned long long)writes[0].end,
          (unsigned long long)textureOffset);return 6;
    }
    if(!MetalIndirectSourceDisjoint(arguments.get(),{GetMetalIndirectWriteFootprint(textureView.get(),true)}) ||
       MetalIndirectSourceDisjoint(arguments.get(),{GetMetalIndirectWriteFootprint(bufferView.get(),true)}) ||
       MetalIndirectSourceDisjoint(arguments.get(),{MetalIndirectWriteFootprint()}))return 17;
    auto invalid=writes;invalid.push_back(GetMetalIndirectWriteFootprint(arguments.get(),false));
    if(MetalIndirectSourceDisjoint(arguments.get(),invalid))return 7;
    // Placement aliases are queried, but never used by GPU work.
    {auto alias=NS::TransferPtr(heap->newBuffer(96,options,0));
      if(!alias || MetalIndirectSourceDisjoint(arguments.get(),{GetMetalIndirectWriteFootprint(alias.get(),false)}))return 8;}
    MTL::CommandBuffer *command=unretained?queue->commandBufferWithUnretainedReferences():queue->commandBuffer();
    auto initial=command->blitCommandEncoder();initial->copyFromBuffer(upload.get(),0,arguments.get(),0,96);initial->endEncoding();
    auto pass=MTL::RenderPassDescriptor::renderPassDescriptor();
    pass->colorAttachments()->object(0)->setTexture(texture.get());
    pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    auto parent=parallel?command->parallelRenderCommandEncoder(pass):NULL;
    auto render=parallel?parent->renderCommandEncoder():command->renderCommandEncoder(pass);
    render->setRenderPipelineState(pipeline.get());
    if(indexed) {
      render->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle,MTL::IndexTypeUInt16,index.get(),0,arguments.get(),16);
      render->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle,MTL::IndexTypeUInt16,index.get(),0,arguments.get(),64);
    } else {
      render->drawPrimitives(MTL::PrimitiveTypeTriangle,arguments.get(),16);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle,arguments.get(),64);
    }
    render->endEncoding();if(parent)parent->endEncoding();
    rdcarray<NS::SharedPtr<MTL::Buffer>> snapshots;
    for(uint64_t offset:{16ULL,64ULL}) {
      MetalIndirectReadback readback;
      if(!CopyMetalRenderIndirectArguments(device.get(),command,arguments.get(),offset,count,writes,readback))return 9;
      snapshots.push_back(readback.snapshot);
    }
    MetalIndirectReadback unused;
    if(CopyMetalRenderIndirectArguments(device.get(),command,arguments.get(),17,count,writes,unused) ||
       CopyMetalRenderIndirectArguments(device.get(),command,arguments.get(),argumentLength,count,writes,unused) ||
       CopyMetalRenderIndirectArguments(device.get(),command,arguments.get(),16,3,writes,unused) ||
       CopyMetalRenderIndirectArguments(device.get(),command,arguments.get(),16,count,invalid,unused))return 10;
    auto tail=command->blitCommandEncoder();tail->fillBuffer(arguments.get(),NS::Range(0,96),0);
    tail->copyFromBuffer(arguments.get(),0,finalArgs.get(),0,96);
    tail->copyFromTexture(texture.get(),0,0,MTL::Origin(0,0,0),MTL::Size(2,2,1),pixels.get(),0,8,16);
    tail->endEncoding();command->commit();command->waitUntilCompleted();if(command->error())return 11;
    for(unsigned i=0;i<2;i++)if(memcmp(snapshots[i]->contents(),data+(i?16:4),count*4))return 12;
    for(unsigned i=0;i<96;i++)if(((const byte *)finalArgs->contents())[i])return 13;
    const byte *result=(const byte *)pixels->contents();
    for(unsigned i=0;i<16;i+=4)if(result[i]!=255||result[i+1]!=64||result[i+2]!=128||result[i+3]!=255)return 14;
    printf("PASS Native pass-end indirect blit: unretained=%u parallel=%u indexed=%u args=3/6 final=0 pixels/aliases/bounds\n",unretained,parallel,indexed);
  }
  return 0;
}
