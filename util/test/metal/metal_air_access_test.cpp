// SPDX-License-Identifier: MIT
#include "renderdoc/driver/metal/metal_air_access.h"
#include <cassert>
#include <fstream>
#include <iostream>

int main(int argc, char **argv)
{
  using namespace MetalAIR;
  const std::string text = R"(
define void @test(%struct.root addrspace(2)* %0, %struct.heap addrspace(2)* %1, i32 %2) {
  %a = bitcast %struct.root addrspace(2)* %0 to i32 addrspace(2)*
  %i = load i32, i32 addrspace(2)* %a, align 4
  %n = mul i32 %i, 24
  %wide = zext i32 %n to i64
  %heap = bitcast %struct.heap addrspace(2)* %1 to i8 addrspace(2)*
  %entry = getelementptr inbounds i8, i8 addrspace(2)* %heap, i64 %wide
  %field = getelementptr inbounds i8, i8 addrspace(2)* %entry, i64 8
  %cast = bitcast i8 addrspace(2)* %field to %struct._texture_2d_t addrspace(1)* addrspace(2)*
  %tex = load %struct._texture_2d_t addrspace(1)*, %struct._texture_2d_t addrspace(1)* addrspace(2)* %cast, align 8
  %sample = call float @air.sample_texture_2d.f32(%struct._texture_2d_t addrspace(1)* %tex)
  %dynamic = mul i32 %2, 24
  %off = zext i32 %dynamic to i64
  %unresolved = getelementptr i8, i8 addrspace(2)* %heap, i64 %off
  %dyncast = bitcast i8 addrspace(2)* %unresolved to %struct._texture_2d_t addrspace(1)* addrspace(2)*
  %dyntex = load %struct._texture_2d_t addrspace(1)*, %struct._texture_2d_t addrspace(1)* addrspace(2)* %dyncast, align 8
  %other = call float @air.read_texture_2d.f32(%struct._texture_2d_t addrspace(1)* %dyntex)
}
!1 = !{i32 0, !"air.indirect_buffer", !"air.location_index", i32 2, i32 1}
!2 = !{i32 1, !"air.indirect_buffer", !"air.location_index", i32 0, i32 1}
)";
  Value root; root.kind = Value::Pointer; root.slot = 2;
  Value heap; heap.kind = Value::Pointer; heap.object = 24;
  unsigned index = 7;
  auto load = [&](const Value &v, unsigned size, Value::Kind kind) {
    if(v.slot == 2 && kind == Value::Integer && size == 4) return Value::Number(index);
    if(v.object == 24 && v.offset == index * 24 + 8 && kind == Value::Texture)
    { Value r = v; r.kind = kind; return r; }
    return Value();
  };
  for(unsigned n : {7U, 13U})
  {
    index = n;
    auto reads = UniformTextureAccess(text, "test", {{0, heap}, {2, root}}, load);
    assert(reads.size() == 1 && reads[0].offset == n * 24 + 8);
    assert(!reads[0].write);
  }
  std::string writes = text;
  const auto call = writes.find("  %sample = call");
  writes.insert(call, "  call void @air.write_texture_2d.rtz.v4f32(%struct._texture_2d_t addrspace(1)* %tex)\n");
  auto accesses = UniformTextureAccess(writes, "test", {{0, heap}, {2, root}}, load);
  assert(accesses.size() == 2 && accesses[0].write && !accesses[1].write &&
         accesses[0].offset == accesses[1].offset);
  assert(accesses[0].numeric=='f' && accesses[1].numeric=='f');
  std::string integerWrites=writes;
  integerWrites.replace(integerWrites.find("@air.write_texture_2d.rtz.v4f32"),
      std::string("@air.write_texture_2d.rtz.v4f32").size(),"@air.write_texture_2d.u.v4i32");
  auto integerAccess=UniformTextureAccess(integerWrites,"test",{{0,heap},{2,root}},load);
  assert(integerAccess.size()==2 && integerAccess[0].write && integerAccess[0].numeric=='u' &&
         integerAccess[1].numeric=='f');
  std::string atomic = writes;
  atomic.replace(atomic.find("@air.write_texture_2d.rtz.v4f32"),
                 std::string("@air.write_texture_2d.rtz.v4f32").size(), "@air.atomic_fetch_add_explicit_2d");
  accesses = UniformTextureAccess(atomic, "test", {{0, heap}, {2, root}}, load);
  assert(accesses.size() == 2 && accesses[0].write && !accesses[1].write);
  assert(UniformTextureAccess(writes, "test", {{0, heap}}, load).empty());
  assert(UniformTextureAccess(text, "missing", {{0, heap}, {2, root}}, load).empty());
  assert(UniformTextureAccess(text, "test", {{0, heap}}, load).empty());
  // An unrelated same-module helper does not invalidate a Native entry.
  const auto withHelper=UniformTextureAccess(text + "\ndefine void @helper() {\n}\n", "test", {{0, heap}, {2, root}}, load);
  assert(withHelper.size()==1 && withHelper[0].offset==index*24+8);
  auto partial = UniformResourceAccess(text,"test",{{0,heap},{2,root}},load);
  assert(partial.validModule && partial.textureCalls==2 && partial.unresolvedTextures==1 && partial.accesses.size()==1);
  assert(!UniformResourceAccess(text,"missing",{{0,heap},{2,root}},load).validModule);
  // Actual IR converter headers use a literal two-pointer struct, rather than
  // a named LLVM type. Resolve only a typed loader-provided AS reference.
  const std::string query = R"(
define void @query(%struct.root addrspace(2)* %0, %struct.heap addrspace(2)* %1) {
  %a = bitcast %struct.root addrspace(2)* %0 to i32 addrspace(2)*
  %i = load i32, i32 addrspace(2)* %a, align 4
  %n = mul i32 %i, 24
  %wide = zext i32 %n to i64
  %heap = bitcast %struct.heap addrspace(2)* %1 to i8 addrspace(2)*
  %entry = getelementptr inbounds i8, i8 addrspace(2)* %heap, i64 %wide
  %headerCast = bitcast i8 addrspace(2)* %entry to { %struct._instance_acceleration_structure_t addrspace(1)*, i32 addrspace(2)* } addrspace(1)* addrspace(2)*
  %header = load { %struct._instance_acceleration_structure_t addrspace(1)*, i32 addrspace(2)* } addrspace(1)*, { %struct._instance_acceleration_structure_t addrspace(1)*, i32 addrspace(2)* } addrspace(1)* addrspace(2)* %headerCast, align 8
  %field = getelementptr { %struct._instance_acceleration_structure_t addrspace(1)*, i32 addrspace(2)* }, { %struct._instance_acceleration_structure_t addrspace(1)*, i32 addrspace(2)* } addrspace(1)* %header, i64 0, i32 0
  %as = load %struct._instance_acceleration_structure_t addrspace(1)*, %struct._instance_acceleration_structure_t addrspace(1)* addrspace(1)* %field, align 8
  call void @air.reset_intersection_query.instancing.triangle_data(%struct._intersection_query_t* %query, <3 x float> %origin, <3 x float> %direction, float 0.0, float 1.0, %struct._instance_acceleration_structure_t addrspace(1)* %as)
}
!1 = !{i32 0, !"air.indirect_buffer", !"air.location_index", i32 2, i32 1}
!2 = !{i32 1, !"air.indirect_buffer", !"air.location_index", i32 0, i32 1}
)";
  bool headerKnown=true,structureKnown=true;
  auto queryLoad=[&](const Value &v,unsigned bytes,Value::Kind kind) {
    if(bytes==4 && v.slot==2 && kind==Value::Integer)return Value::Number(index);
    if(bytes==8 && v.object==24 && v.offset==index*24 && kind==Value::Pointer && headerKnown)
    {Value r;r.kind=kind;r.object=100;r.offset=32;r.descriptorObject=24;r.descriptorOffset=index*24;return r;}
    if(bytes==8 && v.object==100 && v.offset==32 && kind==Value::AccelerationStructure && structureKnown)
    {Value r=v;r.kind=kind;return r;}
    return Value();
  };
  for(unsigned n:{7U,13U})
  {
    index=n;auto report=UniformResourceAccess(query,"query",{{0,heap},{2,root}},queryLoad);
    assert(report.validModule && report.queryResets==1 && !report.unresolvedStructures && report.accesses.size()==1);
    assert(report.accesses[0].kind==Value::AccelerationStructure && report.accesses[0].object==100 &&
           report.accesses[0].offset==32 && report.accesses[0].descriptorObject==24 && report.accesses[0].descriptorOffset==n*24);
    assert(UniformTextureAccess(query,"query",{{0,heap},{2,root}},queryLoad).empty());
  }
  for(unsigned missing:{0U,1U})
  {
    headerKnown=missing!=0;structureKnown=missing!=1;
    const auto report=UniformResourceAccess(query,"query",{{0,heap},{2,root}},queryLoad);
    assert(report.queryResets==1 && report.unresolvedStructures==1 && report.accesses.empty());
  }
  headerKnown=structureKnown=true;
  std::string wrongField=query;
  wrongField.replace(wrongField.find("%header, i64 0, i32 0"),std::string("%header, i64 0, i32 0").size(),"%header, i64 0, i32 1");
  auto unknown=UniformResourceAccess(wrongField,"query",{{0,heap},{2,root}},queryLoad);
  assert(unknown.queryResets==1 && unknown.unresolvedStructures==1 && unknown.accesses.empty());
  std::string unknownCall=query;
  unknownCall.replace(unknownCall.find("%struct._instance_acceleration_structure_t addrspace(1)* %as)"),
      std::string("%struct._instance_acceleration_structure_t addrspace(1)* %as)").size(),"i64 %rawAS)");
  unknown=UniformResourceAccess(unknownCall,"query",{{0,heap},{2,root}},queryLoad);
  assert(unknown.queryResets==1 && unknown.unresolvedStructures==1);
  // Each compiled Metal function has an independent module and metadata namespace.
  const std::string moduleA = "source_filename = \"one\"\n" + text;
  std::string other = text;
  other.replace(other.find("@test("), 6, "@mesh(");
  const std::string moduleB = "source_filename = \"two\"\n" + other;
  auto isolated = UniformTextureAccess(moduleA + moduleB, "mesh", {{0, heap}, {2, root}}, load);
  assert(isolated.size() == 1 && isolated[0].offset == index * 24 + 8);
  const std::string rawRoot = R"(
source_filename = "raw"
%struct.Entry = type { i64, %"struct.metal::texture2d", i64 }
%"struct.metal::texture2d" = type { %struct._texture_2d_t addrspace(1)* }
define void @raw(i64 addrspace(1)* %0) {
  %address = load i64, i64 addrspace(1)* %0, align 8
  %table = inttoptr i64 %address to %struct.Entry addrspace(1)*
  %field = getelementptr inbounds %struct.Entry, %struct.Entry addrspace(1)* %table, i64 0, i32 1, i32 0
  %image = load %struct._texture_2d_t addrspace(1)*, %struct._texture_2d_t addrspace(1)* addrspace(1)* %field, align 8
  call void @air.write_texture_2d.v4f32(%struct._texture_2d_t addrspace(1)* %image)
}
!1 = !{i32 0, !"air.buffer", !"air.location_index", i32 0, i32 1}
)";
  bool typed = true;
  auto rawLoad = [&](const Value &v, unsigned bytes, Value::Kind kind) {
    if(bytes == 8 && v.slot == 2)
    {
      if(typed && kind == Value::Pointer) return heap;
      if(kind == Value::Integer) return Value::Number(24);
    }
    if(kind == Value::Texture && v.object == 24 && v.offset == 8)
    { Value r=v; r.kind=kind; return r; }
    return Value();
  };
  auto rawAccess = UniformTextureAccess(rawRoot, "raw", {{0, root}}, rawLoad);
  assert(rawAccess.size() == 1 && rawAccess[0].write && rawAccess[0].offset == 8);
  typed = false;
  assert(UniformTextureAccess(rawRoot, "raw", {{0, root}}, rawLoad).empty());
  typed = true;
  const std::string sampled=R"(
