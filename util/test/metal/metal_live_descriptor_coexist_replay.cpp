// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <fstream>
REPLAY_PROGRAM_MARKER()
static void Events(const rdcarray<ActionDescription>& actions,rdcarray<uint32_t>& events)
{for(const auto&a:actions){if(a.flags&ActionFlags::Dispatch)events.push_back(a.eventId);Events(a.children,events);}}
int main(int argc,char**argv)
{
  if(argc!=3 && argc!=4)return 1;
  bytebuf expected;expected.resize(32);std::ifstream oracle(argv[2],std::ios::binary);oracle.read((char*)expected.data(),32);if(!oracle)return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);if(!result.OK())return 3;
  IReplayController*controller=nullptr;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK() || !controller){fprintf(stderr,"OpenCapture: %s\n",result.internal_msg?result.internal_msg->c_str():"failed");RENDERDOC_ShutdownReplay();return 4;}
  ResourceId output,source;for(const auto&r:controller->GetResources()){
    if(r.name=="Live coexist output")output=r.resourceId;
    if(r.name=="Live coexist source")source=r.resourceId;
  }
  bytebuf input;
  if(argc==4){input.resize(128);std::ifstream oracleInput(argv[3],std::ios::binary);oracleInput.read((char*)input.data(),128);if(!oracleInput || source==ResourceId())return 7;}
  rdcarray<uint32_t> events;Events(controller->GetRootActions(),events);if(output==ResourceId() || events.size()!=2)return 5;
  unsigned checks=0;
  for(unsigned cycle=0;cycle<3;cycle++){
    controller->SetFrameEvent(0,true);
    if(!input.empty() && controller->GetBufferData(source,0,input.size())!=input){fprintf(stderr,"EID0 source initial bytes mismatch cycle=%u\n",cycle);return 8;}
    for(unsigned at:{1U,0U,1U}){
      controller->SetFrameEvent(events[at],true);auto bytes=controller->GetBufferData(output,0,32);
      bytebuf want=expected;if(!at)memset(want.data()+8,0,24);
      if(bytes!=want){fprintf(stderr,"output mismatch cycle=%u stage=%u bytes=%zu\n",cycle,at,bytes.size());return 6;}checks++;
    }
  }
  if(!input.empty())puts("Private source initial MATCH Native 3 EID0 observations; complete output requires original GPU producer");
  controller->Shutdown();RENDERDOC_ShutdownReplay();printf("GPU COMPLETED; output MATCH Native %u event/rewind/EID0 checks\n",checks);return 0;
}
