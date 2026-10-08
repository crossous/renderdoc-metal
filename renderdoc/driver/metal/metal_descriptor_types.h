// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

namespace MetalDescriptor
{
// Stable capture packet tags. Native resource fields are independently sourced
// by the buffer/texture/sampler factory; a tag is never GPU address provenance.
inline bool Buffer(uint32_t type) { return type<=6; }
inline bool Texture(uint32_t type) { return type>=2 && type<=5; }
inline bool Writable(uint32_t type) { return type==1 || type==3 || type==5; }
inline bool ConstantBuffer(uint32_t type) { return type==6; }
}
