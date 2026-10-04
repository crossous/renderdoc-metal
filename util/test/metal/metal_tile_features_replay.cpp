// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include <cmath>
#include "renderdoc/api/replay/renderdoc_replay.h"
template <> rdcstr DoStringise(const uint32_t &v) { char s[16]; snprintf(s,sizeof(s),"%u",v); return s; }
#include "renderdoc/api/replay/pipestate.inl"
REPLAY_PROGRAM_MARKER()
static void Collect(const rdcarray<ActionDescription> &actions,rdcarray<const ActionDescription *> &out)
{
  for(const auto &a:actions) { if(a.flags&(ActionFlags::Drawcall|ActionFlags::Dispatch))out.push_back(&a); Collect(a.children,out); }
}
static bool Usage(IReplayController *c,ResourceId id,uint32_t event,ResourceUsage type)
{
  for(const auto &u:c->GetUsage(id)) if(u.eventId==event && u.usage==type)return true;
  return false;
}
static bool Pixel(IReplayController *c,ResourceId id,unsigned x,unsigned y,unsigned r,unsigned g,unsigned b)
{
  auto data=c->GetTextureData(id,{0,0,0}); size_t offset=(y*32+x)*4;
  bool ok=data.size()==4096 && data[offset]==r && data[offset+1]==g && data[offset+2]==b && data[offset+3]==255;
  if(data.size()>offset+3)printf("pixel (%u,%u) = %u,%u,%u,%u expected=%u,%u,%u OK=%d\n",x,y,data[offset],data[offset+1],data[offset+2],data[offset+3],r,g,b,ok);
  return ok;
}
int main(int argc,char **argv)
{
  if(argc<3||argc>4)return 1;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);IReplayController *c=nullptr;
  if(result.OK())rdctie(result,c)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!c || !result.OK()){puts("Failed opening capture");return 2;}
  bool tile=!strcmp(argv[2],"tile"), fx=!strcmp(argv[2],"metalfx"), memoryless=!strcmp(argv[2],"memoryless"), combined=!strcmp(argv[2],"combined");
  rdcarray<const ActionDescription *> actions;Collect(c->GetRootActions(),actions);
  bool ok=!actions.empty();unsigned fetches=0,tiles=0,scalers=0;
  for(unsigned cycle=0;cycle<2;cycle++)
  {
    for(size_t n=0;n<actions.size();n++)
    {
      auto a=actions[cycle ? actions.size()-1-n : n];
      c->SetFrameEvent(0,true);c->SetFrameEvent(a->eventId,true);
      const auto &pipe=c->GetPipelineState();const auto &s=*pipe.GetMetalPipelineState();
      if(fx)
      {
        if(s.metalFXSpatial.size()!=9)continue;
        scalers++;
        ok &= s.metalFXSpatial[0]==16 && s.metalFXSpatial[1]==16 && s.metalFXSpatial[2]==32 &&
              s.metalFXSpatial[3]==32 && s.metalFXSpatial[6]==1 && s.metalFXInput!=ResourceId() && s.metalFXOutput!=ResourceId();
        auto inputs=pipe.GetReadOnlyResources(ShaderStage::Compute,true);
        auto outputs=pipe.GetReadWriteResources(ShaderStage::Compute,true);
        ok &= inputs.size()==1 && outputs.size()==1 &&
              inputs[0].descriptor.resource==s.metalFXInput && outputs[0].descriptor.resource==s.metalFXOutput &&
              inputs[0].descriptor.textureType==TextureType::Texture2D &&
              outputs[0].descriptor.format.compType==CompType::Float &&
              pipe.GetShaderReflection(ShaderStage::Compute)==nullptr && pipe.GetComputePipelineObject()==ResourceId();
        for(const auto &binding:inputs) {
          DescriptorRange range=binding.access;
          auto names=c->GetDescriptorLocations(binding.access.descriptorStore,{range});
          ok &= names.size()==1 && names[0].logicalBindName=="MetalFX Input";
        }
        for(const auto &binding:outputs) {
          DescriptorRange range=binding.access;
          auto names=c->GetDescriptorLocations(binding.access.descriptorStore,{range});
          ok &= names.size()==1 && names[0].logicalBindName=="MetalFX Output";
        }
        auto data=c->GetTextureData(s.metalFXOutput,{0,0,0});
        // Linear uniform colour should be preserved by the real spatial scaler (RGBA16F).
        uint16_t expected[]={0x3400,0x3800,0x3a00,0x3c00};
        if(data.size()>=8) { uint16_t sample[4];memcpy(sample,data.data(),8); printf("FX sample=%04x,%04x,%04x,%04x params=%llu,%llu usage=%d,%d\n",sample[0],sample[1],sample[2],sample[3],s.metalFXSpatial[0],s.metalFXSpatial[6],Usage(c,s.metalFXInput,a->eventId,ResourceUsage::MetalFXInput),Usage(c,s.metalFXOutput,a->eventId,ResourceUsage::MetalFXOutput)); }
        ok &= data.size()==32*32*8;
        for(size_t pixel=0;pixel+8<=data.size();pixel+=8)
        {
          uint16_t got[4];memcpy(got,data.data()+pixel,8);
          for(unsigned channel=0;channel<4;channel++)ok &= abs(int(got[channel])-int(expected[channel]))<=16;
        }
        if(argc==4) {
          bytebuf native;native.resize(8192);FILE *f=fopen(argv[3],"rb");
          if(!f)ok=false;else { ok &= fread(native.data(),1,native.size(),f)==native.size(); fclose(f); ok &= native==data; }
          printf("MetalFX native byte comparison=%d\n",ok);
        }
        ok &= Usage(c,s.metalFXInput,a->eventId,ResourceUsage::MetalFXInput) && Usage(c,s.metalFXOutput,a->eventId,ResourceUsage::MetalFXOutput) &&
              !Usage(c,s.metalFXInput,a->eventId,ResourceUsage::CS_Resource) && !Usage(c,s.metalFXOutput,a->eventId,ResourceUsage::CS_RWResource);
        printf("MetalFX EID=%u cycle=%u pixels=%zu OK=%d\n",a->eventId,cycle,data.size()/8,ok);
      }
      else if(s.tileDispatch)
      {
        tiles++;
        ok &= tile && s.computeShader.entryPoint=="tile_image" && s.computeShader.usesImageblock &&
              s.computeShader.reflection && s.computePipelineResourceId==s.pipelineResourceId &&
              s.vertexShader.resourceId==ResourceId() && s.fragmentShader.resourceId==ResourceId() &&
              s.tileWidth==16 && s.tileHeight==16 && s.tileThreads[0]==16 && s.tileThreads[1]==16 &&
              s.tileThreads[2]==1 && s.tileMaxThreads==256 && s.tileSizeMatches &&
              s.imageblockSampleLength==4 && s.threadgroupMemoryLength==32 &&
              s.tileMemoryLengths.size()==1 && s.tileMemoryLengths[0]==16 && s.tileMemoryOffsets[0]==16 &&
              s.computeBuffers.size()==2 && s.computeBuffers[0].byteOffset==4 &&
              s.computeBuffers[1].resourceId==ResourceId() && s.computeBuffers[1].byteSize==2;
        if(s.computeBuffers.size()>0)
        { auto data=c->GetBufferData(s.computeBuffers[0].resourceId,4,4);uint32_t count=0;if(data.size()==4)memcpy(&count,data.data(),4);ok &= count==4; }
        bool binding=false;for(const auto &b:pipe.GetReadWriteResources(ShaderStage::Compute,true))binding |= b.descriptor.resource==s.computeBuffers[0].resourceId && b.descriptor.byteOffset==4;
        ok &= binding && a->outputs[0]==s.colorTargets[0].resource && Usage(c,a->outputs[0],a->eventId,ResourceUsage::ColorTarget);
        ok &= Pixel(c,s.colorTargets[0].resource,8,8,96,64,64);
        printf("Tile EID=%u cycle=%u %ux%u memory=%llu imageblock=%llu binding=%d OK=%d\n",a->eventId,cycle,s.tileWidth,s.tileHeight,s.threadgroupMemoryLength,s.imageblockSampleLength,binding,ok);
      }
      else if(s.fragmentShader.entryPoint=="fetch")
      {
        fetches++;
        // derive ordinal from action order, independent of debug-group/pass event IDs.
        unsigned ordinal=0;for(auto other:actions) { if(other==a)break; if(other->flags&ActionFlags::Drawcall)ordinal++; }
        unsigned expected=ordinal==0?96:combined?192:128;
        ok &= !tile && s.fragmentShader.framebufferFetch.size()==1 && s.fragmentShader.framebufferFetch[0]==0 &&
              s.fragmentShader.rasterOrderGroups.size()==1 && s.fragmentShader.rasterOrderGroups[0]==2 && s.colorTargets.size()>=2 &&
              s.attachmentStorage[0].contains(memoryless?"Memoryless":"Private") &&
              s.attachmentStore[0].contains(memoryless?"DontCare":"Store");
        bool input=false;for(const auto &b:pipe.GetReadOnlyResources(ShaderStage::Fragment,true))
          input |= b.descriptor.resource==s.colorTargets[0].resource && b.access.byteOffset==0x40000;
        ok &= input && Usage(c,s.colorTargets[0].resource,a->eventId,ResourceUsage::InputTarget) &&
              !Usage(c,s.colorTargets[0].resource,a->eventId,ResourceUsage::PS_Resource) &&
              Usage(c,s.colorTargets[0].resource,a->eventId,ResourceUsage::ColorTarget);
        ok &= s.colorBlends.size()>=1 && s.colorBlends[0].enabled==combined;
        if(memoryless) {
          bool clear=false,discard=false;
          for(const auto &use:c->GetUsage(s.colorTargets[0].resource)) {
            clear |= use.usage==ResourceUsage::Clear;
            discard |= use.usage==ResourceUsage::Discard;
          }
          ok &= clear && discard;
        }
        if(combined) {
          unsigned rgb=ordinal==0?160:255;
          ok &= Pixel(c,s.colorTargets[0].resource,8,8,rgb,rgb,rgb);
        }
        ok &= Pixel(c,s.colorTargets[1].resource,8,8,expected,expected,expected);
        printf("Fetch EID=%u cycle=%u input=%d storage=%s source=%s OK=%d\n",a->eventId,cycle,input,s.attachmentStorage[0].c_str(),s.fragmentShader.metadataSource.c_str(),ok);
      }
      else
      {
        ok &= !s.tileDispatch && s.computePipelineResourceId==ResourceId() && s.fragmentShader.framebufferFetch.empty() && (s.fragmentShader.entryPoint=="seed" ? s.fragmentShader.rasterOrderGroups.size()==1 && s.fragmentShader.rasterOrderGroups[0]==2 : s.fragmentShader.rasterOrderGroups.empty());
        printf("Graphics EID=%u cycle=%u entry=%s cleared Tile/fetch/ROG OK=%d\n",a->eventId,cycle,s.fragmentShader.entryPoint.c_str(),ok);
      }
      ok &= c->GetFatalErrorStatus().OK();
    }
  }
  ok &= fx ? scalers==2 : tile ? tiles==2 : fetches==4;
  printf("%s Metal features mode=%s tile=%u fetch=%u fx=%u\n",ok?"PASS":"FAIL",argv[2],tiles,fetches,scalers);
  c->Shutdown();RENDERDOC_ShutdownReplay();return ok?0:3;
}
