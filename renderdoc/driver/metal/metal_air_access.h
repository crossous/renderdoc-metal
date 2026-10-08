// SPDX-License-Identifier: MIT
// Bounded uniform-address evaluation of AIR. Unknown expressions stay unknown;
// this is static descriptor access, not per-invocation GPU feedback.
#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace MetalAIR
{
// Cache only lexical matches against process-lifetime immutable grammar patterns.
// Input storage lives with the cache, so copied smatch iterators remain valid.
// No bindings, values, loader observations or restoration verdicts are cached.
inline bool ImmutableSyntaxMatch(const std::string &text, std::smatch &result,
                                const std::regex &pattern, bool search = false)
{
  struct Entry { std::smatch match; bool accepted = false; };
  using Key = std::pair<std::pair<const std::regex *, bool>, std::string>;
  struct Cache { std::map<Key, Entry> entries; size_t bytes = 0; };
  static thread_local Cache cache;
  Key key{{&pattern, search}, text};
  auto found = cache.entries.find(key);
  if(found != cache.entries.end())
  { result = found->second.match; return found->second.accepted; }
  // Optional acceleration has a fixed host budget, independent of capture or
  // shader identity. Exhaustion falls back to the original matcher.
  constexpr size_t budget = 64 * 1024 * 1024;
  const size_t reservation = sizeof(Key) + sizeof(Entry) + 128 + text.size() +
      (pattern.mark_count() + 1) * sizeof(std::sub_match<std::string::const_iterator>);
  if(cache.entries.size() >= 32768 || reservation > budget - cache.bytes)
    return search ? std::regex_search(text, result, pattern) : std::regex_match(text, result, pattern);
  const bool accepted = search ? std::regex_search(text, result, pattern) :
                                  std::regex_match(text, result, pattern);
  // Negative alternatives dominate grammar classification. Keep capacity for
  // actual syntax, rather than filling it before later modules are visited.
  if(!accepted) return false;
  auto inserted = cache.entries.emplace(std::move(key), Entry()).first;
  auto &entry = inserted->second;
  const auto &stored = inserted->first.second;
  try
  {
    entry.accepted = search ? std::regex_search(stored, entry.match, pattern) :
                              std::regex_match(stored, entry.match, pattern);
  }
  catch(...)
  { cache.entries.erase(inserted); throw; }
  cache.bytes += reservation;
  result = entry.match;
  return entry.accepted;
}
inline bool ImmutableSyntaxMatch(const std::string &text, const std::regex &pattern,
                                bool search = false)
{
  std::smatch ignored;
  return ImmutableSyntaxMatch(text, ignored, pattern, search);
}

// Native register-only API intrinsics have the same effects even when older
// AIR omits readnone. Match their complete prototypes, not a shader name or
// an arbitrary pointer-free external call. This does not evaluate any result.
inline bool RegisterBitIntrinsicDeclaration(const std::string &line)
{
  if(line.find("@air.clz.")==std::string::npos && line.find("@air.ctz.")==std::string::npos &&
     line.find("@air.popcount.")==std::string::npos && line.find("@air.reverse_bits.")==std::string::npos)
    return false;
  static const std::vector<std::string> prototypes=[] {
    std::vector<std::string> result;
    for(unsigned bits : {8U,16U,32U,64U})
      for(unsigned lanes : {0U,2U,3U,4U})
        for(const std::string operation : {"clz","ctz","popcount","reverse_bits"})
        {
          const std::string scalar="i"+std::to_string(bits);
          const std::string type=lanes?"<"+std::to_string(lanes)+" x "+scalar+">":scalar;
          const std::string suffix=lanes?"v"+std::to_string(lanes)+scalar:scalar;
          result.push_back("declare "+type+" @air."+operation+"."+suffix+"("+type+
              ((operation=="clz" || operation=="ctz")?", i1":"")+")");
        }
    return result;
  }();
  for(const auto &prototype:prototypes)
    if(line.compare(0,prototype.size(),prototype)==0 &&
       (line.size()==prototype.size() || line[prototype.size()]==' '))return true;
  return false;
}
struct Value
{
  enum Kind { Unknown, Integer, Pointer, Texture, AccelerationStructure, Sampler, Buffer, ThreadGrid, Aggregate, PointerSet, LiteralPointer, NullBasedPointer, NativeQuery, DescriptorPointer, ShaderConstant } kind = Unknown;
  // Complete pointer-free numeric storage embedded in the original module.
  // Native bytecode owns it; this is not an external object/address or a CPU
  // value proof. Loads remain numerically unknown, including GPU dynamic indices.
  // NullBasedPointer records shader null-origin arithmetic, never a restored
  // resource or a captured address field. Only a set with independently
  // recovered source objects can qualify; a null-only access remains invalid.
  uint64_t object = 0, offset = 0;
  // The descriptor that supplied a pointer survives dereferencing the public
  // AS Header. It is distinct from the referenced header/AS object.
  uint64_t descriptorObject = 0, descriptorOffset = 0;
  int slot = -1;
  bool write = false;
  bool read = false;
  // A texture fence references a writable identity but neither reads pixels
  // nor defines them. Native execution supplies its ordering semantics.
  bool ordering = false;
  // Texture intrinsic numeric family, independent of pointer/heap provenance.
  char numeric = 0;
  bool metadata = false, offsetKnown = true, ranged = false, resourceOnly = false;
  uint64_t maximum = 0;
  // ABI address projection only: no GPU index/range or numerical value proof.
  // A symbolic row offset retains its element stride and constant field. The
  // caller independently restores the current typed namespace. A nonzero
  // namespaceEnd is an exclusive row boundary from a proved API invocation
  // range, not a selected GPU row or shader result. Zero means unbounded.
  uint64_t layoutStride=0, namespaceBase=0, namespaceStride=0, namespaceField=0, namespaceEnd=0;
  std::vector<Value> fields;
  // Bytecode alternatives survive numerical selection for independent
  // relocation checks; they never become resource identities or GPU facts.
  std::vector<Value> literalOrigins;
  uint64_t High() const { return ranged ? maximum : offset; }
  static Value Number(uint64_t n) { Value v; v.kind = Integer; v.offset = n; return v; }
  static Value Range(uint64_t low, uint64_t high)
  { Value v=Number(low);v.maximum=high;v.ranged=low!=high;return v; }
};
// This is pointer provenance, not numerical predicate evaluation. Every
// alternative remains visible to restoration, even when the optional display
// cannot determine which branch executes. Unknown is never silently omitted.
inline Value MergePointerProvenance(const Value &a, const Value &b)
{
  Value result;result.kind=Value::PointerSet;
  auto add=[&](const Value &v) {
    const auto &alternatives=v.kind==Value::PointerSet?v.fields:std::vector<Value>{v};
    for(const auto &candidate:alternatives)
    {
      bool duplicate=false;
      for(const auto &old:result.fields)
        duplicate |= old.kind==candidate.kind && old.object==candidate.object &&
            old.slot==candidate.slot && old.offset==candidate.offset && old.High()==candidate.High() &&
            old.offsetKnown==candidate.offsetKnown && old.metadata==candidate.metadata &&
            old.descriptorObject==candidate.descriptorObject && old.descriptorOffset==candidate.descriptorOffset &&
            old.namespaceBase==candidate.namespaceBase && old.namespaceStride==candidate.namespaceStride &&
            old.namespaceField==candidate.namespaceField && old.namespaceEnd==candidate.namespaceEnd &&
            old.literalOrigins.empty() && candidate.literalOrigins.empty();
      if(!duplicate)result.fields.push_back(candidate);
    }
  };
  add(a);add(b);
  if(result.fields.size()>64)return {};
  return result;
}
// Private slots are Native per-invocation storage, not captured allocations.
// Track only non-escaping pointer slots and all their reaching definitions.
// No branch predicate, scalar value, or shader result is evaluated here. An
// uninitialised path or unknown writer remains an unknown pointer origin.
inline std::map<unsigned,std::vector<std::string>> PrivatePointerSpillOrigins(
    const std::vector<std::string> &body)
{
  const std::string symbol=R"((%[-a-zA-Z$._0-9]+))";
  static const std::regex allocation("^  "+symbol+" = alloca ([^,]+), align [0-9]+.*$");
  static const std::regex store("^  store (?:volatile )?([^,]+), .* "+symbol+", align [0-9]+.*$");
  static const std::regex load("^  "+symbol+" = load (?:volatile )?[^,]+, .* "+symbol+", align [0-9]+.*$");
  static const std::regex label(R"(^([-a-zA-Z$._0-9]+):.*$)");
  static const std::regex target(R"(label %([-a-zA-Z$._0-9]+))");
  static const std::regex token(R"(%[-a-zA-Z$._0-9]+)");
  static const std::regex lifetimeAlias("^  "+symbol+" = bitcast .* "+symbol+" to i8\\*$");
  static const std::regex lifetime("^.*call void @llvm\\.lifetime\\.(start|end)\\.p0i8\\(i64 (?:8|-1), i8\\* (?:nonnull |nocapture )*"+symbol+"\\).*$");
  std::map<std::string,unsigned> allocations;
  for(unsigned i=0;i<body.size();i++)
  {
    std::smatch m;
    if(ImmutableSyntaxMatch(body[i],m,allocation) && !m[2].str().empty() && m[2].str().back()=='*')
      allocations[m[1]]=i;
  }
  if(allocations.empty() || allocations.size()>128)return {};
  std::vector<std::vector<unsigned>> instructions(1),successors(1),predecessors(1);
  std::map<std::string,unsigned> labels;
  std::vector<unsigned> blocks(body.size());
  unsigned block=0;
  for(unsigned i=0;i<body.size();i++)
  {
    std::smatch m;
    if(ImmutableSyntaxMatch(body[i],m,label))
    {
      // LLVM may name the first block or omit its label entirely.
      if(!instructions[block].empty())
      {block=unsigned(instructions.size());instructions.emplace_back();successors.emplace_back();predecessors.emplace_back();}
      if(!labels.emplace(m[1],block).second)return {};
    }
    else if(body[i].find_first_not_of(' ')!=std::string::npos)
    {blocks[i]=block;instructions[block].push_back(i);}
  }
  if(instructions.size()>256)return {}; // shared CPU provenance work budget
  for(unsigned b=0;b<instructions.size();b++)
    for(unsigned i:instructions[b])
    {
      const auto &line=body[i];
      if(line.find("  indirectbr ")==0 || line.find("  invoke ")==0 || line.find("callbr ")!=std::string::npos)return {};
      for(std::sregex_iterator j(line.begin(),line.end(),target),end;j!=end;++j)
      {
        const auto next=labels.find((*j)[1]);if(next==labels.end())return {};
        successors[b].push_back(next->second);predecessors[next->second].push_back(b);
      }
    }
  std::set<unsigned> reachable={0};std::vector<unsigned> queue={0};
  for(size_t i=0;i<queue.size();i++)for(unsigned next:successors[queue[i]])
    if(reachable.insert(next).second)queue.push_back(next);
  std::map<unsigned,std::vector<std::string>> result;
  for(const auto &slot:allocations)
  {
    bool escaped=false;
    std::map<unsigned,std::string> writes;
    std::set<unsigned> reads,lifetimeChanges,aliasInstructions;
    std::set<std::string> aliases={slot.first};
    // A compiler-generated byte-pointer cast used only by LLVM lifetime
    // markers cannot expose the slot to an external writer. Any other use of
    // that alias still escapes; start/end invalidate the stored origin.
    for(unsigned i=0;i<body.size();i++)
    {
      std::smatch m;
      if(ImmutableSyntaxMatch(body[i],m,lifetimeAlias) && aliases.count(m[2]))
      {aliases.insert(m[1]);aliasInstructions.insert(i);}
    }
    for(unsigned i=0;i<body.size();i++)
    {
      unsigned uses=0;
      for(std::sregex_iterator j(body[i].begin(),body[i].end(),token),end;j!=end;++j)
        uses+=aliases.count(j->str())!=0;
      if(!uses || i==slot.second || aliasInstructions.count(i))continue;
      std::smatch m;
      if(uses==1 && ImmutableSyntaxMatch(body[i],m,lifetime) && aliases.count(m[2]))
        lifetimeChanges.insert(i);
      else if(uses==1 && ImmutableSyntaxMatch(body[i],m,store) && m[2]==slot.first)
      {
        // Every writer participates, including unsupported/numerical values;
        // the value loader will keep those origins unknown.
        writes[i]=m[1];
      }
      else if(uses==1 && ImmutableSyntaxMatch(body[i],m,load) && m[2]==slot.first)
        reads.insert(i);
      else escaped=true; // alias/call/lifetime escape requires its own proof
    }
    if(escaped || reads.empty())continue;
    const unsigned uninitialised=unsigned(body.size());
    std::vector<std::set<unsigned>> incoming(instructions.size()),outgoing(instructions.size());
    bool changed=true;unsigned iterations=0;
    while(changed && iterations++<=instructions.size()+1)
    {
      changed=false;
      for(unsigned b:reachable)
      {
        std::set<unsigned> state;
        if(b==0)state.insert(uninitialised);
        for(unsigned prior:predecessors[b])if(reachable.count(prior))
          state.insert(outgoing[prior].begin(),outgoing[prior].end());
        if(incoming[b]!=state){incoming[b]=state;changed=true;}
        for(unsigned i:instructions[b])
        {
          if(i==slot.second || lifetimeChanges.count(i))state={uninitialised};
          if(writes.count(i))state={i};
        }
        if(outgoing[b]!=state){outgoing[b]=state;changed=true;}
      }
    }
    if(changed)continue;
    for(unsigned b:reachable)
    {
      auto state=incoming[b];
      for(unsigned i:instructions[b])
      {
        if(i==slot.second || lifetimeChanges.count(i))state={uninitialised};
        if(writes.count(i))state={i};
        if(reads.count(i) && !state.empty() && !state.count(uninitialised))
          for(unsigned writer:state)result[i].push_back(writes.at(writer));
      }
    }
  }
  return result;
}
using Loader = std::function<Value(const Value &, unsigned, Value::Kind)>;
struct UniformAccessReport
{
  bool validModule = false;
  // Static accesses across branches are candidates, not an assertion that
  // every invocation dereferences every numerical range.
  bool conditionalAccesses = false;
  // Literal pointer constants belong to shader bytecode, not a GPU-produced
  // data field. Known captured GPU VAs among them need independent relocation.
  std::vector<uint64_t> literalPointers;
  // Exact bytecode-derived memory spans also need captured-VA relocation
  // checks. Keep them independent of optional predicate/numerical display.
  std::vector<std::pair<uint64_t,uint64_t>> literalAccesses;
  unsigned pointerSelections = 0;
  unsigned queryResets = 0, textureCalls = 0, unresolvedTextures = 0, unresolvedStructures = 0;
  unsigned samplerCalls = 0, unresolvedSamplers = 0;
  unsigned bufferReads = 0, bufferWrites = 0, unresolvedBuffers = 0;
  unsigned shaderConstantReads = 0;
  // Diagnostics preserve the failing memory instruction independently of
  // optional access display and without changing restoration eligibility.
  std::vector<std::string> unresolvedBufferInstructions;
  unsigned unresolvedCalls = 0;
  unsigned boundedLoops = 0;
  struct BufferAccess {
    Value address; uint64_t bytes = 0; bool write = false;
    // An unconditional integer store at one uniform address can establish
    // exact bytes. This is a value proof, not executing the shader on the CPU.
    Value stored; bool definiteStore = false;
    // Defined-byte production is independent of knowing the stored value.
    // Only an unconditional store to an exact restored address may publish it.
    bool definiteWrite = false;
  };
  std::vector<BufferAccess> buffers;
  std::vector<Value> accesses;
  Value returned;
  unsigned returns = 0, linkedCalls = 0;
};
struct InvocationValues
{
  // Native draw builtins are distinct from Metal buffer binding locations.
  std::map<std::string, Value> builtins;
  // Only a caller with sourced linked-function or same-module definition
  // identity may supply ordinals. Helpers have no Native binding metadata.
  std::map<unsigned, Value> arguments;
  bool localDefinition = false;
};
using CallResolver = std::function<UniformAccessReport(
    const std::string &, const std::vector<Value> &)>;
// Reads only scalar integer/pointer dependencies. No shader arithmetic or GPU writes run here.
// Resource effects share the same sourced alternatives for loads, stores and
// native calls. Bytecode literals are checked for captured VAs independently;
// unknown/data-generated alternatives must never disappear from qualification.
inline void AppendLiteralAccesses(UniformAccessReport &report,const Value &address,uint64_t size)
{
  const auto candidates=address.kind==Value::PointerSet?address.fields:std::vector<Value>{address};
  for(const auto &candidate:candidates)
  {
    auto literals=candidate.literalOrigins;
    if(candidate.kind==Value::LiteralPointer)literals.push_back(candidate);
    for(const auto &literal:literals)
      if(literal.kind==Value::LiteralPointer && !literal.metadata && literal.offsetKnown &&
         !literal.ranged && size && literal.offset<=UINT64_MAX-(size-1))
      {
        const auto span=std::make_pair(literal.offset,size);
        if(std::find(report.literalAccesses.begin(),report.literalAccesses.end(),span)==report.literalAccesses.end())
          report.literalAccesses.push_back(span);
      }
  }
}
inline void AppendBufferCandidates(UniformAccessReport &report, const Value &address,
                                   uint64_t size, bool write, bool unknownExtent=false)
{
  AppendLiteralAccesses(report,address,size);
  bool sourced=false;
  const auto candidates=address.kind==Value::PointerSet?address.fields:std::vector<Value>{address};
  for(auto candidate:candidates)
  {
    if(candidate.kind==Value::ShaderConstant && !write && size)
    {
      report.shaderConstantReads++;sourced=true;
    }
    else if((candidate.kind==Value::Pointer || candidate.kind==Value::DescriptorPointer) &&
       candidate.object && !candidate.metadata && size)
    {
      if(unknownExtent)candidate.offsetKnown=false;
      report.buffers.push_back({candidate,size,write});sourced=true;
    }
    else if(address.kind!=Value::PointerSet ||
            (candidate.kind!=Value::NullBasedPointer &&
             (candidate.kind!=Value::LiteralPointer || candidate.metadata ||
              !candidate.offsetKnown || candidate.ranged || !size ||
              candidate.offset>UINT64_MAX-(size-1))))
      report.unresolvedBuffers++;
  }
  if(address.kind==Value::PointerSet)
  {
    if(!sourced)report.unresolvedBuffers++;
    report.conditionalAccesses=true;
  }
}
inline UniformAccessReport UniformResourceAccess(const std::string &input,
    const std::string &entry, const std::map<unsigned, Value> &bindings, const Loader &load,
    const std::vector<uint64_t> &invocationExtent = {}, bool rangedIntegerLoads = false,
    const InvocationValues &invocation = {}, const CallResolver &resolveCall = {},
    bool pureFloatingMath = false)
{
  try
  {
  // Grammar patterns are immutable across invocations. Compile them once;
  // resource bindings, loaders and access reports remain invocation-local.
  // metal-objdump emits a separate AIR module for each library function. Metadata
  // IDs restart in each module; never resolve another stage's argument ordinals.
  size_t moduleBegin = 0, moduleEnd = input.size(), definition = 0;
  while((definition = input.find("define ", definition)) != std::string::npos)
  {
    const size_t lineEnd = input.find('\n', definition);
    if(input.substr(definition, lineEnd - definition).find("@" + entry + "(") != std::string::npos)
    {
      const size_t source = input.rfind("source_filename =", definition);
      if(source != std::string::npos)
      {
        moduleBegin = source;
        const size_t next = input.find("source_filename =", definition);
        if(next != std::string::npos) moduleEnd = next;
      }
      break;
    }
    definition += 7;
  }
  const std::string text = input.substr(moduleBegin, moduleEnd - moduleBegin);
  std::map<std::string, Value> values;
  std::vector<std::string> body;
  std::istringstream stream(text);
  std::string line, signature;
  bool inside = false;
  unsigned definitions = 0;
  while(std::getline(stream, line))
  {
    if(line.compare(0, 7, "define ") == 0)
    {
      inside = line.find("@" + entry + "(") != std::string::npos;
      if(inside) {definitions++;signature = line;}
    }
    else if(inside && line == "}") inside = false;
    else if(inside) body.push_back(line);
  }
  // Select one actual definition, rather than requiring the entire Native
  // module to contain only one function. Other helpers/entries do not supply
  // this function's body, argument ordinals or binding metadata.
  if(definitions != 1 || signature.empty()) return {};
  static const std::regex argument(R"((%[-a-zA-Z$._0-9]+)(?:,|\)))");
  std::vector<std::string> args;
  for(std::sregex_iterator i(signature.begin(), signature.end(), argument), end; i != end; ++i)
    args.push_back((*i)[1]);
  // Indirect argument-buffer members have their own ordinal/location metadata.
  // Only the entry's direct argument list describes Native binding locations.
  std::map<std::string,std::string> metadata;
  static const std::regex metadataLine(R"(^!([0-9]+) = (.*)$)");
  std::istringstream metadataStream(text);std::string metadataText,argumentList;
  while(std::getline(metadataStream,metadataText))
  {
    std::smatch record;
    if(ImmutableSyntaxMatch(metadataText,record,metadataLine))
    {
      metadata[record[1]]=record[2];
      // The first two entry references describe outputs and direct arguments.
      // Additional function attributes belong to the original Native shader;
      // their spelling/type does not grant or remove binding restoration.
      if(metadataText.find("@"+entry+",")!=std::string::npos &&
         std::regex_search(metadataText,record,std::regex("@"+entry+R"(, ![0-9]+, !([0-9]+)(?:, [^\n]+)?\}$)")))
        argumentList=record[1];
    }
  }
  std::string argumentMetadata=text;
  if(!argumentList.empty())
  {
    const auto list=metadata.find(argumentList);
    if(list==metadata.end())return {};
    argumentMetadata.clear();static const std::regex reference(R"(!([0-9]+))");
    for(std::sregex_iterator i(list->second.begin(),list->second.end(),reference),end;i!=end;++i)
    {
      const auto field=metadata.find((*i)[1]);if(field==metadata.end())return {};
      argumentMetadata+=field->second+"\n";
    }
  }
  else if(invocation.localDefinition)
  {
    // The actual same-module call supplies every ordinal. Never borrow another
    // entry's direct metadata or an indirect member's binding location.
    if(!bindings.empty() || invocation.arguments.size()!=args.size() ||
       (signature.compare(0,16,"define internal ") &&
        signature.compare(0,15,"define private ")))return {};
    argumentMetadata.clear();
  }
  else if(text.find("!air.kernel =")!=std::string::npos || text.find("!air.vertex =")!=std::string::npos ||
          text.find("!air.fragment =")!=std::string::npos || text.find("!air.visible =")!=std::string::npos)return {};
  static const std::regex binding(R"binding(!\{i32 ([0-9]+), !"air.(?:indirect_buffer|buffer)", (?:!"air.buffer_size", i32 [0-9]+, )?!"air.location_index", i32 ([0-9]+),)binding");
  for(std::sregex_iterator i(argumentMetadata.begin(), argumentMetadata.end(), binding), end; i != end; ++i)
  {
    uint64_t arg = std::stoull((*i)[1]), slot = std::stoull((*i)[2]);
    if(slot > 30) continue;
    auto b = bindings.find(unsigned(slot));
    if(arg < args.size() && b != bindings.end()) values[args[arg]] = b->second;
  }
  for(const auto &parameter:invocation.arguments)
  {
    if(parameter.first>=args.size())return {};
    values[args[parameter.first]]=parameter.second;
  }
  static const std::regex builtin(R"builtin(!\{i32 ([0-9]+), !"air.([^"\n]+)",)builtin");
  for(std::sregex_iterator i(argumentMetadata.begin(),argumentMetadata.end(),builtin),end;i!=end;++i)
  {
    const auto b=invocation.builtins.find((*i)[2]);const auto arg=std::stoull((*i)[1]);
    if(b!=invocation.builtins.end() && arg<args.size())values[args[arg]]=b->second;
  }
  static const std::regex gridArgument(R"grid(!\{i32 ([0-9]+), !"air.thread_position_in_grid",)grid");
  if(invocationExtent.size()==3)
    for(std::sregex_iterator i(argumentMetadata.begin(),argumentMetadata.end(),gridArgument),end;i!=end;++i)
    {
      const auto arg=std::stoull((*i)[1]);
      if(arg<args.size()){Value v;v.kind=Value::ThreadGrid;values[args[arg]]=v;}
    }
  const std::string symbol = R"((%[-a-zA-Z$._0-9]+))";
  static const std::regex literalPointerExpression(R"(inttoptr \(i64 ([0-9]+) to [^)]*\))");
  static const std::regex nullExpression(R"((?:^| )null$)");
  static const std::regex integerScalarType("i(8|16|32|64)");
  static const std::regex assignedCall("^  " + symbol + " = ");
  static const std::regex discardInstruction(R"(^.*\bcall void @air\.discard_fragment\(\)(?: .*)?$)");
  static const std::regex discardDeclaration(R"((?:^|\n)declare void @air\.discard_fragment\(\)(?: |\n))");
  static const std::regex floatingMath(R"(^.*\bcall (?:float|half|double|<[0-9]+ x (?:float|half|double)>) @air\.(?:fma|fabs|sqrt|rsqrt|dot)\.(?:f(?:16|32|64)|v[0-9]+f(?:16|32|64))\([^*]*\).*$)");
  const std::string memorySymbol = R"(([%@][-a-zA-Z$._0-9]+))";
  static const std::regex assign("^  " + symbol + " = (.*)$");
  static const std::regex cast("^bitcast .* " + memorySymbol + " to .*$");
  static const std::regex intToPtr("^inttoptr i64 " + symbol + " to .*$");
  static const std::regex gep("^getelementptr(?: inbounds)? (i8|i16|i32|i64|half|float|double), .* " + symbol + ", i(?:32|64) ([^ ,]+)$");
  static const std::regex scalar("^(mul|add|sub|and|shl|lshr|udiv|urem)(?: nuw| nsw)* i(32|64) ([^ ,]+), ([^ ,]+)$");
  static const std::regex extend("^(zext|sext|trunc) i(32|64) ([^ ,]+) to i(32|64)$");
  static const std::regex extract("^extractelement <3 x i32> " + symbol + ", i(?:32|64) ([0-2])$");
  static const std::regex extractField("^extractvalue [^ ]+ " + symbol + ", ([0-9]+)$");
  static const std::regex insertField("^insertvalue [^ ]+ ([^,]+), [^,]+, ([0-9]+)$");
  static const std::regex functionCall(R"(^.*\bcall [^@]+@([-a-zA-Z$._0-9]+)\((.*)\).*$)");
  static const std::regex compare("^icmp (eq|ne|ugt|uge|ult|ule) i(32|64) ([^ ,]+), ([^ ,]+)$");
  static const std::regex choose("^select i1 ([^ ,]+), ([^,]+), (.+)$");
  static const std::regex operand("(?:^| )(%[-a-zA-Z$._0-9]+|[0-9]+|true|false)$");
  static const std::regex read("^load (?:volatile )?(.*), .* " + memorySymbol + ", align [0-9]+.*$");
  static const std::regex store("^  store ([^,]+), (.*) " + symbol + ", align [0-9]+.*$");
  static const std::regex deviceStoreType(R"((?:addrspace\([12]\)\*|ptr addrspace\([12]\))$)");
  static const std::regex storedType(R"(^((?:i[0-9]+|float|half|double|<[^>]+>|%[^ ]+)) )");
  static const std::regex atomicBufferArg("(i(8|16|32|64)) addrspace\\([12]\\)\\* (?:nocapture |readonly |nonnull |noundef )*"+symbol);
  const std::string syncOperand="i32 (?:%[-a-zA-Z$._0-9]+|[0-9]+)";
  static const std::regex synchronizationCall(
        "^.*\\bcall void @air\\.(?:(?:wg|simdgroup)\\.barrier\\("+syncOperand+", "+syncOperand+
        "|atomic\\.fence\\("+syncOperand+", "+syncOperand+", "+syncOperand+")\\).*$");
  struct CallEffects { bool none=false,argumentsOnly=false,inaccessibleOnly=false,convergent=false,readOnly=false,writeOnly=false; };
  std::map<std::string,std::string> attributeGroups;
  static const std::regex attributes(R"(^attributes #([0-9]+) = \{ ([^\n]+) \}$)");
  std::istringstream declarations(text);
  while(std::getline(declarations,line))
  {std::smatch m;if(ImmutableSyntaxMatch(line,m,attributes))attributeGroups[m[1]]=m[2];}
  std::map<std::string,CallEffects> callEffects;
  static const std::regex declaration(R"(^declare [^@]+@([-a-zA-Z$._0-9]+)\([^\n]*\)(.*)$)");
  declarations.clear();declarations.str(text);
  while(std::getline(declarations,line))
  {
    std::smatch m;if(!ImmutableSyntaxMatch(line,m,declaration))continue;
    std::string effects=m[2];std::smatch group;
    if(std::regex_search(effects,group,std::regex(R"(#([0-9]+))")))
    {const auto found=attributeGroups.find(group[1]);if(found!=attributeGroups.end())effects+=" "+found->second;}
    CallEffects e;
    auto attribute=[&](const std::string &name) {
      return std::regex_search(effects,std::regex("(?:^| )"+name+"(?: |$)"));
    };
    e.none=attribute("readnone") || attribute(R"(memory\(none\))") ||
        RegisterBitIntrinsicDeclaration(line);
    // inaccessiblemem_or_argmemonly is not argument-memory-only.
    e.argumentsOnly=attribute("argmemonly");
    e.inaccessibleOnly=attribute("inaccessiblememonly");
    e.convergent=attribute("convergent");
    e.readOnly=attribute("readonly");
    e.writeOnly=attribute("writeonly");
    callEffects[m[1]]=e;
  }
  static const std::regex numericBufferArg("(i[0-9]+|float|half|double|<[^>]+>) addrspace\\(([12])\\)\\* (?:nocapture |readonly |writeonly |nonnull |noundef )*"+symbol);
  static const std::regex textureArg("%struct\\._(?:texture|depth)_[^ ]+ addrspace\\([0-9]+\\)\\* (?:nocapture |readonly |readnone |nonnull |noundef )*" + symbol);
  static const std::regex samplerArg("%struct\\._sampler_t addrspace\\([0-9]+\\)\\* (?:nocapture |readonly |readnone |nonnull |noundef )*" + symbol);
  static const std::regex structureArg("%struct\\._(?:instance|primitive)_acceleration_structure_t addrspace\\([0-9]+\\)\\* (?:nocapture |readonly |readnone |nonnull |noundef )*" + symbol);
  // Native MSL retains typed struct GEPs, whereas UE's converted AIR mostly uses byte
  // GEPs. Resolve only layouts composed of known scalar/pointer/struct fields.
  struct Layout { uint64_t size = 0, align = 1; std::vector<std::string> fields; std::vector<uint64_t> offsets; };
  std::map<std::string, std::string> types;
  static const std::regex typeDefinition(R"((%[^\n=]+) = type \{ ([^\n]+) \})");
  // Unanchored search over all instructions is quadratic for long percent-
  // prefixed expressions. A type declaration cannot cross a newline and must
  // contain this literal separator. Scan lines first, preserving the grammar.
  std::istringstream typeLines(text); std::string typeLine;
  while(std::getline(typeLines, typeLine))
  {
    if(typeLine.find(" = type { ") == std::string::npos) continue;
    std::smatch type;
    if(ImmutableSyntaxMatch(typeLine, type, typeDefinition, true)) types[type[1]] = type[2];
  }
  std::function<Layout(std::string, unsigned)> layout = [&](std::string type, unsigned depth) -> Layout {
    Layout result;
    if(depth > 16 || type.empty()) return result;
    if(type.back() == '*') { result.size = result.align = 8; return result; }
    if(type == "i8") { result.size = result.align = 1; return result; }
    if(type == "i16" || type == "half") { result.size = result.align = 2; return result; }
    if(type == "i32" || type == "float") { result.size = result.align = 4; return result; }
    if(type == "i64" || type == "double") { result.size = result.align = 8; return result; }
    std::smatch vector;
    if(std::regex_match(type,vector,std::regex(R"(^<([0-9]+) x (.+)>$)")))
    {
      auto member=layout(vector[2],depth+1);const auto count=std::stoull(vector[1]);
      if(!member.size || !count || count>16)return result;
      result.size=member.size*count;result.align=1;
      while(result.align<result.size)result.align*=2;
      return result;
    }
    auto named = types.find(type);
    std::string members;
    if(named != types.end()) members = named->second;
    else if(type.size() >= 4 && type.substr(0, 2) == "{ " && type.substr(type.size()-2) == " }")
      members = type.substr(2, type.size()-4);
    else return result;
    std::istringstream fields(members);
    std::string field;
    while(std::getline(fields, field, ','))
    {
      size_t first = field.find_first_not_of(' '), last = field.find_last_not_of(' ');
      if(first == std::string::npos) return Layout();
      field = field.substr(first, last - first + 1);
      Layout member = layout(field, depth + 1);
      if(!member.size || result.size > UINT64_MAX - member.align) return Layout();
      uint64_t offset = (result.size + member.align - 1) / member.align * member.align;
      if(offset > UINT64_MAX - member.size) return Layout();
      result.offsets.push_back(offset); result.fields.push_back(field);
      result.size = offset + member.size; result.align = std::max(result.align, member.align);
    }
    if(result.size > UINT64_MAX - result.align) return Layout();
    result.size = (result.size + result.align - 1) / result.align * result.align;
    return result;
  };
  static const std::regex structGEP("^getelementptr(?: inbounds)? (\\{[^}]+\\}|%[^,]+), .*? " + symbol + "((?:, i(?:32|64) [^,]+)+)$");
  static const std::regex gepIndex(R"(, i(?:32|64) ([^, ]+))");
  // Classify ownership, not initializer values. Only complete internal/private
  // numeric constant definitions belong to this bytecode. External declarations,
  // pointer/handle types and address-containing initializers are not certified.
  auto trim=[](std::string value) {
    const auto first=value.find_first_not_of(' '),last=value.find_last_not_of(' ');
    return first==std::string::npos?std::string():value.substr(first,last-first+1);
  };
  std::function<bool(std::string,unsigned)> numericStorage=[&](std::string type,unsigned depth) {
    type=trim(type);
    if(depth>16 || type.empty())return false;
    if(type=="i1" || type=="i8" || type=="i16" || type=="i32" || type=="i64" ||
       type=="half" || type=="float" || type=="double")return true;
    std::smatch aggregate;
    if(std::regex_match(type,aggregate,std::regex(R"(^\[([0-9]+) x (.+)\]$)")) ||
       std::regex_match(type,aggregate,std::regex(R"(^<([0-9]+) x (.+)>$)")))
      return std::stoull(aggregate[1])>0 && numericStorage(aggregate[2],depth+1);
    const auto named=types.find(type);
    if(named!=types.end())type="{ "+named->second+" }";
    if(type.front()!='{' || type.back()!='}')return false;
    std::string field;int nesting=0;bool member=false;
    for(size_t i=1;i<type.size();i++)
    {
      const char c=i==type.size()-1?',':type[i];
      if(c=='[' || c=='<' || c=='{')nesting++;
      if(c==']' || c=='>' || c=='}')nesting--;
      if(c==',' && !nesting)
      {if(!numericStorage(field,depth+1))return false;field.clear();member=true;}
      else field+=c;
    }
    return member && !nesting;
  };
  static const std::regex globalConstant(R"(^(@[-a-zA-Z$._0-9]+) = (?:internal|private) (?:unnamed_addr |local_unnamed_addr )?addrspace\(2\) constant (.+)$)");
  std::istringstream globals(text);std::string global;
  while(std::getline(globals,global))
  {
    std::smatch constantDefinition;
    if(!ImmutableSyntaxMatch(global,constantDefinition,globalConstant))continue;
    const std::string data=constantDefinition[2];size_t end=0;
    if(data.empty())continue;
    if(data[0]=='[' || data[0]=='<' || data[0]=='{')
    {
      int nesting=0;
      for(;end<data.size();end++)
      {
        if(data[end]=='[' || data[end]=='<' || data[end]=='{')nesting++;
        if(data[end]==']' || data[end]=='>' || data[end]=='}')nesting--;
        if(!nesting){end++;break;}
      }
    }
    else end=data.find(' ');
    if(end==std::string::npos || end>=data.size() ||
       !numericStorage(data.substr(0,end),0))continue;
    const std::string initializer=trim(data.substr(end));
    if(initializer.empty() || initializer[0]==',' || initializer.find('@')!=std::string::npos ||
       initializer.find("ptr")!=std::string::npos || initializer.find("blockaddress")!=std::string::npos ||
       initializer.find("getelementptr")!=std::string::npos)continue;
    Value owned;owned.kind=Value::ShaderConstant;owned.offsetKnown=false;
    values[constantDefinition[1]]=owned;
  }
  static const std::regex constantGEP("^getelementptr(?: inbounds)? .*?, .*? " + memorySymbol +
      "(?:, i(?:32|64) [^,]+)+$");
  auto value = [&](const std::string &s) -> Value {
    if(!s.empty() && (s[0] == '%' || s[0]=='@')) { auto v = values.find(s); return v == values.end() ? Value() : v->second; }
    if(s.empty() || s.find_first_not_of("-0123456789") != std::string::npos) return {};
    try { return Value::Number(std::stoull(s)); } catch(...) { return {}; }
  };
  // Only a canonical single-block, zero-based unit-step loop has a derived
  // counter range. The bound must be independent of every phi recurrence.
  // Its actual scalar bytes still require the caller's submission proof, and
  // callers authorising execution must reject writers overlapping these facts.
  static const std::regex label(R"(^([-a-zA-Z$._0-9]+):.*$)");
  static const std::regex phi("^phi i(32|64) \\[ ([^,]+), " + symbol + " \\], \\[ ([^,]+), " + symbol + " \\]$");
  static const std::regex branch("^  br i1 " + symbol + ", label " + symbol + ", label " + symbol + ".*$");
  static const std::regex symbols(symbol);
  std::map<std::string,std::string> expressions,blocks;
  std::map<std::string,std::vector<std::string>> blockInstructions;
  std::string block;
  for(const auto &instruction:body)
  {
    std::smatch m;
    if(ImmutableSyntaxMatch(instruction,m,label))block="%"+m[1].str();
    else
    {
      blockInstructions[block].push_back(instruction);
      if(ImmutableSyntaxMatch(instruction,m,assign))
      {expressions[m[1]]=m[2];blocks[m[1]]=block;}
    }
  }
  auto invariant = [&](const std::string &bound) {
    std::set<std::string> visiting,done;
    std::function<bool(const std::string &,unsigned)> check=[&](const std::string &name,unsigned depth) {
      if(depth>128)return false;
      auto expression=expressions.find(name);
      if(expression==expressions.end() || done.count(name))return true;
      if(!visiting.insert(name).second || expression->second.compare(0,4,"phi ")==0)return false;
      for(std::sregex_iterator i(expression->second.begin(),expression->second.end(),symbols),end;i!=end;++i)
        if(!check((*i)[1],depth+1))return false;
      visiting.erase(name);done.insert(name);return true;
    };
    return check(bound,0);
  };
  std::map<std::string,std::string> loopBounds;
  for(const auto &expression:expressions)
  {
    std::smatch p;
    if(!ImmutableSyntaxMatch(expression.second,p,phi))continue;
    const auto &loop=blocks[expression.first];
    if(loop.empty())continue;
    const bool first=p[3]==loop && p[4]=="0" && p[5]!=loop;
    const bool second=p[5]==loop && p[2]=="0" && p[3]!=loop;
    if(!first && !second)continue;
    const std::string next=first?p[2].str():p[4].str();
    auto step=expressions.find(next);std::smatch advance;
    if(step==expressions.end() || blocks[next]!=loop ||
       !ImmutableSyntaxMatch(step->second,advance,scalar) || advance[1]!="add" ||
       advance[2]!=p[1] || advance[3]!=expression.first || advance[4]!="1")continue;
    unsigned branches=0;std::smatch backedge;
    for(const auto &instruction:blockInstructions[loop])
      if(instruction.compare(0,5,"  br ")==0)
      {branches++;ImmutableSyntaxMatch(instruction,backedge,branch);}
    if(branches!=1 || backedge.empty() || (backedge[2]==loop)==(backedge[3]==loop))continue;
    auto condition=expressions.find(backedge[1]);std::smatch comparison;
    if(condition==expressions.end() || blocks[condition->first]!=loop ||
       !ImmutableSyntaxMatch(condition->second,comparison,compare) || comparison[2]!=p[1])continue;
    const bool repeat=backedge[2]==loop;
    if(!((repeat && (comparison[1]=="ult" || comparison[1]=="ne")) ||
         (!repeat && comparison[1]=="eq")))continue;
    std::string bound;
    if(comparison[3]==next)bound=comparison[4];
    else if(comparison[1]!="ult" && comparison[4]==next)bound=comparison[3];
    if(bound.empty() || !invariant(bound))continue;
    loopBounds[expression.first]=bound;
  }
  UniformAccessReport report; report.validModule = true;
  auto pointerOperand=[&](const std::string &text) {
    std::smatch m;
    if(ImmutableSyntaxMatch(text,m,literalPointerExpression, true))
    {Value v;v.kind=Value::LiteralPointer;v.offset=std::stoull(m[1]);return v;}
    if(ImmutableSyntaxMatch(text,nullExpression, true))
    {Value v;v.kind=Value::NullBasedPointer;return v;}
    if(ImmutableSyntaxMatch(text,m,operand, true))return value(m[1]);
    return Value();
  };
  const auto privatePointerSpills=PrivatePointerSpillOrigins(body);
  static const std::regex pointerPhi(R"(^phi [^\[]*\* (\[.*)$)");
  static const std::regex phiIncoming(R"(\[ ([^\]]+), %[A-Za-z0-9_.$-]+ \])");
  // A loop's invariant reload can occur after its phi. Bounded forward passes
  // resolve that dependency without executing iterations or shader outputs.
  for(unsigned pass=0;pass<(loopBounds.empty()?1U:3U);pass++)
  {
  report={};report.validModule=true;
  static const std::regex literalPointer(R"(inttoptr \(i64 ([0-9]+) to [^)]*\))");
  for(const auto &instruction:body)
    for(std::sregex_iterator i(instruction.begin(),instruction.end(),literalPointer),end;i!=end;++i)
      report.literalPointers.push_back(std::stoull((*i)[1]));
  unsigned instructionIndex=0;
  for(const std::string &instruction : body)
  {
    const unsigned currentInstruction=instructionIndex++;
    const bool callInstruction = instruction.find("call ") != std::string::npos;
    std::smatch match, operation;
    bool resolvedCall=false;
    Value callResult;
    if(resolveCall && callInstruction && ImmutableSyntaxMatch(instruction,operation,functionCall))
    {
      std::vector<Value> parameters;std::string list=operation[2],parameter;
      int nesting=0;
      for(size_t i=0;i<=list.size();i++)
      {
        const char c=i<list.size()?list[i]:',';
        if(c=='(' || c=='<' || c=='[' || c=='{')nesting++;
        if(c==')' || c=='>' || c==']' || c=='}')nesting--;
        if(c==',' && !nesting)
        {
          std::smatch p;
          parameters.push_back(ImmutableSyntaxMatch(parameter,p,operand, true)?value(p[1]):Value());
          parameter.clear();
        }
        else parameter+=c;
      }
      if(list.empty())parameters.clear();
      if(parameters.size()<=32 && !nesting)
      {
        const auto child=resolveCall(operation[1],parameters);
        if(child.validModule)
        {
          resolvedCall=true;callResult=child.returned;
          report.queryResets+=child.queryResets;report.textureCalls+=child.textureCalls;
          report.unresolvedTextures+=child.unresolvedTextures;report.unresolvedStructures+=child.unresolvedStructures;
          report.samplerCalls+=child.samplerCalls;report.unresolvedSamplers+=child.unresolvedSamplers;
          report.bufferReads+=child.bufferReads;report.bufferWrites+=child.bufferWrites;
          report.unresolvedBuffers+=child.unresolvedBuffers;report.unresolvedCalls+=child.unresolvedCalls;
          report.shaderConstantReads+=child.shaderConstantReads;
          report.unresolvedBufferInstructions.insert(report.unresolvedBufferInstructions.end(),
              child.unresolvedBufferInstructions.begin(),child.unresolvedBufferInstructions.end());
          report.pointerSelections+=child.pointerSelections;
          report.literalPointers.insert(report.literalPointers.end(),child.literalPointers.begin(),child.literalPointers.end());
          report.literalAccesses.insert(report.literalAccesses.end(),child.literalAccesses.begin(),child.literalAccesses.end());
          report.conditionalAccesses |= child.conditionalAccesses;
          report.boundedLoops+=child.boundedLoops;report.linkedCalls+=1+child.linkedCalls;
          report.buffers.insert(report.buffers.end(),child.buffers.begin(),child.buffers.end());
          report.accesses.insert(report.accesses.end(),child.accesses.begin(),child.accesses.end());
        }
      }
    }
    if(ImmutableSyntaxMatch(instruction, match, assign))
    {
      std::string expr = match[2]; Value result;
      if(resolvedCall)result=callResult;
      else if(loopBounds.count(match[1]))
      {
        const auto bound=value(loopBounds.at(match[1]));
        if(bound.kind==Value::Integer && !bound.ranged && bound.offset && bound.offset<=65536)
        {result=Value::Range(0,bound.offset-1);report.boundedLoops++;}
      }
      else if(ImmutableSyntaxMatch(expr, operation, cast)) result = value(operation[1]);
      else if(ImmutableSyntaxMatch(expr,operation,constantGEP) &&
              value(operation[1]).kind==Value::ShaderConstant)
      {
        // Original Native code performs the projection, index and bounds. No
        // CPU scalar lookup or external buffer identity is manufactured.
        result=value(operation[1]);
      }
      else if(ImmutableSyntaxMatch(expr, operation, pointerPhi))
      {
        // Control-flow joins preserve all incoming resource identities. Do
        // not evaluate the branch, or infer a cyclic/GPU-produced pointer.
        const std::string incoming=operation[1];bool first=true;
        for(std::sregex_iterator i(incoming.begin(),incoming.end(),phiIncoming),end;i!=end;++i)
        {
          const Value source=pointerOperand((*i)[1]);
          result=first?source:MergePointerProvenance(result,source);first=false;
        }
        if(!first)report.pointerSelections++;
      }
      else if(ImmutableSyntaxMatch(expr, operation, intToPtr))
      {
        // Raw ulong roots in native MSL are pointers only when the loader has
        // authoritative typed provenance. Never interpret a scalar GPU address.
        Value address = value(operation[1]);
        if(address.kind == Value::Pointer || address.kind == Value::PointerSet || address.kind==Value::DescriptorPointer ||
           (address.kind == Value::Texture &&
            (expr.find(" to %struct._texture_") != std::string::npos ||
             expr.find(" to %struct._depth_") != std::string::npos)) ||
           (address.kind == Value::Sampler &&
            expr.find(" to %struct._sampler_t") != std::string::npos)) result = address;
      }
      else if(ImmutableSyntaxMatch(expr, operation, gep))
      {
        Value base = value(operation[2]), index = value(operation[3]);
        unsigned stride = unsigned(layout(operation[1],0).size);
        std::function<Value(Value)> offset=[&](Value v) {
          for(auto &literal:v.literalOrigins)literal=offset(literal);
          if(v.kind!=Value::Pointer && v.kind!=Value::DescriptorPointer && v.kind!=Value::LiteralPointer && v.kind!=Value::NullBasedPointer)return Value();
          // A typed GPU pointer is not a numerical byte index. Missing/opaque
          // scalar results stay unknown; never erase a known address payload.
          if(index.kind!=Value::Integer && index.kind!=Value::Unknown)return Value();
          if(v.kind==Value::LiteralPointer &&
             (index.kind!=Value::Integer || index.ranged))v.metadata=true;
          if(v.offsetKnown && index.kind==Value::Integer && index.High()<=(UINT64_MAX-v.High())/stride)
          {
            const auto high=v.High();
            if(v.kind==Value::Pointer && v.metadata && !v.ranged && index.ranged && stride &&
               index.High()<(UINT64_MAX-v.offset)/stride)
            {v.namespaceBase=v.offset+index.offset*stride;v.namespaceEnd=v.offset+(index.High()+1)*stride;
             v.namespaceStride=stride;v.namespaceField=0;}
            else if(v.namespaceStride && !index.ranged && index.offset<=UINT64_MAX/stride &&
                    index.offset*stride<v.namespaceStride && v.namespaceField<v.namespaceStride-index.offset*stride)
              v.namespaceField+=index.offset*stride;
            else if(v.kind==Value::Pointer){v.namespaceStride=0;v.namespaceEnd=0;}
            v.offset+=index.offset*stride;v.maximum=high+index.High()*stride;v.ranged=v.offset!=v.maximum;
          }
          else
          {
            if(v.kind==Value::Pointer && v.metadata && v.offsetKnown && !v.ranged &&
               index.kind==Value::Unknown && stride && index.layoutStride<=UINT64_MAX/stride)
            {v.namespaceBase=v.offset;v.namespaceStride=stride*(index.layoutStride?index.layoutStride:1);v.namespaceField=0;v.namespaceEnd=0;}
            else if(v.namespaceStride && index.kind==Value::Integer && !index.ranged &&
                    index.offset<=UINT64_MAX/stride && index.offset*stride<v.namespaceStride &&
                    v.namespaceField<v.namespaceStride-index.offset*stride)
              v.namespaceField+=index.offset*stride;
            else if(v.kind==Value::Pointer)v.namespaceStride=0;
            v.offsetKnown=false;
          }
          return v;
        };
        if(base.kind==Value::PointerSet)
        {result=base;for(auto &v:result.fields)v=offset(v);}
        else result=offset(base);
      }
      else if(ImmutableSyntaxMatch(expr, operation, structGEP))
      {
        Value base = value(operation[2]);
        std::string type = operation[1], indices = operation[3];
        uint64_t offset = 0, highOffset=0, indexedStride=0, rowStride=0, rowLow=0, rowHigh=0, rowField=0; bool known = true, first = true, addressIndex = false;
        for(std::sregex_iterator i(indices.begin(), indices.end(), gepIndex), end; i != end; ++i)
        {
          Value index = value((*i)[1]); Layout current = layout(type, 0);
          if(index.kind!=Value::Integer && index.kind!=Value::Unknown)
          {addressIndex=true;break;}
          if(first && index.kind==Value::Unknown && current.size)
          {indexedStride=current.size;known=false;first=false;continue;}
          if((!known && !indexedStride) || index.kind != Value::Integer || !current.size) { known = false; indexedStride=0; break; }
          uint64_t delta,highDelta;
          if(first)
          {
            if(index.High() > UINT64_MAX / current.size) { known = false; break; }
            delta = index.offset * current.size;highDelta=index.High()*current.size;
            if(index.ranged){rowStride=current.size;rowLow=delta;rowHigh=highDelta;}
            first = false;
          }
          else
          {
            if(index.ranged || index.offset >= current.fields.size()) { known = false; break; }
            delta = highDelta=current.offsets[size_t(index.offset)]; type = current.fields[size_t(index.offset)];
            if(delta>UINT64_MAX-rowField){known=false;break;}rowField+=delta;
          }
          if(highDelta > UINT64_MAX - highOffset) { known = false; break; }
          offset += delta;highOffset+=highDelta;
        }
        std::function<Value(Value)> project=[&](Value candidate) {
          for(auto &literal:candidate.literalOrigins)literal=project(literal);
          if(addressIndex)return Value();
          if(candidate.kind!=Value::Pointer && candidate.kind!=Value::DescriptorPointer && candidate.kind!=Value::LiteralPointer &&
             candidate.kind!=Value::NullBasedPointer)return Value();
          if(candidate.kind==Value::LiteralPointer && (!known || offset!=highOffset))candidate.metadata=true;
          if(known && candidate.offsetKnown && highOffset<=UINT64_MAX-candidate.High())
          {
            const auto high=candidate.High();
            if(candidate.kind==Value::Pointer && candidate.metadata && !candidate.ranged && rowStride &&
               rowField<rowStride && rowHigh<=UINT64_MAX-candidate.offset &&
               rowStride<=UINT64_MAX-candidate.offset-rowHigh)
            {candidate.namespaceBase=candidate.offset+rowLow;candidate.namespaceEnd=candidate.offset+rowHigh+rowStride;
             candidate.namespaceStride=rowStride;candidate.namespaceField=rowField;}
            else if(candidate.namespaceStride && offset==highOffset && offset<candidate.namespaceStride &&
                    candidate.namespaceField<candidate.namespaceStride-offset)
              candidate.namespaceField+=offset;
            else if(candidate.kind==Value::Pointer){candidate.namespaceStride=0;candidate.namespaceEnd=0;}
            candidate.offset+=offset;candidate.maximum=high+highOffset;
            candidate.ranged=candidate.offset!=candidate.maximum;
          }
          else
          {
            if(indexedStride && offset==highOffset && offset<indexedStride &&
               candidate.kind==Value::Pointer && candidate.metadata && candidate.offsetKnown && !candidate.ranged)
            {candidate.namespaceBase=candidate.offset;candidate.namespaceStride=indexedStride;candidate.namespaceField=offset;candidate.namespaceEnd=0;}
            else if(candidate.kind==Value::Pointer)candidate.namespaceStride=0;
            candidate.offsetKnown=false;
          }
          return candidate;
        };
        if(base.kind==Value::PointerSet)
        {result=base;for(auto &candidate:result.fields)candidate=project(candidate);}
        else result=project(base);
      }
      else if(ImmutableSyntaxMatch(expr, operation, scalar))
      {
        Value a = value(operation[3]), b = value(operation[4]);
        // Preserve the compiler's ABI row-address form, without calculating
        // the GPU operand, selecting a row, or turning it into an Integer fact.
        if(operation[1]=="mul" && (a.kind==Value::Unknown || b.kind==Value::Unknown))
        {
          const auto &constant=a.kind==Value::Integer?a:b;
          const auto &dynamic=a.kind==Value::Unknown?a:b;
          if(constant.kind==Value::Integer && !constant.ranged && constant.offset &&
             dynamic.kind==Value::Unknown && !dynamic.layoutStride)
            result.layoutStride=constant.offset;
        }
        if(a.kind == Value::Integer && b.kind == Value::Integer)
        {
          if(a.ranged || b.ranged)
          {
            const uint64_t limit=operation[2]=="32"?UINT32_MAX:UINT64_MAX;
            std::string op=operation[1];uint64_t lo=0,hi=0;bool known=true;
            if(op=="add")
            {known=a.High()<=limit && b.High()<=limit-a.High();if(known){lo=a.offset+b.offset;hi=a.High()+b.High();}}
            else if(op=="sub")
            {known=a.High()<=limit && a.offset>=b.High();if(known){lo=a.offset-b.High();hi=a.High()-b.offset;}}
            else if(op=="mul")
            {known=!b.High() || a.High()<=limit/b.High();if(known){lo=a.offset*b.offset;hi=a.High()*b.High();}}
            else if(op=="udiv")
            {known=b.offset && a.High()<=limit && b.High()<=limit;if(known){lo=a.offset/b.High();hi=a.High()/b.offset;}}
            else if(op=="urem")
            {known=b.offset && a.High()<=limit && b.High()<=limit;if(known)hi=std::min(a.High(),b.High()-1);}
            else if(op=="and")hi=std::min(a.High(),b.High());
            else if(op=="lshr" && !b.ranged && b.offset<std::stoul(operation[2]))
            {lo=a.offset>>b.offset;hi=a.High()>>b.offset;}
            else if(!b.ranged && b.offset<std::stoul(operation[2]))
            {known=a.High()<=(limit>>b.offset);if(known){lo=a.offset<<b.offset;hi=a.High()<<b.offset;}}
            else known=false;
            if(known)result=Value::Range(lo,hi);
            values[match[1]]=result;continue;
          }
          std::string op = operation[1]; uint64_t n = 0;
          if(op == "mul") n = a.offset * b.offset;
          else if(op == "add") n = a.offset + b.offset;
          else if(op == "sub") n = a.offset - b.offset;
          else if(op == "and") n = a.offset & b.offset;
          else if(op == "udiv" || op == "urem")
          {const uint64_t numerator=operation[2]=="32"?uint32_t(a.offset):a.offset;
           const uint64_t denominator=operation[2]=="32"?uint32_t(b.offset):b.offset;
           if(!denominator){values[match[1]]={};continue;}
           n=op=="udiv"?numerator/denominator:numerator%denominator;}
          else if(op == "lshr" && b.offset < std::stoul(operation[2])) n = a.offset >> b.offset;
          else if(b.offset < std::stoul(operation[2])) n = a.offset << b.offset;
          else { values[match[1]] = {}; continue; }
          result = Value::Number(operation[2] == "32" ? uint32_t(n) : n);
        }
      }
      else if(ImmutableSyntaxMatch(expr, operation, extend))
      {
        Value a = value(operation[3]);
        if(a.kind==Value::Unknown && (operation[1]=="zext" || operation[1]=="sext"))
          result.layoutStride=a.layoutStride;
        if(a.kind == Value::Integer)
        {
          if(a.ranged)
          {
            const uint64_t limit=operation[4]=="32"?UINT32_MAX:UINT64_MAX;
            if(a.High()<=limit && !(operation[1]=="sext" && a.High()>INT32_MAX))result=a;
            values[match[1]]=result;continue;
          }
          uint64_t n = operation[2] == "32" ? uint32_t(a.offset) : a.offset;
          if(operation[1] == "sext" && operation[2] == "32") n = int64_t(int32_t(n));
          result = Value::Number(operation[4] == "32" ? uint32_t(n) : n);
        }
      }
      else if(ImmutableSyntaxMatch(expr,operation,extract))
      {
        const auto vector=value(operation[1]);const auto component=std::stoul(operation[2]);
        if(vector.kind==Value::ThreadGrid && invocationExtent.size()==3)
        {const auto count=invocationExtent[component];if(count<=UINT32_MAX)result=Value::Range(0,count?count-1:0);}
        else if(vector.kind==Value::Aggregate && vector.fields.size()==3)result=vector.fields[component];
      }
      else if(ImmutableSyntaxMatch(expr,operation,extractField))
      {
        const auto aggregate=value(operation[1]);const auto field=std::stoull(operation[2]);
        if(aggregate.kind==Value::Aggregate && field<aggregate.fields.size())result=aggregate.fields[field];
      }
      else if(ImmutableSyntaxMatch(expr,operation,insertField))
      {
        const auto field=std::stoull(operation[2]);std::smatch inserted;
        const auto comma=expr.rfind(',');const auto first=expr.find(',');
        if(field<16 && first<comma)
        {
          result=value(operation[1]);if(result.kind!=Value::Aggregate)result={};
          result.kind=Value::Aggregate;result.fields.resize(std::max(result.fields.size(),size_t(field+1)));
          const auto fieldText=expr.substr(first+1,comma-first-1);
          if(ImmutableSyntaxMatch(fieldText,inserted,operand, true))result.fields[field]=value(inserted[1]);
        }
      }
      else if(ImmutableSyntaxMatch(expr, operation, compare))
      {
        const Value a = value(operation[3]), b = value(operation[4]);
        if(a.kind == Value::Integer && b.kind == Value::Integer)
        {
          const uint64_t x = operation[2] == "32" ? uint32_t(a.offset) : a.offset;
          const uint64_t y = operation[2] == "32" ? uint32_t(b.offset) : b.offset;
          const uint64_t highX=a.ranged?a.High():x,highY=b.ranged?b.High():y;
          const uint64_t limit=operation[2]=="32"?UINT32_MAX:UINT64_MAX;
          const std::string op = operation[1];
          if(highX<=limit && highY<=limit)
          {
            int known=-1;
            if(op=="eq" || op=="ne")
            {
              if(highX<y || highY<x)known=op=="ne";
              else if(x==highX && y==highY)known=op=="eq"?x==y:x!=y;
            }
            else if(op=="ugt") {if(x>highY)known=1;else if(highX<=y)known=0;}
            else if(op=="uge") {if(x>=highY)known=1;else if(highX<y)known=0;}
            else if(op=="ult") {if(highX<y)known=1;else if(x>=highY)known=0;}
            else {if(highX<=y)known=1;else if(x>highY)known=0;}
            if(known>=0)result=Value::Number(known);
          }
        }
      }
      else if(ImmutableSyntaxMatch(expr, operation, choose))
      {
        const std::string predicate = operation[1];
        const Value condition = predicate == "true" ? Value::Number(1) :
            predicate == "false" ? Value::Number(0) : value(predicate);
        // Pointer alternatives describe restoration obligations, independently
        // of optional scalar folding. A folded null can occur in a block that
        // the original Native control flow never executes. Retain every real
        // source and literal address instead of turning that block into an
        // unsourced input; unknown alternatives still fail qualification.
        if(operation[2].str().find('*')!=std::string::npos)
        {
          result=MergePointerProvenance(pointerOperand(operation[2]),pointerOperand(operation[3]));
          report.pointerSelections++;
        }
        else if(condition.kind == Value::Integer && !condition.ranged && condition.offset <= 1)
        {
          const std::string selected = condition.offset ? operation[2] : operation[3];
          std::smatch selectedOperand;
          if(ImmutableSyntaxMatch(selected, selectedOperand, operand, true))
            result = value(selectedOperand[1]);
          for(const auto &alternative:{pointerOperand(operation[2]),pointerOperand(operation[3])})
          {
            const auto candidates=alternative.kind==Value::PointerSet?alternative.fields:std::vector<Value>{alternative};
            for(const auto &candidate:candidates)
            {
              result.literalOrigins.insert(result.literalOrigins.end(),candidate.literalOrigins.begin(),candidate.literalOrigins.end());
              if(candidate.kind==Value::LiteralPointer)result.literalOrigins.push_back(candidate);
            }
          }
        }
      }
      else if(ImmutableSyntaxMatch(expr, operation, read))
      {
        Value address = value(operation[2]); std::string type = operation[1];
        const auto size=layout(type,0).size;
        const bool data=!type.empty() && type.back()!='*';
        if(data && (expr.find("addrspace(1)")!=std::string::npos ||
                    expr.find("addrspace(2)")!=std::string::npos))
        {
          if(!address.metadata)
          {
            const unsigned unresolvedBefore=report.unresolvedBuffers;
            report.bufferReads++;
            AppendLiteralAccesses(report,address,size);
            if(!size)report.unresolvedBuffers++;
            else if(address.kind==Value::ShaderConstant)report.shaderConstantReads++;
            else if(address.kind==Value::PointerSet)
              AppendBufferCandidates(report,address,size,false);
            else if((address.kind!=Value::Pointer && address.kind!=Value::DescriptorPointer) || !address.object)report.unresolvedBuffers++;
            else report.buffers.push_back({address,size,false});
            if(report.unresolvedBuffers!=unresolvedBefore)
              report.unresolvedBufferInstructions.push_back(instruction);
          }
        }
        const auto spill=privatePointerSpills.find(currentInstruction);
        if(spill!=privatePointerSpills.end() && !type.empty() && type.back()=='*')
        {
          if(spill->second.size()>1)report.pointerSelections++;
          bool first=true;
          for(const auto &source:spill->second)
          {
            const auto pointer=pointerOperand(source);
            result=first?pointer:MergePointerProvenance(result,pointer);first=false;
          }
        }
        else if(address.kind==Value::Pointer && address.metadata && (!address.offsetKnown || address.ranged) && address.namespaceStride)
        {
          if((!type.empty() && type.back()=='*') || type=="i64")result=load(address,8,Value::Pointer);
          // Numerical descriptor fields stay unknown. They cannot choose a row
          // or manufacture a GPU VA; texture/AS selection needs its own closure.
        }
        else if(address.kind == Value::Pointer && address.offsetKnown && address.ranged &&
           rangedIntegerLoads && (type=="i32" || type=="i64"))
          result=load(address,type=="i32"?4:8,Value::Integer);
        else if(address.kind == Value::Pointer && address.offsetKnown && !address.ranged)
        {
          if((type.find("%struct._instance_acceleration_structure_t") == 0 ||
              type.find("%struct._primitive_acceleration_structure_t") == 0) && type.back() == '*')
            result = load(address, 8, Value::AccelerationStructure);
          else if((type.find("%struct._texture_") == 0 || type.find("%struct._depth_") == 0) && type.back() == '*')
            result = load(address, 8, Value::Texture);
          else if(type.find("%struct._sampler_t") == 0 && type.back() == '*')
            result = load(address, 8, Value::Sampler);
          else if(type == "i32" || type == "i64")
          {
            if(type == "i64") result = load(address, 8, Value::Pointer);
            if(type == "i64" && result.kind != Value::Pointer)
              result = load(address, 8, Value::Texture);
            if(type == "i64" && result.kind != Value::Pointer && result.kind != Value::Texture)
              result = load(address, 8, Value::Sampler);
            if(result.kind != Value::Pointer && result.kind != Value::Texture && result.kind != Value::Sampler)
              result = load(address, type == "i32" ? 4 : 8, Value::Integer);
          }
          else if(!type.empty() && type.back() == '*') result = load(address, 8, Value::Pointer);
        }
      }
      values[match[1]] = result;
    }
    if(instruction.compare(0,6,"  ret ")==0)
    {
      std::smatch returned;report.returns++;
      if(report.returns==1 && ImmutableSyntaxMatch(instruction,returned,operand, true))report.returned=value(returned[1]);
      else report.returned={};
    }
    std::smatch storeOperands;
    const bool parsedStore=ImmutableSyntaxMatch(instruction,storeOperands,store);
    // Classify the destination's outer storage space, not a pointer value's
    // pointee space. A device pointer stored in Native private query state
    // does not write a captured device allocation.
    if(parsedStore && ImmutableSyntaxMatch(storeOperands[2].str(),deviceStoreType, true))
    {
      report.bufferWrites++;
      const auto address=value(storeOperands[3]);std::smatch type;
      const std::string stored=storeOperands[1];
      const auto size=ImmutableSyntaxMatch(stored,type,storedType, true)?layout(type[1],0).size:0;
      AppendLiteralAccesses(report,address,size);
      if(address.kind==Value::PointerSet && size)
        AppendBufferCandidates(report,address,size,true);
      else if(address.kind!=Value::Pointer || !address.object || address.metadata || !size)report.unresolvedBuffers++;
      else
      {
        report.buffers.push_back({address,size,true});
        report.buffers.back().definiteWrite=!address.ranged && address.offsetKnown;
        std::smatch operandValue;
        if(size<=8 && ImmutableSyntaxMatch(type[1].str(),integerScalarType) &&
           ImmutableSyntaxMatch(stored,operandValue,operand, true))
        {
          auto &access=report.buffers.back();access.stored=value(operandValue[1]);
          access.definiteStore=!address.ranged && address.offsetKnown &&
              access.stored.kind==Value::Integer && !access.stored.ranged;
        }
      }
    }
    else if(!parsedStore && instruction.compare(0,8,"  store ")==0 &&
            (instruction.find("addrspace(1)")!=std::string::npos || instruction.find("addrspace(2)")!=std::string::npos))
    {report.bufferWrites++;report.unresolvedBuffers++;}
    // Native synchronization has ordering effects, not descriptor pointer
    // operands. Leave threadgroup contents/results unknown and execute the
    // original GPU barrier/fence; it is not a device-buffer atomic access.
    const bool nativeSynchronization=callInstruction && ImmutableSyntaxMatch(instruction,synchronizationCall);
    // Threadgroup/private allocations are part of Native shader execution,
    // not captured device resources. Classify actual LLVM/local AIR atomics
    // by address space and API operation; do not simulate allocator results.
    static const std::regex localAtomicAPI(
        R"(@air\.atomic\.local\.(?:load|store|exchange|and|or|xor|(?:add|sub|min|max)\.[su])\.i(?:32|64)\()" );
    static const std::regex opaqueLocalOperand(R"(\bptr\b)");
    const bool localAtomicOperation=instruction.find("addrspace(3)*")!=std::string::npos &&
        (instruction.find("atomicrmw ")!=std::string::npos || instruction.find("cmpxchg ")!=std::string::npos ||
         (instruction.find("@air.atomic.local.")!=std::string::npos && ImmutableSyntaxMatch(instruction,localAtomicAPI, true)));
    bool nativeLocalAtomic=false;
    if(localAtomicOperation)
    {
      std::string localOperands=instruction;
      for(size_t position=0;(position=localOperands.find("addrspace(3)*",position))!=std::string::npos;)
        localOperands.erase(position,13);
      nativeLocalAtomic=localOperands.find('*')==std::string::npos && !ImmutableSyntaxMatch(localOperands,opaqueLocalOperand, true);
    }
    const bool atomicBuffer=!nativeLocalAtomic && !nativeSynchronization && instruction.find("_texture_")==std::string::npos &&
        ((instruction.find("call ")!=std::string::npos && (instruction.find("@air.atomic_")!=std::string::npos ||
          instruction.find("@air.atomic.")!=std::string::npos)) ||
         instruction.find("atomicrmw ")!=std::string::npos || instruction.find("cmpxchg ")!=std::string::npos);
    const bool atomicTexture=!nativeSynchronization && instruction.find("call ")!=std::string::npos &&
        instruction.find("_texture_")!=std::string::npos && instruction.find("@air.atomic_")!=std::string::npos;
    const bool atomicStore=(atomicBuffer || atomicTexture) &&
        (instruction.find(".store.")!=std::string::npos || instruction.find("_store_")!=std::string::npos);
    const bool atomicLoad=(atomicBuffer || atomicTexture) &&
        (instruction.find(".load.")!=std::string::npos || instruction.find("_load_")!=std::string::npos);
    if(atomicBuffer)
    {
      if(!atomicStore)report.bufferReads++;
      if(!atomicLoad)report.bufferWrites++;
      unsigned operands=0;
      for(std::sregex_iterator i(instruction.begin(),instruction.end(),atomicBufferArg),end;i!=end;++i)
      {
        operands++;auto address=value((*i)[3]);const auto size=std::stoul((*i)[2])/8;
        if(!atomicStore)AppendBufferCandidates(report,address,size,false);
        if(!atomicLoad)AppendBufferCandidates(report,address,size,true);
      }
      if(operands!=1)report.unresolvedBuffers++;
    }
    // Include conditional call sites, like static API bindings. Writable texture operations
    // use the same resource identity as reads, but must remain UAV/ReadWrite descriptors.
    // Depth handles use their own opaque AIR types. Sampling/compare remains
    // the original GPU operation; only resource and sampler effects are needed
    // for restoration. Coordinates, reference values and compare results are
    // not interpreted as CPU numerical facts or guaranteed output producers.
    std::smatch depthCall;
    static const std::regex depthOperation(
        R"(^air\.(sample|sample_compare|gather|gather_compare|read)_depth_(2d|2d_array|cube|cube_array)\.(?:[^ ]+)$)");
    const bool depthTextureCall=callInstruction && ImmutableSyntaxMatch(instruction,depthCall,functionCall) &&
        ImmutableSyntaxMatch(depthCall[1].str(),depthOperation);
    const bool depthSamplerCall=depthTextureCall &&
        depthCall[1].str().find("air.read_")!=0;
    const bool resourceQuery=instruction.find("call ")!=std::string::npos &&
        instruction.find("@air.get_")!=std::string::npos &&
        (instruction.find("_texture_")!=std::string::npos || instruction.find("_depth_")!=std::string::npos);
    static const std::regex textureFenceCall(
        R"(^  (?:tail )?call void @air\.fence_texture_(1d|1d_array|2d|2d_array|3d|buffer|buffer_1d)\(%struct\._texture_\1_t addrspace\(1\)\* (?:nocapture |nonnull |noundef )*%[-a-zA-Z$._0-9]+\)(?: .*)?$)");
    const bool textureFence=callInstruction && ImmutableSyntaxMatch(instruction,textureFenceCall);
    const bool textureOperation=resourceQuery || textureFence || depthTextureCall || instruction.find("@air.sample_texture_") != std::string::npos ||
       instruction.find("@air.gather_texture_") != std::string::npos ||
       instruction.find("@air.read_texture_") != std::string::npos ||
       instruction.find("@air.write_texture_") != std::string::npos ||
       atomicTexture;
    if(textureOperation)
    {
      report.textureCalls++;
      unsigned operands=0;
      for(std::sregex_iterator i(instruction.begin(), instruction.end(), textureArg), end; i != end; ++i)
      {
        operands++;
        Value texture = value((*i)[1]);
        if(texture.kind == Value::Texture)
        {
          texture.resourceOnly=resourceQuery || textureFence;
          texture.ordering=textureFence;
          texture.write = instruction.find("@air.write_texture_") != std::string::npos ||
                          (atomicTexture && !atomicLoad);
          texture.read = !resourceQuery && !textureFence && (!texture.write || (atomicTexture && !atomicStore));
          if(instruction.find(".u.")!=std::string::npos)texture.numeric='u';
          else if(instruction.find(".s.")!=std::string::npos)texture.numeric='s';
          else if(instruction.find("f32")!=std::string::npos || instruction.find("f16")!=std::string::npos)
            texture.numeric='f';
          report.accesses.push_back(texture);
        }
        else report.unresolvedTextures++;
      }
      if(operands!=1)report.unresolvedTextures++;
    }
    if(depthSamplerCall || instruction.find("@air.sample_texture_") != std::string::npos ||
       instruction.find("@air.gather_texture_") != std::string::npos)
    {
      report.samplerCalls++;
      unsigned operands=0;
      for(std::sregex_iterator i(instruction.begin(), instruction.end(), samplerArg), end; i!=end; ++i)
      {
        operands++;
        Value sampler=value((*i)[1]);
        if(sampler.kind==Value::Sampler)report.accesses.push_back(sampler);
        else report.unresolvedSamplers++;
      }
      if(operands!=1)report.unresolvedSamplers++;
    }
    if(instruction.find("call ") != std::string::npos &&
       instruction.find("@air.reset_intersection_query.") != std::string::npos)
    {
      report.queryResets++;
      unsigned operands = 0;
      for(std::sregex_iterator i(instruction.begin(), instruction.end(), structureArg), end; i != end; ++i)
      {
        operands++;
        Value structure = value((*i)[1]);
        if(structure.kind == Value::AccelerationStructure) report.accesses.push_back(structure);
        else report.unresolvedStructures++;
      }
      if(operands != 1) report.unresolvedStructures++;
    }
    // Native query state is private shader memory. Retain its provenance but
    // never evaluate intersections or scalar results. Reset separately records
    // the real AS operand and its unresolved-source error above.
    bool nativeQuery=false;
    // Classify the supported triangle/instancing API by its actual value ABI,
    // not by which getter a particular shader happens to use. Pointer-returning
    // primitive-data access and output-pointer APIs need their own restoration
    // proof and must not inherit the register-only classification.
    static const std::regex queryOperation(
        R"(^air\.(.*)_intersection_query\.instancing\.triangle_data$)");
    static const std::regex queryIDs(
        R"(^get_(?:candidate|committed)_(?:intersection_type|geometry_id|primitive_id|instance_id|user_instance_id)$)");
    static const std::regex queryDistances(
        R"(^get_(?:candidate_triangle_distance|committed_distance|ray_min_distance)$)");
    static const std::regex queryRays(
        R"(^get_(?:(?:candidate|committed)_ray|world_space_ray)_(?:origin|direction)$)");
    static const std::regex queryTransforms(
        R"(^get_(?:candidate|committed)_(?:object_to_world|world_to_object)_transform$)");
    static const std::regex queryTriangles(
        R"(^get_(?:candidate|committed)_triangle_barycentric_coord$)");
    static const std::regex queryFacing(
        R"(^is_(?:candidate|committed)_triangle_front_facing$)");
    static const std::regex queryArgument("%struct\\._intersection_query_t\\* (?:nocapture |readonly |nonnull |noundef )*" + symbol);
    std::smatch queryCall;
    if(callInstruction && ImmutableSyntaxMatch(instruction,queryCall,functionCall))
    {
      if(queryCall[1]=="air.allocate_intersection_query.instancing.triangle_data" &&
         queryCall[2].str().empty() &&
         instruction.find("call %struct._intersection_query_t* ")!=std::string::npos)
      {
        std::smatch assigned;
        if(ImmutableSyntaxMatch(instruction,assigned,assignedCall, true))
        {Value handle;handle.kind=Value::NativeQuery;values[assigned[1]]=handle;nativeQuery=true;}
      }
      else
      {
        std::smatch queryMethod;
        const std::string name=queryCall[1];
        std::string resultType;
        bool reset=false;
        if(ImmutableSyntaxMatch(name,queryMethod,queryOperation))
        {
          const std::string method=queryMethod[1];
          reset=method=="reset";
          if(reset || method=="deallocate" || method=="abort" ||
             method=="commit_triangle_intersection")resultType="void";
          else if(method=="next" || ImmutableSyntaxMatch(method,queryFacing))resultType="i1";
          else if(ImmutableSyntaxMatch(method,queryIDs))resultType="i32";
          else if(ImmutableSyntaxMatch(method,queryDistances))resultType="float";
          else if(ImmutableSyntaxMatch(method,queryRays))resultType="<3 x float>";
          else if(ImmutableSyntaxMatch(method,queryTriangles))resultType="<2 x float>";
          else if(ImmutableSyntaxMatch(method,queryTransforms))
            resultType="{ <3 x float>, <3 x float>, <3 x float>, <3 x float> }";
        }
        unsigned handles=0;bool known=true;
        const std::string operands=queryCall[2];
        for(std::sregex_iterator i(operands.begin(),operands.end(),queryArgument),end;i!=end;++i)
        {handles++;known &= value((*i)[1]).kind==Value::NativeQuery;}
        // A supported query method must not smuggle unrelated device pointers.
        unsigned pointers=unsigned(std::count(operands.begin(),operands.end(),'*'));
        // Non-reset methods have exactly one private query argument. A wrong
        // return ABI, extra resource operand or arbitrary get_* name is unknown.
        const auto callStart=instruction.find("call ");
        const auto calleeStart=instruction.find('@',callStart);
        std::string returnABI=callStart!=std::string::npos && calleeStart!=std::string::npos
            ? instruction.substr(callStart+5,calleeStart-callStart-5) : std::string();
        static const std::set<std::string> queryMathFlags={
            "fast","nnan","ninf","nsz","arcp","contract","afn","reassoc"};
        while(queryMathFlags.count(returnABI.substr(0,returnABI.find(' '))))
          returnABI.erase(0,returnABI.find(' ')+1);
        nativeQuery=!resultType.empty() && known && handles==1 &&
            pointers==(reset?2U:1U) &&
            returnABI==resultType+" " &&
            (reset || ImmutableSyntaxMatch(operands,queryArgument));
      }
    }
    // These AIR floating intrinsics cannot read/write resource pointers. Their
    // numerical results remain unknown, so they cannot authorise addresses.
    bool nativeNoResources=false,nativeArgumentMemory=false;
    std::smatch nativeCall;
    if(!resolvedCall && callInstruction && ImmutableSyntaxMatch(instruction,nativeCall,functionCall))
    {
      const auto effect=callEffects.find(nativeCall[1]);
      const std::string operands=nativeCall[2];
      if(effect!=callEffects.end())
      {
        // readnone does not describe texture/AS opaque handles. Accept only
        // pointer-free numerical arguments; returned data remain unknown.
        nativeNoResources=effect->second.none && operands.find('*')==std::string::npos &&
            operands.find("ptr ")==std::string::npos && operands.find("%struct.")==std::string::npos;
        // A no-argument Native readonly call confined to driver-private memory
        // has no captured resource input (e.g. an implicit texture-load sampler).
        // Keep its opaque return unknown; never invent descriptor provenance.
        nativeNoResources |= nativeCall[1].str().compare(0,4,"air.")==0 &&
            effect->second.inaccessibleOnly && effect->second.readOnly && operands.empty();
        // Native inter-lane collectives exchange scalar/vector registers and
        // execute on the original GPU. Their convergent declaration is not a
        // CPU result proof, nor permission for device-pointer arguments.
        nativeNoResources |= nativeCall[1].str().compare(0,9,"air.simd_")==0 &&
            effect->second.convergent && operands.find('*')==std::string::npos &&
            operands.find("ptr ")==std::string::npos && operands.find("%struct.")==std::string::npos;
        if(effect->second.argumentsOnly && !atomicBuffer && !nativeLocalAtomic)
        {
          unsigned found=0,pointerCount=0;
          for(char c:operands)pointerCount+=c=='*';
          for(std::sregex_iterator i(operands.begin(),operands.end(),numericBufferArg),end;i!=end;++i)
          {
            found++;auto address=value((*i)[3]);const auto size=layout((*i)[1],0).size;
            const bool readOnly=effect->second.readOnly || (*i)[2]=="2" ||
                i->str().find("readonly ")!=std::string::npos;
            const bool writeOnly=effect->second.writeOnly || i->str().find("writeonly ")!=std::string::npos;
            // The declaration supplies resource effects, not a byte extent or
            // coverage. Preserve each source without numerical display facts.
            if(!writeOnly){report.bufferReads++;AppendBufferCandidates(report,address,size,false,true);}
            if(!readOnly){report.bufferWrites++;AppendBufferCandidates(report,address,size,true,true);}
          }
          nativeArgumentMemory=found && found==pointerCount;
        }
      }
    }
    // Native fragment discard is a control effect, not an external resource
    // call or a guaranteed producer. Preserve original GPU execution, and do
    // not evaluate its predicate or manufacture covered output bytes.
    const bool nativeFragmentControl=callInstruction && pureFloatingMath &&
        ImmutableSyntaxMatch(instruction,discardInstruction) &&
        ImmutableSyntaxMatch(text,discardDeclaration, true);
    report.conditionalAccesses |= nativeFragmentControl;
    const bool math=callInstruction && pureFloatingMath && ImmutableSyntaxMatch(instruction,floatingMath);
    if(instruction.find("call ") != std::string::npos && !resolvedCall && !math && !nativeFragmentControl && !nativeNoResources && !nativeArgumentMemory && !nativeSynchronization && !nativeLocalAtomic && !nativeQuery && !atomicBuffer && !textureOperation &&
       instruction.find("@air.sample_texture_") == std::string::npos &&
       instruction.find("@air.gather_texture_") == std::string::npos &&
       instruction.find("@air.read_texture_") == std::string::npos &&
       instruction.find("@air.write_texture_") == std::string::npos &&
       instruction.find("@llvm.lifetime.") == std::string::npos)
      report.unresolvedCalls++;
  }
  }
  // A conditional/loop body may skip a store. Such stores are still actual
  // access candidates, but cannot establish bytes for subsequent consumers.
  for(const auto &instruction:body)
    if(instruction.compare(0,5,"  br ")==0 || instruction.find("switch ")!=std::string::npos ||
       instruction.find("indirectbr ")!=std::string::npos || instruction.find("invoke ")!=std::string::npos)
    {
      report.conditionalAccesses=true;
      for(auto &access:report.buffers)
      {access.definiteStore=false;access.definiteWrite=false;}
    }
  return report;
  }
  catch(...) { return {}; }
}
// Inspection shows the resolved subset. Numerical completeness is separate
// from resource/pointer restoration; unknown calls are not proof of no use.
inline std::vector<Value> UniformTextureAccess(const std::string &input,
    const std::string &entry, const std::map<unsigned, Value> &bindings, const Loader &load)
{
  std::vector<Value> textures;
  for(const auto &access : UniformResourceAccess(input, entry, bindings, load).accesses)
    if(access.kind == Value::Texture) textures.push_back(access);
  return textures;
}
}