source_filename = "sampling"
%struct.SamplerEntry = type { %struct._sampler_t addrspace(2)*, i64, i64 }
define void @sampling(i8 addrspace(2)* %0, i8 addrspace(2)* %1, i32 %2) {
  %root = getelementptr i8, i8 addrspace(2)* %0, i64 40
  %ptr = bitcast i8 addrspace(2)* %root to %struct.SamplerEntry addrspace(2)* addrspace(2)*
  %table = load %struct.SamplerEntry addrspace(2)*, %struct.SamplerEntry addrspace(2)* addrspace(2)* %ptr, align 8
  %field = getelementptr %struct.SamplerEntry, %struct.SamplerEntry addrspace(2)* %table, i64 5, i32 0
  %sampler = load %struct._sampler_t addrspace(2)*, %struct._sampler_t addrspace(2)* addrspace(2)* %field, align 8
  %imageField = getelementptr i8, i8 addrspace(2)* %1, i64 8
  %imagePtr = bitcast i8 addrspace(2)* %imageField to %struct._texture_2d_t addrspace(1)* addrspace(2)*
  %image = load %struct._texture_2d_t addrspace(1)*, %struct._texture_2d_t addrspace(1)* addrspace(2)* %imagePtr, align 8
  %sampled = call float @air.sample_texture_2d.f32(%struct._texture_2d_t addrspace(1)* %image, %struct._sampler_t addrspace(2)* %sampler)
}
!1 = !{i32 0, !"air.indirect_buffer", !"air.location_index", i32 2, i32 1}
!2 = !{i32 1, !"air.indirect_buffer", !"air.location_index", i32 0, i32 1}
)";
  bool samplerSource=true;
  auto samplerLoad=[&](const Value &address,unsigned bytes,Value::Kind kind) {
    if(bytes!=8)return Value();
    if(kind==Value::Pointer && address.slot==2 && address.offset==40)
    {Value v;v.kind=kind;v.object=48;v.offset=16;return v;}
    if(kind==Value::Sampler && samplerSource && address.object==48 && address.offset>=16 &&
       (address.offset-16)%24==0 && (address.offset-16)/24<6)
    {Value v=address;v.kind=kind;return v;}
    if(kind==Value::Texture && address.object==24 && address.offset==8)
    {Value v=address;v.kind=kind;return v;}
    return Value();
  };
  auto samplerReport=UniformResourceAccess(sampled,"sampling",{{0,heap},{2,root}},samplerLoad);
  assert(samplerReport.validModule && samplerReport.textureCalls==1 && samplerReport.samplerCalls==1 &&
         !samplerReport.unresolvedTextures && !samplerReport.unresolvedSamplers && samplerReport.accesses.size()==2 &&
         samplerReport.accesses[1].kind==Value::Sampler && samplerReport.accesses[1].object==48 &&
         samplerReport.accesses[1].offset==136);
  samplerSource=false;
  assert(UniformResourceAccess(sampled,"sampling",{{0,heap},{2,root}},samplerLoad).unresolvedSamplers==1);
  samplerSource=true;
  for(const std::string index:{"6","%2"})
  {
    auto outside=sampled;outside.replace(outside.find("%table, i64 5"),std::string("%table, i64 5").size(),"%table, i64 "+index);
    auto report=UniformResourceAccess(outside,"sampling",{{0,heap},{2,root}},samplerLoad);
    assert(report.textureCalls==1 && report.samplerCalls==1 && report.unresolvedSamplers==1);
  }
  auto gathered=sampled;gathered.replace(gathered.find("@air.sample_texture_"),std::string("@air.sample_texture_").size(),"@air.gather_texture_");
  assert(UniformResourceAccess(gathered,"sampling",{{0,heap},{2,root}},samplerLoad).samplerCalls==1);
  const std::string bufferModule=R"(
source_filename = "buffer"
define void @buffer(<3 x i32> %0, i8 addrspace(2)* %1, i8 addrspace(2)* %2) {
  %scenePtr = bitcast i8 addrspace(2)* %1 to i8 addrspace(2)* addrspace(2)*
  %scene = load i8 addrspace(2)*, i8 addrspace(2)* addrspace(2)* %scenePtr, align 8
  %widthByte = getelementptr i8, i8 addrspace(2)* %scene, i64 4
  %widthPtr = bitcast i8 addrspace(2)* %widthByte to i32 addrspace(2)*
  %width = load i32, i32 addrspace(2)* %widthPtr, align 4
  %outputByte = getelementptr i8, i8 addrspace(2)* %1, i64 8
  %outputPtr = bitcast i8 addrspace(2)* %outputByte to i8 addrspace(2)* addrspace(2)*
  %output = load i8 addrspace(2)*, i8 addrspace(2)* addrspace(2)* %outputPtr, align 8
  %slotPtr = bitcast i8 addrspace(2)* %output to i32 addrspace(2)*
  %slot = load i32, i32 addrspace(2)* %slotPtr, align 4
  %strideByte = getelementptr i8, i8 addrspace(2)* %output, i64 4
  %stridePtr = bitcast i8 addrspace(2)* %strideByte to i32 addrspace(2)*
  %stride = load i32, i32 addrspace(2)* %stridePtr, align 4
  %heapOffset = mul i32 %slot, 24
  %heapWide = zext i32 %heapOffset to i64
  %heapByte = getelementptr i8, i8 addrspace(2)* %2, i64 %heapWide
  %dataPtr = bitcast i8 addrspace(2)* %heapByte to i32 addrspace(1)* addrspace(2)*
  %data = load i32 addrspace(1)*, i32 addrspace(1)* addrspace(2)* %dataPtr, align 8
  %x = extractelement <3 x i32> %0, i64 0
  %y = extractelement <3 x i32> %0, i64 1
  %rows = mul i32 %y, %width
  %index = add i32 %rows, %x
  %byte = mul i32 %index, %stride
  %word = lshr i32 %byte, 2
  %wide = zext i32 %word to i64
  %destination = getelementptr i32, i32 addrspace(1)* %data, i64 %wide
  store i32 42, i32 addrspace(1)* %destination, align 4
}
!1 = !{i32 0, !"air.thread_position_in_grid", !"air.arg_type_name", !"uint3"}
!2 = !{i32 1, !"air.indirect_buffer", !"air.location_index", i32 2, i32 1}
!3 = !{i32 2, !"air.indirect_buffer", !"air.location_index", i32 0, i32 1}
)";
  Value metadataRoot=root,metadataHeap=heap;metadataRoot.metadata=metadataHeap.metadata=true;
  uint32_t stride=4;bool outputKnown=true;
  auto bufferLoad=[&](const Value &address,unsigned bytes,Value::Kind kind) {
    if(kind==Value::Pointer && address.slot==2 && bytes==8 && (address.offset==0 || address.offset==8))
    {Value r;r.kind=kind;r.object=100+address.offset/8;r.offset=16;return r;}
    if(kind==Value::Integer && bytes==4)
    {
      if(address.object==100 && address.offset==20)return Value::Number(8);
      if(address.object==101 && address.offset==16)return Value::Number(2);
      if(address.object==101 && address.offset==20)return Value::Number(stride);
    }
    if(kind==Value::Pointer && address.object==24 && address.offset==48 && outputKnown)
    {Value r;r.kind=kind;r.object=200;r.offset=32;r.descriptorObject=24;r.descriptorOffset=48;return r;}
    return Value();
  };
  auto bufferReport=UniformResourceAccess(bufferModule,"buffer",{{0,metadataHeap},{2,metadataRoot}},bufferLoad,{8,66,1});
  assert(bufferReport.validModule && bufferReport.bufferReads==3 && bufferReport.bufferWrites==1 &&
         !bufferReport.unresolvedBuffers && bufferReport.buffers.size()==4);
  const auto &write=bufferReport.buffers.back();
  assert(write.write && write.bytes==4 && write.address.object==200 && write.address.offset==32 &&
         write.address.High()==2140 && write.address.offsetKnown && write.address.descriptorOffset==48);
  auto unavailable=UniformResourceAccess(bufferModule,"buffer",{{0,metadataHeap},{2,metadataRoot}},bufferLoad);
  assert(!unavailable.buffers.back().address.offsetKnown);
  outputKnown=false;
  assert(UniformResourceAccess(bufferModule,"buffer",{{0,metadataHeap},{2,metadataRoot}},bufferLoad,{8,66,1}).unresolvedBuffers==1);
  outputKnown=true;stride=UINT32_MAX;
  assert(!UniformResourceAccess(bufferModule,"buffer",{{0,metadataHeap},{2,metadataRoot}},bufferLoad,{8,66,1}).buffers.back().address.offsetKnown);
  const std::string dimensions=R"(
define void @dimensions(i8 addrspace(2)* %0) {
  %field = getelementptr i8, i8 addrspace(2)* %0, i64 8
  %pointer = bitcast i8 addrspace(2)* %field to %struct._texture_2d_t addrspace(1)* addrspace(2)*
  %texture = load %struct._texture_2d_t addrspace(1)*, %struct._texture_2d_t addrspace(1)* addrspace(2)* %pointer, align 8
  %width = call i32 @air.get_width_texture_2d(%struct._texture_2d_t addrspace(1)* %texture)
}
!1 = !{i32 0, !"air.indirect_buffer", !"air.location_index", i32 0, i32 1}
)";
  const auto dimensionReport=UniformResourceAccess(dimensions,"dimensions",{{0,metadataHeap}},samplerLoad);
  assert(dimensionReport.textureCalls==1 && !dimensionReport.unresolvedTextures &&
         dimensionReport.accesses.size()==1 && dimensionReport.accesses[0].resourceOnly);
  std::string fence=dimensions;
  const auto fenceSite=fence.find("  %width = call");
  fence.replace(fenceSite,fence.find('\n',fenceSite)-fenceSite,
      "  call void @air.fence_texture_2d(%struct._texture_2d_t addrspace(1)* %texture)");
  const auto fenceReport=UniformResourceAccess(fence,"dimensions",{{0,metadataHeap}},samplerLoad);
  assert(fenceReport.textureCalls==1 && !fenceReport.unresolvedCalls &&
         !fenceReport.unresolvedTextures && fenceReport.accesses.size()==1);
  assert(fenceReport.accesses[0].ordering && fenceReport.accesses[0].resourceOnly &&
         !fenceReport.accesses[0].write && !fenceReport.accesses[0].read);
  const auto missingFence=UniformResourceAccess(fence,"dimensions",{{0,metadataHeap}},
      [](const Value &,unsigned,Value::Kind){return Value();});
  assert(missingFence.unresolvedTextures>0);
  std::string bufferFence=fence;
  for(size_t at=0;(at=bufferFence.find("texture_2d",at))!=std::string::npos;at+=17)
    bufferFence.replace(at,10,"texture_buffer_1d");
  const auto bufferFenceReport=UniformResourceAccess(bufferFence,"dimensions",{{0,metadataHeap}},samplerLoad);
  assert(!bufferFenceReport.unresolvedCalls && !bufferFenceReport.unresolvedTextures &&
         bufferFenceReport.accesses.size()==1 && bufferFenceReport.accesses[0].ordering);
  std::string wrongFence=fence;
  wrongFence.replace(wrongFence.find("@air.fence_texture_2d"),21,"@air.fence_texture_3d");
  assert(UniformResourceAccess(wrongFence,"dimensions",{{0,metadataHeap}},samplerLoad).unresolvedCalls>0);
  // The original module carries this ordinary numeric input. Dynamic indices
  // remain GPU computations; a constant/null join does not require a fake
  // external buffer or a CPU lookup of the embedded numbers.
  const std::string moduleConstant=R"(
