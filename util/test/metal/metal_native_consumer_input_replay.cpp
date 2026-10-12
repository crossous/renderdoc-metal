// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
REPLAY_PROGRAM_MARKER()
static bytebuf Filled(byte value){bytebuf data;data.resize(24);memset(data.data(),value,24);return data;}
static bytebuf Read(const char *p){ bytebuf b; FILE*f=fopen(p,"rb"); if(!f)return b; fseek(f,0,SEEK_END);b.resize(ftell(f));rewind(f);fread(b.data(),1,b.size(),f);fclose(f);return b; }
int main(int argc,char**argv){
 if(argc!=4 && argc!=5 && argc!=6)return 1;
 const auto initial=Read(argv[2]),outputExpected=Read(argv[3]);
 const auto unrelatedExpected=argc>=5?Read(argv[4]):Filled(0);if(unrelatedExpected.size()!=24)return 1;
 uint64_t resultOffset=0;byte initialResultValue=0xa7;
 if(argc==6){auto layout=Read(argv[5]);if(layout.size()==56){uint64_t v[7];memcpy(v,layout.data(),56);resultOffset=v[5];if(v[6]>255)return 1;initialResultValue=byte(v[6]);}}
 GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
 auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);IReplayController*c=nullptr;
 if(result.OK())rdctie(result,c)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
 if(!result.OK()||!c){fprintf(stderr,"OpenCapture: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");RENDERDOC_ShutdownReplay();return 4;}
 const std::string outputPath=argv[3];
 const auto computeExpected=outputPath.size()>11?Read((outputPath.substr(0,outputPath.size()-11)+".compute-output.bin").c_str()):bytebuf();
 ResourceId computeOutput;
 const auto renderExpected=outputPath.size()>11?Read((outputPath.substr(0,outputPath.size()-11)+".render-output.bin").c_str()):bytebuf();
 ResourceId renderOutput;
 const auto computeTextureExpected=outputPath.size()>11?Read((outputPath.substr(0,outputPath.size()-11)+".compute-texture-output.bin").c_str()):bytebuf();
 ResourceId computeTextureOutput,textureParent;
 const auto textureParentInitial=outputPath.size()>11?Read((outputPath.substr(0,outputPath.size()-11)+".texture-parent-initial.bin").c_str()):bytebuf();
 const auto textureParentExpected=outputPath.size()>11?Read((outputPath.substr(0,outputPath.size()-11)+".texture-parent-output.bin").c_str()):bytebuf();
 ResourceId input,output,unrelated,transportTable,physicalInitial;for(auto&r:c->GetResources()){if(r.name=="Native retirement unrelated compute output")computeOutput=r.resourceId;if(r.name=="Native input snapshot source")input=r.resourceId;if(r.name=="Native input snapshot output")output=r.resourceId;if(r.name=="Native retirement unrelated copy output")unrelated=r.resourceId;if(r.name=="Native transport source table")transportTable=r.resourceId;if(r.name=="Native input physical initial carrier")physicalInitial=r.resourceId;}
 auto transportExpected=[&](){
  bytebuf expected=unrelatedExpected;
  if(argc==6){
   bytebuf layout=Read(argv[5]);if((layout.size()!=40 && layout.size()!=56) || transportTable==ResourceId())return bytebuf();
   uint64_t v[5];memcpy(v,layout.data(),40);if(v[1]>24 || v[2]>24 || v[3]>24-v[1] || v[3]>24-v[2])return bytebuf();
   auto address=c->GetBufferData(transportTable,v[0],8);if(address.size()!=8)return bytebuf();
   for(uint64_t i=0;i<v[3];i++)if(v[1]+i<8)expected[v[2]+i]=address[v[1]+i];
  }
  return expected;
 };
 for(const auto &resource:c->GetResources()){if(resource.name=="Native retirement compute texture parent")textureParent=resource.resourceId;if(resource.name=="Native retirement unrelated render output")renderOutput=resource.resourceId;if(resource.name=="Native retirement unrelated compute texture")computeTextureOutput=resource.resourceId;}
 bool frameBornInput=false;uint32_t computeEvent=0,renderEvent=0;
 const auto &sd=c->GetStructuredFile();
 uint32_t last=0,dispatch=0,unrelatedEvent=0;unsigned n=0;std::function<void(const rdcarray<ActionDescription>&)>visit=[&](auto&as){for(auto&a:as){last=std::max(last,a.eventId);if((a.flags&ActionFlags::Dispatch)&&!computeExpected.empty()){computeEvent=a.eventId;}if(((a.flags&ActionFlags::Dispatch)&&computeExpected.empty())||((a.flags&ActionFlags::Copy)&&a.copyDestination==output)){dispatch=a.eventId;n++;}if(unrelated!=ResourceId()&&(a.flags&ActionFlags::Copy)&&a.copyDestination==unrelated)unrelatedEvent=a.eventId;for(const auto &event:a.events)if(event.chunkIndex<sd.chunks.size()){const auto *chunk=sd.chunks[event.chunkIndex];if(chunk->name.beginsWith("MTLHeap::newBuffer")||chunk->name.beginsWith("MTLDevice::newBuffer")){const auto *buffer=chunk->FindChild("Buffer");if(buffer&&buffer->AsResourceId()==input)frameBornInput=true;}}visit(a.children);}};visit(c->GetRootActions());
 bool good=input!=ResourceId()&&output!=ResourceId()&&n==1&&initial.size()&&outputExpected.size();unsigned checks=0;
 // Clear is real render-pass work even with no draw. Locate the captured
 // factory by its attachment identity, never by a fixed event number.
 std::function<void(const rdcarray<ActionDescription>&)> findRender=[&](const auto &actions){for(const auto &a:actions){for(const auto &event:a.events){if(event.chunkIndex>=sd.chunks.size())continue;const auto *chunk=sd.chunks[event.chunkIndex];if(chunk->name!="MTLCommandBuffer::renderCommandEncoderWithDescriptor")continue;const auto *pass=chunk->FindChild("descriptor");const auto *colors=pass?pass->FindChild("colorAttachments"):nullptr;if(colors)for(size_t i=0;i<colors->NumChildren();i++){const auto *color=colors->GetChild(i);const auto *texture=color->FindChild("texture");if(texture&&texture->AsResourceId()==renderOutput)renderEvent=event.eventId;}}findRender(a.children);}};
 if(!renderExpected.empty())findRender(c->GetRootActions());
 const auto adjacentInitial=resultOffset?c->GetBufferData(unrelated,0,resultOffset):bytebuf();
 if(resultOffset)good&=adjacentInitial.size()==resultOffset;
 // First OpenCapture output is checked before any event seek.
 good&=c->GetBufferData(output,0,outputExpected.size())==outputExpected;
 if(!computeExpected.empty())good&=computeOutput!=ResourceId()&&c->GetBufferData(computeOutput,0,computeExpected.size())==computeExpected;
 if(!textureParentExpected.empty())good&=textureParent!=ResourceId()&&c->GetBufferData(textureParent,0,textureParentExpected.size())==textureParentExpected;
 if(!computeTextureExpected.empty())good&=computeTextureOutput!=ResourceId()&&c->GetTextureData(computeTextureOutput,{0,0,0})==computeTextureExpected;
 if(!renderExpected.empty())good&=renderOutput!=ResourceId()&&renderEvent&&c->GetTextureData(renderOutput,{0,0,0})==renderExpected;
 if(unrelated!=ResourceId()){
  auto actual=c->GetBufferData(unrelated,resultOffset,24),expected=transportExpected();
  if(actual!=expected){printf("First unrelated copy expected/actual:");for(byte v:expected)printf("%02x",v);printf("/");for(byte v:actual)printf("%02x",v);printf("\n");}
  good&=unrelatedEvent!=0&&actual==expected;
 }
 for(unsigned cycle=0;good&&cycle<2;cycle++)for(uint32_t eid:{last,0U,dispatch-1,dispatch,0U,last}){
  c->SetFrameEvent(eid,true);auto expected=outputExpected;if(eid<dispatch)for(size_t i=0;i<expected.size();i+=4){uint32_t value=0xcdcdcdcd;memcpy(expected.data()+i,&value,4);}
  // A frame-born alias has no logical object at EID 0. Check its defined
  // pre-frame physical carrier then, and the actual consumer object after
  // replaying the birth. Do not invent an initial state for the future object.
  const bool notBorn=eid==0 && frameBornInput;
  const bool useCarrier=notBorn && physicalInitial!=ResourceId();
  auto actualInput=c->GetBufferData(input,0,initial.size());
  bool in=notBorn?actualInput.empty():actualInput==initial;
  if(useCarrier)in&=c->GetBufferData(physicalInitial,0,initial.size())==initial;
  bool out=c->GetBufferData(output,0,expected.size())==expected;
  printf("event %u input %s output %s%s\n",eid,in?"MATCH":"FAIL",out?"MATCH":"FAIL",notBorn?" (captured frame birth: absent at EID0)":"");good&=in&&out&&c->GetFatalErrorStatus().OK();if(unrelated!=ResourceId())good&=c->GetBufferData(unrelated,resultOffset,24)==(eid<unrelatedEvent?Filled(initialResultValue):transportExpected());
  if(resultOffset){auto actual=c->GetBufferData(unrelated,0,resultOffset);bool adjacent=actual==adjacentInitial;if(!adjacent){printf("Adjacent live packet FAIL expected/actual: ");for(byte v:adjacentInitial)printf("%02x",v);printf("/");for(byte v:actual)printf("%02x",v);printf("\n");}good&=adjacent;}
  if(!computeExpected.empty()){
   auto expectedCompute=computeExpected;
   if(!computeEvent||eid<computeEvent)for(size_t i=0;i<expectedCompute.size();i+=4){uint32_t v=0xcdcdcdcd;memcpy(expectedCompute.data()+i,&v,4);}
   const bool compute=c->GetBufferData(computeOutput,0,expectedCompute.size())==expectedCompute;
   printf("unrelated compute event %u output %s\n",eid,compute?"MATCH":"FAIL");good&=compute;
  }
  if(!textureParentExpected.empty()){
   const bool parent=c->GetBufferData(textureParent,0,textureParentExpected.size())==(eid<computeEvent?textureParentInitial:textureParentExpected);
   printf("buffer texture physical parent/padding event %u %s\n",eid,parent?"MATCH":"FAIL");good&=parent;
  }
  if(!computeTextureExpected.empty()){
   auto expectedTexture=computeTextureExpected;if(!computeEvent||eid<computeEvent)memset(expectedTexture.data(),0,expectedTexture.size());
   const bool match=c->GetTextureData(computeTextureOutput,{0,0,0})==expectedTexture;
   printf("unrelated compute texture event %u output %s\n",eid,match?"MATCH":"FAIL");good&=match;
  }
  if(!renderExpected.empty()){
   auto expectedRender=renderExpected;if(eid<renderEvent)memset(expectedRender.data(),0,expectedRender.size());
   const bool match=c->GetTextureData(renderOutput,{0,0,0})==expectedRender;
   printf("unrelated render clear event %u output %s\n",eid,match?"MATCH":"FAIL");good&=match;
  }
  checks++;
 }
 c->Shutdown();RENDERDOC_ShutdownReplay();printf("GPU_COMPLETED; output %s; %u input/output/event/EID0 checks\n",good?"MATCH":"FAIL",checks);return good?0:6;
}
