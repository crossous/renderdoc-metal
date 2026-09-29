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

#include "metal_common.h"
#include "metal_core.h"
#include "metal_manager.h"

class WrappedMTLDevice;
class MetalReplay;

struct MetalDrawableInfo
{
  CA::MetalLayer *mtlLayer;
  WrappedMTLTexture *texture;
  NS::UInteger drawableID;
};

class MetalCapturer : public IFrameCapturer
{
public:
  MetalCapturer(WrappedMTLDevice &device) : m_Device(device) {}
  // IFrameCapturer interface
  RDCDriver GetFrameCaptureDriver() { return RDCDriver::Metal; }
  void StartFrameCapture(DeviceOwnedWindow devWnd);
  bool EndFrameCapture(DeviceOwnedWindow devWnd);
  bool DiscardFrameCapture(DeviceOwnedWindow devWnd);

  uint32_t SetObjectAnnotation(void *object, const char *key, RENDERDOC_AnnotationType valueType,
                               uint32_t valueVectorWidth, const RENDERDOC_AnnotationValue *value)
  {
    return 2;
  }
  uint32_t SetCommandAnnotation(void *queueOrCommandBuffer, const char *key,
                                RENDERDOC_AnnotationType valueType, uint32_t valueVectorWidth,
                                const RENDERDOC_AnnotationValue *value)
  {
    return 2;
  }
  // IFrameCapturer interface

private:
  WrappedMTLDevice &m_Device;
};

class WrappedMTLDevice : public WrappedMTLObject
{
public:
  WrappedMTLDevice(MTL::Device *realMTLDevice, ResourceId objId);
  ~WrappedMTLDevice();
  template <typename SerialiserType>
  bool Serialise_MTLCreateSystemDefaultDevice(SerialiserType &ser);
  static WrappedMTLDevice *MTLCreateSystemDefaultDevice(MTL::Device *realMTLDevice);

