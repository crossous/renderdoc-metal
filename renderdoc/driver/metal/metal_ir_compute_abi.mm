// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#include "metal_ir_compute_abi.h"
#include <cstring>

MetalIRComputeABIResult ParseMetalIRComputeRuntimeABI(
    const char *text, size_t length, MetalIRComputeRuntimeABI &abi)
{
  abi = {};
  if(!text || !length || length > 65536) return MetalIRComputeABIResult::Invalid;
  @autoreleasepool
  {
    NSData *data = [NSData dataWithBytes:text length:length];
    id object = [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
    if(![object isKindOfClass:[NSDictionary class]]) return MetalIRComputeABIResult::Invalid;
    NSDictionary *payload = object;
    if(![payload[@"Origin"] isEqual:@"MetalIRRuntimeBindings"])
      return MetalIRComputeABIResult::OtherMetadata;
    auto number = [](id value, uint32_t maximum, uint32_t &out) {
      if(![value isKindOfClass:[NSNumber class]] ||
         CFGetTypeID((__bridge CFTypeRef)value) == CFBooleanGetTypeID()) return false;
      const char *type = [value objCType];
      // JSON booleans and floating values never become ABI integer facts.
      if(!type || !strchr("csilqCSILQ", type[0]) || type[1]) return false;
      const long long n = [value longLongValue];
      if(n < 0 || uint64_t(n) > maximum) return false;
      out = uint32_t(n); return true;
    };
    MetalIRComputeRuntimeABI parsed;
    if(![payload[@"ShaderType"] isEqual:@"Compute"] || payload[@"UsesRayQuery"] ||
       payload[@"UsedResources"] ||
       !number(payload[@"RootBindPoint"], 30, parsed.rootBindPoint) ||
       !number(payload[@"ResourceHeapBindPoint"], 30, parsed.resourceBindPoint) ||
       !number(payload[@"SamplerHeapBindPoint"], 30, parsed.samplerBindPoint) ||
       parsed.rootBindPoint == parsed.resourceBindPoint ||
       parsed.rootBindPoint == parsed.samplerBindPoint ||
       parsed.resourceBindPoint == parsed.samplerBindPoint ||
       !number(payload[@"StaticSamplerCount"], 4096, parsed.staticSamplerCount) ||
       !parsed.staticSamplerCount) return MetalIRComputeABIResult::Invalid;
    id roots = payload[@"TopLevelArgumentBuffer"], state = payload[@"state"];
    if(![roots isKindOfClass:[NSArray class]] || ![roots count] || [roots count] > 33 ||
       ![state isKindOfClass:[NSDictionary class]]) return MetalIRComputeABIResult::Invalid;
    parsed.cbvCount = uint32_t([roots count] - 1);
    for(uint32_t i = 0; i <= parsed.cbvCount; i++)
    {
      id root = roots[i]; uint32_t offset = 0, size = 0, slot = 0, space = 0;
      if(![root isKindOfClass:[NSDictionary class]] ||
         !number(root[@"EltOffset"], 256, offset) || offset != i * 8 ||
         !number(root[@"Size"], 8, size) || size != 8)
        return MetalIRComputeABIResult::Invalid;
      if(i < parsed.cbvCount)
      {
        if(![root[@"Type"] isEqual:@"CBV"] || !number(root[@"Slot"], 31, slot) ||
           slot != i || !number(root[@"Space"], 0, space)) return MetalIRComputeABIResult::Invalid;
      }
      else if(![root[@"Type"] isEqual:@"Table"])
        return MetalIRComputeABIResult::Invalid;
    }
    id threads = state[@"tg_size"];
    if(![threads isKindOfClass:[NSArray class]] || [threads count] != 3)
      return MetalIRComputeABIResult::Invalid;
    uint64_t work = 1;
    for(uint32_t i = 0; i < 3; i++)
    {
      if(!number(threads[i], 1024, parsed.threads[i]) || !parsed.threads[i])
        return MetalIRComputeABIResult::Invalid;
      work *= parsed.threads[i];
    }
    if(work > 1024) return MetalIRComputeABIResult::Invalid;
    parsed.rootBytes = (parsed.cbvCount + 1) * 8;
    abi = parsed;
    return MetalIRComputeABIResult::RuntimeBindings;
  }
}
