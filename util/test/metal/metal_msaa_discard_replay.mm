// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <mach/mach.h>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Last(const rdcarray<ActionDescription> &actions,uint32_t &eid)
{for(const auto &a:actions){if(!a.IsFakeMarker()&&a.eventId>eid)eid=a.eventId;Last(a.children,eid);}}
static uint64_t Resident()
{mach_task_basic_info_data_t info={};mach_msg_type_number_t n=MACH_TASK_BASIC_INFO_COUNT;
 return task_info(mach_task_self(),MACH_TASK_BASIC_INFO,(task_info_t)&info,&n)==KERN_SUCCESS?info.resident_size:0;}
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
 if(argc<2||argc>4)return 2;
 bool fastest=false,stress=false,storePlanes=false;
 for(int i=2;i<argc;i++){
  if(!strcmp(argv[i],"--fastest"))fastest=true;
  else if(!strcmp(argv[i],"--stress"))stress=true;
  else if(!strcmp(argv[i],"--store-planes"))storePlanes=true;
  else return 2;
 }
 GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
 RENDERDOC_SetDebugLogFile(rdcstr(argv[1])+".msaa-debug.log");
 auto f=RENDERDOC_OpenCaptureFile();auto status=f->OpenFile(argv[1],"rdc",nullptr);
 ReplayOptions options;if(fastest)options.optimisation=ReplayOptimisationLevel::Fastest;
 IReplayController *r=nullptr;if(status.OK())rdctie(status,r)=f->OpenCapture(options,nullptr);f->Shutdown();
 if(!status.OK()||!r){fprintf(stderr,"Open: %s\n",status.internal_msg?status.internal_msg->c_str():"unknown");return 3;}
 constexpr unsigned w=70,h=18,count=15;ResourceId outputs[count],sources[count],resolves[count],stencilOutputs[2];unsigned sampleCounts[count]={};
 for(const auto &res:r->GetResources())for(unsigned i=0;i<count;i++){
  char name[64];snprintf(name,sizeof(name),"MSAA discard samples %u",i);if(res.name==name)outputs[i]=res.resourceId;
  snprintf(name,sizeof(name),"MSAA discard source %u",i);if(res.name==name)sources[i]=res.resourceId;
  snprintf(name,sizeof(name),"MSAA discard resolve %u",i);if(res.name==name)resolves[i]=res.resourceId;
  if(i==7||i==8){snprintf(name,sizeof(name),"MSAA discard stencil %u",i);if(res.name==name)stencilOutputs[i-7]=res.resourceId;}
 }
 for(const auto &buffer:r->GetBuffers())for(unsigned i=0;i<count;i++)
   if(buffer.resourceId==outputs[i]&&buffer.length%(w*h*16)==0)sampleCounts[i]=unsigned(buffer.length/(w*h*16));
 uint32_t end=0;Last(r->GetRootActions(),end);int result=0;uint64_t before=0;
 // Resolve-only discards its multisample source; StoreAndResolve preserves it.
 // Both must identify the same resolve event on source and destination usage.
 for(unsigned i:{4U,5U}){
  uint32_t resolveEvent=0;bool discarded=false,resolved=false;
  const auto usage=r->GetUsage(sources[i]);
  for(const auto &u:usage)if(u.usage==ResourceUsage::ResolveSrc)resolveEvent=u.eventId;
  for(const auto &u:usage)if(u.eventId==resolveEvent&&u.usage==ResourceUsage::Discard)discarded=true;
  for(const auto &u:r->GetUsage(resolves[i]))if(u.eventId==resolveEvent&&u.usage==ResourceUsage::ResolveDst)resolved=true;
  if(!resolveEvent||!resolved||discarded!=(i==4)){
   fprintf(stderr,"case=%u resolve usage event=%u sourceDiscard=%d destination=%d\n",i,resolveEvent,discarded,resolved);
   result=13;goto done;
  }
  printf("PASS case=%u resolve usage event=%u sourceDiscard=%d\n",i,resolveEvent,discarded);
 }
 for(unsigned cycle=0;cycle<(stress?204U:2U);cycle++){@autoreleasepool {
  if(cycle==4)before=Resident();r->SetFrameEvent(0,true);r->SetFrameEvent(end,true);
  for(unsigned i=0;i<count;i++){
   if(i==11)continue;const unsigned samples=sampleCounts[i];
   if(samples!=2&&samples!=4&&samples!=8){result=4;goto done;}
   const auto data=r->GetBufferData(outputs[i],0,w*h*samples*16);
   if(data.size()!=w*h*samples*16){fprintf(stderr,"case %u missing sample output %zu\n",i,data.size());result=4;goto done;}
   for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)for(unsigned s=0;s<samples;s++){
    const bool clipped=i==0&&x<4&&y<4,defined=i==5||i==8||clipped;
    if(fastest&&!defined)continue;
    const bool white=y%8<6&&(i==3||i==4||i==9||storePlanes&&i==7?store:load)[y%8][x%64]=='#';
    float value[4];memcpy(value,data.data()+((y*w+x)*samples+s)*16,16);
    for(unsigned c=0;c<(i==1||i==2||i>=6&&i<=8?1U:4U);c++){
     const float expected=clipped||i==5?float(c==0?64:c==1?128:c==2?191:255)/255.f:
       i==8?.25f:white?(i==1?1000.f:i==2?127.f:1.f):0.f;
     if(!std::isfinite(value[c])||fabs(value[c]-expected)>1.e-6f){fprintf(stderr,"case=%u cycle=%u pixel=%u,%u sample=%u c=%u got=%g expected=%g\n",i,cycle,x,y,s,c,value[c],expected);
       for(unsigned yy=0;yy<8;yy++){for(unsigned xx=0;xx<64;xx++){float v;memcpy(&v,data.data()+(yy*w+xx)*samples*16,4);fputc(v>.99f?'#':v>0?'x':'.',stderr);}fputc('\n',stderr);}
       result=5;goto done;}
    }
   }
   printf("PASS cycle=%u case=%u all %u samples%s\n",cycle,i,samples,fastest?" (defined pixels)":"");
  }
  for(unsigned i=7;i<=8;i++){
   auto data=r->GetBufferData(stencilOutputs[i-7],0,w*h*4*16);if(data.size()!=w*h*4*16){result=11;goto done;}
   if(fastest&&i==8)continue;
   for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)for(unsigned s=0;s<4;s++){
    float value;memcpy(&value,data.data()+((y*w+x)*4+s)*16,4);
    const float expected=i==7?1.f:y%8<6&&(storePlanes?store:load)[y%8][x%64]=='#'?1.f:0.f;
    if(value!=expected){fprintf(stderr,"stencil source=%u pixel=%u,%u sample=%u got=%g expected=%g\n",i,x,y,s,value,expected);result=12;goto done;}
   }
   printf("PASS cycle=%u source=%u stencil test consumes all four samples\n",cycle,i);
  }
  for(unsigned i:{4U,5U,6U,11U}){
   const auto data=r->GetTextureData(resolves[i],{0,0,0});const unsigned stride=4;
   if(data.size()!=w*h*stride){fprintf(stderr,"resolve %u bytes %zu\n",i,data.size());result=6;goto done;}
   for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
    const byte *p=data.data()+(y*w+x)*stride;const bool white=y%8<6&&load[y%8][x%64]=='#';
    bool ok=true;
    if(i==4||i==5)ok=p[0]==64&&p[1]==128&&p[2]==191&&p[3]==255;
    else if(i==11&&!fastest)for(unsigned c=0;c<4;c++)ok&=p[c]==(white?255:0);
    else if(i==6&&!fastest){float z;memcpy(&z,p,4);ok=z==(white?1.f:0.f);}
    if(!ok){fprintf(stderr,"resolve %u cycle=%u pixel=%u,%u bytes=%u/%u/%u/%u stencil=%u\n",i,cycle,x,y,p[0],p[1],p[2],p[3],stride==8?p[4]:0);result=7;goto done;}
   }
   printf("PASS cycle=%u resolve=%u %s\n",cycle,i,i==11?"memoryless backing":i>=6&&i<=8?"depth/stencil independent planes":"resolve preserved");
  }
 }}
 if(stress){const uint64_t after=Resident();printf("MSAA discard resident before=%llu after=%llu growth=%llu\n",
   (unsigned long long)before,(unsigned long long)after,(unsigned long long)(after>before?after-before:0));
   if(!before||!after||after>before+16ULL*1024*1024)result=10;}
 done:if(!r->GetFatalErrorStatus().OK())result=8;
 rdcstr log;RENDERDOC_GetLogFileContents(0,log);
 if(FILE *fp=fopen((rdcstr(argv[1])+".msaa-final.log").c_str(),"wb")){fwrite(log.c_str(),1,log.size(),fp);fclose(fp);}
 r->Shutdown();RENDERDOC_ShutdownReplay();return result;
}
