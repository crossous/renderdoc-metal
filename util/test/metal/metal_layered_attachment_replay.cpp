// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include "metal_layered_attachment_cases.h"
#include <algorithm>
#include <cstdio>
REPLAY_PROGRAM_MARKER()
static void Ends(const rdcarray<ActionDescription> &as,rdcarray<uint32_t> &events)
{for(const auto &a:as){if(a.flags&ActionFlags::EndPass)events.push_back(a.eventId);Ends(a.children,events);}}
int main(int argc,char **argv)
{
  if(argc!=3)return 1;const auto *spec=FindLayeredAttachmentCase(argv[2]);if(!spec)return 1;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",NULL);IReplayController *c=nullptr;
  if(result.OK())rdctie(result,c)=file->OpenCapture(ReplayOptions(),NULL);file->Shutdown();if(!result.OK()||!c){fprintf(stderr,"Open failed\n");return 2;}
  ResourceId image;for(const auto &r:c->GetResources())if(r.name=="Layered attachment")image=r.resourceId;
  rdcarray<uint32_t> ends;Ends(c->GetRootActions(),ends);if(image==ResourceId()||ends.size()<2)return 3;
  uint32_t load=ends[ends.size()-2],clear=ends.back();
  for(uint32_t eid:{load,clear,load,0u,clear,0u}){
    c->SetFrameEvent(eid,true);
    for(unsigned mip=0;mip<spec->mips;mip++){
      unsigned width=std::max(1u,spec->width>>mip),height=std::max(1u,spec->height>>mip),depth=spec->volume?std::max(1u,spec->depth>>mip):spec->depth;
      for(unsigned array=0;array<(spec->volume?1u:depth);array++){
        auto bytes=c->GetTextureData(image,{mip,array,0});
        if(!eid && spec->frame){if(!bytes.empty())return 4;continue;}
        if(bytes.size()!=size_t(width)*height*spec->stride*(spec->volume?depth:1))return 5;
        for(unsigned layer=0;layer<(spec->volume?depth:1);layer++){
          unsigned z=spec->volume?layer:array;
          bool selected=eid==clear && mip==spec->level && z>=spec->firstLayer && z<spec->firstLayer+spec->layers;
          for(unsigned i=0;i<width*height;i++)for(unsigned channel=0;channel<(spec->format==55?1u:4u);channel++){
            auto p=bytes.data()+(size_t(layer)*width*height+i)*spec->stride;
            bool good=spec->format==115?((const unsigned short *)p)[channel]==(selected?0x3a00:0x3400):
              spec->format==55?*(const float *)p==(selected?.75f:.25f):p[channel]==(selected?191:64);
            if(!good){fprintf(stderr,"Replay mismatch EID=%u mip=%u layer=%u pixel=%u selected=%d\n",eid,mip,z,i,selected);return 6;}
          }
        }
      }
    }
  }
  c->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS full initial and selected/unselected mip/plane/layer texels, six event selections/EID0");return 0;
}
