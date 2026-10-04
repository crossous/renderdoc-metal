// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include <cmath>
#include <mach/mach.h>
#import <Foundation/Foundation.h>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Last(const rdcarray<ActionDescription> &actions,uint32_t &eid)
{for(const auto &a:actions){if(!a.IsFakeMarker() && a.eventId>eid)eid=a.eventId;Last(a.children,eid);}}
static uint32_t DiscardDraw(const rdcarray<ActionDescription> &actions,ResourceId id)
{for(const auto &a:actions){if((a.flags&ActionFlags::Drawcall)&&a.outputs[0]==id)return a.eventId;
 if(auto eid=DiscardDraw(a.children,id))return eid;}return 0;}
static uint64_t Resident()
{
 mach_task_basic_info_data_t info={};mach_msg_type_number_t count=MACH_TASK_BASIC_INFO_COUNT;
 return task_info(mach_task_self(),MACH_TASK_BASIC_INFO,(task_info_t)&info,&count)==KERN_SUCCESS?info.resident_size:0;
}
// Independent glyph oracle for the first six lines of LOAD/STORE DONT CARE.
static const char *load[] = {
"..#.....##...##..##....##....##..#..#.####..###..##..###..####..",
"..#....#..#.#..#.#.#...#.#..#..#.##.#..#...#....#..#.#..#.#.....",
"..#....#..#.#..#.#..#..#..#.#..#.##.#..#...#....#..#.#..#.###...",
"..#....#..#.####.#..#..#..#.#..#.#.##..#...#....####.###..#.....",
"..#....#..#.#..#.#.#...#.#..#..#.#.##..#...#....#..#.#..#.#.....",
"..####..##..#..#.##....##....##..#..#..#....###.#..#.#..#.####.."};
static const char *store[]={
"...###.####..##..###...##....##..#..#.####..###..##..###..####..",
"..#.....#...#..#.#..#..#.#..#..#.##.#..#...#....#..#.#..#.#.....",
"...#....#...#..#.#..#..#..#.#..#.##.#..#...#....#..#.#..#.###...",
"....#...#...#..#.###...#..#.#..#.#.##..#...#....####.###..#.....",
".....#..#...#..#.#..#..#.#..#..#.#.##..#...#....#..#.#..#.#.....",
"..###...#....##..#..#..##....##..#..#..#....###.#..#.#..#.####.."};
int main(int argc,char **argv)
{
 if(argc!=2 && argc!=3)return 2;
 const bool fastest=argc==3 && strcmp(argv[2],"--fastest")==0;
 const bool stress=argc==3 && strcmp(argv[2],"--stress")==0;
 GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
 auto file=RENDERDOC_OpenCaptureFile();auto status=file->OpenFile(argv[1],"rdc",nullptr);
 ReplayOptions options;if(fastest)options.optimisation=ReplayOptimisationLevel::Fastest;
 IReplayController *r=nullptr;if(status.OK())rdctie(status,r)=file->OpenCapture(options,nullptr);
 file->Shutdown();if(!status.OK()||!r)return 3;
 ResourceId ids[18];for(const auto &res:r->GetResources())for(unsigned i=0;i<18;i++)
 {char name[32];snprintf(name,sizeof(name),"Discard %u",i);if(res.name==name)ids[i]=res.resourceId;}
 int result=0;
 uint32_t end=0;Last(r->GetRootActions(),end);
 uint64_t residentBefore=0;
 for(unsigned cycle=0;cycle<(stress?204U:2U);cycle++) {
  if(cycle==4)residentBefore=Resident();
  @autoreleasepool {
  r->SetFrameEvent(0,true);
  const uint32_t discardDraw=DiscardDraw(r->GetRootActions(),ids[17]);
  if(!discardDraw){result=11;goto done;}
  r->SetFrameEvent(discardDraw,true);
  auto partial=r->GetTextureData(ids[17],{0,0,0});
  if(partial.size()!=130*34*4){result=12;goto done;}
  for(unsigned y=0;y<34;y++)for(unsigned x=0;x<130;x++){
    const byte *p=partial.data()+(y*130+x)*4;const bool clipped=x<4&&y<4;
    if(fastest&&!clipped)continue;const bool white=y%8<6&&load[y%8][x%64]=='#';
    for(unsigned c=0;c<4;c++)if(p[c]!=(clipped?(c==0?64:c==1?128:c==2?191:255):white?255:0)){
      fprintf(stderr,"partial DontCare store EID=%u pixel=%u,%u component=%u got=%u\n",discardDraw,x,y,c,p[c]);result=13;goto done;}
  }
  printf("PASS cycle=%u selected draw before future DontCare store EID=%u\n",cycle,discardDraw);
  r->SetFrameEvent(end,true);
  for(unsigned i=0;i<18;i++) {
   auto data=r->GetTextureData(ids[i],{i==8||i==13?1U:0U,i==8?1U:i==13?4U:0U,0});
   const unsigned w=i==8||i==13?65:130,h=i==8?17:i==13?65:34,stride=i==5||i==6?8:4,planes=i==11||i==12?4:1;
   if(data.size()!=w*h*stride*planes){fprintf(stderr,"case %u bytes %zu expected %u\n",i,data.size(),w*h*stride*planes);result=4;break;}
   // Check all glyph rows and repeat boundaries. Packed/depth aspect
   // preservation and the untouched Clear pass are checked in the same readback.
   for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
    if(fastest && i!=9 && !((i==0 || i==10) && x<4 && y<4) && i!=5 && i!=6)continue;
    if(fastest && i==5)continue; // discarded depth is undefined; the Clear stencil is checked below
    const bool white=(y%8<6)&&((i==7||i==12||i==17?store:load)[y%8][x%64]=='#');
    const byte *p=data.data()+((planes==4?2*w*h:0)+y*w+x)*stride;bool ok=true;
    if((i==0 || i==10) && x<4 && y<4)ok=p[0]==64&&p[1]==128&&p[2]==191&&p[3]==255;
    else if(i==0||i==7||i==8||i>=10)for(unsigned c=0;c<4;c++)ok&=p[c]==(white?255:0);
    else if(i==1){uint32_t v;memcpy(&v,p,4);ok=v==(white?0xffffffffU:0U);}
    else if(i==2||i==4||i==5){float v;memcpy(&v,p,4);ok=v==(white?(i==2?1000.f:1.f):0.f);if(i==5)ok&=p[4]==37;}
    else if(i==3){uint32_t v;memcpy(&v,p,4);ok=v==(white?127U:0U);}
    else if(i==6){float v;memcpy(&v,p,4);ok=v==.25f && (fastest || p[4]==(white?255:0));}
    else if(i==9)ok=p[0]==64&&p[1]==128&&p[2]==191&&p[3]==255;
    if(!ok){fprintf(stderr,"case %u cycle %u pixel %u,%u bytes %u %u %u %u %u\n",i,cycle,x,y,p[0],p[1],p[2],p[3],stride==8?p[4]:0);result=5;goto done;}
   }
   if(fastest && i==5)for(unsigned p=0;p<w*h;p++)if(data[p*8+4]!=37){result=7;goto done;}
   if(planes==4)for(unsigned z:{0U,1U,3U})for(unsigned p=0;p<w*h;p++){
     const byte *v=data.data()+(z*w*h+p)*4;
     if(v[0]!=64||v[1]!=128||v[2]!=191||v[3]!=255){fprintf(stderr,"3D neighbour corrupted case=%u plane=%u pixel=%u\n",i,z,p);result=9;goto done;}
   }
   if(i==13){auto neighbour=r->GetTextureData(ids[i],{1,3,0});
     if(neighbour.size()!=w*h*4){result=9;goto done;}
     for(unsigned p=0;p<w*h;p++){const byte *v=neighbour.data()+p*4;
       if(v[0]!=64||v[1]!=128||v[2]!=191||v[3]!=255){result=9;goto done;}}
   }
   printf("PASS cycle=%u case=%u %s\n",cycle,i,
     fastest?"Fastest preserves defined draw/Clear bytes":"glyph bytes, aspect preservation and subresource");
  }
  }
 }
 if(stress){const uint64_t after=Resident();
  printf("Discard staging resident before=%llu after=%llu growth=%llu\n",(unsigned long long)residentBefore,
   (unsigned long long)after,(unsigned long long)(after>residentBefore?after-residentBefore:0));
  if(!residentBefore||!after||after>residentBefore+16ULL*1024*1024)result=10;
 }
 done:if(!r->GetFatalErrorStatus().OK())result=6;r->Shutdown();RENDERDOC_ShutdownReplay();return result;
}
