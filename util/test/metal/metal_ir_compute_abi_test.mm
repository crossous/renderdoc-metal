// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#include "renderdoc/driver/metal/metal_ir_compute_abi.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv)
{
  @autoreleasepool
  {
    const char *base = "{\"Origin\":\"MetalIRRuntimeBindings\",\"ShaderType\":\"Compute\","
        "\"RootBindPoint\":2,\"ResourceHeapBindPoint\":0,\"SamplerHeapBindPoint\":1,"
        "\"StaticSamplerCount\":6,\"TopLevelArgumentBuffer\":["
        "{\"EltOffset\":0,\"Size\":8,\"Slot\":0,\"Space\":0,\"Type\":\"CBV\"},"
        "{\"EltOffset\":8,\"Size\":8,\"Type\":\"Table\"}],\"state\":{\"tg_size\":[32,1,1]}}";
    MetalIRComputeRuntimeABI abi;
    assert(ParseMetalIRComputeRuntimeABI(base, strlen(base), abi) ==
           MetalIRComputeABIResult::RuntimeBindings);
    assert(abi.rootBindPoint == 2 && abi.resourceBindPoint == 0 && abi.samplerBindPoint == 1 &&
           abi.cbvCount == 1 && abi.rootBytes == 16 && abi.staticSamplerCount == 6 && abi.threads[0] == 32);
    NSDictionary *source = [NSJSONSerialization JSONObjectWithData:
        [NSData dataWithBytes:base length:strlen(base)] options:NSJSONReadingMutableContainers error:NULL];
    auto check = [&](NSDictionary *payload, MetalIRComputeABIResult expected) {
      NSData *data = [NSJSONSerialization dataWithJSONObject:payload options:0 error:NULL];
      assert(data);
      abi.rootBytes = 999;
      assert(ParseMetalIRComputeRuntimeABI((const char *)data.bytes, data.length, abi) == expected);
      if(expected != MetalIRComputeABIResult::RuntimeBindings) assert(abi.rootBytes == 0);
    };
    auto fresh = [&]() -> NSMutableDictionary * {
      NSData *data = [NSJSONSerialization dataWithJSONObject:source options:0 error:NULL];
      return [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:NULL];
    };
    for(NSString *field in @[@"RootBindPoint", @"ResourceHeapBindPoint", @"SamplerHeapBindPoint",
                             @"StaticSamplerCount"])
    {
      for(id value in @[@YES, @(-1), @4097, @2.5, @"2", [NSNull null]])
      { auto p=fresh();p[field]=value;check(p,MetalIRComputeABIResult::Invalid); }
      auto p=fresh();[p removeObjectForKey:field];check(p,MetalIRComputeABIResult::Invalid);
    }
    for(unsigned scenario=0;scenario<14;scenario++)
    {
      auto p=fresh();NSMutableArray *roots=p[@"TopLevelArgumentBuffer"];
      switch(scenario)
      {
        case 0:p[@"RootBindPoint"]=@0;break;
        case 1:p[@"UsesRayQuery"]=@YES;break;
        case 2:p[@"UsedResources"]=@[];break;
        case 3:roots[0][@"Size"]=@4;break;
        case 4:roots[1][@"EltOffset"]=@0;break;
        case 5:roots[0][@"Slot"]=@1;break;
        case 6:roots[0][@"Space"]=@1;break;
        case 7:roots[0][@"Type"]=@"Table";break;
        case 8:[roots removeLastObject];break;
        case 9:p[@"state"][@"tg_size"]=@[@0,@1,@1];break;
        case 10:p[@"state"][@"tg_size"]=@[@1024,@2,@1];break;
        case 11:p[@"state"][@"tg_size"]=@[@32,@1];break;
        case 12:p[@"ShaderType"]=@"Fragment";break;
        case 13:p[@"StaticSamplerCount"]=@0;break;
      }
      check(p,MetalIRComputeABIResult::Invalid);
    }
    auto other=fresh();[other removeObjectForKey:@"Origin"];
    check(other,MetalIRComputeABIResult::OtherMetadata);
    assert(ParseMetalIRComputeRuntimeABI("[]",2,abi)==MetalIRComputeABIResult::Invalid);
    assert(ParseMetalIRComputeRuntimeABI("{",1,abi)==MetalIRComputeABIResult::Invalid);
    unsigned runtime=0,compiler=0;
    for(int i=1;i<argc;i++)
    {
      NSData *data=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[i]]];assert(data);
      auto result=ParseMetalIRComputeRuntimeABI((const char *)data.bytes,data.length,abi);
      assert(result!=MetalIRComputeABIResult::Invalid);
      if(result==MetalIRComputeABIResult::RuntimeBindings)runtime++;else compiler++;
    }
    printf("PASS runtime ABI types/ranges/roles/unknown semantics; %u actual runtime and %u compiler payloads\n",runtime,compiler);
  }
}
