// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include <cmath>
#include "renderdoc/api/replay/renderdoc_replay.h"
template <> rdcstr DoStringise(const uint32_t &v) {char s[16];snprintf(s,sizeof(s),"%u",v);return s;}
#include "renderdoc/api/replay/pipestate.inl"
REPLAY_PROGRAM_MARKER()
static void Collect(const rdcarray<ActionDescription> &actions,rdcarray<const ActionDescription *> &out)
{
  for(const auto &a:actions) {out.push_back(&a);Collect(a.children,out);}
}
static bool Usage(IReplayController *c,ResourceId id,uint32_t eid,ResourceUsage type)
{
  for(auto u:c->GetUsage(id)) if(u.eventId==eid && u.usage==type)return true;
  return false;
}
int main(int argc,char **argv)
{
  if(argc<3)return 1;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);IReplayController *c=nullptr;
  if(result.OK())rdctie(result,c)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!c||!result.OK()){fprintf(stderr,"Open failed\n");return 2;}
  if(!strncmp(argv[2],"legacy-vrr",10))
  {
    rdcarray<const ActionDescription *> actions;Collect(c->GetRootActions(),actions);
    bool ok=true;unsigned maps=0;
    for(auto a:actions)
    {
      if(!(a->flags&(ActionFlags::Drawcall|ActionFlags::MeshDispatch)))continue;
      c->SetFrameEvent(a->eventId,true);
      const auto &r=c->GetPipelineState().GetMetalPipelineState()->rasterizer;
      if(r.rasterizationRateMap==ResourceId())continue;
      maps++;
      ok &= r.rateMapScreenSize==rdcarray<uint32_t>({400,300}) &&
            !r.rateMapHorizontal.empty() && r.rateMapHorizontal.size()==r.rateMapVertical.size() &&
            r.rateMapPhysicalSizes.size()==r.rateMapHorizontal.size()*2 &&
            Usage(c,r.rasterizationRateMap,a->eventId,ResourceUsage::RasterizationRateMap);
      if(r.rateMapHorizontal.size()==2)
        ok &= r.rateMapPhysicalSizes[0]==400 && r.rateMapPhysicalSizes[2]<400;
      printf("Legacy VRR EID=%u layers=%zu usage=%d\n",a->eventId,r.rateMapHorizontal.size(),ok);
    }
    ok &= !strcmp(argv[2],"legacy-vrr-unbound") ? maps==0 : maps>0;
    ok &= c->GetFatalErrorStatus().OK();
    printf("%s legacy VRR bound draws=%u\n",ok?"PASS":"FAIL",maps);
    c->Shutdown();RENDERDOC_ShutdownReplay();return ok?0:3;
  }
  bool temporal=strcmp(argv[2],"vrr")!=0, recovery=!strcmp(argv[2],"recovery"),
       missing=!strcmp(argv[2],"missing")||recovery, automatic=!strcmp(argv[2],"auto"), minimal=!strcmp(argv[2],"minimal");
  rdcarray<const ActionDescription *> all, actions;Collect(c->GetRootActions(),all);
  for(auto a:all)
    if(temporal ? a->customName.contains("MetalFX Temporal") : !!(a->flags&ActionFlags::Drawcall))actions.push_back(a);
  bool ok=actions.size()==(temporal?(recovery?3:2):1);
  bytebuf native;
  if(argc==4) {FILE *f=fopen(argv[3],"rb");if(!f)return 4;native.resize(2*128*128*8);ok &= fread(native.data(),1,native.size(),f)==native.size();fclose(f);}
  for(unsigned cycle=0;cycle<3;cycle++)
  {
    for(size_t n=0;n<actions.size();n++)
    {
      size_t ordinal=cycle==1?actions.size()-1-n:n;auto a=actions[ordinal];
      c->SetFrameEvent(0,true);c->SetFrameEvent(a->eventId,true);
      const auto &pipe=c->GetPipelineState();const auto &s=*pipe.GetMetalPipelineState();
      if(temporal)
      {
        ok &= s.metalFXSpatial.empty() && s.metalFXTemporal.size()==17 &&
              s.metalFXTemporalFloats.size()==7 && s.metalFXTemporalInputs.size()==5;
        if(s.metalFXTemporal.size()!=17||s.metalFXTemporalInputs.size()!=5)break;
        bool scalerObject=false;
        for(auto resource:c->GetResources())
          if(resource.resourceId==s.metalFXScaler)scalerObject=resource.type==ResourceType::StateObject;
        ok &= scalerObject;
        const auto &p=s.metalFXTemporal;const auto &v=s.metalFXTemporalFloats;
        ok &= p[0]==64 && p[2]==128 && p[12]==((ordinal==0&&!missing)||(recovery&&ordinal==2)) && p[13]==1 && p[14]==!minimal &&
              p[8]==automatic && v[2]==1 && v[3]==-1 && s.metalFXHistoryUnavailable==(missing&&(!recovery||ordinal<2));
        auto inputs=pipe.GetReadOnlyResources(ShaderStage::Compute,true);
        auto outputs=pipe.GetReadWriteResources(ShaderStage::Compute,true);
        ok &= inputs.size()==(minimal?3:automatic?4:5) && outputs.size()==1 &&
              outputs[0].descriptor.resource==s.metalFXOutput &&
              pipe.GetShaderReflection(ShaderStage::Compute)==nullptr && pipe.GetComputePipelineObject()==ResourceId();
        const char *names[]={"MetalFX Color","MetalFX Depth","MetalFX Motion Vectors","MetalFX Exposure","MetalFX Reactive Mask"};
        const ResourceUsage roles[]={ResourceUsage::MetalFXInput,ResourceUsage::MetalFXDepthInput,
            ResourceUsage::MetalFXMotionInput,ResourceUsage::MetalFXExposureInput,ResourceUsage::MetalFXReactiveInput};
        for(unsigned i=0;i<5;i++)
        {
          auto id=s.metalFXTemporalInputs[i];
          if((i==3&&(automatic||minimal))||(i==4&&minimal)){ok &= id==ResourceId();continue;}
          bool found=false;
          for(auto b:inputs)if(b.descriptor.resource==id)
          {
            DescriptorRange range=b.access;auto locations=c->GetDescriptorLocations(b.access.descriptorStore,{range});
            found=locations.size()==1&&locations[0].logicalBindName==names[i]&&b.descriptor.textureType==TextureType::Texture2D;
          }
          ok &= found && Usage(c,id,a->eventId,roles[i]) && !Usage(c,id,a->eventId,ResourceUsage::CS_Resource);
        }
        ok &= Usage(c,s.metalFXOutput,a->eventId,ResourceUsage::MetalFXOutput);
        auto pixels=c->GetTextureData(s.metalFXOutput,{0,0,0});ok &= pixels.size()==128*128*8;
        if(!native.empty())
        {
          unsigned differences=0;
          for(size_t i=0;i<pixels.size();i++)differences+=pixels[i]!=native[ordinal*pixels.size()+i];
          printf("Temporal EID=%u native byte differences=%u\n",a->eventId,differences);
          ok &= differences==0;
        }
        auto mv=c->GetTextureData(s.metalFXTemporalInputs[2],{0,0,0});
        uint16_t values[2]={};if(mv.size()>=4)memcpy(values,mv.data(),4);
        ok &= mv.size()==64*64*4 && values[0]==0x3800 && values[1]==0xb400;
      }
      else
      {
        const auto &r=s.rasterizer;
        ok &= r.rasterizationRateMap!=ResourceId() && r.rateMapScreenSize==rdcarray<uint32_t>({400,300}) &&
              r.rateMapHorizontal.size()==1 && r.rateMapVertical.size()==1 && r.rateMapPhysicalSizes.size()==2;
        if(r.rateMapHorizontal.size()!=1||r.rateMapPhysicalSizes.size()!=2)break;
        ok &= r.rateMapHorizontal[0]==rdcarray<float>({.5,.5}) && r.rateMapVertical[0]==rdcarray<float>({.75,.75}) &&
              Usage(c,r.rasterizationRateMap,a->eventId,ResourceUsage::RasterizationRateMap);
        bool stateObject=false;
        for(auto resource:c->GetResources()) if(resource.resourceId==r.rasterizationRateMap)stateObject=resource.type==ResourceType::StateObject;
        ok &= stateObject;
        auto target=s.colorTargets[0].resource;auto pixels=c->GetTextureData(target,{0,0,0});
        size_t offset=(20*r.rateMapPhysicalSizes[0]+20)*4;
        ok &= pixels.size()==size_t(r.rateMapPhysicalSizes[0])*r.rateMapPhysicalSizes[1]*4 &&
              pixels[offset]==64 && pixels[offset+1]==128 && pixels[offset+2]==191 && pixels[offset+3]==255;
        printf("VRR EID=%u physical=%ux%u map usage=%d\n",a->eventId,r.rateMapPhysicalSizes[0],r.rateMapPhysicalSizes[1],ok);
      }
      ok &= c->GetFatalErrorStatus().OK();
      printf("%s %s cycle=%u ordinal=%zu EID=%u\n",ok?"PASS":"FAIL",argv[2],cycle,ordinal,a->eventId);
    }
  }
  // Reset must clear operation and render-pass state.
  c->SetFrameEvent(0,true);auto reset=c->GetPipelineState().GetMetalPipelineState();
  ok &= reset && reset->metalFXTemporal.empty() && reset->rasterizer.rasterizationRateMap==ResourceId();
  c->Shutdown();RENDERDOC_ShutdownReplay();return ok?0:3;
}