  // Serialised MTLDevice APIs
  WrappedMTLLibrary *CaptureAsyncLibrary(MTL::Library *real, NS::String *source,
                                        MTL::CompileOptions *options, bool supported);
  template <typename SerialiserType>
  bool Serialise_asyncLibrary(SerialiserType &ser, WrappedMTLLibrary *library,
                              NS::String *source, MTL::CompileOptions *options, bool supported);
  WrappedMTLRenderPipelineState *CaptureAsyncRenderPipeline(MTL::RenderPipelineState *real,
      MTL::RenderPipelineDescriptor *descriptor, MTL::PipelineOption options, MetalChunk chunk);
  WrappedMTLRenderPipelineState *CaptureAsyncTilePipeline(MTL::RenderPipelineState *real,
      MTL::TileRenderPipelineDescriptor *descriptor, WrappedMTLFunction *tileFunction,
      MTL::PipelineOption options, bool supported);
  WrappedMTLRenderPipelineState *CaptureAsyncMeshPipeline(MTL::RenderPipelineState *real,
      MTL::MeshRenderPipelineDescriptor *descriptor, WrappedMTLFunction *objectFunction,
      WrappedMTLFunction *meshFunction, WrappedMTLFunction *fragmentFunction,
      MTL::PipelineOption options, bool supported);
  WrappedMTLComputePipelineState *CaptureAsyncComputePipeline(MTL::ComputePipelineState *real,
      WrappedMTLFunction *function, MTL::PipelineOption options, MetalChunk chunk);
  WrappedMTLComputePipelineState *CaptureAsyncComputeDescriptor(MTL::ComputePipelineState *real,
      MTL::ComputePipelineDescriptor *descriptor, MTL::PipelineOption options);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLFence *, newFence);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLAccelerationStructure *,
                                          newAccelerationStructureWithSize, NS::UInteger size);
  WrappedMTLAccelerationStructure *newAccelerationStructureWithDescriptor(
      MTL::AccelerationStructureDescriptor *descriptor);
  WrappedMTLHeap *WrapNewHeap(MTL::Heap *real, NS::UInteger size,
                              MTL::StorageMode storageMode, MTL::CPUCacheMode cacheMode,
                              MTL::HazardTrackingMode hazardMode, MTL::HeapType type);
  template <typename SerialiserType>
  bool Serialise_newHeap(SerialiserType &ser, WrappedMTLHeap *heap, NS::UInteger size,
                         MTL::StorageMode storageMode, MTL::CPUCacheMode cacheMode,
                         MTL::HazardTrackingMode hazardMode, MTL::HeapType type);
  WrappedMTLRasterizationRateMap *newRasterizationRateMap(
      MTL::RasterizationRateMapDescriptor *descriptor);
  WrappedMTLCounterSampleBuffer *newCounterSampleBuffer(
      MTL::CounterSampleBufferDescriptor *descriptor, NS::Error **error);
  WrappedMTLDynamicLibrary *newDynamicLibrary(WrappedMTLLibrary *library, NS::Error **error);
  WrappedMTLDynamicLibrary *newDynamicLibraryWithURL(NS::URL *url, NS::Error **error);
  WrappedMTLBinaryArchive *newBinaryArchive(MTL::BinaryArchiveDescriptor *descriptor,
                                           NS::Error **error);
  WrappedMTLLibrary *newStitchedLibrary(WrappedMTLFunction *function,
                                       rdcstr graphName, rdcstr functionName,
                                       uint32_t inputIndex, NS::Error **error);
  WrappedMTLLibrary *CaptureAsyncStitchedLibrary(MTL::Library *real,
      WrappedMTLFunction *function, rdcstr graphName, rdcstr functionName,
      uint32_t inputIndex);
  template <typename SerialiserType>
  bool Serialise_newStitchedLibrary(SerialiserType &ser, WrappedMTLLibrary *library,
                                   WrappedMTLFunction *function, rdcstr graphName,
                                   rdcstr functionName, uint32_t inputIndex);
  template <typename SerialiserType>
  bool Serialise_newBinaryArchive(SerialiserType &ser, WrappedMTLBinaryArchive *archive,
                                 bytebuf &data);
  template <typename SerialiserType>
  bool Serialise_newDynamicLibrary(SerialiserType &ser, WrappedMTLDynamicLibrary *dynamic,
                                  WrappedMTLLibrary *library, bool supported);
  template <typename SerialiserType>
  bool Serialise_newDynamicLibraryWithURL(SerialiserType &ser,
                                         WrappedMTLDynamicLibrary *dynamic,
                                         rdcstr origin, rdcstr installName, bytebuf &data);
  template <typename SerialiserType>
  bool Serialise_newCounterSampleBuffer(SerialiserType &ser,
      WrappedMTLCounterSampleBuffer *sampleBuffer, rdcstr counterSetName,
      uint64_t sampleCount, uint64_t storageMode, bool supported);
  template <typename SerialiserType>
  bool Serialise_newRasterizationRateMap(SerialiserType &ser,
      WrappedMTLRasterizationRateMap *rateMap, MTL::Size screenSize,
      rdcarray<float> horizontal, rdcarray<float> vertical, bool supported,
      rdcarray<rdcarray<float>> extraHorizontal = {},
      rdcarray<rdcarray<float>> extraVertical = {});
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLEvent *, newEvent);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLEvent *, newSharedEvent);
  WrappedMTLEvent *ImportSharedEventHandle(MTL::SharedEvent *real, WrappedMTLEvent *source);
  template <typename SerialiserType>
  bool Serialise_importSharedEventHandle(SerialiserType &ser, WrappedMTLEvent *event,
                                         WrappedMTLEvent *source);
  template <typename SerialiserType>
  bool Serialise_setSharedEventInitialValue(SerialiserType &ser, WrappedMTLEvent *event,
                                           uint64_t value);
  WrappedMTLArgumentEncoder *newArgumentEncoderWithArguments(const NS::Array *arguments);
  template <typename SerialiserType>
  bool Serialise_newArgumentEncoderWithArguments(SerialiserType &ser,
      WrappedMTLArgumentEncoder *encoder, rdcarray<uint64_t> descriptors);
  WrappedMTLArgumentEncoder *newArgumentEncoderWithBufferBinding(MTL::BufferBinding *binding);
  template <typename SerialiserType>
  bool Serialise_newArgumentEncoderWithBufferBinding(SerialiserType &ser,
      WrappedMTLArgumentEncoder *encoder, rdcarray<uint64_t> descriptors,
      uint64_t encodedLength, uint64_t alignment, bool supported);
  uint64_t GetReplayEpoch() const { return m_ReplayEpoch; }
  uint64_t GetCaptureEpoch() const { return m_CaptureEpoch; }
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandQueue *, newCommandQueue);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandQueue *, newCommandQueue,
                                          NS::UInteger maxCommandBufferCount);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLLibrary *, newDefaultLibrary);
  WrappedMTLLibrary *newDefaultLibraryWithBundle(NS::Bundle *bundle, NS::Error **error);
  WrappedMTLLibrary *newLibraryWithFile(NS::String *path, NS::Error **error);
  WrappedMTLLibrary *newLibraryWithURL(NS::URL *url, NS::Error **error);
  // dispatch_data_t has different C++/Objective-C++ typedefs; keep the bridge ABI identical.
  WrappedMTLLibrary *newLibraryWithData(void *data, NS::Error **error);
  WrappedMTLLibrary *CaptureLibraryBinary(MTL::Library *real, MetalChunk chunk,
                                         const rdcstr &origin, bytebuf &data);
  template <typename SerialiserType>
  bool Serialise_newLibraryBinary(SerialiserType &ser, WrappedMTLLibrary *library,
                                  rdcstr origin, bytebuf &data);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLLibrary *, newLibraryWithSource,
                                          NS::String *source, MTL::CompileOptions *options,
                                          NS::Error **error);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDepthStencilState *,
                                          newDepthStencilStateWithDescriptor,
                                          RDMTL::DepthStencilDescriptor &descriptor);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLRenderPipelineState *,
                                          newRenderPipelineStateWithDescriptor,
                                          RDMTL::RenderPipelineDescriptor &descriptor,
                                          NS::Error **error);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLComputePipelineState *,
                                          newComputePipelineStateWithFunction,
                                          WrappedMTLFunction *computeFunction, NS::Error **error);
  WrappedMTLRenderPipelineState *newRenderPipelineStateWithDescriptorOptions(
      MTL::RenderPipelineDescriptor *descriptor, MTL::PipelineOption options,
      MTL::AutoreleasedRenderPipelineReflection *reflection, NS::Error **error);
  template <typename SerialiserType>
  bool Serialise_newRenderPipelineStateWithDescriptorOptions(
      SerialiserType &ser, WrappedMTLRenderPipelineState *pipeline,
      RDMTL::RenderPipelineDescriptor &descriptor, MTL::PipelineOption options, bool supported);
  WrappedMTLRenderPipelineState *newTileRenderPipelineState(
      MTL::TileRenderPipelineDescriptor *descriptor, WrappedMTLFunction *tileFunction,
      MTL::PipelineOption options, MTL::AutoreleasedRenderPipelineReflection *reflection,
      NS::Error **error, bool supported, rdcarray<WrappedMTLFunction *> visibleFunctions,
      rdcarray<WrappedMTLBinaryArchive *> binaryArchives = {});
  template <typename SerialiserType>
  bool Serialise_newTileRenderPipelineState(
      SerialiserType &ser, WrappedMTLRenderPipelineState *pipeline,
      WrappedMTLFunction *tileFunction, rdcarray<uint32_t> colorFormats,
      uint64_t sampleCount, uint64_t maxThreads, bool matchesTileSize,
      uint32_t options, bool supported, rdcarray<WrappedMTLFunction *> visibleFunctions,
      rdcarray<WrappedMTLBinaryArchive *> binaryArchives = {});
  WrappedMTLRenderPipelineState *newMeshRenderPipelineState(
      MTL::MeshRenderPipelineDescriptor *descriptor, WrappedMTLFunction *objectFunction,
      WrappedMTLFunction *meshFunction, WrappedMTLFunction *fragmentFunction,
      MTL::PipelineOption options, MTL::AutoreleasedRenderPipelineReflection *reflection,
      NS::Error **error, bool supported,
      rdcarray<WrappedMTLBinaryArchive *> binaryArchives = {});
  template <typename SerialiserType>
  bool Serialise_newMeshRenderPipelineState(
      SerialiserType &ser, WrappedMTLRenderPipelineState *pipeline,
      WrappedMTLFunction *objectFunction, WrappedMTLFunction *meshFunction,
      WrappedMTLFunction *fragmentFunction, rdcarray<uint32_t> colorFormats,
      uint64_t sampleCount, uint64_t maxMeshThreads, uint32_t options, bool supported,
      uint64_t maxMeshGrid = 0,
      rdcarray<WrappedMTLBinaryArchive *> binaryArchives = {});
  template <typename SerialiserType>
  bool Serialise_newObjectMeshPipelineState(
      SerialiserType &ser, WrappedMTLRenderPipelineState *pipeline,
      WrappedMTLFunction *objectFunction, WrappedMTLFunction *meshFunction,
      WrappedMTLFunction *fragmentFunction, rdcarray<uint32_t> colorFormats,
      uint64_t sampleCount, uint64_t maxObjectThreads, uint64_t maxMeshThreads,
      uint64_t payloadLength, uint64_t maxMeshGrid, uint32_t options, bool supported);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(
      WrappedMTLComputePipelineState *, newComputePipelineStateWithFunctionOptions,
      WrappedMTLFunction *computeFunction, MTL::PipelineOption options,
      MTL::AutoreleasedComputePipelineReflection *reflection, NS::Error **error);
  WrappedMTLComputePipelineState *newComputePipelineStateWithDescriptor(
      MTL::ComputePipelineDescriptor *descriptor, MTL::PipelineOption options,
      MTL::AutoreleasedComputePipelineReflection *reflection, NS::Error **error);
  template <typename SerialiserType>
  bool Serialise_newComputePipelineStateWithDescriptor(
      SerialiserType &ser, WrappedMTLComputePipelineState *pipeline,
      RDMTL::ComputePipelineDescriptor &descriptor, MTL::PipelineOption options, bool supported);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLSamplerState *,
                                          newSamplerStateWithDescriptor,
                                          RDMTL::SamplerDescriptor &descriptor);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLIndirectCommandBuffer *,
                                          newIndirectCommandBufferWithDescriptor,
                                          MTL::IndirectCommandType commandTypes,
                                          bool inheritPipelineState, bool inheritBuffers,
                                          NS::UInteger maxVertexBufferBindCount,
                                          NS::UInteger maxFragmentBufferBindCount,
                                          NS::UInteger maxCount, MTL::ResourceOptions options);
  WrappedMTLTexture *newTextureWithDescriptor(RDMTL::TextureDescriptor &descriptor,
                                              IOSurfaceRef iosurface, NS::UInteger plane);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLTexture *, newTextureWithDescriptor,
                                          RDMTL::TextureDescriptor &descriptor);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLTexture *, newSharedTextureWithDescriptor,
                                          RDMTL::TextureDescriptor &descriptor);
  WrappedMTLTexture *WrapNewSharedTextureWithHandle(MTL::Texture *real,
                                                    WrappedMTLTexture *source);
  template <typename SerialiserType>
  bool Serialise_newSharedTextureWithHandle(SerialiserType &ser, WrappedMTLTexture *texture,
                                            WrappedMTLTexture *source);
  WrappedMTLBuffer *newBufferWithLength(NS::UInteger length, MTL::ResourceOptions options);
  WrappedMTLBuffer *WrapNewBufferNoCopy(MTL::Buffer *real, const void *pointer,
                                       NS::UInteger length, MTL::ResourceOptions options);
  template <typename SerialiserType>
  bool Serialise_newBufferWithBytesNoCopy(SerialiserType &ser, WrappedMTLBuffer *buffer,
                                         bytebuf initialData, uint64_t length,
                                         MTL::ResourceOptions options);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLBuffer *, newBufferWithBytes, const void *pointer,
                                          NS::UInteger length, MTL::ResourceOptions options);

  // Non-Serialised MTLDevice APIs
  bool isDepth24Stencil8PixelFormatSupported();
  MTL::ReadWriteTextureTier readWriteTextureSupport();
  MTL::ArgumentBuffersTier argumentBuffersSupport();
  bool areRasterOrderGroupsSupported();
  bool supports32BitFloatFiltering();
  bool supports32BitMSAA();
  bool supportsQueryTextureLOD();
  bool supportsBCTextureCompression();
  bool supportsPullModelInterpolation();
  bool areBarycentricCoordsSupported();
  bool supportsShaderBarycentricCoordinates();
  bool supportsFeatureSet(MTL::FeatureSet featureSet);
  bool supportsFamily(MTL::GPUFamily gpuFamily);
  bool supportsTextureSampleCount(NS::UInteger sampleCount);
  bool areProgrammableSamplePositionsSupported();
  bool supportsRasterizationRateMapWithLayerCount(NS::UInteger layerCount);
  bool supportsCounterSampling(MTL::CounterSamplingPoint samplingPoint);
  bool supportsVertexAmplificationCount(NS::UInteger count);
  bool supportsDynamicLibraries();
  bool supportsRenderDynamicLibraries();
  bool supportsRaytracing();
  bool supportsFunctionPointers();
  bool supportsFunctionPointersFromRender();
  bool supportsRaytracingFromRender();
  bool supportsPrimitiveMotionBlur();
  bool shouldMaximizeConcurrentCompilation();
  NS::UInteger maximumConcurrentCompilationTaskCount();
  // End of MTLDevice APIs

  CaptureState &GetStateRef() { return m_State; }
  CaptureState GetState() { return m_State; }
  MetalResourceManager *GetResourceManager() { return m_ResourceManager; };
  void WaitForGPU();
  WriteSerialiser &GetThreadSerialiser();
  static rdcstr GetChunkName(uint32_t idx);
  void SetStructuredExport(uint64_t sectionVersion)
  {
    m_SectionVersion = sectionVersion;
    m_State = CaptureState::StructuredExport;
    GetResourceManager()->SetState(m_State);
  }
  void SetReplayVersion(uint64_t sectionVersion) { m_SectionVersion = sectionVersion; }
  SDFile *GetStructuredFile() { return m_StructuredFile; }
  RDResult ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers);
  RDResult ReplayLog(uint32_t endEventID, ReplayLogType replayType);

  // IFrameCapturer interface
  RDCDriver GetFrameCaptureDriver() { return RDCDriver::Metal; }
  void StartFrameCapture(DeviceOwnedWindow devWnd);
  bool EndFrameCapture(DeviceOwnedWindow devWnd);
  bool DiscardFrameCapture(DeviceOwnedWindow devWnd);
  // IFrameCapturer interface

  void CaptureCmdBufCommit(MetalResourceRecord *cbRecord);
  void CaptureCmdBufCPUWrites(MetalResourceRecord *record);
  void CaptureCmdBufEnqueue(MetalResourceRecord *cbRecord);

  void AddFrameCaptureRecordChunk(Chunk *chunk) { m_FrameCaptureRecord->AddChunk(chunk); }
  void RegisterCapturedFrameResource(ResourceId id)
  {
    SCOPED_LOCK(m_CapturedFrameResourcesLock);
    m_CapturedFrameResources.insert(id);
  }
  void RegisterFramePlacementResource(ResourceId id, WrappedMTLHeap *heap)
  {
    m_FramePlacementResources[id] = heap;
  }
  bool IsFramePlacementResource(ResourceId id) const
  {
    return m_FramePlacementResources.count(id) != 0;
  }
  void RegisterFrameBufferTextureView(ResourceId id) { m_FrameBufferTextureViews.insert(id); }
  bool IsFrameBufferTextureView(ResourceId id) const
  {
    return m_FrameBufferTextureViews.count(id) != 0;
  }
  void RegisterBufferTextureParent(ResourceId texture, ResourceId buffer);
  // From ResourceManager interface
  bool Prepare_InitialState(WrappedMTLObject *res);
  uint64_t GetSize_InitialState(ResourceId id, const MetalInitialContents &initial);
  template <typename SerialiserType>
  bool Serialise_InitialState(SerialiserType &ser, ResourceId id, MetalResourceRecord *record,
                              const MetalInitialContents *initial);
  void Create_InitialState(ResourceId id, WrappedMTLObject *live, bool hasData);
  void Apply_InitialState(WrappedMTLObject *live, MetalInitialContents &initial);
  // From ResourceManager interface

  void RegisterMetalLayer(CA::MetalLayer *mtlLayer);
  void UnregisterMetalLayer(CA::MetalLayer *mtlLayer);

  void RegisterDrawableInfo(CA::MetalDrawable *caMtlDrawable, MTL::Texture *realTexture);
  MetalDrawableInfo UnregisterDrawableInfo(MTL::Drawable *mtlDrawable);
  static WrappedMTLTexture *GetDrawableTexture(MTL::Drawable *mtlDrawable);
  void PresentDrawable(CA::MetalDrawable *drawable);

  void AddEvent();
  void AddAction(const ActionDescription &a);

  MetalReplay *GetReplay() { return m_Replay; }
  void AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix);
  void DerivedResource(ResourceId parentLive, ResourceId child);
  template <typename MetalType>
  void DerivedResource(MetalType parent, ResourceId child)
  {
    DerivedResource(GetResID(parent), child);
  }

  void SetLastPresentedIamge(ResourceId lastPresentedImage)
  {
    m_LastPresentedImage = lastPresentedImage;
  }
  ResourceId GetLastPresentedImage() const { return m_LastPresentedImage; }
  void SetReplayRenderTarget(ResourceId target) { m_ReplayRenderTarget = target; }
  ResourceId GetReplayRenderTarget() const { return m_ReplayRenderTarget; }
  bool SetReplayCommandBuffer(WrappedMTLCommandBuffer *commandBuffer);
  bool SelectReplayCommandBuffer(WrappedMTLCommandBuffer *buffer);
  bool CanEncodeReplayEvent(WrappedMTLCommandBuffer *buffer);
  bool IsReplayCommandBufferCommitted(WrappedMTLCommandBuffer *buffer) const;
  bool ReplayCPUBufferUpdate(WrappedMTLBuffer *buffer, uint64_t start, const bytebuf &data);
  bool RecordReplayBufferInitialContents(ResourceId id, const bytebuf &contents);
  bool RestoreReplayPrivateBufferInitialContents();
  bool RecordReplayBCTextureInitialContents(ResourceId id, const bytebuf &contents);
  bool RestoreReplayBCTextureInitialContents();
  WrappedMTLRenderCommandEncoder *GetReplayRenderCommandEncoder() const
  {
    return m_ReplayRenderCommandEncoder;
  }
  WrappedMTLRenderCommandEncoder *GetReplayRenderCommandEncoder(
      WrappedMTLRenderCommandEncoder *encoder);
  WrappedMTLParallelRenderCommandEncoder *GetReplayParallelRenderCommandEncoder() const
  {
    return m_ReplayParallelRenderCommandEncoder;
  }
  WrappedMTLParallelRenderCommandEncoder *GetReplayParallelRenderCommandEncoder(
      WrappedMTLParallelRenderCommandEncoder *encoder);
  void SetReplayParallelRenderCommandEncoder(WrappedMTLParallelRenderCommandEncoder *encoder)
  {
    m_ReplayParallelRenderCommandEncoder = encoder;
  }
  WrappedMTLBlitCommandEncoder *GetReplayBlitCommandEncoder() const
  {
    return m_ReplayBlitCommandEncoder;
  }
  WrappedMTLBlitCommandEncoder *GetReplayBlitCommandEncoder(WrappedMTLBlitCommandEncoder *encoder);
  WrappedMTLComputeCommandEncoder *GetReplayComputeCommandEncoder() const
  {
    return m_ReplayComputeCommandEncoder;
  }
  WrappedMTLComputeCommandEncoder *GetReplayComputeCommandEncoder(
      WrappedMTLComputeCommandEncoder *encoder);
  WrappedMTLAccelerationStructureCommandEncoder *GetReplayAccelerationStructureCommandEncoder() const
  {
    return m_ReplayAccelerationStructureCommandEncoder;
  }
  WrappedMTLAccelerationStructureCommandEncoder *GetReplayAccelerationStructureCommandEncoder(
      WrappedMTLAccelerationStructureCommandEncoder *encoder);
  void SetReplayRenderCommandEncoder(WrappedMTLRenderCommandEncoder *encoder)
  {
    m_ReplayRenderCommandEncoder = encoder;
  }
  void SetReplayBlitCommandEncoder(WrappedMTLBlitCommandEncoder *encoder)
  {
    m_ReplayBlitCommandEncoder = encoder;
  }
  void SetReplayAccelerationStructureCommandEncoder(
      WrappedMTLAccelerationStructureCommandEncoder *encoder)
  {
    m_ReplayAccelerationStructureCommandEncoder = encoder;
  }
  void SetReplayComputeCommandEncoder(WrappedMTLComputeCommandEncoder *encoder)
  {
    m_ReplayComputeCommandEncoder = encoder;
  }
  void MarkReplayCommandBufferCommitted();
  void AssignPendingReplayCPUBufferUpdates(WrappedMTLCommandBuffer *buffer);
  bool ApplyReplayCPUBufferUpdates(WrappedMTLCommandBuffer *buffer);

  enum
  {
    TypeEnum = eResDevice
  };

  static uint64_t g_nextDrawableTLSSlot;
  static IMP g_real_CAMetalLayer_nextDrawable;
  static IMP g_real_CAMetalDrawable_texture;
  static IMP g_real_CAMetalDrawable_present;

