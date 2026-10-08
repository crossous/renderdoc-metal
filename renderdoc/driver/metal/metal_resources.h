/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2026 Baldur Karlsson
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

#include "core/resource_manager.h"
#include "metal_common.h"
#include "metal_indirect_readback.h"
#include "metal_render_indirect_readback.h"
#include "metal_types.h"
#include <memory>

struct MetalResourceRecord;
class WrappedMTLDevice;
class MetalResourceManager;

enum MetalResourceType
{
  eResUnknown = 0,
  eResBuffer,
  eResCommandBuffer,
  eResCommandQueue,
  eResDevice,
  eResDepthStencilState,
  eResLibrary,
  eResFunction,
  eResRenderPipelineState,
  eResTexture,
  eResRenderCommandEncoder,
  eResBlitCommandEncoder,
  eResComputePipelineState,
  eResComputeCommandEncoder,
  eResArgumentEncoder,
  eResSamplerState,
  eResIndirectCommandBuffer,
  eResIndirectRenderCommand,
  eResFence,
  eResEvent,
  eResHeap,
  eResRasterizationRateMap,
  eResCounterSampleBuffer,
  eResDynamicLibrary,
  eResFunctionHandle,
  eResVisibleFunctionTable,
  eResIntersectionFunctionTable,
  eResAccelerationStructure,
  eResAccelerationStructureCommandEncoder,
  eResBinaryArchive,
  eResParallelRenderCommandEncoder,
  eResMax
};

DECLARE_REFLECTION_ENUM(MetalResourceType);

struct WrappedMTLObject
{
  WrappedMTLObject() = delete;
  WrappedMTLObject(WrappedMTLDevice *wrappedMTLDevice, CaptureState &captureState)
      : m_Real(NULL), m_Device(wrappedMTLDevice), m_State(captureState)
  {
  }
  WrappedMTLObject(void *mtlObject, ResourceId objId, WrappedMTLDevice *wrappedMTLDevice,
                   CaptureState &captureState)
      : m_Real(mtlObject), m_ID(objId), m_Device(wrappedMTLDevice), m_State(captureState)
  {
  }
  ~WrappedMTLObject() = default;

  MTL::Device *GetDevice() { return (MTL::Device *)m_Device; }
  MetalResourceManager *GetResourceManager();
  void AddEvent();
  void AddAction(const ActionDescription &a);

  void *m_ObjcBridge = NULL;
  void *m_Real;
  ResourceId m_ID;
  MetalResourceType m_Type = eResUnknown;
  bool m_OwnsReal = false;
  bool m_RetainsDeviceBridge = false;
  // A native GPU identity is stable for this object's lifetime. Retain its diagnostic
  // record for future captures, without adding a chunk on every descriptor update.
  int32_t m_CapturedAliasable = 0;
  int32_t m_CapturedGPUIdentity = 0;
  MetalResourceRecord *m_Record = NULL;
  WrappedMTLDevice *m_Device;
  CaptureState &m_State;
};

ResourceId GetResID(WrappedMTLObject *obj);

inline ResourceId GetResID(WrappedMTLResource *obj)
{
  return GetResID((WrappedMTLObject *)obj);
}

template <typename WrappedType>
MetalResourceRecord *GetRecord(WrappedType *obj)
{
  if(obj == NULL)
    return NULL;

  return obj->m_Record;
}

template <typename RealType>
RealType Unwrap(WrappedMTLObject *obj)
{
  if(obj == NULL)
    return RealType();

  return (RealType)obj->m_Real;
}

// template magic voodoo to unwrap types
template <typename inner>
struct UnwrapHelper
{
};

