// SPDX-License-Identifier: MIT
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
#include "renderdoc/driver/metal/metal_shader_features.h"
int main(int argc,char **argv)
{
  if(argc!=2)return 1;
  std::ifstream file(argv[1]);std::string air((std::istreambuf_iterator<char>(file)),{});
  auto fetch=MetalFeatures::AIR(air,"fetch"), plain=MetalFeatures::AIR(air,"plain"), tile=MetalFeatures::AIR(air,"tile_image");
  assert(fetch.known && fetch.fetch==std::set<unsigned>({0}) && fetch.groups==std::set<unsigned>({2}));
  assert(plain.known && plain.fetch.empty() && plain.groups.empty());
  assert(tile.known && tile.imageblock && tile.fetch.empty());
  // Multiple entries in one module, with cyclic/disconnected metadata: follow only the entry graph.
  const std::string shared=R"(source_filename = "shared"
!1 = !{void ()* @first, !2, !3}
!2 = !{!4}
!3 = !{!3}
!4 = !{!"air.render_target", i32 0, i32 0}
!5 = !{void ()* @second, !2, !6}
!6 = !{!7}
!7 = !{i32 0, !"air.render_target", i32 1, !"air.raster_order_group", i32 7}
)";
  auto first=MetalFeatures::AIR(shared,"first"), second=MetalFeatures::AIR(shared,"second");
  assert(first.known && first.fetch.empty() && first.groups.empty());
  assert(second.fetch==std::set<unsigned>({1}) && second.groups==std::set<unsigned>({7}));
  const std::string source=R"(struct Colour { half4 c [[color(0),raster_order_group(2)]]; };
fragment Colour only_output() { return {}; }
fragment Colour input(Colour previous) { return previous; }
fragment half4 unrelated() { return half4(1); }
// fragment Colour commented(Colour fake) {}
)";
  auto output=MetalFeatures::MSL(source,"only_output"), input=MetalFeatures::MSL(source,"input"), unrelated=MetalFeatures::MSL(source,"unrelated");
  assert(output.fetch.empty() && output.groups==std::set<unsigned>({2}));
  assert(input.fetch==std::set<unsigned>({0}) && input.groups==std::set<unsigned>({2}));
  assert(unrelated.fetch.empty() && unrelated.groups.empty() && !MetalFeatures::MSL(source,"commented").known);
  puts("PASS actual compiler AIR, entry isolation, metadata cycles, input/output distinction and source comments");
}
