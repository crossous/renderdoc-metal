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
  std::string atomic = writes;
  atomic.replace(atomic.find("@air.write_texture_2d.rtz.v4f32"),
                 std::string("@air.write_texture_2d.rtz.v4f32").size(), "@air.atomic_fetch_add_explicit_2d");
  accesses = UniformTextureAccess(atomic, "test", {{0, heap}, {2, root}}, load);
  assert(accesses.size() == 2 && accesses[0].write && !accesses[1].write);
  assert(UniformTextureAccess(writes, "test", {{0, heap}}, load).empty());
  assert(UniformTextureAccess(text, "missing", {{0, heap}, {2, root}}, load).empty());
  assert(UniformTextureAccess(text, "test", {{0, heap}}, load).empty());
  assert(UniformTextureAccess(text + "\ndefine void @helper() {\n}\n", "test", {{0, heap}, {2, root}}, load).empty());
  // Each compiled Metal function has an independent module and metadata namespace.
  const std::string moduleA = "source_filename = \"one\"\n" + text;
  std::string other = text;
  other.replace(other.find("@test("), 6, "@mesh(");
  const std::string moduleB = "source_filename = \"two\"\n" + other;
  auto isolated = UniformTextureAccess(moduleA + moduleB, "mesh", {{0, heap}, {2, root}}, load);
  assert(isolated.size() == 1 && isolated[0].offset == index * 24 + 8);
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
  std::cout << "PASS reads/writes/atomics, uniform index changes, unknown dynamic index, missing root/function, ambiguous module\n";
}
