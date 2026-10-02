// SPDX-License-Identifier: MIT
#include "renderdoc/driver/metal/metal_indirect_readback.h"
#include <cstdio>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
int main()
{
  auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
  auto device=NS::TransferPtr(MTL::CreateSystemDefaultDevice());
  auto queue=NS::TransferPtr(device->newCommandQueue());
  const char *code=R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void writer(device uint *args [[buffer(0)]], constant uint3 &counts [[buffer(1)]])
{args[0]=counts.x;args[1]=counts.y;args[2]=counts.z;}
kernel void consumer(device atomic_uint *out [[buffer(0)]], constant uint &weight [[buffer(1)]])
{atomic_fetch_add_explicit(out,weight,memory_order_relaxed);atomic_fetch_add_explicit(out+1,1u,memory_order_relaxed);}
kernel void buffer_consumer(device atomic_uint *out [[buffer(0)]], device const uint *weight [[buffer(1)]])
{atomic_fetch_add_explicit(out,weight[0],memory_order_relaxed);atomic_fetch_add_explicit(out+1,1u,memory_order_relaxed);}
)MSL";
  NS::Error *error=NULL;
  auto library=NS::TransferPtr(device->newLibrary(NS::String::string(code,NS::UTF8StringEncoding),NULL,&error));
  if(!library)return 2;
  auto makePipeline=[&](const char *name) {
    auto function=NS::TransferPtr(library->newFunction(NS::String::string(name,NS::UTF8StringEncoding)));
    return NS::TransferPtr(device->newComputePipelineState(function.get(),&error));
  };
  auto writer=makePipeline("writer"), consumer=makePipeline("consumer"), bufferConsumer=makePipeline("buffer_consumer");
  if(!writer||!consumer||!bufferConsumer)return 3;
  for(unsigned concurrent=0;concurrent<2;concurrent++)for(unsigned buffer=0;buffer<2;buffer++) {
    auto args=NS::TransferPtr(device->newBuffer(64,MTL::ResourceStorageModePrivate));
    auto output=NS::TransferPtr(device->newBuffer(64,MTL::ResourceStorageModeShared));
    auto weights=NS::TransferPtr(device->newBuffer(16,MTL::ResourceStorageModeShared));
    auto finalArgs=NS::TransferPtr(device->newBuffer(64,MTL::ResourceStorageModeShared));
    if(!args||!output||!weights||!finalArgs)return 4;
    uint32_t *out=(uint32_t *)output->contents();for(unsigned i=0;i<16;i++)out[i]=0x13572468;
    out[4]=out[5]=0;((uint32_t *)weights->contents())[2]=7;
    MTL::CommandBuffer *command=queue->commandBufferWithUnretainedReferences();
    MTL::ComputeCommandEncoder *encoder=command->computeCommandEncoder(concurrent?MTL::DispatchTypeConcurrent:MTL::DispatchTypeSerial);
    MetalComputeIndirectCapture capture;
    rdcarray<NS::SharedPtr<MTL::Buffer>> snapshots;
    const uint32_t counts[]={1,3,2};
    for(unsigned i=0;i<3;i++) {
      uint32_t groupCounts[4]={counts[i],1,1,0};rdcarray<byte> bytes((byte *)groupCounts,sizeof(groupCounts));
      encoder->setComputePipelineState(writer.get());capture.BindPipeline(writer.get());
      encoder->setBuffer(args.get(),16,0);capture.BindBuffer(args.get(),16,0);
      encoder->setBytes(bytes.data(),bytes.size(),1);capture.BindBytes(bytes,1);
      encoder->dispatchThreadgroups(MTL::Size(1,1,1),MTL::Size(1,1,1));encoder->memoryBarrier(MTL::BarrierScopeBuffers);
      auto pipeline=buffer?bufferConsumer.get():consumer.get();
      encoder->setComputePipelineState(pipeline);capture.BindPipeline(pipeline);
      encoder->setBuffer(output.get(),16,0);capture.BindBuffer(output.get(),16,0);
      if(buffer) {
        encoder->setBuffer(weights.get(),4,1);capture.BindBuffer(weights.get(),4,1);
        encoder->setBufferOffset(8,1);capture.SetBufferOffset(8,1);
      } else {
        const uint32_t weight=(i+1)*10;rdcarray<byte> data((const byte *)&weight,sizeof(weight));
        encoder->setBytes(data.data(),data.size(),1);capture.BindBytes(data,1);
      }
      MetalIndirectReadback readback;
      if(!capture.Snapshot(device.get(),encoder,args.get(),16,readback))return 5;
      // The completion handler retains all debug resources, including a copy PSO
      // released by Clear before unretained submission. Only the snapshot stays here.
      snapshots.push_back(readback.snapshot);
      command->addCompletedHandler([readback](MTL::CommandBuffer *) {});
      encoder->dispatchThreadgroups(args.get(),16,MTL::Size(1,1,1));
    }
    const uint32_t zero[4]={0,1,1,0};encoder->memoryBarrier(MTL::BarrierScopeBuffers);
    encoder->setComputePipelineState(writer.get());encoder->setBuffer(args.get(),16,0);encoder->setBytes(zero,sizeof(zero),1);
    encoder->dispatchThreadgroups(MTL::Size(1,1,1),MTL::Size(1,1,1));encoder->endEncoding();capture.Clear();
    auto blit=command->blitCommandEncoder();blit->copyFromBuffer(args.get(),0,finalArgs.get(),0,64);blit->endEncoding();
    command->commit();command->waitUntilCompleted();if(command->error())return 6;
    for(unsigned i=0;i<3;i++) {
      const uint32_t *saved=(const uint32_t *)snapshots[i]->contents();
      if(saved[0]!=counts[i]||saved[1]!=1||saved[2]!=1||saved[3]!=0x52444349)return 7;
    }
    const uint32_t *tail=(const uint32_t *)finalArgs->contents();
    if(tail[4]!=0||tail[5]!=1||tail[6]!=1||out[4]!=(buffer?42U:130U)||out[5]!=6)return 8;
    for(unsigned i=0;i<16;i++)if(i!=4&&i!=5&&out[i]!=0x13572468)return 9;
    printf("PASS Native capture component: concurrent=%u buffer=%u per-use=1/3/2 final=0 output=%u count=6 unretained-CB debug lifetime\n",concurrent,buffer,out[4]);
  }
  return 0;
}
