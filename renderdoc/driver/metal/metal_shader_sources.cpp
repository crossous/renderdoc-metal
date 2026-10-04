// SPDX-License-Identifier: MIT
#include "metal_replay.h"
#include <dlfcn.h>

// HSRD/HSRC -> SARC layout follows YuAo/MetalLibraryArchive's documented container.
// Read archives in memory only: debug filenames never become filesystem paths.
rdcarray<ShaderSourceFile> ExtractMetalDebugSources(const bytebuf &binary)
{
  rdcarray<ShaderSourceFile> files;
  if(binary.size() < 88 || memcmp(binary.data(), "MTLB", 4)) return files;
  auto number = [&](size_t pos, size_t bytes, uint64_t &value) {
    if(pos > binary.size() || bytes > binary.size() - pos) return false;
    value = 0; for(size_t i = 0; i < bytes; i++) value |= uint64_t(binary[pos+i]) << (8*i);
    return true;
  };
  uint64_t functionOffset, functionSize, publicOffset;
  if(!number(24,8,functionOffset) || !number(32,8,functionSize) ||
     !number(40,8,publicOffset) || functionOffset > binary.size() ||
     functionSize > binary.size() - functionOffset || publicOffset > binary.size()) return files;
  size_t ext = size_t(functionOffset+functionSize);
  if(ext > binary.size()-4) return files;
  ext += 4;
  uint64_t offset = 0, size = 0;
  bool workingDirectory = false;
  while(ext+6 <= publicOffset)
  {
    if(!memcmp(binary.data()+ext,"ENDT",4)) break;
    uint64_t length; if(!number(ext+4,2,length) || length > publicOffset-ext-6) return files;
    if(length == 16 && (!memcmp(binary.data()+ext,"HSRD",4) || !memcmp(binary.data()+ext,"HSRC",4)))
    {
      workingDirectory = !memcmp(binary.data()+ext,"HSRD",4);
      number(ext+6,8,offset); number(ext+14,8,size); break;
    }
    ext += size_t(6+length);
  }
  if(!offset || offset > binary.size() || size > binary.size()-offset || size < 4) return files;
  const size_t limit = size_t(offset+size);
  size_t pos = size_t(offset);
  auto string = [&](size_t &cursor, size_t end, rdcstr &out) {
    size_t begin = cursor;
    while(cursor < end && binary[cursor]) cursor++;
    if(cursor == end || cursor-begin > 65536) return false;
    out.assign((const char *)binary.data()+begin,cursor-begin); cursor++; return true;
  };
  uint64_t count; if(!number(pos,4,count) || count > 64) return files;
  pos += 4; rdcstr unused;
  if(!string(pos,limit,unused) || (workingDirectory && !string(pos,limit,unused))) return files;
  using Decompress = int (*)(char *, unsigned *, char *, unsigned, int, int);
  void *module = dlopen("/usr/lib/libbz2.dylib", RTLD_LAZY | RTLD_LOCAL);
  if(!module) return files;
  auto decompress = (Decompress)dlsym(module,"BZ2_bzBuffToBuffDecompress");
  if(!decompress) { dlclose(module); return files; }
  uint64_t totalText = 0;
  for(size_t archive=0;archive<count;archive++)
  {
    uint64_t groupSize;
    if(pos > limit-4 || !number(pos,4,groupSize)) break;
    pos += 4;
    if(groupSize > limit-pos || groupSize < 12) break;
    size_t groupEnd = pos+size_t(groupSize);
    while(pos+8 <= groupEnd && memcmp(binary.data()+pos,"ENDT",4))
    {
      const bool source = !memcmp(binary.data()+pos,"SARC",4);
      uint64_t tagSize; if(!number(pos+4,4,tagSize) || tagSize > groupEnd-pos-8) break;
      size_t content = pos+8, tagEnd = content+size_t(tagSize); pos = tagEnd;
      if(!source || !string(content,tagEnd,unused) || tagEnd-content > 8*1024*1024) continue;
      unsigned outputSize = 8*1024*1024;
      bytebuf output; output.resize(outputSize);
      if(decompress((char *)output.data(),&outputSize,(char *)binary.data()+content,
                    unsigned(tagEnd-content),0,0) != 0) continue;
      // Source archives are ustar. Only copy bounded regular-file text; no links or extraction.
      for(size_t tar=0;tar+512 <= outputSize;)
      {
        const byte *header = output.data()+tar;
        if(!header[0]) break;
        uint64_t length = 0; bool valid = true;
        for(unsigned i=124;i<136;i++)
        {
          byte ch=header[i]; if(ch==0 || ch==' ') break;
          if(ch<'0'||ch>'7'||length > 8*1024*1024/8) { valid=false; break; }
          length=length*8+(ch-'0');
        }
        if(!valid || length > outputSize-tar-512) break;
        rdcstr name((const char *)header,strnlen((const char *)header,100));
        if(header[345]) name=rdcstr((const char *)header+345,strnlen((const char *)header+345,155))+"/"+name;
        const bool shader=name.endsWith(".metal") || name.endsWith(".h") || name.endsWith(".hpp");
        if(shader && (header[156]==0 || header[156]=='0') && length && files.size()<256 &&
           length <= 8*1024*1024-totalText && !memchr(header+512,0,size_t(length)))
        {
          files.push_back({name,rdcstr((const char *)header+512,size_t(length))});
          totalText += length;
        }
        tar += 512 + size_t((length+511)/512)*512;
      }
    }
    pos=groupEnd;
  }
  dlclose(module);
  return files;
}