@palette = internal unnamed_addr addrspace(2) constant [6 x i32] [i32 11, i32 13, i32 17, i32 19, i32 23, i32 29]
define i32 @owned(i64 %index, i1 %predicate) {
  %p = getelementptr [6 x i32], [6 x i32] addrspace(2)* @palette, i64 0, i64 %index
  %choice = select i1 %predicate, i32 addrspace(2)* %p, i32 addrspace(2)* null
  %number = load i32, i32 addrspace(2)* %choice, align 4
  ret i32 %number
}
)";
  auto noExternalLoad=[](const Value &,unsigned,Value::Kind)->Value {assert(false);return {};};
  const auto owned=UniformResourceAccess(moduleConstant,"owned",{},noExternalLoad);
  assert(owned.validModule && owned.unresolvedBuffers==0 && owned.shaderConstantReads==1 &&
         owned.buffers.empty() && owned.returned.kind==Value::Unknown);
  // Opaque trailing function attributes cannot replace the direct parameter
  // list or leak an indirect member's local ordinal into Native buffer bindings.
  const std::string entryAttributes=R"(
define i32 @attributes(float %raster, i32 addrspace(2)* %input) {
  %number = load i32, i32 addrspace(2)* %input, align 4
  ret i32 %number
}
!air.fragment = !{!1}
!1 = !{i32 (float, i32 addrspace(2)*)* @attributes, !2, !3, !"opaque, function attribute", !9}
!2 = !{}
!3 = !{!4, !5}
!4 = !{i32 0, !"air.position"}
!5 = !{i32 1, !"air.buffer", !"air.location_index", i32 11, i32 1, !"air.struct_type_info", !6}
!6 = !{!7}
!7 = !{i32 0, !"air.buffer", !"air.location_index", i32 2, i32 1}
!9 = !{!"another attribute"}
)";
  Value attributeDirect;attributeDirect.kind=Value::Pointer;attributeDirect.object=311;
  Value attributeMember;attributeMember.kind=Value::Pointer;attributeMember.object=999;
  const auto attributed=UniformResourceAccess(entryAttributes,"attributes",{{11,attributeDirect},{2,attributeMember}},
      [](const Value &,unsigned,Value::Kind)->Value{return {};});
  assert(attributed.validModule && !attributed.unresolvedBuffers && attributed.buffers.size()==1 &&
         attributed.buffers[0].address.object==311 && attributed.returned.kind==Value::Unknown);
  assert(UniformResourceAccess(entryAttributes,"attributes",{{2,attributeMember}},
      [](const Value &,unsigned,Value::Kind)->Value{return {};}).unresolvedBuffers>0);
  std::string missingArguments=entryAttributes;
  missingArguments.erase(missingArguments.find("!3 ="),std::string("!3 = !{!4, !5}\n").size());
  assert(!UniformResourceAccess(missingArguments,"attributes",{{11,attributeDirect}},{}).validModule);
  const std::string localModule=R"(
define i32 @module_entry(i32 addrspace(1)* %input) {
  %number = call fastcc i32 @local_load(i32 addrspace(1)* %input)
  ret i32 %number
}
define internal fastcc i32 @local_load(i32 addrspace(1)* %input) {
  %number = load i32, i32 addrspace(1)* %input, align 4
  ret i32 %number
}
define internal void @unused_writer(i32 addrspace(1)* %input) {
  store i32 19, i32 addrspace(1)* %input, align 4
  ret void
}
!air.kernel = !{!1}
!1 = !{i32 (i32 addrspace(1)*)* @module_entry, !2, !3}
!2 = !{}
!3 = !{!4}
!4 = !{i32 0, !"air.buffer", !"air.location_index", i32 11, i32 1}
)";
  const CallResolver moduleCalls=[&](const std::string &symbol,const std::vector<Value> &parameters) {
    assert(symbol=="local_load");InvocationValues invocation;invocation.localDefinition=true;
    for(unsigned i=0;i<parameters.size();i++)invocation.arguments[i]=parameters[i];
    return UniformResourceAccess(localModule,symbol,{},[](const Value &,unsigned,Value::Kind){return Value();},
        {},false,invocation);
  };
  const auto moduleSelected=UniformResourceAccess(localModule,"module_entry",{{11,attributeDirect}},
      [](const Value &,unsigned,Value::Kind){return Value();},{},false,{},moduleCalls);
  assert(moduleSelected.validModule && moduleSelected.linkedCalls==1 &&
         moduleSelected.bufferReads==1 && moduleSelected.bufferWrites==0 &&
         moduleSelected.unresolvedBuffers==0 && moduleSelected.unresolvedCalls==0 &&
         moduleSelected.buffers[0].address.object==311 && moduleSelected.returned.kind==Value::Unknown);
  assert(UniformResourceAccess(localModule,"module_entry",{},
      [](const Value &,unsigned,Value::Kind){return Value();},{},false,{},moduleCalls).unresolvedBuffers>0);
  InvocationValues incompleteHelper;incompleteHelper.localDefinition=true;
  assert(!UniformResourceAccess(localModule,"local_load",{}, {},{},false,incompleteHelper).validModule);
  assert(!UniformResourceAccess(localModule+"\ndefine i32 @module_entry() {\n  ret i32 0\n}\n",
      "module_entry",{},{}).validModule);
  const std::string nativeDiscard=R"(
define void @fragment_control() {
  call void @air.discard_fragment()
  ret void
}
declare void @air.discard_fragment() nounwind
)";
  const auto discardReport=UniformResourceAccess(nativeDiscard,"fragment_control",{}, {},{},false,{}, {},true);
  assert(discardReport.validModule && discardReport.unresolvedCalls==0 &&
         discardReport.conditionalAccesses && discardReport.buffers.empty());
  assert(UniformResourceAccess(nativeDiscard,"fragment_control",{},{}).unresolvedCalls==1);
  std::string missingDiscardDeclaration=nativeDiscard.substr(0,nativeDiscard.find("declare"));
  assert(UniformResourceAccess(missingDiscardDeclaration,"fragment_control",{}, {},{},false,{}, {},true).unresolvedCalls==1);
  std::string missingDefinition=moduleConstant;
  missingDefinition.replace(0,missingDefinition.find("define"),
      "@palette = external addrspace(2) constant [6 x i32]\n");
  assert(UniformResourceAccess(missingDefinition,"owned",{},noExternalLoad).unresolvedBuffers>0);
  std::string pointerConstant=R"(
@address = internal addrspace(2) constant i64 91418487808
define i32 @raw_address() {
  %bits = load i64, i64 addrspace(2)* @address, align 8
  %pointer = inttoptr i64 %bits to i32 addrspace(1)*
  %number = load i32, i32 addrspace(1)* %pointer, align 4
  ret i32 %number
}
)";
  const auto pointerOwned=UniformResourceAccess(pointerConstant,"raw_address",{},noExternalLoad);
  assert(pointerOwned.shaderConstantReads==1 && pointerOwned.unresolvedBuffers>0 && pointerOwned.buffers.empty());
  std::string writeConstant=moduleConstant;
  writeConstant.insert(writeConstant.find("  ret"),"  store i32 9, i32 addrspace(2)* %choice, align 4\n");
  assert(UniformResourceAccess(writeConstant,"owned",{},noExternalLoad).unresolvedBuffers>0);
  std::string rawHandle = rawRoot;
  const auto textureLoad = rawHandle.find("  %image = load");
  const auto textureLoadEnd = rawHandle.find('\n', textureLoad);
  rawHandle.replace(textureLoad, textureLoadEnd - textureLoad,
      "  %bits = bitcast %struct._texture_2d_t addrspace(1)* addrspace(1)* %field to i64 addrspace(1)*\n"
      "  %handle = load i64, i64 addrspace(1)* %bits, align 8\n"
      "  %image = inttoptr i64 %handle to %struct._texture_2d_t addrspace(1)*");
  rawAccess = UniformTextureAccess(rawHandle, "raw", {{0, root}}, rawLoad);
  assert(rawAccess.size() == 1 && rawAccess[0].write && rawAccess[0].offset == 8);
  auto scalarHandle = [&](const Value &v, unsigned bytes, Value::Kind kind) {
    if(v.object == 24 && v.offset == 8)
      return kind == Value::Integer ? Value::Number(8) : Value();
    return rawLoad(v, bytes, kind);
  };
  assert(UniformTextureAccess(rawHandle, "raw", {{0, root}}, scalarHandle).empty());
  if(argc == 2)
  {
    std::ifstream file(argv[1]); std::stringstream buf; buf << file.rdbuf();
    auto ue = UniformTextureAccess(buf.str(), "Main_0000aee4_f632e2ef", {{0, heap}, {2, root}},
      [](const Value &v, unsigned bytes, Value::Kind kind) {
        if(kind == Value::Pointer && v.slot == 2)
        { Value r; r.kind = kind; r.object = 100 + v.offset; return r; }
        if(kind == Value::Integer && v.object >= 100) return Value::Number(v.offset + 1);
        if(kind == Value::Texture && v.object == 24)
        { Value r=v; r.kind=kind; return r; }
        return Value();
      });
    assert(!ue.empty());
    std::cout << "Actual UE AIR resolves " << ue.size() << " static texture call sites with synthetic uniform bytes\n";
  }
  if(argc == 3)
  {
    std::ifstream file(argv[1]); assert(file.good()); std::stringstream buf; buf << file.rdbuf();
    // Parser-shape evidence only. These synthetic scalar values/objects are
    // deliberately unrelated to capture values and cannot authorize replay.
    auto report = UniformResourceAccess(buf.str(), argv[2], {{0, heap}, {2, root}},
      [](const Value &v, unsigned bytes, Value::Kind kind) {
        if(kind == Value::Pointer && v.slot == 2)
        { Value r; r.kind=kind; r.object=100+v.offset; return r; }
        if(kind == Value::Integer && v.object>=100 && v.object<1000)
          return Value::Number(v.offset+1);
        if(kind == Value::Pointer && v.object==24 && !(v.offset%24))
        { Value r; r.kind=kind; r.object=1000;r.descriptorObject=24;r.descriptorOffset=v.offset;return r; }
        if((kind==Value::AccelerationStructure && v.object==1000 && !v.offset) ||
           (kind==Value::Texture && v.object==24 && v.offset%24==8))
        { Value r=v;r.kind=kind;return r; }
        return Value();
      });
    assert(report.validModule && report.queryResets);
    unsigned structures=0;
    for(const auto &access:report.accesses)structures+=access.kind==Value::AccelerationStructure;
    std::cout << "PARSER ONLY synthetic roots: AS calls=" << report.queryResets << " resolved=" << structures
              << " unresolved=" << report.unresolvedStructures << " texture calls=" << report.textureCalls
              << " unresolved=" << report.unresolvedTextures << "\n";
  }
  const std::string selection = R"(
