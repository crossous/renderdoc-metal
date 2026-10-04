/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "common_pipestate.h"

// NOTE: Python sees namespaces flattened to a prefix, so MetalPipe::State is MetalState.
namespace MetalPipe
{
DOCUMENT(R"(
MetalShader()
MetalShader(other: MetalShader)

Describes a shader function bound to a Metal pipeline stage.
)");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;
  Shader &operator=(const Shader &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the Metal function.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(The reflection data for this shader.

:type: ShaderReflection
)");
  const ShaderReflection *reflection = NULL;

  DOCUMENT(R"(The source entry point name.

:type: str
)");
  rdcstr entryPoint;

  DOCUMENT(R"(The pipeline stage this function is bound to.

:type: ShaderStage
)");
  ShaderStage stage = ShaderStage::Vertex;

  // Metal-specific entry metadata. Empty metadataSource means unavailable, not disabled.
  rdcstr metadataSource;
  rdcarray<uint32_t> framebufferFetch;
  rdcarray<uint32_t> rasterOrderGroups;
  bool usesImageblock = false;
};

DOCUMENT(R"(
MetalVertexBuffer()
MetalVertexBuffer(other: MetalVertexBuffer)

Describes a Metal vertex buffer binding.
)");
struct VertexBuffer
{
  DOCUMENT("");
  VertexBuffer() = default;
  VertexBuffer(const VertexBuffer &) = default;
  VertexBuffer &operator=(const VertexBuffer &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the bound buffer.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(The byte offset from the start of the buffer.

:type: int
)");
  uint64_t byteOffset = 0;

  DOCUMENT(R"(The number of bytes available from :data:`byteOffset`.

:type: int
)");
  uint64_t byteSize = 0;

  DOCUMENT(R"(The vertex stride when it is available from a vertex descriptor.

:type: int
)");
  uint32_t byteStride = 0;

  DOCUMENT(R"(Whether this binding advances per instance instead of per vertex.

:type: bool
)");
  bool perInstance = false;

  DOCUMENT(R"(The Metal vertex layout step rate.

:type: int
)");
  uint32_t stepRate = 1;
  rdcstr stepFunction = "PerVertex";
};

DOCUMENT(R"(
MetalBufferBinding()
MetalBufferBinding(other: MetalBufferBinding)

Describes a Metal shader buffer binding.
)");
struct BufferBinding
{
  DOCUMENT("");
  BufferBinding() = default;
  BufferBinding(const BufferBinding &) = default;
  BufferBinding &operator=(const BufferBinding &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the bound buffer.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(The byte offset from the start of the buffer.

:type: int
)");
  uint64_t byteOffset = 0;

  DOCUMENT(R"(The number of bytes available from :data:`byteOffset`.

:type: int
)");
  uint64_t byteSize = 0;
};

DOCUMENT(R"(
MetalArgumentBuffer()
MetalArgumentBuffer(other: MetalArgumentBuffer)

Describes the directly encoded resources referenced by one Metal argument buffer.
)");
struct ArgumentBuffer
{
  DOCUMENT("");
  ArgumentBuffer() = default;
  ArgumentBuffer(const ArgumentBuffer &) = default;
  ArgumentBuffer &operator=(const ArgumentBuffer &) = default;

  DOCUMENT(R"(The argument-buffer allocation and range.

:type: MetalBufferBinding
)");
  BufferBinding buffer;

  DOCUMENT(R"(Direct texture members indexed by argument id.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> textures;

  DOCUMENT(R"(Direct sampler members indexed by argument id.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> samplers;
};

DOCUMENT(R"(
MetalVertexAttribute()
MetalVertexAttribute(other: MetalVertexAttribute)

Describes an attribute in a Metal vertex descriptor.
)");
struct VertexAttribute
{
  DOCUMENT("");
  VertexAttribute() = default;
  VertexAttribute(const VertexAttribute &) = default;
  VertexAttribute &operator=(const VertexAttribute &) = default;

  DOCUMENT(R"(The Metal attribute index.

:type: int
)");
  uint32_t attributeIndex = 0;

  DOCUMENT(R"(The vertex buffer slot used by the attribute.

:type: int
)");
  uint32_t bufferIndex = 0;

  DOCUMENT(R"(The byte offset within each vertex element.

:type: int
)");
  uint32_t byteOffset = 0;

  DOCUMENT(R"(The format of the attribute.

:type: ResourceFormat
)");
  ResourceFormat format;
};

DOCUMENT(R"(
MetalRasterizer()
MetalRasterizer(other: MetalRasterizer)

Describes Metal dynamic rasterizer state used by the current draw.
)");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer()
  {
    viewport.enabled = false;
    scissor.enabled = false;
  }
  Rasterizer(const Rasterizer &) = default;
  Rasterizer &operator=(const Rasterizer &) = default;

  Viewport viewport;
  Scissor scissor;
  rdcarray<Viewport> viewports;
  rdcarray<Scissor> scissors;
  CullMode cullMode = CullMode::NoCull;
  bool frontCCW = false;
  FillMode fillMode = FillMode::Solid;
  bool depthClip = true;
  float depthBias = 0.0f;
  float slopeScaledDepthBias = 0.0f;
  float depthBiasClamp = 0.0f;
  bool rasterizationEnabled = true;
  // VRR is a render-pass state object, not a shading-rate texture.
  ResourceId rasterizationRateMap;
  rdcarray<uint32_t> rateMapScreenSize, rateMapPhysicalSizes;
  rdcarray<rdcarray<float>> rateMapHorizontal, rateMapVertical;
};

DOCUMENT(R"(
MetalDepthStencil()
MetalDepthStencil(other: MetalDepthStencil)

Describes the Metal depth state used by the current draw.
)");
struct DepthStencil
{
  DOCUMENT("");
  DepthStencil() = default;
  DepthStencil(const DepthStencil &) = default;
  DepthStencil &operator=(const DepthStencil &) = default;

