// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Actions(const rdcarray<ActionDescription> &as,uint32_t &dispatch,uint32_t &last,uint32_t wanted,uint32_t &count)
{for(const auto &a:as){last=last>a.eventId?last:a.eventId;if(a.flags&ActionFlags::Dispatch){count++;if(count==wanted)dispatch=a.eventId;}Actions(a.children,dispatch,last,wanted,count);}}
int main(int argc,char **argv)
{
 if(argc!=3)return 2;
 @autoreleasepool {
 id<MTLDevice> native=MTLCreateSystemDefaultDevice();
 NSMutableArray<id<MTLTexture>> *padding=[NSMutableArray new];
 auto paddingDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:2 height:2 mipmapped:NO];
 paddingDesc.storageMode=MTLStorageModeShared;
 for(unsigned i=0;i<32;i++){id<MTLTexture> t=[native newTextureWithDescriptor:paddingDesc];if(!t||!t.gpuResourceID._impl)return 13;[padding addObject:t];}
 GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
 auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
 if(!result.OK())return 3;IReplayController *controller=nullptr;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
 if(!result.OK()||!controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
 ResourceId table,output,image;bool layerHalfPattern=false,volumePattern=false,packedVolumePattern=false,packedRenderPattern=false,shortPattern=false;
 for(const auto &r:controller->GetResources()) {if(r.name=="Frame packed render table"){table=r.resourceId;packedRenderPattern=true;}if(r.name=="Frame short integer table"){table=r.resourceId;shortPattern=true;}if(r.name=="Frame color table")table=r.resourceId;if(r.name=="Frame packed volume table"){table=r.resourceId;packedVolumePattern=true;}if(r.name=="Frame integer volume table"){table=r.resourceId;volumePattern=true;}if(r.name=="Frame half array table"){table=r.resourceId;layerHalfPattern=true;}if(r.name=="Frame color output")output=r.resourceId;}
 for(const auto &b:controller->GetBuffers()) {
  if(b.length==16)output=b.resourceId;
  if(b.length==24){const auto data=controller->GetBufferData(b.resourceId,0,24);uint64_t packet[3]={};
   if(data.size()==24){memcpy(packet,data.data(),24);if(packet[2]==0x4141414141414141ULL)table=b.resourceId;}}
 }
 for(const auto *c:controller->GetStructuredFile().chunks)if(c->name=="MTLBuffer::DescriptorSlotBinding" && c->FindChild("kind")->AsUInt64()==1) {
  image=c->FindChild("resource")->AsResourceId();
 }
 TextureDescription imageDesc;
 for(const auto &t:controller->GetTextures())if(t.resourceId==image)imageDesc=t;
 ResourceId parent;uint32_t baseMip=0,baseSlice=0;
 for(const auto *c:controller->GetStructuredFile().chunks)
  if(c->name=="MTLTexture::newTextureViewWithPixelFormat" && c->FindChild("View") && c->FindChild("View")->AsResourceId()==image) {
   parent=c->FindChild("Source")->AsResourceId();baseMip=uint32_t(c->FindChild("levels")->FindChild("location")->AsUInt64());baseSlice=uint32_t(c->FindChild("slices")->FindChild("location")->AsUInt64());
  }
 if(parent!=ResourceId()) {
  TextureDescription parentDesc;for(const auto &t:controller->GetTextures())if(t.resourceId==parent)parentDesc=t;
  if(parentDesc.resourceId!=parent || baseMip>=parentDesc.mips || !imageDesc.mips || imageDesc.mips>parentDesc.mips-baseMip ||
     imageDesc.width!=((parentDesc.width>>baseMip)?parentDesc.width>>baseMip:1U) ||
     imageDesc.height!=((parentDesc.height>>baseMip)?parentDesc.height>>baseMip:1U))return 5;
 }
 const bool array=imageDesc.type==TextureType::Texture2DArray;
 const bool half=imageDesc.format.compByteWidth==2;
 const bool integer=imageDesc.format.compType==CompType::UInt;
 const bool unorm8=imageDesc.format.compType==CompType::UNorm && imageDesc.format.type==ResourceFormatType::Regular && imageDesc.format.compByteWidth==1;
 const bool packed10=imageDesc.format.type==ResourceFormatType::R10G10B10A2;
 const bool packed11=imageDesc.format.type==ResourceFormatType::R11G11B10;
 const bool scalar=imageDesc.format.compCount==1;
 if(layerHalfPattern && (!half || !array || imageDesc.format.compCount!=4 || imageDesc.arraysize>8))return 5;
 uint32_t dispatch=0,last=0,count=0;Actions(controller->GetRootActions(),dispatch,last,packedRenderPattern?3:2,count);
 if(!dispatch||table==ResourceId()||output==ResourceId()||image==ResourceId()||imageDesc.resourceId!=image||!imageDesc.width||!imageDesc.height||!imageDesc.mips){fprintf(stderr,"Missing dispatch=%u table=%d output=%d image=%d chunks=%zu\n",dispatch,table!=ResourceId(),output!=ResourceId(),image!=ResourceId(),controller->GetStructuredFile().chunks.size());for(const auto &r:controller->GetResources())fprintf(stderr,"resource %s\n",r.name.c_str());return 5;}
 unsigned writes=0;bool readAtDispatch=false,writeAtRead=false;
 for(const auto &use:controller->GetUsage(image)) {
  if(use.usage==ResourceUsage::CS_RWResource)writes++;
  if(use.eventId==dispatch){readAtDispatch|=use.usage==ResourceUsage::CS_Resource;writeAtRead|=use.usage==ResourceUsage::CS_RWResource;}
 }
 const bool unknownAccess=getenv("RENDERDOC_METAL_FRAME_EXPECT_UNKNOWN_ACCESS")!=nullptr;
 if(!unknownAccess && (writes!=2||!readAtDispatch||writeAtRead)){fprintf(stderr,"Missing typed source usage: writes=%u read=%d writeAtRead=%d\n",writes,readAtDispatch,writeAtRead);for(const auto &use:controller->GetUsage(image))fprintf(stderr,"Usage EID=%u kind=%u\n",use.eventId,unsigned(use.usage));return 14;}
 if(unknownAccess){controller->SetFrameEvent(dispatch,true);for(const auto &access:controller->GetDescriptorAccess())if(access.type==DescriptorType::Image || access.type==DescriptorType::ReadWriteImage)return 14;printf("PASS Native JIT access display unknown; full output oracle remains enabled\n");}
 printf("PASS typed frame texture CS_RW usages=%u dimensions=%ux%u mips=%u\n",writes,imageDesc.width,imageDesc.height,imageDesc.mips);
 auto checkImage=[&](bool overwrite) {
  if(imageDesc.mips>1) {
   if((imageDesc.format.compByteWidth!=2 && imageDesc.format.compByteWidth!=4) || imageDesc.type!=TextureType::Texture2D)return false;
   for(unsigned mip=0;mip<imageDesc.mips;mip++) {
    const unsigned w=((imageDesc.width>>mip)?(imageDesc.width>>mip):1U),h=((imageDesc.height>>mip)?(imageDesc.height>>mip):1U);
    const auto data=controller->GetTextureData(image,{mip,0,0});
    if(data.size()!=size_t(w)*h*imageDesc.format.compCount*imageDesc.format.compByteWidth)return false;
    const float value=float((overwrite?24:8)+mip)/32.f;const _Float16 half=(_Float16)value;uint16_t bits=0;memcpy(&bits,&half,2);
    for(size_t i=0;i<data.size();i+=imageDesc.format.compByteWidth){
     if(imageDesc.format.compByteWidth==2){uint16_t actual=0;memcpy(&actual,data.data()+i,2);if(actual!=bits)return false;}
     else {float actual=0;memcpy(&actual,data.data()+i,4);if(actual!=value)return false;}
    }
    if(parent!=ResourceId() && data!=controller->GetTextureData(parent,{baseMip+mip,0,0}))return false;
    for(const auto &xy:rdcarray<rdcpair<unsigned,unsigned>>{{0,0},{w-1,h-1}}) {
     const auto pixel=controller->PickPixel(image,xy.first,xy.second,{mip,0,0},CompType::Typeless);
     for(unsigned component=0;component<4;component++)if(pixel.floatValue[component]!=(component<imageDesc.format.compCount?value:component==3?1.f:0.f))return false;
    }
   }
   return true;
  }
  const size_t bytes=packed11?4:imageDesc.format.compByteWidth*imageDesc.format.compCount;
  if(parent!=ResourceId()) {
   const auto viewData=controller->GetTextureData(image,{0,0,0});
   const auto parentData=controller->GetTextureData(parent,{baseMip,baseSlice,0});
   if(viewData.empty() || viewData!=parentData)return false;
   for(const auto &xy:rdcarray<rdcpair<unsigned,unsigned>>{{0,0},{imageDesc.width-1,imageDesc.height-1}}) {
    const auto v=controller->PickPixel(image,xy.first,xy.second,{0,0,0},CompType::Typeless);
    const auto p=controller->PickPixel(parent,xy.first,xy.second,{baseMip,baseSlice,0},CompType::Typeless);
    for(unsigned component=0;component<4;component++)if(v.floatValue[component]!=p.floatValue[component])return false;
   }
  }
  for(unsigned slice=0;slice<(array?imageDesc.arraysize:1u);slice++) {
   const auto data=controller->GetTextureData(image,{0,slice,0});
   if(parent!=ResourceId() && data!=controller->GetTextureData(parent,{baseMip,baseSlice+slice,0}))return false;
   if(data.size()!=size_t(imageDesc.width)*imageDesc.height*bytes*(array?1:imageDesc.depth))return false;
   const float values[]={overwrite?0.75f:0.25f,overwrite?0.25f:0.5f,overwrite?0.5f:0.75f,1.f};
   const uint16_t reds[]={0x3000,0x3400,0x3600,0x3800,0x3900,0x3a00,0x3b00,0x3c00};
   const uint16_t halves[]={uint16_t(layerHalfPattern?reds[overwrite?7-slice:slice]:(overwrite?0x3a00:0x3400)),uint16_t(overwrite?0x3400:0x3800),uint16_t(overwrite?0x3800:0x3a00),0x3c00};
   for(size_t i=0;i<data.size();i+=bytes) {
    if(packed10) {uint32_t v=0;memcpy(&v,data.data()+i,4);const uint32_t expected=(overwrite?767u:256u)|((overwrite?256u:512u)<<10)|((overwrite?512u:767u)<<20)|(3u<<30);if(v!=expected)return false;continue;}
    if(packed11) {uint32_t v=0;memcpy(&v,data.data()+i,4);const uint32_t reds[]={0x300,0x340,0x360,0x380,0x390,0x3a0,0x3b0,0x3c0};
     uint32_t red=array?reds[overwrite?7-slice:slice]:(overwrite?0x3a0:0x340);
     if(packedVolumePattern) {
       const auto z=unsigned(i/(size_t(imageDesc.width)*imageDesc.height*4));
       const float value=float(overwrite?64-z:1+z)/128.f;uint32_t bits=0;memcpy(&bits,&value,4);
       red=(((bits>>23)&255)-112)*64+((bits>>17)&63);
     }
     const uint32_t expected=red|((overwrite?0x340u:0x380u)<<11)|((overwrite?0x1c0u:0x1d0u)<<22);if(v!=expected)return false;continue;}
    if(unorm8) {if(data[i]!=(array?(overwrite?192-8*slice:64+8*slice):(overwrite?191:64)))return false;continue;}
    if(integer) {uint32_t v=0;memcpy(&v,data.data()+i,imageDesc.format.compByteWidth);if(v!=(shortPattern?(overwrite?65535u-unsigned(i/(size_t(imageDesc.width)*2)):32768u+unsigned(i/(size_t(imageDesc.width)*2))):(volumePattern?(overwrite?192u-unsigned(i/(size_t(imageDesc.width)*imageDesc.height*4)):64u+unsigned(i/(size_t(imageDesc.width)*imageDesc.height*4))):(overwrite?192u:64u))))return false;continue;}
    for(unsigned c=0;c<imageDesc.format.compCount;c++) {
    if(half){uint16_t v=0;memcpy(&v,data.data()+i+2*c,2);if(v!=halves[c])return false;}
    else {float v=0;memcpy(&v,data.data()+i+4*c,4);if(v!=values[c])return false;}
   }}
  }
  const float values[]={overwrite?0.75f:0.25f,overwrite?0.25f:0.5f,overwrite?0.5f:0.75f,1.f};
  const unsigned layers=array?imageDesc.arraysize:imageDesc.depth;
  for(unsigned slice : {0u,layers-1})for(const auto &location:rdcarray<rdcpair<uint32_t,uint32_t>>{{0,0},{imageDesc.width/2,imageDesc.height/2},{imageDesc.width-1,imageDesc.height-1}}) {
   const auto pixel=controller->PickPixel(image,location.first,location.second,{0,slice,0},CompType::Typeless);
   for(unsigned c=0;c<4;c++) {
    if(integer){const uint32_t expected=c==0?(shortPattern?(overwrite?65535-location.second:32768+location.second):volumePattern?(overwrite?192-slice:64+slice):(overwrite?192:64)):0;if(pixel.uintValue[c]!=expected)return false;}
    else if(unorm8){const float expected=c==0?float(array?(overwrite?192-8*slice:64+8*slice):(overwrite?191:64))/255.f:c==3?1.f:0.f;if(pixel.floatValue[c]!=expected)return false;}
    else if(packed10){const float expected=c==0?float(overwrite?767:256)/1023.f:c==1?float(overwrite?256:512)/1023.f:c==2?float(overwrite?512:767)/1023.f:1.f;if(pixel.floatValue[c]!=expected)return false;}
    else if(packed11){const float expected=c==0?(packedVolumePattern?float(overwrite?64-slice:1+slice)/128.f:array?float(overwrite?8-slice:1+slice)/8.f:(overwrite?0.75f:0.25f)):c==1?(overwrite?0.25f:0.5f):c==2?(overwrite?0.5f:0.75f):1.f;if(pixel.floatValue[c]!=expected)return false;}
    else if(layerHalfPattern){const float expected=c==0?float(overwrite?8-slice:1+slice)/8.f:values[c];if(pixel.floatValue[c]!=expected)return false;}
    else if(pixel.floatValue[c]!=(scalar&&c>0&&c<3?0.f:values[c]))return false;
   }
  }
  return true;
 };
 uint32_t clearEvent=0;
 if(packedRenderPattern) {
  if(!packed10 || imageDesc.type!=TextureType::Texture2D)return 5;
  for(const auto &use:controller->GetUsage(image))if(use.usage==ResourceUsage::Clear) {
   if(clearEvent)return 15;clearEvent=use.eventId;
  }
  if(!clearEvent || clearEvent>=dispatch)return 15;
 }
 auto checkClear=[&]() {
  const auto raw=controller->GetTextureData(image,{0,0,0});
  if(raw.size()!=size_t(imageDesc.width)*imageDesc.height*4)return false;
  const uint32_t expected=128u|(384u<<10)|(639u<<20)|(3u<<30);
  for(size_t i=0;i<raw.size();i+=4){uint32_t v=0;memcpy(&v,raw.data()+i,4);if(v!=expected)return false;}
  for(const auto &xy:rdcarray<rdcpair<unsigned,unsigned>>{{0,0},{imageDesc.width-1,imageDesc.height-1}}) {
   const auto pixel=controller->PickPixel(image,xy.first,xy.second,{0,0,0},CompType::Typeless);
   const float expected[]={128.f/1023.f,384.f/1023.f,639.f/1023.f,1.f};
   for(unsigned c=0;c<4;c++)if(pixel.floatValue[c]!=expected[c])return false;
  }
  return true;
 };
 for(unsigned cycle=0;cycle<4;cycle++) {
  controller->SetFrameEvent(last,true);if(!checkImage(true))return 6;
  controller->SetFrameEvent(dispatch,true);if(!checkImage(false))return 7;
  auto data=controller->GetBufferData(output,0,16);uint32_t words[4]={};if(data.size()!=16)return 8;memcpy(words,data.data(),16);
  if(words[0]!=(shortPattern?32768+imageDesc.height-1:packedVolumePattern?2*imageDesc.depth:volumePattern?64+imageDesc.depth-1:(layerHalfPattern||(packed11&&array))?32*imageDesc.arraysize:unorm8&&array?64+8*(imageDesc.arraysize-1):imageDesc.mips>1?64+8*(imageDesc.mips-1):64)||words[1]!=(scalar?0:128)||words[2]!=(scalar?0:192)||words[3]!=0xdeadbeef)return 9;
  data=controller->GetBufferData(table,0,24);uint64_t packet[3]={};if(data.size()!=24)return 10;memcpy(packet,data.data(),24);
  if(packet[0]||!packet[1]||packet[1]==strtoull(argv[2],nullptr,10)||packet[2]!=0x4141414141414141ULL)return 11;
  printf("GPU frame family PASS cycle=%u pixels=%llu result=%u/%u/%u NativeID=%llu\n",cycle,(unsigned long long)imageDesc.width*imageDesc.height*(array?imageDesc.arraysize:imageDesc.depth),words[0],words[1],words[2],(unsigned long long)packet[1]);
  if(packedRenderPattern){controller->SetFrameEvent(clearEvent,true);if(!checkClear())return 16;printf("PASS packed RT clear pixels cycle=%u EID=%u\n",cycle,clearEvent);}
  controller->SetFrameEvent(0,true);data=controller->GetBufferData(table,0,24);memcpy(packet,data.data(),24);
  if(packet[0]||packet[1]||packet[2])return 12;
 }
 controller->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS frame family full pixels, PickPixel, typed GPU ID and zero-at-EID0");return 0;
 }
}