define void @select_pointer(i32 addrspace(1)* %p, i32 %size) {
  %empty = icmp eq i32 %size, 0
  %target = select i1 %empty, i32 addrspace(1)* inttoptr (i64 1024 to i32 addrspace(1)*), i32 addrspace(1)* %p
  store i32 0, i32 addrspace(1)* %target, align 4
}
!1 = !{i32 0, !"air.buffer", !"air.location_index", i32 0, i32 1}
!2 = !{i32 1, !"air.buffer", !"air.location_index", i32 2, i32 1}
)";
  Value destination;destination.kind=Value::Pointer;destination.object=101;
  auto selected=UniformResourceAccess(selection,"select_pointer",{{0,destination},{2,Value::Number(256)}},{});
  assert(selected.validModule && selected.bufferWrites==1 && !selected.unresolvedBuffers &&
      selected.buffers.size()==1 && selected.buffers[0].address.object==101);
  auto sentinel=UniformResourceAccess(selection,"select_pointer",{{0,destination},{2,Value::Number(0)}},{});
  assert(sentinel.validModule && sentinel.bufferWrites==1 && !sentinel.unresolvedBuffers &&
      sentinel.pointerSelections==1 && sentinel.buffers.size()==1 &&
      !sentinel.buffers[0].definiteStore && sentinel.literalAccesses.size()==1 &&
      sentinel.literalAccesses[0]==std::make_pair(uint64_t(1024),uint64_t(4)));
  auto unknownCondition=UniformResourceAccess(selection,"select_pointer",{{0,destination}},{});
  assert(unknownCondition.validModule && !unknownCondition.unresolvedBuffers &&
      unknownCondition.pointerSelections==1 && unknownCondition.buffers.size()==1 &&
      unknownCondition.buffers[0].address.object==101 && !unknownCondition.buffers[0].definiteStore &&
      unknownCondition.literalPointers.size()==1 && unknownCondition.literalPointers[0]==1024);
  std::string twoPointers=selection;
  twoPointers.replace(twoPointers.find("inttoptr (i64 1024 to i32 addrspace(1)*)"),
      std::string("inttoptr (i64 1024 to i32 addrspace(1)*)").size(),"%other");
  twoPointers.replace(twoPointers.find("i32 %size)"),std::string("i32 %size)").size(),
      "i32 %size, i32 addrspace(1)* %other)");
  InvocationValues candidates;candidates.arguments[0]=destination;
  Value alternatePointer=destination;alternatePointer.object=102;candidates.arguments[2]=alternatePointer;
  auto alternatives=UniformResourceAccess(twoPointers,"select_pointer",{}, {},{},true,candidates);
  assert(!alternatives.unresolvedBuffers && alternatives.buffers.size()==2 &&
      alternatives.pointerSelections==1 && alternatives.literalPointers.empty());
  candidates.arguments.erase(2);
  auto missingAlternative=UniformResourceAccess(twoPointers,"select_pointer",{}, {},{},true,candidates);
  assert(missingAlternative.unresolvedBuffers==1 && missingAlternative.buffers.size()==1);
  auto atomicSelection=selection;
  atomicSelection.replace(atomicSelection.find("store i32 0, i32 addrspace(1)* %target, align 4"),
      std::string("store i32 0, i32 addrspace(1)* %target, align 4").size(),
      "%result = call i32 @air.atomic.global.max.u.i32(i32 addrspace(1)* %target, i32 0)");
  auto selectedAtomic=UniformResourceAccess(atomicSelection,"select_pointer",{{0,destination}},{});
  assert(!selectedAtomic.unresolvedBuffers && !selectedAtomic.unresolvedCalls &&
      selectedAtomic.bufferReads==1 && selectedAtomic.bufferWrites==1 && selectedAtomic.buffers.size()==2 &&
      selectedAtomic.pointerSelections==1 && !selectedAtomic.buffers.back().definiteStore);
  auto projectedSelection=atomicSelection;
  projectedSelection.insert(0,"%struct.atomic = type { i32 }\n");
  projectedSelection.insert(projectedSelection.find("  %result = call"),
      "  %cast_target = bitcast i32 addrspace(1)* %target to %struct.atomic addrspace(1)*\n"
      "  %projected = getelementptr inbounds %struct.atomic, %struct.atomic addrspace(1)* %cast_target, i64 0, i32 0\n");
  projectedSelection.replace(projectedSelection.find("i32 addrspace(1)* %target, i32 0"),
      std::string("i32 addrspace(1)* %target, i32 0").size(),"i32 addrspace(1)* %projected, i32 0");
  auto projectedAtomic=UniformResourceAccess(projectedSelection,"select_pointer",{{0,destination}},{});
  assert(!projectedAtomic.unresolvedBuffers && projectedAtomic.buffers.size()==2 &&
      projectedAtomic.buffers[0].address.object==101);
  auto derivedLiteral=projectedSelection;
  derivedLiteral.replace(derivedLiteral.find("i64 0, i32 0"),std::string("i64 0, i32 0").size(),"i64 1, i32 0");
  auto projectedLiteral=UniformResourceAccess(derivedLiteral,"select_pointer",{{0,destination}},{});
  assert(!projectedLiteral.unresolvedBuffers && projectedLiteral.buffers.size()==2 &&
      projectedLiteral.literalAccesses.size()==1 &&
      projectedLiteral.literalAccesses[0].first==1028 &&
      projectedLiteral.literalAccesses[0].second==4);
  auto numericallySelected=UniformResourceAccess(derivedLiteral,"select_pointer",
      {{0,destination},{2,Value::Number(256)}},{});
  assert(!numericallySelected.unresolvedBuffers && numericallySelected.literalAccesses.size()==1 &&
      numericallySelected.literalAccesses[0].first==1028);
  auto unknownDerived=derivedLiteral;
  unknownDerived.replace(unknownDerived.find("i64 1, i32 0"),
      std::string("i64 1, i32 0").size(),"i64 %delta, i32 0");
  assert(UniformResourceAccess(unknownDerived,"select_pointer",{{0,destination}},{}).unresolvedBuffers);
  auto missingAtomic=UniformResourceAccess(atomicSelection,"select_pointer",{},{});
  assert(missingAtomic.unresolvedBuffers && missingAtomic.buffers.empty());
  auto argumentSelection=atomicSelection;
  argumentSelection.replace(argumentSelection.find("air.atomic.global.max.u.i32"),
      std::string("air.atomic.global.max.u.i32").size(),"native_argument_memory");
  argumentSelection+="\ndeclare i32 @native_argument_memory(i32 addrspace(1)*, i32) #0\nattributes #0 = { nounwind argmemonly }\n";
  auto selectedArgument=UniformResourceAccess(argumentSelection,"select_pointer",{{0,destination}},{});
  assert(!selectedArgument.unresolvedBuffers && !selectedArgument.unresolvedCalls &&
      selectedArgument.buffers.size()==2 && !selectedArgument.buffers[0].address.offsetKnown);
  const std::string nullableSource=R"AIR(define void @nullable(i32 addrspace(1)* %p, i32 %index, i32 %predicate) {
  %condition = icmp ne i32 %predicate, 0
  %selected = select i1 %condition, i32 addrspace(1)* %p, i32 addrspace(1)* null
  %extended = zext i32 %index to i64
  %at = getelementptr i32, i32 addrspace(1)* %selected, i64 %extended
  %loaded = load i32, i32 addrspace(1)* %at, align 4
  store i32 %loaded, i32 addrspace(1)* %at, align 4
  ret void
}
)AIR";
  InvocationValues nullableArgs;nullableArgs.arguments[0]=destination;
  auto nullable=UniformResourceAccess(nullableSource,"nullable",{}, {},{},true,nullableArgs);
  assert(!nullable.unresolvedBuffers && nullable.buffers.size()==2 && nullable.pointerSelections==1 &&
      nullable.literalPointers.empty() && nullable.buffers[0].address.object==101 &&
      !nullable.buffers[0].address.offsetKnown && !nullable.buffers[1].definiteStore);
  auto onlyNull=UniformResourceAccess(nullableSource,"nullable",{},{});
  assert(onlyNull.unresolvedBuffers && onlyNull.buffers.empty());
  // CPU-known predicates must not discard a restored source from Native
  // control flow. This is provenance, not an assertion that null was accessed.
  for(uint64_t predicate : {0ULL,1ULL})
  {
    auto foldedArgs=nullableArgs;foldedArgs.arguments[2]=Value::Number(predicate);
    auto folded=UniformResourceAccess(nullableSource,"nullable",{}, {},{},true,foldedArgs);
    assert(!folded.unresolvedBuffers && folded.buffers.size()==2 &&
        folded.pointerSelections==1 && folded.buffers[0].address.object==101);
    foldedArgs.arguments.erase(0);
    assert(UniformResourceAccess(nullableSource,"nullable",{}, {},{},true,foldedArgs).unresolvedBuffers);
  }
  auto nullOnlySource=nullableSource;
  nullOnlySource.replace(nullOnlySource.find("i32 addrspace(1)* %p, i32 addrspace(1)* null"),
      std::string("i32 addrspace(1)* %p, i32 addrspace(1)* null").size(),
      "i32 addrspace(1)* null, i32 addrspace(1)* null");
  auto nullOnly=UniformResourceAccess(nullOnlySource,"nullable",{}, {},{},true,nullableArgs);
  assert(nullOnly.unresolvedBuffers && nullOnly.buffers.empty());
  auto opaqueAddress=nullableSource;
  opaqueAddress.replace(opaqueAddress.find("i32 addrspace(1)* null"),
      std::string("i32 addrspace(1)* null").size(),"i32 addrspace(1)* %opaque");
  assert(UniformResourceAccess(opaqueAddress,"nullable",{}, {},{},true,nullableArgs).unresolvedBuffers);
  // A typed address payload used as the index needs address recovery; it must
  // not become an ordinary unknown numerical index through a cast.
  auto directAddress=nullableSource;
  directAddress.replace(directAddress.find("i32 %index"),
      std::string("i32 %index").size(),"i64 %index");
  directAddress.replace(directAddress.find("%extended = zext i32 %index to i64"),
      std::string("%extended = zext i32 %index to i64").size(),"%extended = bitcast i64 %index to i64");
  nullableArgs.arguments[1]=destination;
  assert(UniformResourceAccess(directAddress,"nullable",{}, {},{},true,nullableArgs).unresolvedBuffers);
  auto structAddress=directAddress;
  structAddress.insert(0,"%struct.element = type { i32 }\n");
  structAddress.replace(structAddress.find("  %at = getelementptr i32, i32 addrspace(1)* %selected, i64 %extended"),
      std::string("  %at = getelementptr i32, i32 addrspace(1)* %selected, i64 %extended").size(),
      "  %structured = bitcast i32 addrspace(1)* %selected to %struct.element addrspace(1)*\n"
      "  %at = getelementptr %struct.element, %struct.element addrspace(1)* %structured, i64 %extended, i32 0");
  assert(UniformResourceAccess(structAddress,"nullable",{}, {},{},true,nullableArgs).unresolvedBuffers);
  nullableArgs.arguments.erase(1);
  auto structNullable=UniformResourceAccess(structAddress,"nullable",{}, {},{},true,nullableArgs);
  assert(!structNullable.unresolvedBuffers && structNullable.buffers.size()==2 &&
      !structNullable.buffers[0].address.offsetKnown);

  // A GPU-produced integer is never literal bytecode pointer provenance.
  auto gpuPointer=MergePointerProvenance(destination,Value::Number(1024));
  assert(gpuPointer.fields.size()==2 && gpuPointer.fields[1].kind==Value::Integer);

  const std::string nativeSync=R"AIR(define void @sync(i32 %flags) {
  call void @air.simdgroup.barrier(i32 2, i32 1)
  call void @air.wg.barrier(i32 0, i32 1)
  call void @air.atomic.fence(i32 2, i32 4, i32 1)
  ret void
}
)AIR";
  auto synchronized=UniformResourceAccess(nativeSync,"sync",{},{});
  assert(synchronized.validModule && !synchronized.unresolvedCalls && !synchronized.unresolvedBuffers &&
      !synchronized.bufferReads && !synchronized.bufferWrites && synchronized.buffers.empty());
  auto unsupportedSync=nativeSync;
  unsupportedSync.replace(unsupportedSync.find("@air.wg.barrier"),std::string("@air.wg.barrier").size(),"@unknown.synchronization");
  assert(UniformResourceAccess(unsupportedSync,"sync",{},{}).unresolvedCalls==1);

  const std::string externalCall="define void @external() {\n  call void @unknown_writer()\n}\n";
  const auto unknownExternal=UniformResourceAccess(externalCall,"external",{},{});
  assert(unknownExternal.validModule && unknownExternal.unresolvedCalls==1);
  const std::string declaredEffects=R"AIR(define void @effects(i32 addrspace(1)* %p, float %v) {
  %numerical = call float @native_numerical(float %v)
  %atomic = call i32 @native_argument_memory(i32 addrspace(1)* %p, i32 7)
  ret void
}
declare float @native_numerical(float) #0
declare i32 @native_argument_memory(i32 addrspace(1)*, i32) #1
attributes #0 = { nounwind readnone }
attributes #1 = { nounwind argmemonly }
)AIR";
  InvocationValues effectArguments;effectArguments.arguments[0]=destination;
  auto effects=UniformResourceAccess(declaredEffects,"effects",{}, {},{},true,effectArguments);
  assert(effects.validModule && !effects.unresolvedCalls && !effects.unresolvedBuffers &&
      effects.bufferReads==1 && effects.bufferWrites==1 && effects.buffers.size()==2 &&
      !effects.buffers[0].address.offsetKnown && !effects.buffers[1].definiteStore);
  auto constantEffects=declaredEffects;
  for(size_t at=0;(at=constantEffects.find("addrspace(1)",at))!=std::string::npos;at+=12)
    constantEffects.replace(at,12,"addrspace(2)");
  auto constantArgument=UniformResourceAccess(constantEffects,"effects",{}, {},{},true,effectArguments);
  assert(!constantArgument.unresolvedCalls && constantArgument.bufferReads==1 &&
      constantArgument.bufferWrites==0);
  auto nativeAtomic=declaredEffects;
  for(size_t at=0;(at=nativeAtomic.find("native_argument_memory",at))!=std::string::npos;at+=24)
    nativeAtomic.replace(at,std::string("native_argument_memory").size(),"air.atomic.global.max.u.i32");
  nativeAtomic.replace(nativeAtomic.find("nounwind argmemonly"),std::string("nounwind argmemonly").size(),"nounwind");
  auto atomicEffects=UniformResourceAccess(nativeAtomic,"effects",{}, {},{},true,effectArguments);
  assert(!atomicEffects.unresolvedCalls && !atomicEffects.unresolvedBuffers && atomicEffects.bufferWrites==1);
  auto missingEffects=UniformResourceAccess(declaredEffects,"effects",{}, {},{},true);
  assert(missingEffects.unresolvedBuffers);
  auto inaccessibleEffects=declaredEffects;
  inaccessibleEffects.replace(inaccessibleEffects.find("nounwind argmemonly"),
      std::string("nounwind argmemonly").size(),"nounwind inaccessiblemem_or_argmemonly");
  auto inaccessibleCall=UniformResourceAccess(inaccessibleEffects,"effects",{}, {},{},true,effectArguments);
  assert(inaccessibleCall.unresolvedCalls==1);
  auto missingDeclarations=declaredEffects.substr(0,declaredEffects.find("declare float"));
  auto unknownEffects=UniformResourceAccess(missingDeclarations,"effects",{}, {},{},true,effectArguments);
  assert(unknownEffects.unresolvedCalls==2);

  const std::string internalSampler=R"AIR(