private:
  static void MTLFixupForMetalDriverAssert();
  static void MTLHookObjcMethods();
  void FirstFrame();
  void AdvanceFrame();
  void Present(MetalResourceRecord *record);

  void CaptureClearSubmittedCmdBuffers();
  void CaptureCmdBufSubmit(MetalResourceRecord *record);
  void ReleaseCapturedCommandBuffer(MetalResourceRecord *record);
  void EndCaptureFrame(ResourceId backbuffer);

  template <typename SerialiserType>
  bool Serialise_CaptureScope(SerialiserType &ser);
  template <typename SerialiserType>
  bool Serialise_BeginCaptureFrame(SerialiserType &ser);

  void AddResourceCurChunk(ResourceDescription &descr);

  bool ProcessChunk(ReadSerialiser &ser, MetalChunk chunk);
  RDResult ContextReplayLog(CaptureState readType, uint32_t endEventID,
                            ReplayLogType replayType);
  bool FinishReplayCommands();
  bool ResetReplayCPUUpdatedBuffers();
  WrappedMTLTexture *Common_NewTexture(RDMTL::TextureDescriptor &descriptor, MetalChunk chunkType,
                                       bool ioSurfaceTexture, IOSurfaceRef iosurface,
                                       NS::UInteger plane);
  WrappedMTLTexture *WrapDrawableTexture(MTL::Texture *realTexture);
  WrappedMTLBuffer *Common_NewBuffer(bool withBytes, const void *pointer, NS::UInteger length,
                                     MTL::ResourceOptions options);

  MetalResourceManager *m_ResourceManager = NULL;
  uint64_t m_ReplayEpoch = 0;
  uint64_t m_CaptureEpoch = 0;
  Threading::CriticalSection m_CapturedFrameResourcesLock;
  std::set<ResourceId> m_CapturedFrameResources;
  std::map<ResourceId, WrappedMTLHeap *> m_FramePlacementResources;
  std::set<ResourceId> m_FrameBufferTextureViews;
  ResourceId m_LastPresentedImage;
  ResourceId m_ReplayRenderTarget;

  // Dummy objects used for serialisation replay
  WrappedMTLBuffer *m_DummyBuffer = NULL;
  WrappedMTLHeap *m_DummyReplayHeap = NULL;
  WrappedMTLRasterizationRateMap *m_DummyReplayRateMap = NULL;
  WrappedMTLTexture *m_DummyReplayTexture = NULL;
  WrappedMTLCommandBuffer *m_DummyReplayCommandBuffer = NULL;
  WrappedMTLCommandQueue *m_DummyReplayCommandQueue = NULL;
  WrappedMTLLibrary *m_DummyReplayLibrary = NULL;
  WrappedMTLBinaryArchive *m_DummyReplayBinaryArchive = NULL;
  WrappedMTLRenderPipelineState *m_DummyReplayRenderPipelineState = NULL;
  WrappedMTLComputePipelineState *m_DummyReplayComputePipelineState = NULL;
  WrappedMTLVisibleFunctionTable *m_DummyReplayVisibleFunctionTable = NULL;
  WrappedMTLIntersectionFunctionTable *m_DummyReplayIntersectionFunctionTable = NULL;
  WrappedMTLRenderCommandEncoder *m_DummyReplayRenderCommandEncoder = NULL;
  WrappedMTLParallelRenderCommandEncoder *m_DummyReplayParallelRenderCommandEncoder = NULL;
  WrappedMTLBlitCommandEncoder *m_DummyReplayBlitCommandEncoder = NULL;
  WrappedMTLAccelerationStructureCommandEncoder *
      m_DummyReplayAccelerationStructureCommandEncoder = NULL;
  WrappedMTLComputeCommandEncoder *m_DummyReplayComputeCommandEncoder = NULL;
  WrappedMTLArgumentEncoder *m_DummyReplayArgumentEncoder = NULL;
  WrappedMTLIndirectCommandBuffer *m_DummyReplayIndirectCommandBuffer = NULL;
  WrappedMTLIndirectRenderCommand *m_DummyReplayIndirectRenderCommand = NULL;

  MetalReplay *m_Replay = NULL;

  // Back buffer and swap chain emulation
  Threading::CriticalSection m_CapturePotentialBackBuffersLock;
  std::unordered_set<WrappedMTLTexture *> m_CapturePotentialBackBuffers;
  Threading::CriticalSection m_CaptureOutputLayersLock;
  std::unordered_set<CA::MetalLayer *> m_CaptureOutputLayers;
  WrappedMTLTexture *m_CapturedBackbuffer = NULL;
  std::atomic<bool> m_DirectPresentEndPending{false};
  Threading::CriticalSection m_CaptureDrawablesLock;
  rdcflatmap<MTL::Drawable *, MetalDrawableInfo> m_CaptureDrawableInfos;

  CaptureState m_State;
  StreamReader *m_FrameReader = NULL;
  uint64_t m_CurChunkOffset = 0;
  WrappedMTLCommandBuffer *m_ReplayCommandBuffer = NULL;
  struct ReplayCommandBufferState
  {
    WrappedMTLCommandBuffer *buffer = NULL;
    WrappedMTLRenderCommandEncoder *render = NULL;
    WrappedMTLParallelRenderCommandEncoder *parallel = NULL;
    WrappedMTLBlitCommandEncoder *blit = NULL;
    WrappedMTLAccelerationStructureCommandEncoder *acceleration = NULL;
    WrappedMTLComputeCommandEncoder *compute = NULL;
    ResourceId renderTarget;
    bool committed = false;
    bool cpuUpdatesApplied = false;
  };
  std::map<ResourceId, ReplayCommandBufferState> m_ReplayCommandBuffers;
  rdcarray<ResourceId> m_ReplayCommandBufferOrder;
  WrappedMTLRenderCommandEncoder *m_ReplayRenderCommandEncoder = NULL;
  WrappedMTLParallelRenderCommandEncoder *m_ReplayParallelRenderCommandEncoder = NULL;
  WrappedMTLBlitCommandEncoder *m_ReplayBlitCommandEncoder = NULL;
  WrappedMTLAccelerationStructureCommandEncoder *m_ReplayAccelerationStructureCommandEncoder = NULL;
  WrappedMTLComputeCommandEncoder *m_ReplayComputeCommandEncoder = NULL;
  bool m_ReplayCommandBufferCommitted = false;
  bool m_ReplayChunkIsGPUWork = false;
  struct CPUBufferUpdate
  {
    ResourceId buffer;
    uint64_t offset;
    bytebuf data;
  };
  // Snapshot updates are captured immediately before submission, after the encoded GPU chunks.
  // Associate them with that submission during loading, then apply them before its replay commit
  // (including a partial replay's final implicit commit), once per command buffer.
  std::map<ResourceId, rdcarray<CPUBufferUpdate>> m_ReplayCPUBufferUpdates;
  rdcarray<CPUBufferUpdate> m_PendingReplayCPUBufferUpdates;
  std::map<ResourceId, bytebuf> m_ReplayBufferInitialContents;
  std::map<ResourceId, bytebuf> m_ReplayBCTextureInitialContents;
  // Includes unchanged Shared initial states as well as buffers with frame CPU writes.
  std::set<ResourceId> m_ReplayCPUUpdatedBuffers;
  Threading::CriticalSection m_BufferTextureParentsLock;
  std::set<ResourceId> m_BufferTextureParents;
  std::map<ResourceId, ResourceId> m_BufferTextureParentByView;
  bool m_AppControlledCapture = false;
  SDFile *m_StructuredFile = NULL;
  SDFile *m_StoredStructuredData = NULL;

  uint64_t threadSerialiserTLSSlot;
  Threading::CriticalSection m_ThreadSerialisersLock;
  rdcarray<WriteSerialiser *> m_ThreadSerialisers;
  uint64_t m_SectionVersion = 0;

  MetalCapturer m_Capturer;
  uint32_t m_FrameCounter = 0;
  rdcarray<FrameDescription> m_CapturedFrames;
  Threading::RWLock m_CapTransitionLock;
  MetalResourceRecord *m_FrameCaptureRecord = NULL;

  // record the command buffer records to insert them individually
  // (even if they were recorded locklessly in parallel)
  // queue submit order will enforce/display ordering, record order is not important
  Threading::CriticalSection m_CaptureCommandBuffersLock;
  rdcarray<MetalResourceRecord *> m_CaptureCommandBuffersEnqueued;
  rdcarray<MetalResourceRecord *> m_CaptureCommandBuffersSubmitted;

  PerformanceTimer m_CaptureTimer;
  MetalInitParams m_InitParams;

  MTL::CommandQueue *m_mtlCommandQueue = NULL;
};

inline void MetalCapturer::StartFrameCapture(DeviceOwnedWindow devWnd)
{
  return m_Device.StartFrameCapture(devWnd);
}
inline bool MetalCapturer::EndFrameCapture(DeviceOwnedWindow devWnd)
{
  return m_Device.EndFrameCapture(devWnd);
}
inline bool MetalCapturer::DiscardFrameCapture(DeviceOwnedWindow devWnd)
{
  return m_Device.DiscardFrameCapture(devWnd);
}
