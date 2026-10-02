// SPDX-License-Identifier: MIT
// Compile captured pipeline inputs and inspect reflection. Creates no command queue/buffer.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
static NSArray *arguments(NSArray<MTLArgument *> *inputs)
{
  NSMutableArray *result=[NSMutableArray new];
  for(MTLArgument *a in inputs) {
    NSMutableDictionary *item=[@{@"name":a.name?:@"",@"index":@(a.index),@"type":@(a.type),
      @"access":@(a.access),@"active":@(a.active),@"arrayLength":@(a.arrayLength)} mutableCopy];
    if(a.type==MTLArgumentTypeBuffer) {
      item[@"dataType"]=@(a.bufferDataType);item[@"bytes"]=@(a.bufferDataSize);item[@"alignment"]=@(a.bufferAlignment);
      NSMutableArray *members=[NSMutableArray new];
      for(MTLStructMember *m in a.bufferStructType.members) {
        NSMutableDictionary *entry=[@{@"name":m.name?:@"",@"offset":@(m.offset),@"argumentIndex":@(m.argumentIndex),@"type":@(m.dataType)} mutableCopy];
        MTLPointerType *p=m.pointerType;
        if(p) {entry[@"pointerAccess"]=@(p.access);entry[@"pointerElementType"]=@(p.elementType);
          entry[@"pointerIsArgumentBuffer"]=@(p.elementIsArgumentBuffer);entry[@"pointerBytes"]=@(p.dataSize);}
        [members addObject:entry];
      }
      item[@"members"]=members;
    }
    [result addObject:item];
  }
  return result;
}
int main(int argc,char **argv)
{
  if(argc!=2)return 2;
  @autoreleasepool {
    NSString *folder=[NSString stringWithUTF8String:argv[1]];NSError *error=nil;
    NSDictionary *manifest=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:@"manifest.json"]] options:0 error:&error];
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    if(!device||!manifest)return 3;
    NSMutableDictionary *libraries=[NSMutableDictionary new],*functions=[NSMutableDictionary new];
    for(NSString *identity in manifest[@"libraries"]) {
      NSDictionary *spec=manifest[@"libraries"][identity];NSData *code=[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:spec[@"path"]]];
      if(!code)return 4;
      dispatch_data_t bytes=dispatch_data_create(code.bytes,code.length,dispatch_get_main_queue(),DISPATCH_DATA_DESTRUCTOR_DEFAULT);
      id<MTLLibrary> library=[device newLibraryWithData:bytes error:&error];
      if(!library){fprintf(stderr,"Library %s: %s\n",identity.UTF8String,error.localizedDescription.UTF8String);return 5;}
      libraries[identity]=library;
    }
    for(NSString *identity in manifest[@"functions"]) {
      NSDictionary *spec=manifest[@"functions"][identity];MTLFunctionDescriptor *d=[MTLFunctionDescriptor new];
      d.name=spec[@"name"];d.options=(MTLFunctionOptions)[spec[@"options"] unsignedLongLongValue];
      id<MTLFunction> function=[libraries[[spec[@"library"] stringValue]] newFunctionWithDescriptor:d error:&error];
      if(!function){fprintf(stderr,"Function %s: %s\n",identity.UTF8String,error.localizedDescription.UTF8String);return 6;}
      functions[identity]=function;
    }
    NSMutableArray *results=[NSMutableArray new];
    NSArray *keys=@[@"label",@"sampleCount",@"rasterSampleCount",@"alphaToCoverageEnabled",@"alphaToOneEnabled",@"rasterizationEnabled",@"maxVertexAmplificationCount",
      @"depthAttachmentPixelFormat",@"stencilAttachmentPixelFormat",@"inputPrimitiveTopology",@"tessellationPartitionMode",@"maxTessellationFactor",@"tessellationFactorScaleEnabled",
      @"tessellationFactorFormat",@"tessellationControlPointIndexType",@"tessellationFactorStepFunction",@"tessellationOutputWindingOrder",@"supportIndirectCommandBuffers",
      @"supportAddingVertexBinaryFunctions",@"supportAddingFragmentBinaryFunctions",@"maxVertexCallStackDepth",@"maxFragmentCallStackDepth"];
    for(NSDictionary *input in manifest[@"pipelines"]) {
      NSDictionary *spec=input[@"descriptor"];MTLRenderPipelineDescriptor *d=[MTLRenderPipelineDescriptor new];
      for(NSString *key in keys)[d setValue:spec[key] forKey:key];
      d.vertexFunction=functions[[spec[@"vertexFunction"] stringValue]];
      d.fragmentFunction=functions[[spec[@"fragmentFunction"] stringValue]];
      for(NSUInteger i=0;i<[spec[@"colorAttachments"] count];i++) {
        NSDictionary *color=spec[@"colorAttachments"][i];
        for(NSString *key in color)[d.colorAttachments[i] setValue:color[key] forKey:key];
      }
      for(NSUInteger i=0;i<[spec[@"vertexBuffers"] count];i++)d.vertexBuffers[i].mutability=(MTLMutability)[spec[@"vertexBuffers"][i][@"mutability"] unsignedLongLongValue];
      for(NSUInteger i=0;i<[spec[@"fragmentBuffers"] count];i++)d.fragmentBuffers[i].mutability=(MTLMutability)[spec[@"fragmentBuffers"][i][@"mutability"] unsignedLongLongValue];
      for(NSString *stage in @[@"vertex",@"fragment"]) {
        NSMutableArray *linked=[NSMutableArray new];
        for(NSNumber *identity in spec[[stage stringByAppendingString:@"LinkedFunctions"]][@"functions"])[linked addObject:functions[identity.stringValue]];
        MTLLinkedFunctions *value=[MTLLinkedFunctions new];value.functions=linked;
        [d setValue:value forKey:[stage stringByAppendingString:@"LinkedFunctions"]];
      }
      MTLRenderPipelineReflection *reflection=nil;
      id<MTLRenderPipelineState> pipeline=[device newRenderPipelineStateWithDescriptor:d options:(MTLPipelineOption)[input[@"options"] unsignedLongLongValue] reflection:&reflection error:&error];
      if(!pipeline||!reflection){fprintf(stderr,"Pipeline %s: %s\n",[input[@"id"] description].UTF8String,error.localizedDescription.UTF8String);return 7;}
      [results addObject:@{@"id":input[@"id"],@"label":d.label?:@"",@"vertex":arguments(reflection.vertexArguments),@"fragment":arguments(reflection.fragmentArguments)}];
      fprintf(stderr,"Native compiler PASS pipeline=%s\n",[input[@"id"] description].UTF8String);
    }
    NSDictionary *report=@{@"device":device.name,@"pipelines":results,@"mode":@"exact Native compiler/reflection; no GPU command queue, command buffer or submission"};
    NSData *json=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:&error];
    if(!json)return 8;fwrite(json.bytes,1,json.length,stdout);fputc('\n',stdout);
  }
  return 0;
}