%struct._sampler_t = type opaque
define void @internal(i32 addrspace(1)* %p) {
  %s = call %struct._sampler_t addrspace(2)* @air.get_read_sampler()
  store i32 1, i32 addrspace(1)* %p, align 4
  ret void
}
declare %struct._sampler_t addrspace(2)* @air.get_read_sampler() #0
attributes #0 = { nounwind readonly inaccessiblememonly }
)AIR";
  auto internal=UniformResourceAccess(internalSampler,"internal",{}, {},{},true,effectArguments);
  assert(!internal.unresolvedCalls && !internal.unresolvedBuffers && internal.buffers.size()==1);
  for(const auto &change:std::vector<std::pair<std::string,std::string>>{
      {"air.get_read_sampler","external_sampler"},
      {"readonly","writeonly"},{"inaccessiblememonly","inaccessiblemem_or_argmemonly"}})
  {
    auto unsupported=internalSampler;
    for(size_t at=0;(at=unsupported.find(change.first,at))!=std::string::npos;at+=change.second.size())
      unsupported.replace(at,change.first.size(),change.second);
    assert(UniformResourceAccess(unsupported,"internal",{}, {},{},true,effectArguments).unresolvedCalls==1);
  }
  auto escapedSampler=internalSampler;
  escapedSampler.insert(escapedSampler.find("  store i32"),
      "  %escaped = bitcast %struct._sampler_t addrspace(2)* %s to i32 addrspace(1)*\n"
      "  %read = load i32, i32 addrspace(1)* %escaped, align 4\n");
  assert(UniformResourceAccess(escapedSampler,"internal",{}, {},{},true,effectArguments).unresolvedBuffers);

  const std::string lanes=R"AIR(
define void @lanes(i32 addrspace(1)* %p, i32 %v) {
  %mask = call i32 @air.simd_ballot.i32(i1 true)
  %first = call i1 @air.simd_is_first()
  %lane = call i32 @air.simd_broadcast_first.u.i32(i32 %v)
  %wide = zext i32 %lane to i64
  %at = getelementptr i32, i32 addrspace(1)* %p, i64 %wide
  store i32 %mask, i32 addrspace(1)* %at, align 4
  ret void
}
declare i32 @air.simd_ballot.i32(i1) #0
declare i1 @air.simd_is_first() #0
declare i32 @air.simd_broadcast_first.u.i32(i32) #0
attributes #0 = { convergent nounwind }
)AIR";
  auto laneEffects=UniformResourceAccess(lanes,"lanes",{}, {},{},true,effectArguments);
  assert(!laneEffects.unresolvedCalls && !laneEffects.unresolvedBuffers &&
      laneEffects.buffers.size()==1 && !laneEffects.buffers[0].address.offsetKnown &&
      !laneEffects.buffers[0].definiteStore);
  auto pointerLane=lanes;
  const std::string laneCall="@air.simd_broadcast_first.u.i32(i32 %v)";
  pointerLane.replace(pointerLane.find(laneCall),laneCall.size(),
      "@air.simd_broadcast_first.u.i32(i32 addrspace(1)* %p)");
  assert(UniformResourceAccess(pointerLane,"lanes",{}, {},{},true,effectArguments).unresolvedCalls==1);
  auto unqualifiedLane=lanes;
  unqualifiedLane.replace(unqualifiedLane.find("convergent nounwind"),
      std::string("convergent nounwind").size(),"nounwind");
  assert(UniformResourceAccess(unqualifiedLane,"lanes",{}, {},{},true,effectArguments).unresolvedCalls==3);

  const std::string pointerJoin=R"AIR(
define void @join(i32 addrspace(1)* %p, i64 %index) {
  %joined = phi i32 addrspace(1)* [ %p, %left ], [ null, %right ]
  %at = getelementptr i32, i32 addrspace(1)* %joined, i64 %index
  %read = load i32, i32 addrspace(1)* %at, align 4
  ret void
}
)AIR";
  auto joined=UniformResourceAccess(pointerJoin,"join",{}, {},{},true,effectArguments);
  assert(!joined.unresolvedBuffers && joined.buffers.size()==1 &&
      joined.buffers[0].address.object==effectArguments.arguments.at(0).object &&
      !joined.buffers[0].address.offsetKnown);
  auto unknownJoin=pointerJoin;
  unknownJoin.replace(unknownJoin.find("[ null, %right ]"),std::string("[ null, %right ]").size(),
      "[ %missing, %right ]");
  assert(UniformResourceAccess(unknownJoin,"join",{}, {},{},true,effectArguments).unresolvedBuffers);

  const std::string atomics=R"AIR(
define void @atomics(i32 addrspace(1)* %p, %struct._texture_2d_t addrspace(1)* %t) {
  call void @air.atomic.global.store.i32(i32 addrspace(1)* %p, i32 7)
  %a = call i32 @air.atomic.global.load.i32(i32 addrspace(1)* %p)
  %b = call i32 @air.atomic.global.xchg.i32(i32 addrspace(1)* %p, i32 9)
  call void @air.atomic_store_explicit_texture_2d.u.v4i32(%struct._texture_2d_t addrspace(1)* %t)
  %c = call <4 x i32> @air.atomic_load_explicit_texture_2d.u.v4i32(%struct._texture_2d_t addrspace(1)* %t)
  %d = call <4 x i32> @air.atomic_fetch_add_explicit_texture_2d.u.v4i32(%struct._texture_2d_t addrspace(1)* %t)
  ret void
}
)AIR";
  auto atomicArguments=effectArguments;
  Value atomicImage;atomicImage.kind=Value::Texture;atomicImage.object=42;atomicImage.offset=8;
  atomicArguments.arguments[1]=atomicImage;
  auto roles=UniformResourceAccess(atomics,"atomics",{}, {},{},true,atomicArguments);
  assert(!roles.unresolvedCalls && !roles.unresolvedBuffers && !roles.unresolvedTextures &&
      roles.bufferReads==2 && roles.bufferWrites==2 && roles.buffers.size()==4 && roles.accesses.size()==3 &&
      !roles.accesses[0].read && roles.accesses[0].write &&
      roles.accesses[1].read && !roles.accesses[1].write &&
      roles.accesses[2].read && roles.accesses[2].write);

  const std::string indexedLoop=R"(
define void @indexed_loop(i32 addrspace(2)* %settings, i32 addrspace(1)* %indices, i32 addrspace(1)* %target) {
entry:
  %count = load i32, i32 addrspace(2)* %settings, align 4
  br label %loop
loop:
  %i = phi i32 [ %next, %loop ], [ 0, %entry ]
  %wide = zext i32 %i to i64
  %index_address = getelementptr i32, i32 addrspace(1)* %indices, i64 %wide
  %index = load i32, i32 addrspace(1)* %index_address, align 4
  %in_range = icmp ult i32 %index, 64
  %bounded_target = select i1 %in_range, i32 addrspace(1)* %target, i32 addrspace(1)* null
  %index_wide = zext i32 %index to i64
  %write_address = getelementptr i32, i32 addrspace(1)* %bounded_target, i64 %index_wide
  store i32 17, i32 addrspace(1)* %write_address, align 4
  %next = add nuw i32 %i, 1
  %again = icmp ult i32 %next, %count
  br i1 %again, label %loop, label %exit
exit:
  ret void
}
!1 = !{i32 0, !"air.buffer", !"air.location_index", i32 2, i32 1}
!2 = !{i32 1, !"air.buffer", !"air.location_index", i32 0, i32 1}
!3 = !{i32 2, !"air.buffer", !"air.location_index", i32 1, i32 1}
)";
  Value indices=destination;indices.object=102;
  unsigned loopCount=4;bool knownIndices=true;
  auto loopLoad=[&](const Value &address,unsigned bytes,Value::Kind kind) {
    if(kind!=Value::Integer || bytes!=4)return Value();
    if(address.object==101)return Value::Number(loopCount);
    if(address.object==102 && address.offset==0 && address.High()==12 && knownIndices)
      return Value::Range(3,55);
    return Value();
  };
  auto loopReport=[&](const std::string &module) {
    return UniformResourceAccess(module,"indexed_loop",{{2,destination},{0,indices},{1,destination}},loopLoad,{},true);
  };
  auto loop=loopReport(indexedLoop);
  assert(loop.validModule && loop.boundedLoops==1 && !loop.unresolvedBuffers &&
      loop.buffers.back().address.offset==12 && loop.buffers.back().address.High()==220);
  // A scalar-only loader must never be called on a range and silently return
  // its first element as the whole indexed input's value.
  auto noRange=UniformResourceAccess(indexedLoop,"indexed_loop",{{2,destination},{0,indices},{1,destination}},loopLoad);
  assert(!noRange.unresolvedBuffers && !noRange.buffers.back().address.offsetKnown &&
      noRange.buffers.back().address.object==destination.object);
  knownIndices=false;assert(!loopReport(indexedLoop).buffers.back().address.offsetKnown || loopReport(indexedLoop).unresolvedBuffers);
  knownIndices=true;
  // Missing CPU loop ranges reduce numerical display precision, while the
  // restored target identity remains independent of those ranges.
  for(unsigned count:{0U,65537U,UINT32_MAX})
  {
    loopCount=count;auto unknownLoop=loopReport(indexedLoop);
    assert(!unknownLoop.unresolvedBuffers && !unknownLoop.boundedLoops &&
        !unknownLoop.buffers.back().address.offsetKnown &&
        unknownLoop.buffers.back().address.object==destination.object);
  }
  loopCount=4;
  for(const auto &change:std::vector<std::pair<std::string,std::string>>{
      {"%i, 1","%i, 2"},{"%next, %count","%next, %next"},
      {"%next = add nuw","%next = mul nuw"},
      {"label %loop, label %exit","label %exit, label %loop"}})
  {
    auto unsupported=indexedLoop;const auto where=unsupported.find(change.first);assert(where!=std::string::npos);
    unsupported.replace(where,change.first.size(),change.second);
    auto unknownLoop=loopReport(unsupported);
    assert(!unknownLoop.unresolvedBuffers && !unknownLoop.boundedLoops &&
        !unknownLoop.buffers.back().address.offsetKnown &&
        unknownLoop.buffers.back().address.object==destination.object);
  }
  auto equalityExit=indexedLoop;
  equalityExit.replace(equalityExit.find("icmp ult i32 %next"),std::string("icmp ult i32 %next").size(),"icmp eq i32 %next");
  equalityExit.replace(equalityExit.find("label %loop, label %exit"),std::string("label %loop, label %exit").size(),"label %exit, label %loop");
  assert(loopReport(equalityExit).boundedLoops==1 && !loopReport(equalityExit).unresolvedBuffers);
  const std::string caller=R"AIR(
