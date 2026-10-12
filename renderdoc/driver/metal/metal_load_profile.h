// SPDX-License-Identifier: MIT
#pragma once
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

// Optional wall-time profiling only. It does not add GPU waits/readbacks, change
// resource checks, or memoize any restoration decision. Exclusive times remove
// nested work on the same thread; parallel work must not be added to this tree.
namespace MetalLoadProfile
{
using Clock=std::chrono::steady_clock;
struct Scope;
struct Record { double inclusive=0,exclusive=0; size_t calls=0; };
struct Context { Scope *current=nullptr;std::map<std::string,Record> records; };
inline Context &GetContext() { static thread_local Context context;return context; }
inline bool Enabled()
{
  static const bool enabled=std::getenv("RENDERDOC_METAL_PROFILE_LOAD")!=nullptr;
  return enabled;
}
inline void Count(const char *name, size_t count=1)
{
  if(Enabled())GetContext().records[name].calls+=count;
}
struct Scope
{
  const char *name;bool enabled,report;Scope *parent=nullptr;
  double children=0;Clock::time_point start;
  Scope(const char *phase,bool print=false):name(phase),enabled(Enabled()),report(print)
  {
    if(!enabled)return;
    auto &context=GetContext();parent=context.current;
    if(report && !parent)context.records.clear();
    context.current=this;start=Clock::now();
  }
  ~Scope()
  {
    if(!enabled)return;
    const double seconds=std::chrono::duration<double>(Clock::now()-start).count();
    auto &context=GetContext();context.current=parent;
    auto &record=context.records[name];record.inclusive+=seconds;
    record.exclusive+=seconds-children;record.calls++;
    if(parent)parent->children+=seconds;
    if(report)
      for(const auto &r:context.records)
        std::fprintf(stderr,"MetalLoadProfile {\"phase\":\"%s\",\"inclusive_seconds\":%.9f,\"exclusive_seconds\":%.9f,\"calls\":%zu}\n",
            r.first.c_str(),r.second.inclusive,r.second.exclusive,r.second.calls);
  }
  Scope(const Scope &)=delete;Scope &operator=(const Scope &)=delete;
};
}
