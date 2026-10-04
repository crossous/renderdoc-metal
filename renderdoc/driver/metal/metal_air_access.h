// SPDX-License-Identifier: MIT
// Bounded uniform-address evaluation of AIR. Unknown expressions stay unknown;
// this is static descriptor access, not per-invocation GPU feedback.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace MetalAIR
{
struct Value
{
  enum Kind { Unknown, Integer, Pointer, Texture } kind = Unknown;
  uint64_t object = 0, offset = 0;
  int slot = -1;
  bool write = false;
  static Value Number(uint64_t n) { Value v; v.kind = Integer; v.offset = n; return v; }
};
using Loader = std::function<Value(const Value &, unsigned, Value::Kind)>;
// Reads only scalar integer/pointer dependencies. No shader arithmetic or GPU writes run here.
inline std::vector<Value> UniformTextureAccess(const std::string &input,
    const std::string &entry, const std::map<unsigned, Value> &bindings, const Loader &load)
{
  try
  {
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
      definitions++;
      inside = line.find("@" + entry + "(") != std::string::npos;
      if(inside) signature = line;
    }
    else if(inside && line == "}") inside = false;
    else if(inside) body.push_back(line);
  }
  // Metadata argument ordinals must belong to this function, not a linked helper.
  if(definitions != 1 || signature.empty()) return {};
  const std::regex argument(R"((%[-a-zA-Z$._0-9]+)(?:,|\)))");
  std::vector<std::string> args;
  for(std::sregex_iterator i(signature.begin(), signature.end(), argument), end; i != end; ++i)
    args.push_back((*i)[1]);
  const std::regex binding(R"(!\{i32 ([0-9]+), !"air.indirect_buffer", (?:!"air.buffer_size", i32 [0-9]+, )?!"air.location_index", i32 ([0-9]+),)");
  for(std::sregex_iterator i(text.begin(), text.end(), binding), end; i != end; ++i)
  {
    uint64_t arg = std::stoull((*i)[1]), slot = std::stoull((*i)[2]);
    if(slot > 30) continue;
    auto b = bindings.find(unsigned(slot));
    if(arg < args.size() && b != bindings.end()) values[args[arg]] = b->second;
  }
  const std::string symbol = R"((%[-a-zA-Z$._0-9]+))";
  const std::regex assign("^  " + symbol + " = (.*)$");
  const std::regex cast("^bitcast .* " + symbol + " to .*$");
  const std::regex gep("^getelementptr(?: inbounds)? i(8|16|32|64), .* " + symbol + ", i(?:32|64) ([^ ,]+)$");
  const std::regex scalar("^(mul|add|and|shl) i(32|64) ([^ ,]+), ([^ ,]+)$");
  const std::regex extend("^(zext|sext|trunc) i(32|64) ([^ ,]+) to i(32|64)$");
  const std::regex read("^load (.*), .* " + symbol + ", align [0-9]+.*$");
  const std::regex textureArg("%struct\\._texture_[^ ]+ addrspace\\([0-9]+\\)\\* (?:nocapture |readonly |readnone |nonnull |noundef )*" + symbol);
  // Native MSL retains typed struct GEPs, whereas UE's converted AIR mostly uses byte
  // GEPs. Resolve only layouts composed of known scalar/pointer/struct fields.
  struct Layout { uint64_t size = 0, align = 1; std::vector<std::string> fields; std::vector<uint64_t> offsets; };
  std::map<std::string, std::string> types;
  const std::regex typeDefinition(R"((%[^\n=]+) = type \{ ([^\n]+) \})");
  for(std::sregex_iterator i(text.begin(), text.end(), typeDefinition), end; i != end; ++i)
    types[(*i)[1]] = (*i)[2];
  std::function<Layout(std::string, unsigned)> layout = [&](std::string type, unsigned depth) -> Layout {
    Layout result;
    if(depth > 16 || type.empty()) return result;
    if(type.back() == '*') { result.size = result.align = 8; return result; }
    if(type == "i8") { result.size = result.align = 1; return result; }
    if(type == "i16" || type == "half") { result.size = result.align = 2; return result; }
    if(type == "i32" || type == "float") { result.size = result.align = 4; return result; }
    if(type == "i64" || type == "double") { result.size = result.align = 8; return result; }
    auto named = types.find(type);
    if(named == types.end()) return result;
    std::istringstream fields(named->second);
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
  const std::regex structGEP("^getelementptr(?: inbounds)? (%[^,]+), [^,]* " + symbol + "((?:, i(?:32|64) [^,]+)+)$");
  const std::regex gepIndex(R"(, i(?:32|64) ([^, ]+))");
  auto value = [&](const std::string &s) -> Value {
    if(!s.empty() && s[0] == '%') { auto v = values.find(s); return v == values.end() ? Value() : v->second; }
    if(s.empty() || s.find_first_not_of("-0123456789") != std::string::npos) return {};
    try { return Value::Number(std::stoull(s)); } catch(...) { return {}; }
  };
  std::vector<Value> used;
  for(const std::string &instruction : body)
  {
    std::smatch match, operation;
    if(std::regex_match(instruction, match, assign))
    {
      std::string expr = match[2]; Value result;
      if(std::regex_match(expr, operation, cast)) result = value(operation[1]);
      else if(std::regex_match(expr, operation, gep))
      {
        Value base = value(operation[2]), index = value(operation[3]);
        unsigned stride = unsigned(std::stoul(operation[1]) / 8);
        if(base.kind == Value::Pointer && index.kind == Value::Integer &&
           index.offset <= (UINT64_MAX - base.offset) / stride)
        { result = base; result.offset += index.offset * stride; }
      }
      else if(std::regex_match(expr, operation, structGEP))
      {
        Value base = value(operation[2]);
        std::string type = operation[1], indices = operation[3];
        uint64_t offset = 0; bool known = base.kind == Value::Pointer, first = true;
        for(std::sregex_iterator i(indices.begin(), indices.end(), gepIndex), end; i != end; ++i)
        {
          Value index = value((*i)[1]); Layout current = layout(type, 0);
          if(!known || index.kind != Value::Integer || !current.size) { known = false; break; }
          uint64_t delta;
          if(first)
          {
            if(index.offset > UINT64_MAX / current.size) { known = false; break; }
            delta = index.offset * current.size; first = false;
          }
          else
          {
            if(index.offset >= current.fields.size()) { known = false; break; }
            delta = current.offsets[size_t(index.offset)]; type = current.fields[size_t(index.offset)];
          }
          if(delta > UINT64_MAX - offset) { known = false; break; }
          offset += delta;
        }
        if(known && offset <= UINT64_MAX - base.offset)
        { result = base; result.offset += offset; }
      }
      else if(std::regex_match(expr, operation, scalar))
      {
        Value a = value(operation[3]), b = value(operation[4]);
        if(a.kind == Value::Integer && b.kind == Value::Integer)
        {
          std::string op = operation[1]; uint64_t n = 0;
          if(op == "mul") n = a.offset * b.offset;
          else if(op == "add") n = a.offset + b.offset;
          else if(op == "and") n = a.offset & b.offset;
          else if(b.offset < std::stoul(operation[2])) n = a.offset << b.offset;
          else { values[match[1]] = {}; continue; }
          result = Value::Number(operation[2] == "32" ? uint32_t(n) : n);
        }
      }
      else if(std::regex_match(expr, operation, extend))
      {
        Value a = value(operation[3]);
        if(a.kind == Value::Integer)
        {
          uint64_t n = operation[2] == "32" ? uint32_t(a.offset) : a.offset;
          if(operation[1] == "sext" && operation[2] == "32") n = int64_t(int32_t(n));
          result = Value::Number(operation[4] == "32" ? uint32_t(n) : n);
        }
      }
      else if(std::regex_match(expr, operation, read))
      {
        Value address = value(operation[2]); std::string type = operation[1];
        if(address.kind == Value::Pointer)
        {
          if(type.find("%struct._texture_") == 0 && type.back() == '*')
            result = load(address, 8, Value::Texture);
          else if(type == "i32" || type == "i64")
            result = load(address, type == "i32" ? 4 : 8, Value::Integer);
          else if(!type.empty() && type.back() == '*') result = load(address, 8, Value::Pointer);
        }
      }
      values[match[1]] = result;
    }
    // Include conditional call sites, like static API bindings. Writable texture operations
    // use the same resource identity as reads, but must remain UAV/ReadWrite descriptors.
    if(instruction.find("@air.sample_texture_") != std::string::npos ||
       instruction.find("@air.read_texture_") != std::string::npos ||
       instruction.find("@air.write_texture_") != std::string::npos ||
       instruction.find("@air.atomic_") != std::string::npos)
      for(std::sregex_iterator i(instruction.begin(), instruction.end(), textureArg), end; i != end; ++i)
      {
        Value texture = value((*i)[1]);
        if(texture.kind == Value::Texture)
        {
          texture.write = instruction.find("@air.write_texture_") != std::string::npos ||
                          instruction.find("@air.atomic_") != std::string::npos;
          used.push_back(texture);
        }
      }
  }
  return used;
  }
  catch(...) { return {}; }
}
}