#define WRAPPED_TYPE_HELPERS(CPPTYPE)          \
  template <>                                  \
  struct UnwrapHelper<MTL::CPPTYPE *>          \
  {                                            \
    typedef CONCAT(WrappedMTL, CPPTYPE) Outer; \
  };                                           \
  extern MTL::CPPTYPE *Unwrap(WrappedMTL##CPPTYPE *obj);

METALCPP_WRAPPED_PROTOCOLS(WRAPPED_TYPE_HELPERS)
#undef WRAPPED_TYPE_HELPERS

inline MTL::Resource *Unwrap(WrappedMTLResource *obj)
{
  return Unwrap<MTL::Resource *>((WrappedMTLObject *)obj);
}

enum class MetalCmdBufferStatus : uint8_t
{
  Unknown,
  Enqueued,
  Committed,
  Submitted,
};

struct MetalCapturedComputeIndirectArguments
{
  ResourceId command, encoder, buffer;
  uint64_t offset = 0, epoch = 0;
  uint32_t ordinal = 0;
  MetalIndirectReadback readback;
};

struct MetalCapturedRenderIndirectArguments
{
  ResourceId command,encoder,pass,buffer;
  uint64_t epoch=0,offset=0;
  uint32_t ordinal=0,wordCount=0;
  bool writesDeclared=false;
  MetalIndirectReadback readback;
};

// Immutable AS rebuild input and the native submission whose status proves completion.
// No callback captures this snapshot or retains the AS wrapper.
struct MetalASInitialBuild
{
  uint32_t kind = 1; // 1=triangle, 2=indexed, 3=boxes, 5=direct TLAS, 8=multi-indexed, 9=indirect TLAS
  bool compacted = false;
  ResourceId sizeSource;
  rdcarray<uint64_t> sizeParameters;
  ResourceId source;
  ResourceId indexSource;
  bytebuf indices;
  rdcarray<ResourceId> children;
  rdcarray<std::shared_ptr<MetalASInitialBuild>> childBuilds;
  // Queried AS identities, associated with the typed children, never guessed
  // from untyped buffer words. Capture-only; replay derives them from metadata.
  rdcarray<uint64_t> childGPUIdentities;
  rdcarray<uint64_t> parameters;
  bytebuf vertices;
  NS::SharedPtr<MTL::CommandBuffer> submission;
  // Internal GPU copies at an AS-only submission boundary. Materialise only
  // after the submission and copied-source dependencies have completed.
  NS::SharedPtr<MTL::Buffer> inputReadback;
  NS::SharedPtr<MTL::Buffer> indexReadback;
  rdcarray<NS::SharedPtr<MTL::CommandBuffer>> dependencies;
};

static constexpr size_t MetalMaxIndirectASChildren = 1024;
bool ValidMetalASInitialInstances(const bytebuf &bytes, uint64_t count, size_t children,
    uint64_t descriptorType = uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeDefault),
    size_t childLimit = 4);
bool MetalASInitialInstanceSpan(const rdcarray<uint64_t> &parameters,
    uint64_t length, uint64_t &packedBytes);
bool PackMetalASInitialInstances(const byte *data, uint64_t length,
    const rdcarray<uint64_t> &parameters, size_t children, bytebuf &packed);
bool ValidMetalASPrivateInstanceInput(WrappedMTLBuffer *input);
bool ValidMetalASInactiveInstances(const bytebuf &raw, uint64_t count);
bool PackMetalASInactiveInstances(const byte *data, uint64_t length,
    const rdcarray<uint64_t> &parameters, bytebuf &raw);
bool ValidMetalASEmptyIndirectParameters(const rdcarray<uint64_t> &parameters, uint64_t length);
bool MetalASIndirectInstanceSpan(const rdcarray<uint64_t> &parameters,
    uint64_t length, uint64_t &packedBytes);
bool ConvertMetalASIndirectInstances(const bytebuf &raw, uint64_t count,
    const rdcarray<uint64_t> &identities, bytebuf &userIDInstances);
bool PackMetalASIndirectInstances(const byte *data, uint64_t length,
    const rdcarray<uint64_t> &parameters, const rdcarray<uint64_t> &identities, bytebuf &packed);
bool MetalASInitialBoxSpan(const rdcarray<uint64_t> &parameters, uint64_t &bytes);
bool ValidMetalASInitialBoxes(const rdcarray<uint64_t> &parameters, const bytebuf &boxes);
bool ValidMetalASInstance(const MTL::AccelerationStructureInstanceDescriptor &data, size_t children);
bool MaterialiseMetalASInitialBuild(const std::shared_ptr<MetalASInitialBuild> &build);
bool MetalASInitialIndexedSpan(const rdcarray<uint64_t> &parameters,
    const bytebuf &indices, uint64_t &vertexBytes);
// Ten values per indexed geometry, preserving order and shared input buffer offsets.
bool ValidMetalASMultiIndexedParameters(const rdcarray<uint64_t> &parameters,
    uint64_t vertexLength, uint64_t indexLength);
bool ValidMetalASMultiIndexedInputs(const rdcarray<uint64_t> &parameters,
    const bytebuf &vertices, const bytebuf &indices);
MTL::PrimitiveAccelerationStructureDescriptor *MetalASMultiIndexedDescriptor(
    MTL::Buffer *vertices, MTL::Buffer *indices, const rdcarray<uint64_t> &parameters);

