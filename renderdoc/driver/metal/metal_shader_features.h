// SPDX-License-Identifier: MIT
#pragma once
#include <regex>
#include <map>
#include <sstream>
#include <functional>
#include <set>
#include <string>

namespace MetalFeatures
{
struct Features
{
  std::set<unsigned> fetch, groups;
  bool imageblock = false, known = false;
};
// AIR metadata IDs restart in each module. Match the entry before reading its metadata;
// a colour output is not a framebuffer input (input nodes start with an argument ordinal).
inline Features AIR(const std::string &text, const std::string &entry)
{
  Features f;
  try
  {
    size_t begin = 0;
    while((begin = text.find("source_filename =", begin)) != std::string::npos)
    {
      size_t end = text.find("source_filename =", begin + 1);
      std::string module = text.substr(begin, end == std::string::npos ? end : end - begin);
      std::map<unsigned, std::string> nodes;
      std::istringstream lines(module);
      std::string line;
      const std::regex node(R"(!([0-9]+) = !\{(.*)\})");
      const std::regex reference(R"(!([0-9]+))");
      unsigned root = ~0U;
      while(std::getline(lines, line))
      {
        std::smatch m;
        if(!std::regex_match(line, m, node)) continue;
        unsigned id = unsigned(std::stoul(m[1]));
        nodes[id] = m[2];
        if(nodes[id].find("@" + entry + ",") != std::string::npos ||
           nodes[id].find("@\"" + entry + "\",") != std::string::npos) root = id;
      }
      if(root != ~0U)
      {
        // Follow only this entry's output/argument metadata. Linked helpers or other
        // entries may coexist in a module and must not contribute their features.
        std::set<unsigned> visited;
        std::string reachable;
        std::function<void(unsigned)> visit = [&](unsigned id) {
          auto n = nodes.find(id);
          if(n == nodes.end() || !visited.insert(id).second) return;
          reachable += "!{" + n->second + "}\n";
          for(std::sregex_iterator i(n->second.begin(), n->second.end(), reference), e; i != e; ++i)
            visit(unsigned(std::stoul((*i)[1])));
        };
        visit(root);
        f.known = true;
        const std::regex input(R"(!\{i32 [0-9]+, !"air.render_target", i32 ([0-7])(?:,|\}))");
        const std::regex group(R"rx(!"air.raster_order_group", i32 ([0-7])(?:,|\}))rx");
        for(std::sregex_iterator i(reachable.begin(), reachable.end(), input), e; i != e; ++i)
          f.fetch.insert(unsigned(std::stoul((*i)[1])));
        for(std::sregex_iterator i(reachable.begin(), reachable.end(), group), e; i != e; ++i)
          f.groups.insert(unsigned(std::stoul((*i)[1])));
        f.imageblock = reachable.find("!\"air.imageblock\"") != std::string::npos;
        return f;
      }
      begin++;
    }
  }
  catch(const std::exception &) { return {}; }
  return f;
}
// Source-only libraries have no AIR bytes. Inspect the declaration and its argument types,
// never every entry in the library. This is explicitly labelled source metadata in the UI.
// Macros/templates are deliberately left unknown rather than guessing their expansion.
inline Features MSL(std::string source, const std::string &entry)
{
  Features f;
  source = std::regex_replace(source, std::regex(R"(/\*[\s\S]*?\*/|//[^\n]*)"), " ");
  std::smatch match;
  if(!std::regex_search(source, match,
      std::regex("\\b(fragment|kernel|vertex|mesh|object)\\s+([^;{}]*?)\\b" + entry + "\\s*\\(")))
    return f;
  size_t start = match.position() + match.length(), end = start;
  unsigned depth = 1;
  for(; end < source.size() && depth; end++)
  { if(source[end] == '(') depth++; else if(source[end] == ')') depth--; }
  if(depth) return f;
  std::string args = source.substr(start, end - start - 1);
  std::string used = std::string(match[2]) + args;
  const std::regex structs(R"(\bstruct\s+([A-Za-z_][A-Za-z_0-9]*)\s*\{([^{}]*)\})");
  // Only structs actually named by the input declaration may contribute fetch inputs.
  std::string inputs = args;
  auto expand = [&](std::string value) {
    std::set<std::string> visited;
    for(unsigned pass = 0; pass < 8; pass++)
      for(std::sregex_iterator i(source.begin(), source.end(), structs), e; i != e; ++i)
      {
        std::string type = (*i)[1];
        if(visited.count(type) || !std::regex_search(value, std::regex("\\b" + type + "\\b"))) continue;
        visited.insert(type); value += " " + std::string((*i)[2]);
      }
    return value;
  };
  inputs = expand(inputs);
  used = expand(used);
  const std::regex colour(R"(\bcolor\s*\(\s*([0-7])\s*\))");
  const std::regex group(R"(\braster_order_group\s*\(\s*([0-7])\s*\))");
  for(std::sregex_iterator i(inputs.begin(), inputs.end(), colour), e; i != e; ++i) f.fetch.insert(unsigned(std::stoul((*i)[1])));
  for(std::sregex_iterator i(used.begin(), used.end(), group), e; i != e; ++i) f.groups.insert(unsigned(std::stoul((*i)[1])));
  f.imageblock = std::regex_search(args, std::regex(R"(\bimageblock\s*<)"));
  // Source presence alone doesn't prove absent features after preprocessing.
  f.known = !f.fetch.empty() || !f.groups.empty() || f.imageblock;
  return f;
}
}
