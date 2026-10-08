// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
 @autoreleasepool {
  id<MTLDevice> device=MTLCreateSystemDefaultDevice(); NSError *error=nil;
  const bool r16Large=getenv("RENDERDOC_METAL_FRAME_R16_MAXSIZE")!=nullptr;
  const unsigned r16Mips=getenv("RENDERDOC_METAL_FRAME_R16_MIPS")?unsigned(strtoul(getenv("RENDERDOC_METAL_FRAME_R16_MIPS"),nullptr,10)):0;
  const bool generic=getenv("RENDERDOC_METAL_FRAME_GENERIC_WIDTH")!=nullptr;
  const unsigned viewCount=getenv("RENDERDOC_METAL_FRAME_GENERIC_VIEW_COUNT")?unsigned(strtoul(getenv("RENDERDOC_METAL_FRAME_GENERIC_VIEW_COUNT"),nullptr,10)):1;
  const bool mipView=getenv("RENDERDOC_METAL_FRAME_VIEW_MIP")!=nullptr;
  const unsigned viewMip=mipView?unsigned(strtoul(getenv("RENDERDOC_METAL_FRAME_VIEW_MIP"),nullptr,10)):0;
  if(mipView && (!r16Mips || viewMip>=r16Mips || viewCount>r16Mips-viewMip))return 19;
  if(r16Mips && (r16Mips<2 || (!generic && r16Mips>(r16Large?10:9))))return 19;
  const bool array=getenv("RENDERDOC_METAL_FRAME_FAMILY_ARRAY")!=nullptr;
  const bool wide=getenv("RENDERDOC_METAL_FRAME_FAMILY_FLOAT32")!=nullptr;
  const bool sliceView=getenv("RENDERDOC_METAL_FRAME_GENERIC_SLICE_BASE")!=nullptr;
  const bool view=mipView||sliceView;
  const bool integer=getenv("RENDERDOC_METAL_FRAME_FAMILY_UINT")!=nullptr;
  const bool unorm8=getenv("RENDERDOC_METAL_FRAME_FAMILY_UNORM8")!=nullptr;
  const bool packed10=getenv("RENDERDOC_METAL_FRAME_FAMILY_PACKED10")!=nullptr;
  const bool packed11=getenv("RENDERDOC_METAL_FRAME_FAMILY_PACKED11")!=nullptr;
  const bool atomic=getenv("RENDERDOC_METAL_FRAME_FAMILY_ATOMIC")!=nullptr;
  const bool work=getenv("RENDERDOC_METAL_FRAME_FAMILY_WORK")!=nullptr;
  const unsigned dispatchCount=getenv("RENDERDOC_METAL_FRAME_FAMILY_DISPATCHES")?unsigned(strtoul(getenv("RENDERDOC_METAL_FRAME_FAMILY_DISPATCHES"),nullptr,10)):3;
  if(dispatchCount<3||dispatchCount>256)return 18;
  const bool extra=integer||unorm8||packed10||packed11;
  if(unsigned(integer)+unsigned(unorm8)+unsigned(packed10)+unsigned(packed11)>1 || (atomic&&!integer))return 16;
  const bool actual=getenv("RENDERDOC_METAL_FRAME_FAMILY_ACTUAL")!=nullptr;
  const bool twoD=getenv("RENDERDOC_METAL_FRAME_FAMILY_2D")!=nullptr;
  const bool extended2D=getenv("RENDERDOC_METAL_FRAME_FAMILY_EXTENDED_2D")!=nullptr;
  const bool packed2D=getenv("RENDERDOC_METAL_FRAME_PACKED_2D")!=nullptr;
  const bool packedRender=getenv("RENDERDOC_METAL_FRAME_PACKED_RENDER")!=nullptr;
  if(packedRender && (!packed2D || !twoD || !packed10 || work || dispatchCount!=3))return 19;
  const bool packedMax=getenv("RENDERDOC_METAL_FRAME_PACKED_MAXSIZE")!=nullptr;
  const bool packedSmall=getenv("RENDERDOC_METAL_FRAME_PACKED_SMALL")!=nullptr;
  const bool r8Array=getenv("RENDERDOC_METAL_FRAME_R8_ARRAY")!=nullptr;
  const bool r8Max=getenv("RENDERDOC_METAL_FRAME_R8_ARRAY_MAXSIZE")!=nullptr;
  const bool r8Small=getenv("RENDERDOC_METAL_FRAME_R8_ARRAY_SMALL")!=nullptr;
  const bool r11Array=getenv("RENDERDOC_METAL_FRAME_R11_ARRAY")!=nullptr;
  const bool r11Max=getenv("RENDERDOC_METAL_FRAME_R11_ARRAY_MAXSIZE")!=nullptr;
  const bool r11Layers=getenv("RENDERDOC_METAL_FRAME_R11_ARRAY_LAYERS")!=nullptr;
  const bool r11Small=getenv("RENDERDOC_METAL_FRAME_R11_ARRAY_SMALL")!=nullptr;
  const char *uintVolume=getenv("RENDERDOC_METAL_FRAME_UINT_VOLUME");
  const bool volume=uintVolume!=nullptr;
  if(volume && (!integer || array || twoD || wide || atomic || !work || dispatchCount!=3 || r16Mips ||
      (strcmp(uintVolume,"UE") && strcmp(uintVolume,"max") && strcmp(uintVolume,"small"))))return 19;
  const char *r11Volume=getenv("RENDERDOC_METAL_FRAME_R11_VOLUME");
  const bool packedVolume=r11Volume!=nullptr;
  if(packedVolume && (!packed11 || array || twoD || wide || volume || atomic || !work || dispatchCount!=3 ||
      (strcmp(r11Volume,"UE") && strcmp(r11Volume,"max") && strcmp(r11Volume,"small"))))return 19;
  const char *r11Large2D=getenv("RENDERDOC_METAL_FRAME_R11_LARGE_2D");
  const bool largePacked=r11Large2D!=nullptr;
  if(largePacked && (!packed11 || !twoD || array || wide || volume || packedVolume || atomic || !work ||
      dispatchCount!=3 || r16Mips || extended2D ||
      (strcmp(r11Large2D,"max") && strcmp(r11Large2D,"rect") && strcmp(r11Large2D,"small"))))return 19;
  const char *r16Uint2D=getenv("RENDERDOC_METAL_FRAME_R16_UINT_2D");
  const bool short2D=r16Uint2D!=nullptr;
  if(short2D && (!integer || !twoD || array || wide || volume || packedVolume || atomic || !work ||
      dispatchCount!=3 || r16Mips || extended2D ||
      (strcmp(r16Uint2D,"UE") && strcmp(r16Uint2D,"max") && strcmp(r16Uint2D,"small"))))return 19;
  const bool halfArray=getenv("RENDERDOC_METAL_FRAME_HALF_ARRAY")!=nullptr;
  const bool halfMax=getenv("RENDERDOC_METAL_FRAME_HALF_ARRAY_MAXSIZE")!=nullptr;
  const bool halfLayers=getenv("RENDERDOC_METAL_FRAME_HALF_ARRAY_LAYERS")!=nullptr;
  const bool halfSmall=getenv("RENDERDOC_METAL_FRAME_HALF_ARRAY_SMALL")!=nullptr;
  const bool halfLegacyRT=getenv("RENDERDOC_METAL_FRAME_HALF_ARRAY_LEGACY_RTV")!=nullptr;
  const bool halfLegacy64=getenv("RENDERDOC_METAL_FRAME_HALF_ARRAY_LEGACY64")!=nullptr;
  if(halfArray && (!array || extra || wide || twoD || extended2D || r16Mips || work || dispatchCount!=3 || unsigned(halfMax)+unsigned(halfLayers)+unsigned(halfSmall)>1))return 19;
  if((halfLegacyRT && (!halfArray || !halfSmall)) || (halfLegacy64 && !halfLegacyRT))return 19;
  if(r11Array && (!packed11 || !array || twoD || extended2D || r16Mips || work || dispatchCount!=3 || unsigned(r11Max)+unsigned(r11Layers)+unsigned(r11Small)>1))return 19;
  if(r8Array && (!unorm8 || !array || twoD || extended2D || r16Mips || work || dispatchCount!=3 || (r8Max&&r8Small)))return 19;
  if(packed2D && (!packed10 || !twoD || extended2D || r16Mips || work || dispatchCount!=3 || (packedMax&&packedSmall)))return 19;
  if(r16Mips && (!twoD || !extended2D || array || extra || wide || work || (dispatchCount!=3 && !generic)))return 19;
  if(twoD && array)return 15;
  if(extended2D && (!twoD || extra || array))return 15;
  NSUInteger width=short2D?(!strcmp(r16Uint2D,"small")?3:4096):largePacked?(!strcmp(r11Large2D,"max")?4096:!strcmp(r11Large2D,"rect")?2048:3):packedVolume?(!strcmp(r11Volume,"max")?64:!strcmp(r11Volume,"small")?3:10):volume?(!strcmp(uintVolume,"max")?256:!strcmp(uintVolume,"small")?3:192):halfArray?(halfMax?512:halfLayers?256:halfSmall?3:320):r11Array?(r11Max?512:r11Layers?256:r11Small?3:320):r8Array?(r8Max?512:r8Small?3:320):packed2D?(packedMax?512:packedSmall?3:320):r16Mips?(r16Large?512:256):extended2D?160:integer?(twoD?128:1):unorm8?512:packed10?1:actual?64:3;
  NSUInteger height=short2D?(!strcmp(r16Uint2D,"max")?512:!strcmp(r16Uint2D,"small")?5:16):largePacked?(!strcmp(r11Large2D,"max")?4096:!strcmp(r11Large2D,"rect")?1376:5):packedVolume?(!strcmp(r11Volume,"max")?64:!strcmp(r11Volume,"small")?5:8):volume?(!strcmp(uintVolume,"max")?64:!strcmp(uintVolume,"small")?5:48):halfArray?(halfMax?512:halfLayers?128:halfSmall?5:240):r11Array?(r11Max?512:r11Layers?128:r11Small?5:240):r8Array?(r8Max?512:r8Small?5:240):packed2D?(packedMax?512:packedSmall?5:240):r16Mips?(r16Large?512:128):extended2D?120:integer||packed10?1:unorm8?512:actual?64:5;
  NSUInteger layers=packedVolume?(!strcmp(r11Volume,"max")?64:!strcmp(r11Volume,"small")?2:26):volume?(!strcmp(uintVolume,"max")?64:!strcmp(uintVolume,"small")?2:48):halfArray?(halfMax?2:halfLayers?8:halfSmall?3:1):r11Array?(r11Max?2:r11Layers?8:r11Small?3:1):r8Array?(r8Max?8:r8Small?3:1):twoD||packed10?1:integer?3:actual?64:2;
  if(generic) {
   width=strtoul(getenv("RENDERDOC_METAL_FRAME_GENERIC_WIDTH"),nullptr,10);
   height=strtoul(getenv("RENDERDOC_METAL_FRAME_GENERIC_HEIGHT"),nullptr,10);
   if(!(twoD||array) || (twoD&&!r16Mips) || !width || !height || width>2048 || height>2048)return 19;
   if(array && getenv("RENDERDOC_METAL_FRAME_GENERIC_LAYERS"))layers=strtoul(getenv("RENDERDOC_METAL_FRAME_GENERIC_LAYERS"),nullptr,10);
  }
  const NSUInteger parentLayers=layers;
  const NSUInteger sliceBase=sliceView?strtoul(getenv("RENDERDOC_METAL_FRAME_GENERIC_SLICE_BASE"),nullptr,10):0;
  const NSUInteger sliceCount=sliceView?strtoul(getenv("RENDERDOC_METAL_FRAME_GENERIC_SLICE_COUNT"),nullptr,10):layers;
  if(sliceView){if(!array || !sliceCount || sliceBase>=parentLayers || sliceCount>parentLayers-sliceBase)return 19;layers=sliceCount;}
  NSString *source=@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct BufferEntry { const device uint *value [[id(0)]]; ulong zero [[id(1)]]; ulong metadata [[id(2)]]; };
struct Entry { ulong zero [[id(0)]]; texture3d<float,access::read_write> image [[id(1)]]; ulong metadata [[id(2)]]; };
kernel void read_color(const device ulong *root [[buffer(0)]], device uint *result [[buffer(1)]]) {
 const device Entry *table=reinterpret_cast<const device Entry *>(root[0]);
 float4 c=table[0].image.read(uint3(2,4,1));
 result[0]=uint(c.r*256); result[1]=uint(c.g*256); result[2]=uint(c.b*256);
 const device BufferEntry *input=reinterpret_cast<const device BufferEntry *>(root[2]);
 result[3]=table[0].metadata==0x4141414141414141ul&&input[0].value[0]==13&&input[0].metadata==0x5151515151515151ul?0xdeadbeef:0xbad;
}
kernel void write_color(const device ulong *root [[buffer(0)]]) {
 const device Entry *table=reinterpret_cast<const device Entry *>(root[0]);
 const float4 value=root[1]?float4(0.75,0.25,0.5,1):float4(0.25,0.5,0.75,1);
 for(uint z=0;z<2;z++)for(uint y=0;y<5;y++)for(uint x=0;x<3;x++)
  table[0].image.write(value,uint3(x,y,z));
}
)MSL";
  if(array){source=[source stringByReplacingOccurrencesOfString:@"texture3d<float,access::read_write>" withString:@"texture2d_array<float,access::read_write>"];
   source=[source stringByReplacingOccurrencesOfString:@".read(uint3(2,4,1))" withString:@".read(uint2(2,4),1)"];
   source=[source stringByReplacingOccurrencesOfString:@".write(value,uint3(x,y,z))" withString:@".write(value,uint2(x,y),z)"];}

  if(twoD){source=[source stringByReplacingOccurrencesOfString:@"texture3d<float,access::read_write>" withString:@"texture2d<float,access::read_write>"];
   source=[source stringByReplacingOccurrencesOfString:@".read(uint3(2,4,1))" withString:@".read(uint2(2,4))"];
   source=[source stringByReplacingOccurrencesOfString:@".write(value,uint3(x,y,z))" withString:@".write(value,uint2(x,y))"];}
  if(integer){source=[source stringByReplacingOccurrencesOfString:@"<float,access::read_write>" withString:@"<uint,access::read_write>"];
   source=[source stringByReplacingOccurrencesOfString:@"float4 c=" withString:@"uint4 c="];
   source=[source stringByReplacingOccurrencesOfString:@"const float4 value=root[1]?float4(0.75,0.25,0.5,1):float4(0.25,0.5,0.75,1);" withString:@"const uint4 value=root[1]?uint4(192,64,128,1):uint4(64,128,192,1);"];
   source=[source stringByReplacingOccurrencesOfString:@"uint(c.r*256)" withString:@"c.r"];
   source=[source stringByReplacingOccurrencesOfString:@"uint(c.g*256)" withString:@"c.g"];
   source=[source stringByReplacingOccurrencesOfString:@"uint(c.b*256)" withString:@"c.b"];}
  else {source=[source stringByReplacingOccurrencesOfString:@"uint(c.r*256)" withString:@"uint(c.r*256+0.5)"];
   source=[source stringByReplacingOccurrencesOfString:@"uint(c.g*256)" withString:@"uint(c.g*256+0.5)"];
   source=[source stringByReplacingOccurrencesOfString:@"uint(c.b*256)" withString:@"uint(c.b*256+0.5)"];}
  if(atomic)source=[source stringByReplacingOccurrencesOfString:@"table[0].image.write(value,uint2(x,y));"
    withString:@"{ table[0].image.atomic_exchange(uint2(x,y),uint4(0)); table[0].image.atomic_fetch_add(uint2(x,y),value); }"];
  source=[source stringByReplacingOccurrencesOfString:@".read(uint2(2,4),1)" withString:[NSString stringWithFormat:@".read(uint2(%lu,%lu),%lu)",(unsigned long)width-1,(unsigned long)height-1,(unsigned long)layers-1]];
  source=[source stringByReplacingOccurrencesOfString:@"uint2(2,4)" withString:[NSString stringWithFormat:@"uint2(%lu,%lu)",(unsigned long)width-1,(unsigned long)height-1]];
  source=[source stringByReplacingOccurrencesOfString:@"uint3(2,4,1)" withString:[NSString stringWithFormat:@"uint3(%lu,%lu,%lu)",(unsigned long)width-1,(unsigned long)height-1,(unsigned long)layers-1]];
  source=[source stringByReplacingOccurrencesOfString:@"for(uint z=0;z<2;z++)for(uint y=0;y<5;y++)for(uint x=0;x<3;x++)"
    withString:[NSString stringWithFormat:@"for(uint z=0;z<%lu;z++)for(uint y=0;y<%lu;y++)for(uint x=0;x<%lu;x++)",(unsigned long)layers,(unsigned long)height,(unsigned long)width]];
  if(r16Mips) {
   source=[source stringByReplacingOccurrencesOfString:[NSString stringWithFormat:@"for(uint z=0;z<%lu;z++)for(uint y=0;y<%lu;y++)for(uint x=0;x<%lu;x++)",(unsigned long)layers,(unsigned long)height,(unsigned long)width]
     withString:@"for(uint mip=0;mip<table[0].image.get_num_mip_levels();mip++)for(uint y=0;y<table[0].image.get_height(mip);y++)for(uint x=0;x<table[0].image.get_width(mip);x++)"];
   source=[source stringByReplacingOccurrencesOfString:@"table[0].image.write(value,uint2(x,y));"
     withString:@"table[0].image.write(float4((root[1]?24.0:8.0)+float(mip))/32.0,uint2(x,y),mip);"];
   source=[source stringByReplacingOccurrencesOfString:[NSString stringWithFormat:@".read(uint2(%lu,%lu))",(unsigned long)width-1,(unsigned long)height-1]
     withString:[NSString stringWithFormat:@".read(uint2(%lu,%lu),%u)",(unsigned long)MAX(NSUInteger(1),width>>(mipView?viewMip+viewCount-1:r16Mips-1))-1,(unsigned long)MAX(NSUInteger(1),height>>(mipView?viewMip+viewCount-1:r16Mips-1))-1,mipView?viewCount-1:r16Mips-1]];
  }
  if(r8Array)source=[source stringByReplacingOccurrencesOfString:@"table[0].image.write(value,uint2(x,y),z);"
    withString:@"table[0].image.write(float4((root[1]?192.0-8.0*z:64.0+8.0*z)/255.0),uint2(x,y),z);"];
  if(r11Array || halfArray)source=[source stringByReplacingOccurrencesOfString:@"table[0].image.write(value,uint2(x,y),z);"
    withString:@"table[0].image.write(float4(root[1]?(8.0-z)/8.0:(1.0+z)/8.0,root[1]?0.25:0.5,root[1]?0.5:0.75,1.0),uint2(x,y),z);"];
  if(packedVolume)source=[source stringByReplacingOccurrencesOfString:@"table[0].image.write(value,uint3(x,y,z));"
    withString:@"table[0].image.write(float4(root[1]?(64.0-z)/128.0:(1.0+z)/128.0,root[1]?0.25:0.5,root[1]?0.5:0.75,1.0),uint3(x,y,z));"];
  if(volume)source=[source stringByReplacingOccurrencesOfString:@"table[0].image.write(value,uint3(x,y,z));"
    withString:@"table[0].image.write(uint4(root[1]?192-z:64+z,0,0,1),uint3(x,y,z));"];
  if(r16Mips || packed2D || r8Array || r11Array || halfArray || volume || packedVolume || largePacked || short2D) {
   // The table is immutable during these dispatches. Keep its texture handle
   // local; usage validation deliberately does not infer dynamic loop phis.
   source=[source stringByReplacingOccurrencesOfString:@"table[0].image" withString:@"image"];
   source=[source stringByReplacingOccurrencesOfString:@"const device Entry *table=reinterpret_cast<const device Entry *>(root[0]);"
     withString:@"const device Entry *table=reinterpret_cast<const device Entry *>(root[0]); const auto image=table[0].image;"];
  }
  if(work) {
   source=[source stringByReplacingOccurrencesOfString:@"kernel void write_color(const device ulong *root [[buffer(0)]])"
     withString:@"kernel void write_color(const device ulong *root [[buffer(0)]],uint3 position [[thread_position_in_grid]])"];
   source=[source stringByReplacingOccurrencesOfString:[NSString stringWithFormat:@"for(uint z=0;z<%lu;z++)for(uint y=0;y<%lu;y++)for(uint x=0;x<%lu;x++)",(unsigned long)layers,(unsigned long)height,(unsigned long)width]
     withString:[NSString stringWithFormat:@"const uint x=position.x,y=position.y,z=position.z; if(x>=%lu||y>=%lu||z>=%lu)return;",(unsigned long)width,(unsigned long)height,(unsigned long)layers]];
  }
  if(volume) {
   // Keep the existing per-dispatch work budget: each invocation writes at
   // most four adjacent voxels, including a short row at the small boundary.
   source=[source stringByReplacingOccurrencesOfString:@"const uint x=position.x,y=position.y,z=position.z;" withString:@"const uint x=position.x*4,y=position.y,z=position.z;"];
   source=[source stringByReplacingOccurrencesOfString:@"image.write(uint4(root[1]?192-z:64+z,0,0,1),uint3(x,y,z));"
     withString:[NSString stringWithFormat:@"for(uint dx=0;dx<4;dx++)if(x+dx<%lu)image.write(uint4(root[1]?192-z:64+z,0,0,1),uint3(x+dx,y,z));",(unsigned long)width]];
  }
  if(largePacked) {
   // 8x8 pixels per invocation keep 4096² within the existing work budget.
   // Bounds apply to each pixel, including a partial tile at the rectangle edge.
   source=[source stringByReplacingOccurrencesOfString:@"const uint x=position.x,y=position.y,z=position.z;"
     withString:@"const uint x=position.x*8,y=position.y*8,z=position.z;"];
   source=[source stringByReplacingOccurrencesOfString:@"image.write(value,uint2(x,y));"
     withString:[NSString stringWithFormat:@"for(uint dy=0;dy<8;dy++)for(uint dx=0;dx<8;dx++)if(x+dx<%lu&&y+dy<%lu)image.write(value,uint2(x+dx,y+dy));",(unsigned long)width,(unsigned long)height]];
  }
  if(short2D) {
   source=[source stringByReplacingOccurrencesOfString:@"const uint x=position.x,y=position.y,z=position.z;"
     withString:@"const uint x=position.x*8,y=position.y,z=position.z;"];
   source=[source stringByReplacingOccurrencesOfString:@"image.write(value,uint2(x,y));"
     withString:[NSString stringWithFormat:@"for(uint dx=0;dx<8;dx++)if(x+dx<%lu)image.write(uint4(root[1]?65535-y:32768+y,0,0,1),uint2(x+dx,y));",(unsigned long)width]];
  }
  if(const char *path=getenv("RENDERDOC_METAL_FRAME_SHADER_EXPORT")) {
   return [source writeToFile:[NSString stringWithUTF8String:path] atomically:YES encoding:NSUTF8StringEncoding error:&error]?0:20;
  }
  id<MTLLibrary> library=nil;
  if(const char *path=getenv("RENDERDOC_METAL_FRAME_LIBRARY")) {
   NSData *code=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:path]];
   if(!code.length)return 20;
   dispatch_data_t binary=dispatch_data_create(code.bytes,code.length,dispatch_get_main_queue(),DISPATCH_DATA_DESTRUCTOR_DEFAULT);
   library=[device newLibraryWithData:binary error:&error];
  } else library=[device newLibraryWithSource:source options:nil error:&error];
  id<MTLComputePipelineState> pipeline=library?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"read_color"] error:&error]:nil;
  id<MTLComputePipelineState> writer=library?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"write_color"] error:&error]:nil;
  if(!pipeline||!writer){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
  auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:short2D?MTLPixelFormatR16Uint:halfArray?MTLPixelFormatRGBA16Float:r16Mips?MTLPixelFormatR16Float:integer?MTLPixelFormatR32Uint:unorm8?MTLPixelFormatR8Unorm:packed10?MTLPixelFormatRGB10A2Unorm:packed11?MTLPixelFormatRG11B10Float:array?MTLPixelFormatR32Float:wide?MTLPixelFormatRGBA32Float:MTLPixelFormatRGBA16Float width:width height:height mipmapped:NO];
  if(r16Mips)d.mipmapLevelCount=r16Mips;
  if(const char *format=getenv("RENDERDOC_METAL_FRAME_GENERIC_FORMAT"))d.pixelFormat=(MTLPixelFormat)strtoul(format,nullptr,10);
  d.textureType=twoD?MTLTextureType2D:array?MTLTextureType2DArray:MTLTextureType3D;d.arrayLength=array?parentLayers:1;d.depth=array||twoD?1:layers;
  d.resourceOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
  d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite|((!array&&!wide&&!integer&&!packed10&&!r16Mips)?MTLTextureUsageRenderTarget:0);
  if(packedVolume||largePacked)d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
  if(packedRender)d.usage|=MTLTextureUsageRenderTarget;
  if(halfLegacyRT)d.usage|=MTLTextureUsageRenderTarget;
  if(atomic) {if(@available(macOS 14.0,*))d.usage|=MTLTextureUsageShaderAtomic;else return 17;}
  MTLTextureDescriptor *rw=[d copy];rw.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
  const auto layout=[device heapTextureSizeAndAlignWithDescriptor:d];
  const auto rwLayout=[device heapTextureSizeAndAlignWithDescriptor:rw];
  const NSUInteger rwOffset=rwLayout.align?(layout.size+rwLayout.align-1)/rwLayout.align*rwLayout.align:0;
  NSUInteger stride=(rwOffset+rwLayout.size+layout.align-1)/layout.align*layout.align;
  if(!layout.size||!layout.align||!rwLayout.size||!rwLayout.align||stride*2>(largePacked?576U*1024*1024:generic||actual||twoD||r8Array||r11Array||halfArray||volume||packedVolume?16U*1024*1024:1024*1024))return 3;
  auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;
  hd.hazardTrackingMode=MTLHazardTrackingModeTracked;const NSUInteger requestedOffset=getenv("RENDERDOC_METAL_FRAME_GENERIC_OFFSET")?strtoul(getenv("RENDERDOC_METAL_FRAME_GENERIC_OFFSET"),nullptr,10):0;
  const NSUInteger baseOffset=(requestedOffset+layout.align-1)/layout.align*layout.align;
  hd.size=MAX(NSUInteger(4096),stride*2+baseOffset);
  if(const char *bytes=getenv("RENDERDOC_METAL_FRAME_HEAP_BYTES")) {
   const uint64_t requested=strtoull(bytes,nullptr,10);
   if(requested<hd.size||requested>576ULL*1024*1024)return 18;
   hd.size=NSUInteger(requested);
  }
  id<MTLHeap> heap=[device newHeapWithDescriptor:hd];id<MTLCommandQueue> queue=[device newCommandQueue];
  const uint64_t zero[3]={};id<MTLBuffer> table=[device newBufferWithBytes:zero length:24 options:MTLResourceStorageModeShared];
  id<MTLBuffer> output=[device newBufferWithLength:16 options:MTLResourceStorageModeShared];
  table.label=short2D?@"Frame short integer table":packedRender?@"Frame packed render table":packedVolume?@"Frame packed volume table":volume?@"Frame integer volume table":halfArray?@"Frame half array table":@"Frame color table";output.label=@"Frame color output";
  const uint32_t value=13;id<MTLBuffer> input=[device newBufferWithBytes:&value length:4 options:MTLResourceStorageModeShared];
  const uint64_t inputPacket[]={input.gpuAddress,0,0x5151515151515151ULL};
  id<MTLBuffer> inputTable=[device newBufferWithBytes:inputPacket length:24 options:MTLResourceStorageModeShared];
  CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
  layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
  RENDERDOC_API_1_7_0 *api=nullptr;
  if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH")) {
   auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
   if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 4;
   api->SetCaptureFilePathTemplate(path);RENDERDOC_AnnotationValue v={};v.uint32=generic?65:volume||packedVolume||largePacked||short2D?65:halfLegacy64?64:extended2D||packed2D||r8Array||r11Array||halfArray?65:work||dispatchCount>4?46:extra?44:actual||twoD?43:39;
   if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 5;
  }
  auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t e) {
   if(!api)return uint32_t(0);RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=e;
   return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
  };
  if(annotation(table,"metal.descriptorTable",1,0,1,24)||annotation(inputTable,"metal.descriptorTable",1,0,1,24)||
     annotation(inputTable,"metal.descriptorSlotEvent",0,1,0,0)||annotation(inputTable,"metal.descriptorSlotEvent",0,1,2,0)||
     annotation(inputTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)input,0))return 6;
  uint64_t capturedIDs[2]={};
  for(unsigned capture=0;capture<(api?2u:1u);capture++) {
   memset(output.contents,0,16);memset(table.contents,0,24);
   if(api)api->StartFrameCapture(nullptr,nullptr);
   id<MTLTexture> image=[heap newTextureWithDescriptor:d offset:baseOffset+capture*stride];if(!image)return 7;
   // Exact UE usage (read/write, no render target) also has a legal CPU birth.
   id<MTLTexture> rwImage=[heap newTextureWithDescriptor:rw offset:baseOffset+capture*stride+rwOffset];if(!rwImage)return 8;
   id<MTLTexture> sampled=view?[image newTextureViewWithPixelFormat:d.pixelFormat textureType:array?MTLTextureType2DArray:MTLTextureType2D levels:NSMakeRange(viewMip,viewCount) slices:NSMakeRange(sliceBase,array?sliceCount:1)]:image;
   if(!sampled)return 9;capturedIDs[capture]=sampled.gpuResourceID._impl;
   uint64_t packet[3]={0,capturedIDs[capture],0x4141414141414141ULL};memcpy(table.contents,packet,24);
   if(annotation(table,"metal.descriptorSlotEvent",0,capture+1,0,5)||annotation(table,"metal.descriptorSlotEvent",0,capture+1,2,5)||
      annotation(table,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)sampled,0))return 10;
   id<MTLCommandBuffer> command=[queue commandBuffer];
   auto encode=[&](id<MTLComputePipelineState> state,uint64_t phase,bool read) {
    id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:state];
    if(annotation(compute,"metal.descriptorInlineLayout",0,0,2,16)||
       annotation(compute,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)table,0)||
       annotation(compute,"metal.descriptorInlineBinding",0,1,(uint64_t)(__bridge void *)inputTable,0))return false;
    const uint64_t root[]={table.gpuAddress,phase,inputTable.gpuAddress,0};[compute setBytes:root length:32 atIndex:0];
    if(read)[compute setBuffer:output offset:0 atIndex:1];
    [compute useResource:input usage:MTLResourceUsageRead];[compute useResource:inputTable usage:MTLResourceUsageRead];
    [compute useResource:table usage:MTLResourceUsageRead];[compute useResource:sampled usage:read?MTLResourceUsageRead:MTLResourceUsageWrite];
    const NSUInteger tx=volume?8:twoD?8:4,ty=volume?8:twoD?(integer?1:8):4,tz=twoD?1:4;
    const MTLSize threads=work&&!read?MTLSizeMake(tx,ty,tz):MTLSizeMake(1,1,1);
    const MTLSize groups=work&&!read?MTLSizeMake((width+(largePacked||short2D?8:volume?4:1)*tx-1)/((largePacked||short2D?8:volume?4:1)*tx),(height+(largePacked?8:1)*ty-1)/((largePacked?8:1)*ty),(layers+tz-1)/tz):MTLSizeMake(1,1,1);
    [compute dispatchThreadgroups:groups threadsPerThreadgroup:threads];[compute endEncoding];return true;
   };
   if(packedRender) {
    auto clear=[MTLRenderPassDescriptor renderPassDescriptor];
    clear.colorAttachments[0].texture=image;clear.colorAttachments[0].loadAction=MTLLoadActionClear;
    clear.colorAttachments[0].storeAction=MTLStoreActionStore;
    clear.colorAttachments[0].clearColor=MTLClearColorMake(0.125,0.375,0.625,1);
    [[command renderCommandEncoderWithDescriptor:clear] endEncoding];
    if(!encode(pipeline,0,true))return 14;
    [command commit];[command waitUntilCompleted];
    const uint32_t *clearWords=(const uint32_t *)output.contents;
    if(command.status!=MTLCommandBufferStatusCompleted || clearWords[0]!=32 || clearWords[1]!=96 ||
       clearWords[2]!=160 || clearWords[3]!=0xdeadbeef)return 21;
    printf("Native packed RT clear PASS gpu=%u/%u/%u\n",clearWords[0],clearWords[1],clearWords[2]);
    command=[queue commandBuffer];
   }
   if(!encode(writer,0,false))return 14;
   for(unsigned dispatch=1;dispatch+1<dispatchCount;dispatch++)if(!encode(pipeline,0,true))return 14;
   if(!encode(writer,1,false))return 14;
   id<CAMetalDrawable> drawable=[layer nextDrawable];auto present=[MTLRenderPassDescriptor renderPassDescriptor];
   present.colorAttachments[0].texture=drawable.texture;present.colorAttachments[0].loadAction=MTLLoadActionClear;present.colorAttachments[0].storeAction=MTLStoreActionStore;
   [[command renderCommandEncoderWithDescriptor:present] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
   const uint32_t *result=(const uint32_t *)output.contents;
   if(command.status!=MTLCommandBufferStatusCompleted||result[0]!=(short2D?32768+height-1:packedVolume?2*layers:volume?64+layers-1:(r11Array||halfArray)?32*layers:r8Array?64+8*(layers-1):r16Mips?64+8*((mipView?viewCount:r16Mips)-1):64)||result[1]!=((r11Array||halfArray)?128:(array||integer||unorm8||r16Mips)?0:128)||result[2]!=((r11Array||halfArray)?192:(array||integer||unorm8||r16Mips)?0:192)||result[3]!=0xdeadbeef)return 11;
   if(api&&!api->EndFrameCapture(nullptr,nullptr))return 12;
   printf("Native frame family PASS capture=%u gpu=%u/%u/%u id=%llu\n",capture,result[0],result[1],result[2],(unsigned long long)capturedIDs[capture]);
   if(annotation(table,"metal.descriptorSlotEvent",0,capture+1,1,5))return 13;
   memset(table.contents,0,24);sampled=nil;image=nil;rwImage=nil;
  }
  printf("FRAME_IDS=%llu,%llu captures=%u\n",(unsigned long long)capturedIDs[0],(unsigned long long)capturedIDs[1],api?2:1);
 }
 return 0;
}