struct MetalASInitialCandidate
{
  ResourceId target;
  ResourceId encoder;
  ResourceId copySource;
  bool compactCopy = false;
  ResourceId refitSource;
  std::shared_ptr<MetalASInitialBuild> priorBuild;
  NS::SharedPtr<MTL::Buffer> input;
  NS::SharedPtr<MTL::Buffer> indexInput;
  std::shared_ptr<MetalASInitialBuild> build;
};

// Frame builds keep their original stream position and API metadata. Their GPU
// input is frozen at encoder end and serialised only after native completion.
struct MetalASFrameBuild
{
  Chunk *chunk = NULL;
  SDChunkMetaData metadata;
  uint32_t metadataFlags = 0;
  ResourceId encoder, target, source, scratch;
  uint64_t scratchOffset = 0;
  std::shared_ptr<MetalASInitialBuild> build;
};
void WriteMetalASFrameBuild(WriteSerialiser &ser, const MetalASFrameBuild &evidence);


struct MetalCmdBufferRecordingInfo
{
  MetalCmdBufferRecordingInfo(WrappedMTLCommandQueue *parentQueue) : queue(parentQueue) {}
  MetalCmdBufferRecordingInfo() = delete;
  MetalCmdBufferRecordingInfo(const MetalCmdBufferRecordingInfo &) = delete;
  MetalCmdBufferRecordingInfo(MetalCmdBufferRecordingInfo &&) = delete;
  MetalCmdBufferRecordingInfo &operator=(const MetalCmdBufferRecordingInfo &) = delete;
  ~MetalCmdBufferRecordingInfo() {}
  WrappedMTLCommandQueue *queue;

  // A submitted command buffer's record can outlive the application's autorelease pool.
  // Keep both the proxy (and therefore its wrapper) and the native buffer alive until the
  // record has been serialised or submitted outside a capture.
  NS::Object *retainedProxy = NULL;
  MTL::CommandBuffer *retainedNative = NULL;
  // A completed present may release the drawable's surface even while the command
  // buffer remains alive. Thumbnail readback needs this acquisition through capture end.
  NS::Object *retainedPresentedTextureProxy = NULL;
  MTL::Texture *retainedPresentedTextureNative = NULL;
  MTL::Drawable *retainedDrawable = NULL;

  // The MetalLayer to present
  CA::MetalLayer *outputLayer = NULL;
  // The texture to present
  WrappedMTLTexture *backBuffer = NULL;
  MetalCmdBufferStatus status = MetalCmdBufferStatus::Unknown;
  uint64_t captureCommitEpoch = 0;
  bool presented = false;
  rdcarray<MetalASInitialCandidate> initialASBuilds;
  rdcarray<MetalASFrameBuild> frameASBuilds;
  rdcarray<MetalCapturedComputeIndirectArguments> indirectArguments;
  Threading::CriticalSection renderIndirectLock;
  std::map<ResourceId,rdcarray<MetalIndirectWriteFootprint>> renderIndirectWrites;
  rdcarray<MetalCapturedRenderIndirectArguments> renderIndirectArguments;
};

struct MetalBufferInfo
{
  MetalBufferInfo() = delete;
  MetalBufferInfo(MTL::StorageMode mode) : storageMode(mode), data(NULL), length(0) {}
  MTL::StorageMode storageMode;
  bytebuf baseSnapshot;
  byte *data;
  size_t length;
};

struct MetalResourceRecord : public ResourceRecord
{
public:
  enum
  {
    NullResource = NULL
  };

  MetalResourceRecord(ResourceId id)
      : ResourceRecord(id, true), m_Resource(NULL), m_Type(eResUnknown), ptrUnion(NULL)
  {
  }
  ~MetalResourceRecord();
  bool MarkResourceFrameReferenced(ResourceId id, FrameRefType type);
  void DiscardBackgroundBufferMarkers();
  bool HasOnlyASInitialCommands();
  void MarkASInitialReferences(WrappedMTLAccelerationStructure *structure);
  WrappedMTLObject *m_Resource;
  MetalResourceType m_Type;
  // Capture-side API identity; a Native parent pointer is not a proxy address.
  ResourceId textureParent;

  // Each entry is only used by specific record types
  union
  {
    void *ptrUnion;                          // for initialisation to NULL
    MetalCmdBufferRecordingInfo *cmdInfo;    // only for command buffers
    MetalBufferInfo *bufInfo;                // only for buffers
  };
};
