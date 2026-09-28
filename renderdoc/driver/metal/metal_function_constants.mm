// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_function_constants.h"
#import <Metal/Metal.h>
#import <objc/runtime.h>
#import <dispatch/dispatch.h>

@interface RDCMetalConstantState : NSObject
{
@public
  MetalFunctionSnapshot snapshot;
}
@end
@implementation RDCMetalConstantState
@end

static char stateKey;
static IMP originalInit, originalPublicInit, originalCopy, originalIndex, originalRange, originalName, originalReset;
static thread_local unsigned setterDepth;
static bool hooksInstalled;
static Class trackedClass;

static RDCMetalConstantState *State(id object, bool create)
{
  RDCMetalConstantState *state = (RDCMetalConstantState *)objc_getAssociatedObject(object, &stateKey);
  if(!state && create && object)
  {
    state = [[RDCMetalConstantState alloc] init];
    // Objects whose initialization was not observed may already contain unknown values.
    state->snapshot.supported = false;
    objc_setAssociatedObject(object, &stateKey, state, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [state release];
  }
  return state;
}

static void RecordConstant(id object, const void *value, MTLDataType type, NSUInteger index,
                           NSString *name)
{
  RDCMetalConstantState *state = State(object, true);
  const uint32_t size = MetalFunctionConstantSize(type);
  if(!state) return;
  if(!size || !value || index >= 65536 || (name && name.length == 0) ||
     state->snapshot.constantValues.size() >= 4096)
  {
    state->snapshot.supported = false;
    return;
  }
  state->snapshot.constantNames.push_back(name ? name.UTF8String : "");
  state->snapshot.constantIndices.push_back(index);
  state->snapshot.constantTypes.push_back(type);
  state->snapshot.constantValues.push_back(bytebuf((const byte *)value, size));
}

static id ConstantInit(id object, SEL selector)
{
  id result = ((id (*)(id, SEL))originalInit)(object, selector);
  if(!result) return nil;
  @synchronized(result)
  {
    State(result, true)->snapshot = MetalFunctionSnapshot();
  }
  return result;
}

static id PublicConstantInit(id object, SEL selector)
{
  id result = ((id (*)(id, SEL))originalPublicInit)(object, selector);
  if(result && !State(result, false)) State(result, true)->snapshot = MetalFunctionSnapshot();
  return result;
}

static id ConstantCopy(id object, SEL selector, NSZone *zone)
{
  @synchronized(object)
  {
    id result = ((id (*)(id, SEL, NSZone *))originalCopy)(object, selector, zone);
    RDCMetalConstantState *source = State(object, false);
    RDCMetalConstantState *destination = State(result, true);
    if(destination)
    {
      destination->snapshot = source ? source->snapshot : MetalFunctionSnapshot();
      if(!source) destination->snapshot.supported = false;
    }
    return result;
  }
}

static void ConstantIndex(id object, SEL selector, const void *value, MTLDataType type,
                          NSUInteger index)
{
  @synchronized(object)
  {
    const bool outer = setterDepth++ == 0;
    ((void (*)(id, SEL, const void *, MTLDataType, NSUInteger))originalIndex)(object, selector, value, type, index);
    --setterDepth;
    if(outer) RecordConstant(object, value, type, index, nil);
  }
}

static void ConstantRange(id object, SEL selector, const void *values, MTLDataType type,
                          NSRange range)
{
  @synchronized(object)
  {
    const bool outer = setterDepth++ == 0;
    ((void (*)(id, SEL, const void *, MTLDataType, NSRange))originalRange)(object, selector, values, type, range);
    --setterDepth;
    if(outer)
    {
      const uint32_t size = MetalFunctionConstantSize(type);
      if(!size || !values || range.location > 65536 || range.length > 65536 - range.location ||
         range.length > 4096)
        State(object, true)->snapshot.supported = false;
      else
        for(NSUInteger i = 0; i < range.length; i++)
          RecordConstant(object, (const byte *)values + i * size, type, range.location + i, nil);
    }
  }
}

static void ConstantName(id object, SEL selector, const void *value, MTLDataType type, NSString *name)
{
  @synchronized(object)
  {
    const bool outer = setterDepth++ == 0;
    ((void (*)(id, SEL, const void *, MTLDataType, NSString *))originalName)(object, selector, value, type, name);
    --setterDepth;
    if(outer) RecordConstant(object, value, type, 0, name);
  }
}

static void ConstantReset(id object, SEL selector)
{
  @synchronized(object)
  {
    ((void (*)(id, SEL))originalReset)(object, selector);
    State(object, true)->snapshot = MetalFunctionSnapshot();
  }
}

static bool Hook(Class cls, SEL selector, IMP replacement, IMP &original)
{
  Method method = class_getInstanceMethod(cls, selector);
  if(!method) return false;
  original = method_getImplementation(method);
  // Add an override for inherited methods; never replace NSObject's implementation globally.
  if(!class_addMethod(cls, selector, replacement, method_getTypeEncoding(method)))
    class_replaceMethod(cls, selector, replacement, method_getTypeEncoding(method));
  return true;
}

void RegisterMetalFunctionConstantHooks()
{
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    // This public class is a class cluster. Discover the concrete class from a public allocation;
    // do not hardcode a private class name or inspect its storage layout.
    id probe = [[MTLFunctionConstantValues alloc] init];
    Class cls = object_getClass(probe);
    trackedClass = cls;
    [probe release];
    hooksInstalled = Hook(cls, @selector(init), (IMP)ConstantInit, originalInit) &&
        Hook(cls, @selector(copyWithZone:), (IMP)ConstantCopy, originalCopy) &&
        Hook(cls, @selector(setConstantValue:type:atIndex:), (IMP)ConstantIndex, originalIndex) &&
        Hook(cls, @selector(setConstantValues:type:withRange:), (IMP)ConstantRange, originalRange) &&
        Hook(cls, @selector(setConstantValue:type:withName:), (IMP)ConstantName, originalName) &&
        Hook(cls, @selector(reset), (IMP)ConstantReset, originalReset);
    if(cls != [MTLFunctionConstantValues class])
      hooksInstalled &= Hook([MTLFunctionConstantValues class], @selector(init),
                             (IMP)PublicConstantInit, originalPublicInit);
  });
}

MetalFunctionSnapshot CaptureMetalFunctionSnapshot(NS::String *name,
                                                   MTL::FunctionConstantValues *values,
                                                   MTL::FunctionDescriptor *descriptor)
{
  MetalFunctionSnapshot result;
  if(descriptor) values = descriptor->constantValues();
  @synchronized((id)values)
  {
    RDCMetalConstantState *state = State((id)values, false);
    if(state) result = state->snapshot;
    else if(values) result.supported = false;
  }
  if(values && (!hooksInstalled || object_getClass((id)values) != trackedClass)) result.supported = false;
  if(descriptor)
  {
    name = descriptor->name();
    if(descriptor->specializedName()) result.specializedName = descriptor->specializedName()->utf8String();
    result.options = descriptor->options();
    if(result.options != 0 || descriptor->binaryArchives()->count() != 0)
      result.supported = false;
  }
  if(name) result.name = name->utf8String();
  return result;
}