  ResourceId resourceId;
  CompareFunction depthFunction = CompareFunction::AlwaysTrue;
  bool depthWrites = false;
  bool stencilEnabled = false;
  StencilFace frontFace;
  StencilFace backFace;
};

DOCUMENT(R"(
The current Metal render pipeline state.
)");
struct State
{
#if !defined(RENDERDOC_EXPORTS)
  State() = delete;
  State(const State &) = delete;
#endif

  DOCUMENT(R"(The currently bound render pipeline state object.

:type: ResourceId
)");
  ResourceId pipelineResourceId;

  // Tile is a render-encoder dispatch path. The common compute shader/bindings below
  // expose its kernel to shared resource and shader viewers, not a compute queue/PSO.
  rdcarray<uint64_t> metalFXSpatial;
  ResourceId metalFXInput, metalFXOutput;
  rdcarray<uint64_t> metalFXTemporal;
  rdcarray<float> metalFXTemporalFloats;
  // Color, depth, motion vectors, exposure, reactive mask. Empty optional slots stay empty.
  rdcarray<ResourceId> metalFXTemporalInputs;
  ResourceId metalFXScaler;
  bool metalFXHistoryUnavailable = false;
  bool tileDispatch = false;
  uint32_t tileWidth = 0, tileHeight = 0;
  uint32_t tileThreads[3] = {};
  uint64_t tileMaxThreads = 0;
  bool tileSizeMatches = false;
  uint64_t imageblockSampleLength = 0, threadgroupMemoryLength = 0;
  rdcarray<uint64_t> tileMemoryLengths, tileMemoryOffsets;
  rdcarray<rdcstr> attachmentStorage, attachmentLoad, attachmentStore, attachmentStoreOptions;
  rdcstr depthStorage, depthLoad, depthStore, stencilStorage, stencilLoad, stencilStore;

  DOCUMENT(R"(The bound compute pipeline, if the current action is a dispatch.

:type: ResourceId
)");
  ResourceId computePipelineResourceId;

  DOCUMENT(R"(The bound compute function.

:type: MetalShader
)");
  Shader computeShader;

