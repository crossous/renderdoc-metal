// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

struct InitialTextureSpec
{
  unsigned format, type, width, height, depth, arrays, mips, bytes, block, components;
};

inline std::vector<InitialTextureSpec> InitialTextureSpecs(bool shared=false)
{
  // Expected sizes are independent of the driver's format/footprint helpers.
  const unsigned formats[][4] = {
      {70,4,1,4}, {80,4,1,4}, {81,4,1,4}, {73,4,1,4}, {10,1,1,1}, {11,1,1,1}, {13,1,1,1},
      {20,2,1,1}, {23,2,1,1}, {25,2,1,1}, {30,2,1,2}, {53,4,1,1}, {55,4,1,1}, {60,4,1,2},
      {63,4,1,2}, {65,4,1,2}, {90,4,1,4}, {92,4,1,3}, {103,8,1,2}, {105,8,1,2},
      {110,8,1,4}, {115,8,1,4}, {123,16,1,4}, {125,16,1,4},
      {130,8,4,4}, {131,8,4,4}, {134,16,4,4}, {135,16,4,4}, {142,16,4,2},
      {151,16,4,3}, {152,16,4,4}, {153,16,4,4}};
  std::vector<InitialTextureSpec> specs;
  for(const auto &f:formats)
    for(unsigned type:{2u,3u,5u,6u,7u})
    {
      if(shared&&f[0]!=70&&f[0]!=80&&f[0]!=53&&f[0]!=115)continue;
      if(f[2]>1 && type==7)continue;
      const bool cube=type==5||type==6;
      specs.push_back({f[0],type,3,cube?3u:5u,type==7?2u:1u,
                       type==3||type==6?2u:1u,cube?2u:3u,f[1],f[2],f[3]});
    }
  if(!shared)for(unsigned format:{250u,252u,260u})for(unsigned type:{2u,3u})
    specs.push_back({format,type,3,5,1,type==3?2u:1u,3,format==250?2u:format==260?8u:4u,1,1});
  return specs;
}

inline std::vector<InitialTextureSpec> BufferTextureInitialSpecs()
{
  const unsigned formats[][3]={{53,4,1},{123,16,4},{125,16,4},{70,4,4},{55,4,1},{72,4,4},
    {105,8,2},{54,4,1},{25,2,1},{65,4,2},{103,8,2},{73,4,4},{23,2,1},{115,8,4},{63,4,2},{10,1,1}};
  std::vector<InitialTextureSpec> result;
  for(const auto &f:formats)result.push_back({f[0],9,7,1,1,1,1,f[1],1,f[2]});
  return result;
}

inline unsigned InitialTextureSlices(const InitialTextureSpec &s)
{ return (s.type==5||s.type==6)?6*s.arrays:s.arrays; }

inline uint8_t InitialTextureByte(const InitialTextureSpec &s,unsigned mip,unsigned slice,
                                  unsigned z,unsigned y,unsigned xbyte)
{
  if(s.block>1)return uint8_t(17+19*mip+11*slice+7*y+3*xbyte);
  unsigned x=xbyte/s.bytes,c=xbyte%s.bytes;
  if(s.format==70||s.format==80||s.format==81)
  {
    if(c==3)return 255;
    return uint8_t(17+19*mip+11*slice+13*z+7*y+3*x+23*c);
  }
  uint8_t pixel[16]={};
  if(s.format==250||s.format==252||s.format==260)
  {
    const float value=0.25f+0.125f*mip+0.0625f*slice;
    if(s.format==250){uint16_t normalized=uint16_t(value*65535.0f);memcpy(pixel,&normalized,2);}
    else memcpy(pixel,&value,4);
    if(s.format==260)pixel[4]=uint8_t(17+mip+slice);
    return pixel[c];
  }
  for(unsigned component=0;component<s.components;component++)
  {
    const float scalar=0.25f+mip*0.125f+slice*0.03125f+z*0.0625f+y*0.015625f+x*0.0078125f+component*0.125f;
    if(s.format==25||s.format==65||s.format==115)
    { _Float16 value=(_Float16)scalar;memcpy(pixel+2*component,&value,2); }
    else if(s.format==55||s.format==105||s.format==125)
    { memcpy(pixel+4*component,&scalar,4); }
    else
    {
      const unsigned width=s.bytes/s.components;
      uint32_t value=17+19*mip+11*slice+13*z+7*y+3*x+23*component;
      if(s.format==60||s.format==110)value+=0x2300;
      if(s.format==54)value=uint32_t(-int32_t(value));
      if(s.format==90||s.format==92)
      { value=0x781e03c0u+19*mip+11*slice+13*z+7*y+3*x;memcpy(pixel,&value,4);break; }
      memcpy(pixel+width*component,&value,width);
    }
  }
  return pixel[c];
}