source_filename = "caller"
%result = type { <4 x float>, i32 }
define void @caller(i32 %instance, i32 %base, i32 addrspace(1)* %target, i32 addrspace(2)* %input) {
entry:
  %relative = sub i32 %instance, %base
  %pair = call %result @attached(i32 addrspace(2)* %input, i32 %relative, i32 %base)
  %index = extractvalue %result %pair, 1
  %address = getelementptr i32, i32 addrspace(1)* %target, i32 %index
  store i32 17, i32 addrspace(1)* %address, align 4
  ret void
}
!1 = !{i32 0, !"air.instance_id", !"air.arg_type_name", !"uint"}
!2 = !{i32 1, !"air.base_instance", !"air.arg_type_name", !"uint"}
!3 = !{i32 2, !"air.buffer", !"air.location_index", i32 0, i32 1}
!4 = !{i32 3, !"air.buffer", !"air.location_index", i32 1, i32 1}
)AIR";
  const std::string callee=R"AIR(
source_filename = "attached"
%result = type { <4 x float>, i32 }
define %result @attached(i32 addrspace(2)* %input, i32 %relative, i32 %base) {
entry:
  %absolute = add i32 %relative, %base
  %address = getelementptr i32, i32 addrspace(2)* %input, i32 %absolute
  %index = load i32, i32 addrspace(2)* %address, align 4
  %pair = insertvalue %result undef, i32 %index, 1
  ret %result %pair
}
)AIR";
  InvocationValues draw;draw.builtins["instance_id"]=Value::Range(7,8);
  draw.builtins["base_instance"]=Value::Number(7);
  Value linkedInput;linkedInput.kind=Value::Pointer;linkedInput.object=900;
  auto linkedLoad=[](const Value &v,unsigned bytes,Value::Kind kind) {
    return v.object==900 && bytes==4 && kind==Value::Integer && v.offset==28 && v.High()==32?
        Value::Range(3,55):Value();
  };
  CallResolver resolver=[&](const std::string &name,const std::vector<Value> &parameters) {
    if(name!="attached" || parameters.size()!=3)return UniformAccessReport();
    InvocationValues invocation;
    for(unsigned i=0;i<parameters.size();i++)invocation.arguments[i]=parameters[i];
    return UniformResourceAccess(callee,"attached",{},linkedLoad,{},true,invocation);
  };
  auto linked=UniformResourceAccess(caller,"caller",{{0,destination},{1,linkedInput}},linkedLoad,{},true,draw,resolver);
  assert(linked.validModule && linked.linkedCalls==1 && linked.bufferReads==1 && linked.bufferWrites==1 &&
      !linked.unresolvedCalls && !linked.unresolvedBuffers && linked.buffers.back().address.offset==12 &&
      linked.buffers.back().address.High()==220);
  // Nested argument-buffer members reuse parameter ordinals. Only the entry's
  // direct parameter list may identify a vertex builtin or root binding.
  const std::string scopedCaller=caller+R"AIR(
!air.vertex = !{!20}
!20 = !{void (i32, i32, i32 addrspace(1)*, i32 addrspace(2)*)* @caller, !22, !21}
!21 = !{!1, !2, !3, !4}
!22 = !{}
!99 = !{i32 0, !"air.buffer", !"air.location_index", i32 92, i32 1}
)AIR";
  auto scoped=UniformResourceAccess(scopedCaller,"caller",{{0,destination},{1,linkedInput}},linkedLoad,{},true,draw,resolver);
  assert(scoped.validModule && scoped.linkedCalls==1 && !scoped.unresolvedBuffers &&
      scoped.buffers.back().address.offset==12 && scoped.buffers.back().address.High()==220);
  // Converted entry records append shader metadata after the argument list.
  auto convertedCaller=scopedCaller;
  const auto direct=convertedCaller.find("@caller, !22, !21}");
  assert(direct!=std::string::npos);
  convertedCaller.replace(direct,std::string("@caller, !22, !21}").size(),"@caller, !22, !21, !99}");
  auto converted=UniformResourceAccess(convertedCaller,"caller",{{0,destination},{1,linkedInput}},linkedLoad,{},true,draw,resolver);
  assert(converted.validModule && converted.linkedCalls==1 && !converted.unresolvedBuffers &&
      converted.buffers.back().address.offset==12 && converted.buffers.back().address.High()==220);
  assert(!UniformResourceAccess(scopedCaller,"absent",{},linkedLoad).validModule);
  auto missingLinked=UniformResourceAccess(caller,"caller",{{0,destination},{1,linkedInput}},linkedLoad,{},true,draw);
  assert(missingLinked.unresolvedCalls==1 && !missingLinked.buffers.back().address.offsetKnown);
  draw.builtins["base_instance"]=Value::Number(9);
  auto underflow=UniformResourceAccess(caller,"caller",{{0,destination},{1,linkedInput}},linkedLoad,{},true,draw,resolver);
  assert(underflow.unresolvedBuffers || !underflow.buffers.back().address.offsetKnown);
  const std::string division=R"air(define void @division(i32 %n, i32 %d, i32 addrspace(1)* %p) {
  %q = udiv i32 %n, %d
  %r = urem i32 %n, %d
  %x = getelementptr i32, i32 addrspace(1)* %p, i32 %q
  %y = getelementptr i32, i32 addrspace(1)* %p, i32 %r
  store i32 7, i32 addrspace(1)* %x, align 4
  store i32 9, i32 addrspace(1)* %y, align 4
  ret void
}
)air";
  InvocationValues arithmetic;arithmetic.arguments={{0,Value::Range(0,63)},{1,Value::Number(4)},{2,destination}};
  auto divided=UniformResourceAccess(division,"division",{}, {},{},true,arithmetic);
  assert(divided.validModule && !divided.unresolvedBuffers && divided.buffers.size()==2 &&
      divided.buffers[0].address.High()==60 && divided.buffers[1].address.High()==12);
  arithmetic.arguments[1]=Value::Range(4,8);
  auto varyingDivisor=UniformResourceAccess(division,"division",{}, {},{},true,arithmetic);
  assert(!varyingDivisor.unresolvedBuffers && varyingDivisor.buffers[0].address.High()==60 &&
      varyingDivisor.buffers[1].address.High()==28);
  arithmetic.arguments[1]=Value::Range(0,8);
  auto maybeZero=UniformResourceAccess(division,"division",{}, {},{},true,arithmetic);
  assert(!maybeZero.buffers[0].address.offsetKnown && !maybeZero.buffers[1].address.offsetKnown);
  arithmetic.arguments[0]=Value::Number(UINT64_MAX);arithmetic.arguments[1]=Value::Number(2);
  auto wrapped=UniformResourceAccess(division,"division",{}, {},{},true,arithmetic);
  assert(wrapped.buffers[0].address.offset==4ULL*(UINT32_MAX/2) && wrapped.buffers[1].address.offset==4);
  arithmetic.arguments[1]=Value::Number(1ULL<<32);
  auto wrappedZero=UniformResourceAccess(division,"division",{}, {},{},true,arithmetic);
  assert(!wrappedZero.buffers[0].address.offsetKnown && !wrappedZero.buffers[1].address.offsetKnown);
  const std::string nativeGroups=R"air(define void @groups(i32 %local, <3 x i32> %group, i32 addrspace(1)* %p) {
  %z = extractelement <3 x i32> %group, i64 2
  %i = add i32 %z, %local
  %x = getelementptr i32, i32 addrspace(1)* %p, i32 %i
  store i32 1, i32 addrspace(1)* %x, align 4
  ret void
}
!air.kernel = !{!0}
!0 = !{void (i32, <3 x i32>, i32 addrspace(1)*)* @groups, !1, !2}
!1 = !{}
!2 = !{!3, !4, !5}
!3 = !{i32 0, !"air.thread_index_in_threadgroup", !"air.arg_name", !"local"}
!4 = !{i32 1, !"air.threadgroup_position_in_grid", !"air.arg_name", !"group"}
!5 = !{i32 2, !"air.buffer", !"air.location_index", i32 0, i32 1, !"air.write"}
)air";
  InvocationValues groupValues;Value group;group.kind=Value::Aggregate;
  group.fields={Value::Range(0,1),Value::Range(0,1),Value::Range(0,2)};
  groupValues.builtins["threadgroup_position_in_grid"]=group;
  groupValues.builtins["thread_index_in_threadgroup"]=Value::Range(0,7);
  auto grouped=UniformResourceAccess(nativeGroups,"groups",{{0,destination}},{},{128,128,128},true,groupValues);
  assert(grouped.validModule && !grouped.unresolvedBuffers && grouped.buffers[0].address.High()==36);
  auto absentGroups=UniformResourceAccess(nativeGroups,"groups",{{0,destination}},{},{128,128,128},true);
  assert(!absentGroups.buffers[0].address.offsetKnown);
  assert(absentGroups.buffers[0].address.object==destination.object &&
         absentGroups.buffers[0].address.kind==Value::Pointer && !absentGroups.unresolvedBuffers);
  auto missingIdentity=UniformResourceAccess(nativeGroups,"groups",{}, {},{128,128,128},true,groupValues);
  assert(missingIdentity.unresolvedBuffers && missingIdentity.buffers.empty());
  const std::string scalarStore=R"air(define void @scalar(i32 %v, i32 addrspace(1)* %p) {
  store i32 %v, i32 addrspace(1)* %p, align 4
  ret void
}
)air";
  InvocationValues scalarValues;scalarValues.arguments={{0,Value::Number(0)},{1,destination}};
  auto exact=UniformResourceAccess(scalarStore,"scalar",{},{},{},true,scalarValues);
  assert(exact.buffers.size()==1 && exact.buffers[0].definiteStore && exact.buffers[0].stored.offset==0);
  scalarValues.arguments[0]=Value::Range(0,1);
  auto varying=UniformResourceAccess(scalarStore,"scalar",{},{},{},true,scalarValues);
  assert(!varying.buffers[0].definiteStore);
  assert(varying.buffers[0].definiteWrite);
  scalarValues.arguments[0]=Value::Number(7);scalarValues.arguments[1]=destination;scalarValues.arguments[1].ranged=true;scalarValues.arguments[1].maximum=destination.offset+4;
  auto scatter=UniformResourceAccess(scalarStore,"scalar",{},{},{},true,scalarValues);
  assert(!scatter.buffers[0].definiteStore);
  assert(!scatter.buffers[0].definiteWrite);
  const std::string conditionalStore=R"air(define void @scalar(i1 %condition, i32 addrspace(1)* %p) {
  br i1 %condition, label %yes, label %end
yes:
  store i32 0, i32 addrspace(1)* %p, align 4
  br label %end
