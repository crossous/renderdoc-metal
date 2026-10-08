// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cmath>
#include "metal_texture_initial_spec.h"
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &actions,std::vector<uint32_t> &dispatch,uint32_t &last)
{for(const auto &a:actions){if(a.flags&ActionFlags::Dispatch)dispatch.push_back(a.eventId);last=std::max(last,a.eventId);Find(a.children,dispatch,last);}}
int main(int argc,char **argv)
{
  if(argc!=2&&argc!=3)return 2;const bool legacy=argc==3&&strcmp(argv[2],"16")==0;if(argc==3&&!legacy)return 2;GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
  ICaptureFile *file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);IReplayController *controller=nullptr;
  if(!result.OK())return 3;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK()||!controller){fprintf(stderr,"Open failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  auto specs=BufferTextureInitialSpecs();if(legacy)specs.resize(16);std::vector<ResourceId> images;bool readWrite=false;
  for(auto &s:specs)
  {
    ResourceId id;
    for(const auto *c:controller->GetStructuredFile().chunks)if(c->name=="MTLBuffer::newTextureWithDescriptor")
    {const auto *d=c->FindChild("descriptor");if(d->FindChild("pixelFormat")->AsUInt64()==s.format) {
      id=c->FindChild("Texture")->AsResourceId();s.width=(unsigned)d->FindChild("width")->AsUInt64();
      if(s.width!=7&&(s.format!=113||s.width!=8192))return 23;
      if(s.format==113)readWrite=d->FindChild("usage")->AsUInt64()==3;
    }}
    if(id==ResourceId())return 5;images.push_back(id);
  }
  ResourceId output;for(const auto &b:controller->GetBuffers())if(b.length==8)output=b.resourceId;
  ResourceId read16,after16;
  for(const auto &r:controller->GetResources()) {
    if(r.name=="TextureBuffer uint16 read")read16=r.resourceId;
    if(r.name=="TextureBuffer uint16 after write")after16=r.resourceId;
  }
  std::vector<uint32_t> dispatch;uint32_t last=0;Find(controller->GetRootActions(),dispatch,last);
  if(dispatch.size()!=(legacy?1u:readWrite?4u:2u)||output==ResourceId()||(!legacy&&(read16==ResourceId()||(readWrite&&after16==ResourceId()))))return 6;
  const uint32_t positions[3]={0,specs.back().width/2,specs.back().width-1},written[4]={65535,60000,50000,40000};
  auto verify16=[&](ResourceId id,bool after) {
    const auto bytes=controller->GetBufferData(id,0,48);if(bytes.size()!=48)return false;
    uint32_t words[12];memcpy(words,bytes.data(),48);
    for(unsigned i=0;i<3;i++)for(unsigned c=0;c<4;c++)
      if(words[i*4+c]!=(after?written[c]-i:32768+8191*c+(positions[i]%101)))return false;
    return true;
  };
  if(!legacy) {
    bool readUsed=false,writeUsed=!readWrite;
    for(const auto &u:controller->GetUsage(images.back())) {
      if(u.eventId==dispatch[1]&&u.usage==ResourceUsage::CS_Resource)readUsed=true;
      if(readWrite&&u.eventId==dispatch[2]&&u.usage==ResourceUsage::CS_RWResource)writeUsed=true;
    }
    if(!readUsed||!writeUsed)return 24;
  }
  size_t checks=0;
  for(unsigned cycle=0;cycle<4;cycle++)
  {
    controller->SetFrameEvent(last,true);auto altered=controller->GetTextureData(images[0],{0,0,0});uint32_t first=0;
    if(altered.size()!=28)return 7;memcpy(&first,altered.data(),4);if(first!=0x01020304)return 8;
    if(readWrite) {
      controller->SetFrameEvent(dispatch[2],true);
      auto changed=controller->GetTextureData(images.back(),{0,0,0});if(changed.size()!=specs.back().width*8)return 16;
      for(unsigned x=0;x<specs.back().width;x++)for(unsigned c=0;c<4;c++) {
        uint16_t actual=0;memcpy(&actual,changed.data()+x*8+c*2,2);unsigned expected=32768+8191*c+(x%101);
        for(unsigned i=0;i<3;i++)if(x==positions[i])expected=written[c]-i;
        if(actual!=expected)return 17;
      }
      controller->SetFrameEvent(dispatch.back(),true);if(!verify16(after16,true))return 18;
    }
    controller->SetFrameEvent(legacy?dispatch[0]:dispatch[1],true);if(!legacy&&!verify16(read16,false))return 19;auto outputBytes=controller->GetBufferData(output,0,8);uint32_t words[2]={};
    if(outputBytes.size()!=8)return 9;memcpy(words,outputBytes.data(),8);if(words[0]!=17||words[1]!=0xdeadbeef)return 10;
    controller->SetFrameEvent(0,true);
    if(!legacy) {
      auto reset=controller->GetBufferData(read16,0,48);if(reset.size()!=48)return 25;
      for(auto value:reset)if(value)return 26;
      if(readWrite) {auto resetAfter=controller->GetBufferData(after16,0,48);if(resetAfter.size()!=48)return 27;for(auto value:resetAfter)if(value)return 28;}
    }
    for(size_t i=0;i<specs.size();i++)
    {
      const auto &s=specs[i];const auto data=controller->GetTextureData(images[i],{0,0,0});
      if(data.size()!=s.width*s.bytes){fprintf(stderr,"Readback size failed format=%u size=%zu\n",s.format,data.size());return 11;}
      for(unsigned x=0;x<data.size();x++)if(data[x]!=InitialTextureByte(s,0,0,0,0,x)){fprintf(stderr,"Raw bytes failed format=%u byte=%u\n",s.format,x);return 12;}
      auto pixel=controller->PickPixel(images[i],0,0,{0,0,0},CompType::Typeless);
      for(unsigned c=0;c<s.components;c++)
      {
        const unsigned raw=s.format==113?32768+8191*c:17+23*c;
        if(s.format==53||s.format==123||s.format==103||s.format==73||s.format==23||s.format==63||s.format==113)
        {if(pixel.uintValue[c]!=raw)return 13;}
        else if(s.format==54){if(pixel.intValue[c]!=-17)return 14;}
        else
        {
          const float expected=s.format==70&&c==3?1.0f:s.format==70||s.format==10?raw/255.0f:s.format==72?raw/127.0f:0.25f+0.125f*c;
          if(fabs(pixel.floatValue[c]-expected)>0.00001f){fprintf(stderr,"PickPixel failed format=%u c=%u value=%g expected=%g\n",s.format,c,pixel.floatValue[c],expected);return 15;}
        }
      }
      checks++;
    }
  }
  controller->Shutdown();RENDERDOC_ShutdownReplay();printf("PASS TextureBuffer%zu: %zu raw/pixel checks, 4 seek cycles, GPU17/DEADBEEF, uint16 width=%u RW=%u first/middle/last four channels high-bit integers, exact parent offsets and overwrite restore\n",specs.size(),checks,specs.back().width,readWrite);return 0;
}
