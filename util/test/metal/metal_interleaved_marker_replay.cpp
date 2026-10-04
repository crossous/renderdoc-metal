// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include <set>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static ResourceId outputs[2];
static std::set<uint32_t> actionIDs;
static rdcarray<uint32_t> syntheticIDs;
static bool Walk(const rdcarray<ActionDescription> &actions, rdcstr owner,
                 uint32_t &last, unsigned &fills, unsigned &continuations,
                 rdcarray<rdcpair<uint32_t, unsigned>> &checks)
{
  for(const auto &a:actions)
  {
    if(!actionIDs.insert(a.eventId).second) return false;
    if(a.IsFakeMarker()) { continuations++; syntheticIDs.push_back(a.eventId); }
    else
    {
      if(a.eventId<=last) { fprintf(stderr,"Non-monotonic EID %u after %u\n",a.eventId,last);return false; }
      last=a.eventId;
    }
    printf("action %u fake=%d name=%s owner=%s\n",a.eventId,a.IsFakeMarker(),a.customName.c_str(),owner.c_str());
    rdcstr current=owner;
    if(a.customName=="Owner A" || a.customName=="Owner B") current=a.customName;
    if((a.customName=="Nested A" || a.customName=="Signpost A") && current!="Owner A") return false;
    if(a.customName=="Signpost B" && current!="Owner B") return false;
    if((a.flags & ActionFlags::Clear) && !(a.flags & ActionFlags::PassBoundary))
    {
      if(current!="Owner A" && current!="Owner B") return false;
      outputs[current=="Owner A"?0:1]=a.copyDestination;
      checks.push_back({a.eventId,current=="Owner A"?17U:34U});fills++;
    }
    if(!Walk(a.children,current,last,fills,continuations,checks)) return false;
  }
  return true;
}
int main(int argc,char **argv)
{
  if(argc!=2)return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(env,args);
  auto f=RENDERDOC_OpenCaptureFile();auto r=f->OpenFile(argv[1],"rdc",nullptr);
  if(!r.OK())return 3;IReplayController *c=nullptr;rdctie(r,c)=f->OpenCapture(ReplayOptions(),nullptr);f->Shutdown();
  if(!r.OK() || !c){fprintf(stderr,"Open failed: %s\n",r.internal_msg?r.internal_msg->c_str():"unknown");return 4;}
  uint32_t last=0;unsigned fills=0,continuations=0;rdcarray<rdcpair<uint32_t,unsigned>> checks;
  bool good=Walk(c->GetRootActions(),"",last,fills,continuations,checks) && fills==2 && continuations==0 && checks.size()==2 &&
              checks[0].second==34 && checks[1].second==17;
  ResourceId a=outputs[0],b=outputs[1];
  printf("tree good=%d, output identities A=%d B=%d\n",good,a!=ResourceId(),b!=ResourceId());
  for(unsigned cycle=0;cycle<2 && good;cycle++)for(const auto &check:checks)
  {
    c->SetFrameEvent(0,true);c->SetFrameEvent(check.first,true);
    if(!c->GetFatalErrorStatus().OK()){good=false;break;}
    auto bytes=c->GetBufferData(check.second==17?a:b,0,64);
    printf("seek %u cycle %u size=%zu first=%u expected=%u\n",check.first,cycle,bytes.size(),bytes.empty()?0:bytes[0],check.second);
    if(bytes.size()!=64){good=false;break;}
    for(byte v:bytes)good &= v==check.second;
  }
  for(uint32_t eid : syntheticIDs)
  {
    good &= eid > last;
    if(!good) break;
    c->SetFrameEvent(0, true); c->SetFrameEvent(eid, true);
    good &= c->GetFatalErrorStatus().OK();
  }
  c->Shutdown();RENDERDOC_ShutdownReplay();
  printf("%s submission-ordered actions, correct marker owners, continuations=%u, fills=%u, reset readbacks\n",good?"PASS":"FAIL",continuations,fills);
  return good?0:5;
}