  DOCUMENT(R"(The direct compute texture bindings, indexed by Metal texture slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> computeTextures;

  DOCUMENT(R"(The direct compute sampler bindings, indexed by Metal sampler slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> computeSamplers;

  DOCUMENT(R"(The direct compute buffer bindings, indexed by Metal buffer slot.

:type: List[MetalBufferBinding]
)");
  rdcarray<BufferBinding> computeBuffers;

  DOCUMENT(R"(The bound vertex function.

:type: MetalShader
)");
  Shader vertexShader;

  DOCUMENT(R"(The bound fragment function.

:type: MetalShader
)");
  Shader fragmentShader;

  DOCUMENT(R"(The bound object (task/amplification) function.

:type: MetalShader
)");
  Shader taskShader;
  DOCUMENT(R"(The bound mesh function.

:type: MetalShader
)");
  Shader meshShader;
  DOCUMENT(R"(Object-stage buffers, indexed by Metal buffer slot.

:type: List[MetalBufferBinding]
)");
  rdcarray<BufferBinding> taskBuffers;
  DOCUMENT(R"(Mesh-stage buffers, indexed by Metal buffer slot.

:type: List[MetalBufferBinding]
)");
  rdcarray<BufferBinding> meshBuffers;
  DOCUMENT(R"(Object-stage textures, indexed by Metal texture slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> taskTextures;
  DOCUMENT(R"(Mesh-stage textures, indexed by Metal texture slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> meshTextures;
  DOCUMENT(R"(Object-stage samplers, indexed by Metal sampler slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> taskSamplers;
  DOCUMENT(R"(Mesh-stage samplers, indexed by Metal sampler slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> meshSamplers;
  DOCUMENT(R"(Compute-stage acceleration structures, indexed by Metal binding slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> computeAccelerationStructures;
  DOCUMENT(R"(Vertex-stage acceleration structures, indexed by Metal binding slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> vertexAccelerationStructures;
  DOCUMENT(R"(Fragment-stage acceleration structures, indexed by Metal binding slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> fragmentAccelerationStructures;

  DOCUMENT(R"(The current primitive topology.

:type: Topology
)");
  Topology topology = Topology::Unknown;

  DOCUMENT(R"(The current vertex buffer bindings, indexed by Metal buffer slot.

:type: List[MetalVertexBuffer]
)");
  rdcarray<VertexBuffer> vertexBuffers;

  DOCUMENT(R"(The vertex-stage storage buffer bindings, indexed by Metal buffer slot.

:type: List[MetalBufferBinding]
)");
  rdcarray<BufferBinding> vertexStorageBuffers;

  DOCUMENT(R"(The attributes from the currently bound Metal vertex descriptor.

:type: List[MetalVertexAttribute]
)");
  rdcarray<VertexAttribute> vertexAttributes;

  DOCUMENT(R"(The vertex-stage texture bindings, indexed by Metal texture slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> vertexTextures;

  DOCUMENT(R"(The vertex-stage sampler bindings, indexed by Metal sampler slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> vertexSamplers;

  DOCUMENT(R"(The fragment-stage buffer bindings, indexed by Metal buffer slot.

:type: List[MetalBufferBinding]
)");
  rdcarray<BufferBinding> fragmentBuffers;

  DOCUMENT(R"(The directly encoded fragment argument buffers, indexed by Metal buffer slot.

:type: List[MetalArgumentBuffer]
)");
  rdcarray<ArgumentBuffer> fragmentArgumentBuffers;

  DOCUMENT(R"(The fragment-stage texture bindings, indexed by Metal texture slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> fragmentTextures;

  DOCUMENT(R"(The fragment-stage sampler bindings, indexed by Metal sampler slot.

:type: List[ResourceId]
)");
  rdcarray<ResourceId> fragmentSamplers;

  DOCUMENT(R"(The index buffer used by the current indexed draw.

:type: MetalVertexBuffer
)");
  VertexBuffer indexBuffer;

  DOCUMENT(R"(The argument buffer region used by the current indirect draw.

:type: MetalBufferBinding
)");
  BufferBinding indirectBuffer;

  DOCUMENT(R"(The dynamic rasterizer state used by the current draw.

:type: MetalRasterizer
)");
  Rasterizer rasterizer;

  DOCUMENT(R"(The bound depth-stencil state.

:type: MetalDepthStencil
)");
  DepthStencil depthStencil;

  DOCUMENT(R"(The raster sample count of the current render pipeline.

:type: int
)");
  uint32_t sampleCount = 1;

  DOCUMENT(R"(Whether alpha-to-coverage is enabled in the current render pipeline.

:type: bool
)");
  bool alphaToCoverageEnabled = false;

  DOCUMENT(R"(Whether alpha-to-one is enabled in the current render pipeline.

:type: bool
)");
  bool alphaToOneEnabled = false;

  DOCUMENT(R"(The current color render targets.

:type: List[Descriptor]
)");
  rdcarray<Descriptor> colorTargets;

  DOCUMENT(R"(The resolve targets paired with the current color render targets.

:type: List[Descriptor]
)");
  rdcarray<Descriptor> resolveTargets;

  DOCUMENT(R"(The blend configuration for each color attachment in the render pipeline.

:type: List[ColorBlend]
)");
  rdcarray<ColorBlend> colorBlends;

  DOCUMENT(R"(The current depth render target.

:type: Descriptor
)");
  Descriptor depthTarget;

  // Inspection metadata only: native Metal state is still replayed by its captured API calls.
  float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  uint32_t patchControlPoints = 0;
  BufferBinding tessellationFactors;
  uint64_t tessellationInstanceStride = 0;
  rdcstr tessellationPartitionMode;
  rdcstr tessellationStepFunction;
  rdcstr tessellationOutputWinding;
  uint32_t maxTessellationFactor = 0;
  bool tessellationFactorScaleEnabled = false;
  float tessellationFactorScale = 1.0f;
};
};    // namespace MetalPipe

DECLARE_REFLECTION_STRUCT(MetalPipe::Shader);
DECLARE_REFLECTION_STRUCT(MetalPipe::VertexBuffer);
DECLARE_REFLECTION_STRUCT(MetalPipe::BufferBinding);
DECLARE_REFLECTION_STRUCT(MetalPipe::ArgumentBuffer);
DECLARE_REFLECTION_STRUCT(MetalPipe::VertexAttribute);
DECLARE_REFLECTION_STRUCT(MetalPipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(MetalPipe::DepthStencil);
DECLARE_REFLECTION_STRUCT(MetalPipe::State);
