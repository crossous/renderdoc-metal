// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
REPLAY_PROGRAM_MARKER()
static void Dispatches(const rdcarray<ActionDescription> &actions,rdcarray<uint32_t> &events)
{for(const auto &a:actions){if(a.flags&ActionFlags::Dispatch)events.push_back(a.eventId);Dispatches(a.children,events);}}
static uint32_t CopyEvent(const rdcarray<ActionDescription> &actions,ResourceId destination)
{for(const auto &a:actions){if((a.flags&ActionFlags::Copy) && a.copyDestination==destination)return a.eventId;
  const auto child=CopyEvent(a.children,destination);if(child)return child;}return 0;}
static uint64_t ResourceIdNum(ResourceId id) { uint64_t n=0;memcpy(&n,&id,sizeof(n));return n; }
int main(int argc,char **argv)
{
  if(argc!=2)return 1;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",NULL);
  if(!result.OK()){file->Shutdown();RENDERDOC_ShutdownReplay();return 2;}
  IReplayController *controller=NULL;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),NULL);file->Shutdown();
  if(!result.OK() || !controller){RENDERDOC_ShutdownReplay();return 3;}
  ResourceId contributions;
  ResourceId a,b,cbv,heap,indices,source,counter,dimensionTexture,readback,dimensionBacking,queryStructure,partialInput,birthPrefix;
  const unsigned queryCreationInput=getenv("RENDERDOC_METAL_RUNTIME_QUERY_CREATION_INPUT")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_CREATION_INPUT")):0;
  const bool queryPartialAlias=getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_ALIAS")!=nullptr;
  const bool queryPartialHeap=getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_HEAP")!=nullptr;
  for(const auto &r:controller->GetResources())
  {
    if(r.name=="Runtime disjoint birth prefix")birthPrefix=r.resourceId;
    if(r.name=="Runtime frame texture dimensions")dimensionTexture=r.resourceId;
    if(r.name=="Runtime texture readback")readback=r.resourceId;
    if(r.name=="Runtime frame view backing")dimensionBacking=r.resourceId;
    if(r.name=="Runtime scalar counter")counter=r.resourceId;
    if(r.name=="Runtime query contributions")contributions=r.resourceId;
    if(r.name=="Runtime query TLAS")queryStructure=r.resourceId;
    if(r.name=="Runtime output A")a=r.resourceId;
    if(r.name=="Runtime output B")b=r.resourceId;
    if(r.name=="Runtime private CBV")cbv=r.resourceId;
    if(r.name=="Runtime resource heap")heap=r.resourceId;
    if(r.name=="Runtime loop indices")indices=r.resourceId;
    if(r.name=="Runtime loop source")source=r.resourceId;
  }
  for(const auto *chunk:controller->GetStructuredFile().chunks)
  {
    if(chunk->name=="MTLBuffer::DeclareRayASHeader")
      queryStructure=chunk->FindChild("structure")->AsResourceId();
    if(queryPartialHeap && chunk->name=="MTLBuffer::DescriptorSlotBinding" &&
       chunk->FindChild("buffer")->AsResourceId()==heap &&
       chunk->FindChild("offset")->AsUInt64()==72 && chunk->FindChild("kind")->AsUInt64()==0)
      partialInput=chunk->FindChild("resource")->AsResourceId();
  }
  rdcarray<uint32_t> events;Dispatches(controller->GetRootActions(),events);
  const bool scalarCounter=counter!=ResourceId();
  const bool textureAtomics=getenv("RENDERDOC_METAL_RUNTIME_TEXTURE_ATOMICS")!=nullptr;
  const bool textureBufferView=getenv("RENDERDOC_METAL_RUNTIME_TEXTURE_BUFFER_VIEW")!=nullptr;
  const bool viewSharedRoot=getenv("RENDERDOC_METAL_RUNTIME_VIEW_SHARED_ROOT")!=nullptr;
  const uint32_t secondCounter=scalarCounter && events.size()==4?events[2]:0;
  const uint32_t imageCopy=textureAtomics?CopyEvent(controller->GetRootActions(),readback):0;
  const uint32_t firstProducer=scalarCounter && (events.size()==4 || events.size()==6)?events[0]:0;
  const uint32_t firstTextureProducer=textureAtomics && events.size()==6?events[1]:0;
  const uint32_t secondTextureProducer=textureAtomics && events.size()==6?events[4]:0;
  if(textureAtomics && events.size()==6)events={events[2],events[5]};
  if(scalarCounter && events.size()==4)events={events[1],events[3]};
  const bool partialGPUProducer=getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_GPU_PRODUCER")!=nullptr;
  const uint32_t partialWriter=partialGPUProducer && events.size()==3?events[0]:0;
  if(partialWriter)events={events[1],events[2]};
  const bool contributionProducer=getenv("RENDERDOC_METAL_RUNTIME_QUERY_CONTRIBUTION_PRODUCER")!=nullptr;
  const uint32_t contributionWriter=contributionProducer && events.size()==3?events[0]:0;
  if(contributionWriter)events={events[1],events[2]};
  bool ok=a!=ResourceId() && b!=ResourceId() && cbv!=ResourceId() && heap!=ResourceId() && events.size()==2;
  unsigned checks=0,descriptors=0;
  const bool loop=indices!=ResourceId();
  const bool queryGroupIndices=getenv("RENDERDOC_METAL_RUNTIME_QUERY_GROUP_INDICES")!=nullptr;
  const bool queryDynamicHeap=getenv("RENDERDOC_METAL_RUNTIME_QUERY_DYNAMIC_HEAP")!=nullptr;
  const bool nativeRayQuery=getenv("RENDERDOC_METAL_RUNTIME_NATIVE_QUERY")!=nullptr;
  const bool readModifyWrite=getenv("RENDERDOC_METAL_RUNTIME_READ_MODIFY_WRITE")!=nullptr;
  const unsigned rmwOffset=(readModifyWrite || nativeRayQuery) && getenv("RENDERDOC_METAL_RUNTIME_RMW_OFFSET")?atoi(getenv("RENDERDOC_METAL_RUNTIME_RMW_OFFSET")):0;
  const uint32_t positionsA[4]={3,12,27,55},positionsB[4]={7,19,35,63};
  rdcarray<uint32_t> selections;
  if(ok)selections={0U,events[0]-1,events[0],events[1]-1,events[1],~0U,events[0],0U,events[1],events[0]-1,events[0],0U};
  ResourceId partialProducer=partialInput;
  if(queryPartialAlias)
  {
    ResourceId placementHeap;
    for(const auto *c:controller->GetStructuredFile().chunks)
      if(c->name=="MTLHeap::newBuffer(offset)" && c->FindChild("Buffer")->AsResourceId()==partialInput)
        placementHeap=c->FindChild("Heap")->AsResourceId();
    for(const auto *c:controller->GetStructuredFile().chunks)
      if(c->name=="MTLHeap::newBuffer(offset)" && c->FindChild("Heap")->AsResourceId()==placementHeap &&
         c->FindChild("Buffer")->AsResourceId()!=partialInput)partialProducer=c->FindChild("Buffer")->AsResourceId();
  }
  const uint32_t partialCopy=partialGPUProducer?partialWriter:queryPartialHeap?CopyEvent(controller->GetRootActions(),partialProducer):0;
  if(queryPartialHeap)
  {ok &= partialInput!=ResourceId() && (queryCreationInput || partialCopy!=0);if(!queryCreationInput)selections.append({partialCopy-1,partialCopy});}
  if(textureAtomics){ok &= readback!=ResourceId() && imageCopy!=0;
    selections.append({firstTextureProducer-1,firstTextureProducer,secondTextureProducer-1,secondTextureProducer,imageCopy-1,imageCopy});}
  if(contributionProducer){ok &= contributions!=ResourceId() && contributionWriter!=0;
    if(contributionWriter)selections.append({contributionWriter-1,contributionWriter});}
  const uint32_t birthPrefixCopy=birthPrefix!=ResourceId()?CopyEvent(controller->GetRootActions(),birthPrefix):0;
  if(getenv("RENDERDOC_METAL_RUNTIME_QUERY_BIRTH_PREFIX"))
  {
    ok &= birthPrefix!=ResourceId() && birthPrefixCopy!=0;
    if(birthPrefixCopy)selections.append({birthPrefixCopy-1,birthPrefixCopy});
  }
  if(ok)for(unsigned cycle=0;cycle<4;cycle++)
    for(uint32_t eid:selections)
    {
      controller->SetFrameEvent(eid,true);
      if(getenv("RENDERDOC_METAL_RUNTIME_QUERY_RETIRED_ROW"))
      {
        const unsigned row=4+(getenv("RENDERDOC_METAL_RUNTIME_QUERY_COLD_ROWS")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_COLD_ROWS")):0);
        const auto raw=controller->GetBufferData(heap,uint64_t(row)*24,24);
        uint64_t pointer=~0ULL,texture=~0ULL,metadata=0;
        if(raw.size()==24){memcpy(&pointer,raw.data(),8);memcpy(&texture,raw.data()+8,8);memcpy(&metadata,raw.data()+16,8);}
        ok &= raw.size()==24 && !pointer && !texture && metadata==256;
      }
      if(contributionProducer)
      {
        const unsigned offset=getenv("RENDERDOC_METAL_RUNTIME_QUERY_CONTRIBUTION_OFFSET")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_CONTRIBUTION_OFFSET")):0;
        const unsigned instances=atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_INSTANCES"));
        const auto all=controller->GetBufferData(contributions,0,offset+instances*4);ok &= all.size()==offset+instances*4;
        for(unsigned i=0;i<all.size();i++)ok &= all[i]==(i<offset?0x6a:eid>=contributionWriter?0:0xff);
      }
      const bool first=eid>=events[0],second=eid>=events[1];
      if(birthPrefixCopy && eid>=birthPrefixCopy)
      {
        const auto prefix=controller->GetBufferData(birthPrefix,0,4);
        uint32_t word=0;if(prefix.size()==4)memcpy(&word,prefix.data(),4);
        ok &= prefix.size()==4 && word==3033;
      }
      if(queryPartialHeap && (queryCreationInput?eid>=events[0]:eid>=partialCopy) && (!queryPartialAlias || eid>=events[0]))
      {
        const unsigned offset=atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_OFFSET"));
        const auto defined=controller->GetBufferData(partialInput,offset,4);
        uint32_t word=0;if(defined.size()==4)memcpy(&word,defined.data(),4);
        ok &= defined.size()==4 && word==(queryCreationInput && queryCreationInput<3?0U:3033U);
        if(queryCreationInput)
        {
          const unsigned length=atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_SIZE"));
          const auto all=controller->GetBufferData(partialInput,0,length);ok &= all.size()==length;
          if(all.size()==length)for(unsigned i=0;i<length;i++)
          {
            byte expected=queryCreationInput>=3?9:0;
            if(queryCreationInput>=3 && i>=offset && i<offset+4)expected=(3033U>>(8*(i-offset)))&255;
            ok &= all[i]==expected;
          }
        }
        // Earlier events/EID0 and padding have no defined value in the Native
        // program. Do not invent an initial-state oracle for those bytes.
      }
      for(const auto &entry:{rdcpair<ResourceId,unsigned>(a,3+rmwOffset),rdcpair<ResourceId,unsigned>(b,7+rmwOffset)})
      {
        const auto bytes=controller->GetBufferData(entry.first,0,256);ok &= bytes.size()==256;
        if(bytes.size()!=256)continue;
        uint32_t values[64];memcpy(values,bytes.data(),sizeof(values));
        for(unsigned i=0;i<64;i++)
        {
          const bool written=entry.first==a?first:second;
          uint32_t expected=written && i==entry.second?(entry.first==a?123U:456U)+unsigned(readModifyWrite):0x09090909U;
          if(queryGroupIndices && written && i>=entry.second && i<entry.second+3)
            expected=entry.first==a?123U:456U;
          if(getenv("RENDERDOC_METAL_RUNTIME_LITERAL_PROJECTION") && written &&
             i>=entry.second && i<entry.second+4)
            expected=(entry.first==a?123U:456U)+i-entry.second+unsigned(readModifyWrite);
          if(loop)
          {
            expected=0x09090909U;
            const auto positions=entry.first==a?positionsA:positionsB;
            if(written)for(unsigned j=0;j<4;j++)if(i==positions[j])expected=11*(j+1)+(entry.first==a?123:456);
          }
          if(queryDynamicHeap && i==63)expected=entry.first==a?1011:2022;
          if(values[i]!=expected && cycle==0)
            printf("output mismatch EID=%u index=%u actual=%u expected=%u\n",eid,i,values[i],expected);
          ok &= values[i]==expected;
        }
      }
      if(scalarCounter)
      {
        const auto bytes=controller->GetBufferData(counter,0,64);ok &= bytes.size()==64;
        if(bytes.size()==64)for(unsigned i=0;i<64;i++)ok &= bytes[i]==(eid>=firstProducer && i<4?0:9);
      }
      if(textureAtomics && eid>=firstTextureProducer)
      {
        const auto image=controller->GetTextureData(dimensionTexture,Subresource());
        ok &= image.size()==256;
        const bool initializedAgain=eid>=secondTextureProducer;
        const bool added=initializedAgain?second:first;
        if(image.size()==256)for(unsigned i=0;i<64;i++)
        {
          uint32_t value;memcpy(&value,image.data()+i*4,4);
          ok &= value==0x12345+i+(added?1:0);
        }
      }
      if(textureAtomics)
      {
        const auto bytes=controller->GetBufferData(readback,0,2112);ok &= bytes.size()==2112;
        if(bytes.size()==2112)for(unsigned i=0;i<2112;i++)
        {
          const unsigned relative=i>=32?i-32:2112,row=relative/256,column=relative%256;
          byte expected=9;
          if(eid>=imageCopy && row<8 && column<32)expected=(0x12346+row*8+column/4)>>(8*(column%4));
          ok &= bytes[i]==expected;
        }
      }
      if(textureBufferView && eid!=0)
      {
        const auto bytes=controller->GetBufferData(dimensionBacking,0,1024);
        ok &= dimensionBacking!=ResourceId() && bytes.size()==1024;
        if(bytes.size()==1024)for(unsigned i=0;i<256;i++)
        {
          // The original API program leaves these texels undefined until its
          // first Native writer. Do not invent initial zero/padding values.
          if(getenv("RENDERDOC_METAL_RUNTIME_PARTIAL_VIEW_INIT") && !first && i>=68 && i<128)continue;
          uint32_t value;memcpy(&value,bytes.data()+i*4,4);
          const bool secondRoot=eid>=secondCounter;
          const uint32_t parameters[4]={secondRoot?2U:1U,secondRoot?7U:3U,
              getenv("RENDERDOC_METAL_RUNTIME_CALL_EFFECTS")?(secondRoot?568U:901U):(secondRoot?456U:123U),1};
          const uint32_t expected=viewSharedRoot && i>=4 && i<8?parameters[i-4]:
              first && i>=64 && i<128?0x12345+i-64+(second?2:1):0x09090909;
          ok &= value==expected;
        }
      }
      auto bytes=controller->GetBufferData(cbv,0,64);ok &= bytes.size()==64;
      if(bytes.size()==64)
        for(unsigned i=0;i<64;i++)if(i<16 || i>=(loop?40U:32U) || eid==0)ok &= bytes[i]==9;
      if(loop)
      {
        const auto input=controller->GetBufferData(indices,0,64),values=controller->GetBufferData(source,0,64);
        ok &= source!=ResourceId() && input.size()==64 && values.size()==64;
        if(input.size()==64)for(unsigned i=0;i<64;i++)if(i<16 || i>=32 || eid==0)ok &= input[i]==9;
        if(values.size()==64)
        {
          uint32_t words[16];memcpy(words,values.data(),64);
          for(unsigned i=0;i<16;i++)ok &= words[i]==(i<4?11*(i+1):0x09090909U);
        }
      }
      if(eid==events[0] || eid==events[1])
      {
        bool target=false,indexRead=false,sourceRead=false,dimensionQuery=false,queryAccess=false;
        for(const auto &access:controller->GetDescriptorAccess())
        {
          if(access.type==DescriptorType::AccelerationStructure)
          {
            ok &= nativeRayQuery && access.descriptorStore==heap && access.byteOffset==0;
            DescriptorRange range;range.offset=0;range.count=1;range.descriptorSize=24;range.type=access.type;
            const auto values=controller->GetDescriptors(heap,{range});
            const auto locations=controller->GetDescriptorLocations(heap,{range});
            ok &= queryStructure!=ResourceId() && values.size()==1 && values[0].resource==queryStructure &&
                values[0].type==DescriptorType::AccelerationStructure && locations.size()==1 &&
                locations[0].category==DescriptorCategory::ReadOnlyResource;
            if(cycle==0)printf("query descriptor EID=%u count=%zu actual=%llu expected=%llu type=%u location=%u\n",eid,values.size(),values.empty()?0ULL:(unsigned long long)ResourceIdNum(values[0].resource),(unsigned long long)ResourceIdNum(queryStructure),values.empty()?0U:unsigned(values[0].type),locations.empty()?0U:unsigned(locations[0].category));
            queryAccess=true;
          }
          if(dimensionTexture!=ResourceId() && access.descriptorStore==heap &&
             access.type==(textureBufferView?DescriptorType::ReadWriteTypedBuffer:
               textureAtomics?DescriptorType::ReadWriteImage:DescriptorType::Image) && access.byteOffset==96)
          {
            DescriptorRange range;range.offset=96;range.count=1;range.descriptorSize=24;range.type=access.type;
            const auto values=controller->GetDescriptors(heap,{range});
            const auto locations=controller->GetDescriptorLocations(heap,{range});
            TextureDescription description;
            for(const auto &texture:controller->GetTextures())
              if(texture.resourceId==dimensionTexture)description=texture;
            ok &= values.size()==1 && values[0].resource==(textureBufferView?dimensionBacking:dimensionTexture) &&
                (!textureBufferView || (values[0].view==dimensionTexture && values[0].byteOffset==256 && values[0].byteSize==256)) &&
                locations.size()==1 && locations[0].category==(textureAtomics || textureBufferView?
                  DescriptorCategory::ReadWriteResource:DescriptorCategory::ReadOnlyResource) &&
                description.width==(textureBufferView?64U:8U) && description.height==(textureBufferView?1U:8U);
            dimensionQuery=true;
          }
          if(access.descriptorStore==heap && access.type==DescriptorType::ReadWriteBuffer &&
             access.byteOffset==(eid==events[0]?24U:48U))
          {
            target=true;
            DescriptorRange range;range.offset=access.byteOffset;range.count=1;
            range.descriptorSize=24;range.type=access.type;
            const auto values=controller->GetDescriptors(heap,{range});
            const auto locations=controller->GetDescriptorLocations(heap,{range});
            ok &= values.size()==1 && values[0].resource==(eid==events[0]?a:b) &&
                values[0].type==DescriptorType::ReadWriteBuffer &&
                values[0].byteOffset==0 && values[0].byteSize==256 && locations.size()==1 &&
                locations[0].category==DescriptorCategory::ReadWriteResource;
          }
          if(loop && access.descriptorStore==heap && access.type==DescriptorType::Buffer &&
             (access.byteOffset==72 || access.byteOffset==96))
          {
            const bool index=access.byteOffset==72;
            DescriptorRange range;range.offset=access.byteOffset;range.count=1;
            range.descriptorSize=24;range.type=access.type;
            const auto values=controller->GetDescriptors(heap,{range});
            const auto locations=controller->GetDescriptorLocations(heap,{range});
            ok &= values.size()==1 && values[0].resource==(index?indices:source) &&
                values[0].type==DescriptorType::Buffer && values[0].byteOffset==(index?16U:0U) &&
                values[0].byteSize==16 && locations.size()==1 &&
                locations[0].category==DescriptorCategory::ReadOnlyResource;
            if(index)indexRead=true;else sourceRead=true;
          }
        }
        ok &= (!nativeRayQuery || queryAccess);
        ok &= target && (!loop || (indexRead && sourceRead)) &&
            (dimensionTexture==ResourceId() || dimensionQuery);descriptors++;
      }
      checks++;
    }
  controller->Shutdown();RENDERDOC_ShutdownReplay();
  printf("%s runtime dynamic typed buffer: 2 writes/full outputs/CBV padding, %u event/EID0 choices, %u public access checks, %s\n",ok?"PASS":"FAIL",checks,descriptors,nativeRayQuery?"actual AS query validated":"unused AS excluded");
  if(loop)printf("%s indexed loop: two 4-element scatters, complete indices/source/padding, %u three-resource public query points\n",ok?"PASS":"FAIL",descriptors);
  return ok?0:4;
}