end:
  ret void
}
)air";
  InvocationValues conditionalValues;conditionalValues.arguments[1]=destination;
  auto skipped=UniformResourceAccess(conditionalStore,"scalar",{}, {},{},true,conditionalValues);
  assert(skipped.buffers.size()==1 && !skipped.buffers[0].definiteStore && skipped.conditionalAccesses);
  assert(!skipped.buffers[0].definiteWrite);
  assert(!exact.conditionalAccesses);
  const std::string registerBits=R"air(define void @register_bits(i32 %value, i32 addrspace(1)* %p) {
  %bits = call i32 @air.clz.i32(i32 %value, i1 false)
  %dest = getelementptr i32, i32 addrspace(1)* %p, i32 %bits
  store i32 %bits, i32 addrspace(1)* %dest, align 4
  ret void
}
declare i32 @air.clz.i32(i32, i1) nounwind
)air";
  InvocationValues registerValues;registerValues.arguments={{0,Value::Number(1)},{1,destination}};
  auto nativeBits=UniformResourceAccess(registerBits,"register_bits",{}, {},{},true,registerValues);
  assert(nativeBits.validModule && !nativeBits.unresolvedCalls && !nativeBits.unresolvedBuffers && nativeBits.buffers.size()==1);
  assert(nativeBits.buffers[0].address.object==destination.object && !nativeBits.buffers[0].address.offsetKnown);
  assert(!nativeBits.buffers[0].definiteStore && nativeBits.buffers[0].stored.kind==Value::Unknown);
  assert(!nativeBits.buffers[0].definiteWrite);
  auto unknownStoredBits=registerBits;
  unknownStoredBits.replace(unknownStoredBits.find("i32 addrspace(1)* %dest, align 4"),
      std::string("i32 addrspace(1)* %dest, align 4").size(),"i32 addrspace(1)* %p, align 4");
  const auto gpuValue=UniformResourceAccess(unknownStoredBits,"register_bits",{}, {},{},true,registerValues);
  assert(!gpuValue.unresolvedCalls && !gpuValue.unresolvedBuffers && gpuValue.buffers.size()==1 &&
      gpuValue.buffers[0].definiteWrite && !gpuValue.buffers[0].definiteStore &&
      gpuValue.buffers[0].stored.kind==Value::Unknown);
  for(const std::string prototype:{"declare i32 @air.clz.i32(i32, i1) nounwind","declare i16 @air.ctz.i16(i16, i1)","declare <4 x i32> @air.popcount.v4i32(<4 x i32>)","declare i64 @air.reverse_bits.i64(i64)"})
    assert(RegisterBitIntrinsicDeclaration(prototype));
  for(const std::string prototype:{"declare i32 @air.clz.i32(i32 addrspace(1)*)","declare i64 @air.clz.i32(i32, i1)","declare i32 @air.clz.i32(i32)","declare i32 @air.some_external.i32(i32)","declare i32 @air.clz.i32(i32, i1, i32)"})
    assert(!RegisterBitIntrinsicDeclaration(prototype));
  std::string unregisteredBitCall=registerBits;
  for(size_t i=0;(i=unregisteredBitCall.find("air.clz.i32",i))!=std::string::npos;i+=21)
    unregisteredBitCall.replace(i,11,"air.some_external.i32");
  assert(UniformResourceAccess(unregisteredBitCall,"register_bits",{}, {},{},true,registerValues).unresolvedCalls==1);
  std::cout << "PASS Native register effects without numerical result/address proof; external/pointer/mismatched signatures remain unknown\n";
  const std::string localAtomicModule=R"(
@allocator = internal addrspace(3) global i32 undef
define void @local_allocator(i32 addrspace(1)* %output) {
  %old = atomicrmw add i32 addrspace(3)* @allocator, i32 1 seq_cst, align 4
  %offset = zext i32 %old to i64
  %target = getelementptr i32, i32 addrspace(1)* %output, i64 %offset
  store i32 %old, i32 addrspace(1)* %target, align 4
  ret void
}
)";
  InvocationValues localAtomicValues;localAtomicValues.arguments[0]=destination;
  auto localAtomicReport=UniformResourceAccess(localAtomicModule,"local_allocator",{}, {},{},true,localAtomicValues);
  assert(localAtomicReport.validModule && localAtomicReport.unresolvedBuffers==0 && localAtomicReport.unresolvedCalls==0);
  assert(localAtomicReport.bufferReads==0 && localAtomicReport.bufferWrites==1 && localAtomicReport.buffers.size()==1);
  assert(!localAtomicReport.buffers[0].address.offsetKnown && localAtomicReport.buffers[0].stored.kind==Value::Unknown);
  std::string localIntrinsicModule=localAtomicModule;
  const auto localInstruction=localIntrinsicModule.find("  %old = atomicrmw");
  const auto localInstructionEnd=localIntrinsicModule.find('\n',localInstruction);
  localIntrinsicModule.replace(localInstruction,localInstructionEnd-localInstruction,
      "  %old = call i32 @air.atomic.local.add.u.i32(i32 addrspace(3)* getelementptr inbounds (i32, i32 addrspace(3)* @allocator, i64 0), i32 1, i32 0, i32 1, i1 true)");
  const auto localIntrinsicReport=UniformResourceAccess(localIntrinsicModule,"local_allocator",{}, {},{},true,localAtomicValues);
  assert(localIntrinsicReport.unresolvedBuffers==0 && localIntrinsicReport.unresolvedCalls==0 && localIntrinsicReport.bufferWrites==1);
  assert(!localIntrinsicReport.buffers[0].address.offsetKnown);
  std::string unresolvedDeviceAtomic=localAtomicModule;
  const auto atomicSpace=unresolvedDeviceAtomic.find("atomicrmw add i32 addrspace(3)");
  unresolvedDeviceAtomic.replace(atomicSpace,29,"atomicrmw add i32 addrspace(1)");
  assert(UniformResourceAccess(unresolvedDeviceAtomic,"local_allocator",{}, {},{},true,localAtomicValues).unresolvedBuffers>0);
  std::cout << "PASS Native local atomics: no external resource effect or CPU result; unresolved device atomics stay rejected\n";
  const std::string queryModule=R"(
%struct._intersection_query_t = type opaque
%struct._instance_acceleration_structure_t = type opaque
define void @native_query(%struct._instance_acceleration_structure_t addrspace(1)* %as, i32 addrspace(2)* %meta, i32 addrspace(1)* %output) {
  %query = call %struct._intersection_query_t* @air.allocate_intersection_query.instancing.triangle_data()
  %local = alloca i32 addrspace(2)*, align 8
  store i32 addrspace(2)* %meta, i32 addrspace(2)** %local, align 8
  call void @air.reset_intersection_query.instancing.triangle_data(%struct._intersection_query_t* %query, %struct._instance_acceleration_structure_t addrspace(1)* %as)
  %next = call i1 @air.next_intersection_query.instancing.triangle_data(%struct._intersection_query_t* %query)
  %index = call i32 @air.get_committed_instance_id_intersection_query.instancing.triangle_data(%struct._intersection_query_t* nocapture readonly %query)
  %target = getelementptr i32, i32 addrspace(1)* %output, i32 %index
  store i32 %index, i32 addrspace(1)* %target, align 4
  call void @air.deallocate_intersection_query.instancing.triangle_data(%struct._intersection_query_t* %query)
  ret void
}
)";
  InvocationValues queryValues;Value queryAS;queryAS.kind=Value::AccelerationStructure;queryAS.object=72;
  queryValues.arguments={{0,queryAS},{1,destination},{2,destination}};
  const auto queryReport=UniformResourceAccess(queryModule,"native_query",{}, {},{},true,queryValues);
  assert(queryReport.validModule && queryReport.queryResets==1 && queryReport.unresolvedStructures==0);
  assert(queryReport.unresolvedCalls==0 && queryReport.unresolvedBuffers==0 && queryReport.bufferWrites==1);
  assert(queryReport.accesses.size()==1 && queryReport.accesses[0].object==72);
  assert(queryReport.buffers.size()==1 && !queryReport.buffers[0].address.offsetKnown);
  assert(queryReport.buffers[0].stored.kind==Value::Unknown);
  queryValues.arguments[0]={};
  assert(UniformResourceAccess(queryModule,"native_query",{}, {},{},true,queryValues).unresolvedStructures==1);
  std::string unknownQuery=queryModule;
  unknownQuery.replace(unknownQuery.find("%query = call"),std::string("%query = call").size(),"%other = call");
  assert(UniformResourceAccess(unknownQuery,"native_query",{}, {},{},true,queryValues).unresolvedCalls>=3);
  std::string deviceStore=queryModule;
  deviceStore.replace(deviceStore.find("i32 addrspace(2)** %local"),std::string("i32 addrspace(2)** %local").size(),"i32 addrspace(2)* addrspace(1)* %local");
  assert(UniformResourceAccess(deviceStore,"native_query",{}, {},{},true,queryValues).unresolvedBuffers>0);
  std::cout << "PASS Native query private effects, opaque results, AS/missing-handle/device-store controls\n";
  queryValues.arguments[0]=queryAS;
  const std::string queryGetter="  %index = call i32 @air.get_committed_instance_id_intersection_query.instancing.triangle_data(%struct._intersection_query_t* nocapture readonly %query)";
  auto queryOperationReport=[&](const std::string &type,const std::string &method,
                                const std::string &arguments) {
    std::string module=queryModule;
    module.replace(module.find(queryGetter),queryGetter.size(),
        "  %index = call "+type+" @air."+method+
        "_intersection_query.instancing.triangle_data("+arguments+")");
    return UniformResourceAccess(module,"native_query",{}, {},{},true,queryValues);
  };
  const std::string privateQuery="%struct._intersection_query_t* nocapture readonly %query";
  for(const std::string phase : {"candidate","committed"})
  {
    for(const std::string field : {"intersection_type","geometry_id","primitive_id","instance_id","user_instance_id"})
    {
      const auto r=queryOperationReport("i32","get_"+phase+"_"+field,privateQuery);
      assert(r.unresolvedCalls==0 && r.unresolvedStructures==0 && r.bufferWrites==1);
      assert(!r.buffers[0].address.offsetKnown && r.buffers[0].stored.kind==Value::Unknown);
    }
    for(const std::string field : {"origin","direction"})
      assert(queryOperationReport("<3 x float>","get_"+phase+"_ray_"+field,privateQuery).unresolvedCalls==0);
    for(const std::string field : {"object_to_world","world_to_object"})
      assert(queryOperationReport("{ <3 x float>, <3 x float>, <3 x float>, <3 x float> }",
          "get_"+phase+"_"+field+"_transform",privateQuery).unresolvedCalls==0);
    assert(queryOperationReport("<2 x float>","get_"+phase+"_triangle_barycentric_coord",privateQuery).unresolvedCalls==0);
    assert(queryOperationReport("i1","is_"+phase+"_triangle_front_facing",privateQuery).unresolvedCalls==0);
  }
  for(const std::string method : {"get_candidate_triangle_distance","get_committed_distance","get_ray_min_distance"})
    assert(queryOperationReport("float",method,privateQuery).unresolvedCalls==0);
  for(const std::string field : {"origin","direction"})
    assert(queryOperationReport("<3 x float>","get_world_space_ray_"+field,privateQuery).unresolvedCalls==0);
  assert(queryOperationReport("void","abort",privateQuery).unresolvedCalls==0);
  assert(queryOperationReport("fast float","get_committed_distance",privateQuery).unresolvedCalls==0);
  assert(queryOperationReport("reassoc nsz arcp contract afn <3 x float>","get_committed_ray_origin",privateQuery).unresolvedCalls==0);
  assert(queryOperationReport("i32","get_committed_invented_field",privateQuery).unresolvedCalls>0);
  assert(queryOperationReport("i32 addrspace(1)*","get_committed_primitive_data",privateQuery).unresolvedCalls>0);
  assert(queryOperationReport("i32 addrspace(1)*","get_committed_primitive_id",privateQuery).unresolvedCalls>0);
  assert(queryOperationReport("i32","get_committed_primitive_id",privateQuery+", i32 addrspace(1)* %output").unresolvedCalls>0);
  assert(queryOperationReport("i32","get_committed_primitive_id",privateQuery+", i32 0").unresolvedCalls>0);
  assert(queryOperationReport("i32","get_committed_primitive_id","%struct._intersection_query_t* %unknown").unresolvedCalls>0);
  std::cout << "PASS Native triangle query value ABI families; pointer returns, extra operands and missing handles remain unknown\n";
  const std::string namespaceModule=R"(
%Row = type { i64, i64, i64 }
define void @native_namespace(%Row addrspace(2)* %heap, i32 %index) {
  %wide = zext i32 %index to i64
  %field = getelementptr %Row, %Row addrspace(2)* %heap, i64 %wide, i32 0
  %bits = load i64, i64 addrspace(2)* %field, align 8
  %buffer = inttoptr i64 %bits to i32 addrspace(1)*
  %element = getelementptr i32, i32 addrspace(1)* %buffer, i64 %wide
  %read = load i32, i32 addrspace(1)* %element, align 4
  ret void
}
)";
  InvocationValues namespaceInputs;Value namespaceTable;namespaceTable.kind=Value::Pointer;
  namespaceTable.object=88;namespaceTable.metadata=true;namespaceInputs.arguments[0]=namespaceTable;
  auto namespaceLoad=[](const Value &address,unsigned bytes,Value::Kind kind) {
    Value result;
    if(address.object==88 && address.metadata && !address.offsetKnown &&
       address.namespaceBase==0 && address.namespaceStride==24 && !address.namespaceField &&
       bytes==8 && kind==Value::Pointer)
    {result.kind=Value::DescriptorPointer;result.object=88;result.namespaceStride=24;result.offsetKnown=false;}
    return result;
  };
  auto namespaceReport=UniformResourceAccess(namespaceModule,"native_namespace",{},namespaceLoad,{},true,namespaceInputs);
  assert(namespaceReport.unresolvedBuffers==0 && namespaceReport.buffers.size()==1);
  assert(namespaceReport.buffers[0].address.kind==Value::DescriptorPointer && !namespaceReport.buffers[0].address.offsetKnown);
  // A Native invocation range preserves a row window; it does not choose
  // one numerical row. Different windows keep independent restoration keys.
  InvocationValues boundedInputs=namespaceInputs;boundedInputs.arguments[1]=Value::Range(1,3);
  auto boundedLoad=[](const Value &address,unsigned bytes,Value::Kind kind) {
    Value result;
    if(address.metadata && address.object==88 && address.namespaceStride==24 &&
       !address.namespaceField && address.namespaceBase==24 && address.namespaceEnd==96 &&
       bytes==8 && kind==Value::Pointer)
    {result=address;result.kind=Value::DescriptorPointer;result.offsetKnown=false;result.ranged=false;result.metadata=false;}
    return result;
  };
  const auto boundedReport=UniformResourceAccess(namespaceModule,"native_namespace",{},boundedLoad,{},true,boundedInputs);
  assert(!boundedReport.unresolvedBuffers && boundedReport.buffers.size()==1);
  assert(boundedReport.buffers[0].address.namespaceBase==24 && boundedReport.buffers[0].address.namespaceEnd==96);
  std::string volatileNarrow=namespaceModule;
  volatileNarrow.replace(volatileNarrow.find("  %read = load i32, i32 addrspace(1)* %element, align 4"),
      std::string("  %read = load i32, i32 addrspace(1)* %element, align 4").size(),
      "  %narrow = bitcast i32 addrspace(1)* %element to i16 addrspace(1)*\n  %read = load volatile i16, i16 addrspace(1)* %narrow, align 2");
  auto volatileReport=UniformResourceAccess(volatileNarrow,"native_namespace",{},boundedLoad,{},true,boundedInputs);
  assert(!volatileReport.unresolvedBuffers && volatileReport.buffers.size()==1 && volatileReport.buffers[0].bytes==2);
  Value secondWindow=boundedReport.buffers[0].address;secondWindow.namespaceEnd=120;
  assert(MergePointerProvenance(boundedReport.buffers[0].address,secondWindow).fields.size()==2);
  boundedInputs.arguments[1]=Value::Range(1,4);
  assert(UniformResourceAccess(namespaceModule,"native_namespace",{},boundedLoad,{},true,boundedInputs).unresolvedBuffers>0);
  boundedInputs.arguments[1]=Value::Range(UINT64_MAX/24,UINT64_MAX/24+1);
  assert(UniformResourceAccess(namespaceModule,"native_namespace",{},boundedLoad,{},true,boundedInputs).unresolvedBuffers>0);
  boundedInputs.arguments[1]=Value::Range(1,3);
  std::string boundedWrongField=namespaceModule;
  boundedWrongField.replace(boundedWrongField.find("i32 0\n"),6,"i32 1\n");
  assert(UniformResourceAccess(boundedWrongField,"native_namespace",{},boundedLoad,{},true,boundedInputs).unresolvedBuffers>0);
  std::string boundedWrongStride=namespaceModule;
  boundedWrongStride.replace(boundedWrongStride.find("{ i64, i64, i64 }"),17,"{ i64, i64 }");
  assert(UniformResourceAccess(boundedWrongStride,"native_namespace",{},boundedLoad,{},true,boundedInputs).unresolvedBuffers>0);
  std::cout << "PASS typed namespace row windows: invocation bounds, separate keys; overflow, missing field/range/stride rejected\n";
  std::string byteNamespace=namespaceModule;
  const auto namespaceAt=byteNamespace.find("  %wide ="),namespaceEnd=byteNamespace.find("  %bits =");
  byteNamespace.replace(namespaceAt,namespaceEnd-namespaceAt,
      "  %step = mul i32 %index, 24\n  %wide = sext i32 %step to i64\n  %base = bitcast %Row addrspace(2)* %heap to i8 addrspace(2)*\n  %row = getelementptr i8, i8 addrspace(2)* %base, i64 %wide\n  %field = bitcast i8 addrspace(2)* %row to i64 addrspace(2)*\n");
  auto byteNamespaceReport=UniformResourceAccess(byteNamespace,"native_namespace",{},namespaceLoad,{},true,namespaceInputs);
  assert(byteNamespaceReport.unresolvedBuffers==0 && byteNamespaceReport.buffers.size()==1);
  assert(byteNamespaceReport.buffers[0].address.kind==Value::DescriptorPointer);
  std::string wrongNamespaceField=namespaceModule;
  wrongNamespaceField.replace(wrongNamespaceField.find("i32 0\n"),6,"i32 1\n");
  assert(UniformResourceAccess(wrongNamespaceField,"native_namespace",{},namespaceLoad,{},true,namespaceInputs).unresolvedBuffers>0);
  std::string wrongNamespaceStride=byteNamespace;wrongNamespaceStride.replace(wrongNamespaceStride.find("%index, 24"),10,"%index, 16");
  assert(UniformResourceAccess(wrongNamespaceStride,"native_namespace",{},namespaceLoad,{},true,namespaceInputs).unresolvedBuffers>0);
  assert(UniformResourceAccess(namespaceModule,"native_namespace",{},[](const Value &,unsigned,Value::Kind){return Value();},{},true,namespaceInputs).unresolvedBuffers>0);
  namespaceInputs.arguments[0].metadata=false;
  assert(UniformResourceAccess(namespaceModule,"native_namespace",{},namespaceLoad,{},true,namespaceInputs).unresolvedBuffers>0);
  std::cout << "PASS typed-table ABI row projection without numerical index; missing namespace, wrong field/stride remain unknown\n";
  const std::string privatePointerModule=R"(
define void @private_pointer(i32 addrspace(1)* %p, i32 addrspace(1)* %q, i1 %condition) {
  %slot = alloca i32 addrspace(1)*, align 8
  store volatile i32 addrspace(1)* %p, i32 addrspace(1)** %slot, align 8
  %origin = load volatile i32 addrspace(1)*, i32 addrspace(1)** %slot, align 8
  store i32 9, i32 addrspace(1)* %origin, align 4
  ret void
}
)";
  InvocationValues spillValues;Value alternative=destination;alternative.object=destination.object+1;
  spillValues.arguments={{0,destination},{1,alternative},{2,{}}};
  auto checkSpill=[&](const std::string &module) {
    return UniformResourceAccess(module,"private_pointer",{}, {},{},true,spillValues);
  };
  auto spillReport=checkSpill(privatePointerModule);
  assert(spillReport.validModule && !spillReport.unresolvedBuffers && spillReport.buffers.size()==1);
  assert(spillReport.buffers[0].address.object==destination.object && spillReport.buffers[0].address.offset==destination.offset);
  std::string lifeSpill=privatePointerModule;
  const auto lifeStore=lifeSpill.find("  store volatile");
  lifeSpill.insert(lifeStore,"  %life = bitcast i32 addrspace(1)** %slot to i8*\n  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %life)\n");
  lifeSpill.insert(lifeSpill.find("  ret void"),"  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %life)\n");
  assert(!checkSpill(lifeSpill).unresolvedBuffers);
  std::string deadSpill=lifeSpill;
  deadSpill.insert(deadSpill.find("  %origin"),"  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %life)\n");
  assert(checkSpill(deadSpill).unresolvedBuffers>0);
  std::string escapedLife=lifeSpill;
  escapedLife.insert(escapedLife.find("  %origin"),"  call void @external_writer(i8* %life)\n");
  assert(checkSpill(escapedLife).unresolvedBuffers>0);
  const auto initialSpill=privatePointerModule.find("  store volatile");
  const auto spillRead=privatePointerModule.find("  %origin");
  std::string branchedSpill=privatePointerModule;
  branchedSpill.replace(initialSpill,spillRead-initialSpill,R"(  br i1 %condition, label %left, label %right
left:
  store i32 addrspace(1)* %p, i32 addrspace(1)** %slot, align 8
  br label %joined
right:
  store i32 addrspace(1)* %q, i32 addrspace(1)** %slot, align 8
  br label %joined
joined:
)");
  spillReport=checkSpill(branchedSpill);
  assert(!spillReport.unresolvedBuffers && spillReport.buffers.size()==2);
  assert(spillReport.buffers[0].address.object!=spillReport.buffers[1].address.object);
  std::string uninitialisedSpill=branchedSpill;
  const auto otherStore=uninitialisedSpill.find("  store i32 addrspace(1)* %q");
  uninitialisedSpill.erase(otherStore,uninitialisedSpill.find('\n',otherStore)-otherStore+1);
  assert(checkSpill(uninitialisedSpill).unresolvedBuffers>0);
  spillValues.arguments[1]={};
  assert(checkSpill(branchedSpill).unresolvedBuffers>0);
  spillValues.arguments[1]=alternative;
  std::string futureSpill=privatePointerModule;
  const auto init=futureSpill.substr(initialSpill,spillRead-initialSpill);
  futureSpill.erase(initialSpill,spillRead-initialSpill);
  futureSpill.insert(futureSpill.find("  store i32 9"),init);
  assert(checkSpill(futureSpill).unresolvedBuffers>0);
  std::string escapedSpill=privatePointerModule;
  escapedSpill.insert(spillRead,"  call void @external_pointer_writer(i32 addrspace(1)** %slot)\n");
  const auto escapedReport=checkSpill(escapedSpill);
  assert(escapedReport.unresolvedBuffers>0 && escapedReport.unresolvedCalls>0);
  std::string loopSpill=privatePointerModule;
  loopSpill.insert(spillRead,"  br label %loop\nloop:\n");
  loopSpill.replace(loopSpill.find("  ret void"),std::string("  ret void").size(),
      "  store i32 addrspace(1)* %q, i32 addrspace(1)** %slot, align 8\n  br i1 %condition, label %loop, label %exit\nexit:\n  ret void");
  spillReport=checkSpill(loopSpill);
  assert(!spillReport.unresolvedBuffers && spillReport.buffers.size()==2);
  std::cout << "PASS private pointer provenance: reaching branch/loop origins; uninitialised/future/unknown/escape remain rejected\n";
  std::cout << "PASS reads/writes/atomics, uniform index changes, unknown dynamic index, missing root/function, ambiguous module\n";
}
