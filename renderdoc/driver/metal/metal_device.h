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

bool ValidateMetalPipelineFunction(WrappedMTLFunction *function, MTL::FunctionType type);

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
                               uint32_t valueVectorWidth, const RENDERDOC_AnnotationValue *value);
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
  bool DeferTerminalBufferPurge(ResourceId id);
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
  Threading::RWLock &GetCaptureTransitionLock() { return m_CapTransitionLock; }
  Threading::CriticalSection &GetCaptureSubmissionLock() { return m_CaptureCommandBuffersLock; }
  void RecordCaptureSubmission(MTL::CommandBuffer *buffer);
  bool WaitForCaptureSubmittedGPU();
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
  SDFile *DetachStructuredFile()
  {
    SDFile *file = m_StoredStructuredData;
    m_StoredStructuredData = m_StructuredFile = NULL;
    return file;
  }
  RDResult ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers);
  RDResult ReplayLog(uint32_t endEventID, ReplayLogType replayType);

  // IFrameCapturer interface
  RDCDriver GetFrameCaptureDriver() { return RDCDriver::Metal; }
  void StartFrameCapture(DeviceOwnedWindow devWnd);
  bool EndFrameCapture(DeviceOwnedWindow devWnd);
  bool DiscardFrameCapture(DeviceOwnedWindow devWnd);
  // IFrameCapturer interface

  struct PendingCapturePresent
  {
    MetalResourceRecord *record = NULL;
    WrappedMTLTexture *backBuffer = NULL;
    CA::MetalLayer *layer = NULL;
    NS::Object *textureProxy = NULL;
    MTL::Texture *textureNative = NULL;
    MTL::Drawable *drawable = NULL;
    uint64_t commitEpoch = 0;
  };
  void CaptureCmdBufCommit(MetalResourceRecord *cbRecord,
                           rdcarray<PendingCapturePresent> &presents);
  void ProcessCapturePresents(rdcarray<PendingCapturePresent> &presents);
  void CaptureCmdBufCPUWrites(MetalResourceRecord *record);
  void CaptureCmdBufEnqueue(MetalResourceRecord *cbRecord);

  // Diagnostic metadata, not descriptor relocation support. kind: buffer VA=0,
  // texture resource ID=1, sampler resource ID=2.
  void CaptureGPUIdentity(WrappedMTLObject *object, uint32_t kind, uint64_t value);
  template <typename SerialiserType>
  bool Serialise_CaptureGPUIdentity(SerialiserType &ser, ResourceId resource,
                                    uint32_t kind, uint64_t value);
  struct ComputeIndirectArgumentsEvidence
  {
    ResourceId command, buffer;
    uint64_t offset = 0;
    rdcarray<uint32_t> groups;
  };
  uint32_t m_CapturedComputeIndirectArgumentsCount = 0;
  bool m_HasCapturedComputeIndirectArguments = false;
  std::map<rdcpair<ResourceId, uint32_t>, ComputeIndirectArgumentsEvidence> m_CapturedComputeIndirectArguments;
  template <typename SerialiserType>
  bool Serialise_CaptureComputeIndirectArgumentsCount(SerialiserType &ser, uint32_t count);
  template <typename SerialiserType>
  bool Serialise_CaptureComputeIndirectArguments(SerialiserType &ser, ResourceId command,
      ResourceId encoder, uint32_t ordinal, ResourceId buffer, uint64_t offset, rdcarray<uint32_t> groups);
  bool CapturingRenderIndirectArguments() const;
  void CaptureRenderIndirectWrite(WrappedMTLCommandBuffer *command, ResourceId pass,
      WrappedMTLResource *resource);
  void CaptureRenderIndirectAttachments(WrappedMTLCommandBuffer *command, ResourceId pass,
      const RDMTL::RenderPassDescriptor &descriptor);
  void CaptureRenderIndirectCall(WrappedMTLCommandBuffer *command, ResourceId encoder,
      ResourceId pass, uint32_t ordinal, WrappedMTLBuffer *buffer, uint64_t offset,
      uint32_t wordCount, bool writesDeclared);
  void FlushRenderIndirectCaptures(WrappedMTLCommandBuffer *command, ResourceId pass);
  struct RenderIndirectArgumentsEvidence
  {
    ResourceId command,pass,buffer;
    uint64_t offset=0;
    uint32_t wordCount=0;
    rdcarray<uint32_t> arguments;
  };
  bool m_HasCapturedRenderIndirectArguments=false;
  uint32_t m_CapturedRenderIndirectArgumentsCount=0;
  std::map<rdcpair<ResourceId,uint32_t>,RenderIndirectArgumentsEvidence> m_CapturedRenderIndirectArguments;
  template <typename SerialiserType>
  bool Serialise_CaptureRenderIndirectArgumentsCount(SerialiserType &ser,uint32_t count);
  template <typename SerialiserType>
  bool Serialise_CaptureRenderIndirectArguments(SerialiserType &ser,ResourceId command,
      ResourceId encoder,ResourceId pass,uint32_t ordinal,ResourceId buffer,uint64_t offset,
      uint32_t wordCount,bool writesDeclared,rdcarray<uint32_t> arguments);
  // Explicit annotation schemas: 0=VA64, 1=IR resource24, 2=IR sampler24,
  // 3=pointer/texture/sampler packet. v1 is CPU-authored; v2 declares GPU destinations
  // and captures application-marked complete-entry CPU writes separately from GPU output.
  // v3 adds non-overlapping frame-created Shared/Private placement buffers and supported views.
  struct DescriptorTable
  {
    ResourceId buffer;
    uint64_t offset = 0, count = 0, stride = 0;
    uint32_t schema = 0;
  };
  template <typename SerialiserType>
  bool Serialise_DeclareDescriptorTable(SerialiserType &ser, ResourceId buffer, uint64_t offset,
                                        uint64_t count, uint64_t stride, uint32_t schema);
  template <typename SerialiserType>
  bool Serialise_DeclareDescriptorCoverage(SerialiserType &ser, uint32_t version);
  template <typename SerialiserType>
  bool Serialise_DeclareDescriptorGPUWrites(SerialiserType &ser, ResourceId buffer);
  template <typename SerialiserType>
  bool Serialise_DescriptorCPUWrite(SerialiserType &ser, ResourceId buffer, uint64_t start,
                                     bytebuf data);
  template <typename SerialiserType>
  bool Serialise_DescriptorSlotEvent(SerialiserType &ser, ResourceId buffer, uint64_t offset,
      uint64_t generation, uint32_t event, uint32_t descriptorType, bytebuf data);
  template <typename SerialiserType>
  bool Serialise_DescriptorSlotBinding(SerialiserType &ser, ResourceId buffer, uint64_t offset,
      ResourceId resource, uint32_t kind, uint64_t memberOffset);
  template <typename SerialiserType>
  bool Serialise_DescriptorInlineLayout(SerialiserType &ser, ResourceId encoder, uint32_t stage,
      uint32_t index, uint64_t count, uint64_t stride);
  template <typename SerialiserType>
  bool Serialise_DescriptorInlineBinding(SerialiserType &ser, ResourceId encoder, uint32_t stage,
      uint32_t index, uint64_t entry, ResourceId resource, uint64_t memberOffset);
  template <typename SerialiserType>
  bool Serialise_DescriptorSlotProducer(SerialiserType &ser, ResourceId buffer, uint64_t offset,
      ResourceId encoder, ResourceId source, uint64_t sourceOffset);
  void NoteDescriptorDispatch(ResourceId encoder)
  {
    if(m_DescriptorCoverage >= 6) m_DescriptorDispatches[encoder]++;
  }
  bool HasValidatedSourcedComputeDispatch(ResourceId encoder) const
  { return m_DescriptorCoverage >= 39 && m_DescriptorValidatedComputeResources.count(encoder) != 0; }
  void AddValidatedSourcedComputeUsage(ResourceId encoder);
  void SaveDescriptorHistoryChunk(ResourceId buffer, Chunk *chunk);
  void SnapshotDescriptorHistory();
  void ClearDescriptorHistorySnapshot();
  // v4: sourced static Shared descriptors and compute inline VAs. v5 adds a
  // full-entry typed GPU copy, preserving actual GPU destination writes.
  // Full UE coverage is deliberately not declared by the provider.
  struct DescriptorSource { ResourceId resource; uint64_t offset = 0; };
  struct DescriptorSlotShadow
  {
    uint64_t generation = 0;
    uint32_t type = 0;
    bool live = false;
    bool gpuExpected = false;
    DescriptorSource gpuCopySource;
    std::map<uint32_t, DescriptorSource> sources, gpuCopySources;
    bytebuf data;
    DescriptorSource source;
  };
  using DescriptorSlotKey = rdcpair<ResourceId, uint64_t>;
  std::map<DescriptorSlotKey, DescriptorSlotShadow> m_DescriptorSlotShadow, m_DescriptorSlotInitial;
  std::set<DescriptorSlotKey> m_CapturedLiveDescriptorSlots;
  std::map<DescriptorSlotKey, std::map<uint32_t, ResourceId>> m_CapturedDescriptorSlotSources;
  std::set<ResourceId> m_CaptureRetiredDescriptorBackings;
  std::map<DescriptorSlotKey, DescriptorSlotShadow> m_DescriptorPreludeRetirements;
  struct DescriptorPreludeBufferCopy
  {
    ResourceId source, destination;
    uint64_t sourceOffset, destinationOffset, size;
  };
  rdcarray<DescriptorPreludeBufferCopy> m_DescriptorPreludeBufferCopies;
  std::set<ResourceId> m_DescriptorPreludeBlitEncoders;
  bool IsDescriptorPreludeRetirement(const DescriptorSlotKey &key,
                                    const DescriptorSlotShadow &slot) const;
  struct DescriptorInlineShadow
  {
    uint64_t count = 0, stride = 0;
    std::map<uint64_t, DescriptorSource> sources;
  };
  std::map<rdcpair<ResourceId, uint64_t>, DescriptorInlineShadow> m_DescriptorInlineShadow;
  bool m_DescriptorShadowFrame = false;
  bool ReadDescriptorSlotEvent(ResourceId buffer, uint64_t offset, uint64_t generation,
      uint32_t event, uint32_t type, const bytebuf &data, bool initialCPUValue = false);
  bool ReadDescriptorSlotBinding(ResourceId buffer, uint64_t offset, ResourceId source,
      uint32_t kind, uint64_t memberOffset);
  bool PatchDescriptorSource(const DescriptorSource &source, uint64_t captured, uint64_t &replacement);
  bool PatchDescriptorSlotField(const DescriptorSource &source, uint32_t kind,
      uint64_t captured, uint64_t &replacement);
  bool PatchDescriptorSlot(const DescriptorSlotShadow &slot, bytebuf &data);
  std::map<DescriptorSlotKey, DescriptorSlotShadow> m_DescriptorGPUCopyExpected;
  std::map<ResourceId, uint32_t> m_DescriptorDispatches;
  bool TrackDescriptorGPUCopy(ResourceId source, uint64_t sourceOffset,
      ResourceId destination, uint64_t destinationOffset, uint64_t size);
  bool OverlayDescriptorSlotBuffer(ResourceId buffer, bool initialRestore = false);
  bool IsRetiredDescriptorBacking(ResourceId buffer) const;
  bool OverlayAliasedDescriptorTables(ResourceId buffer);
  bool PrepareDescriptorSlotShadow();
  bool ValidateDescriptorSlotFrame();
  bool RelocateGraphicsDescriptorBytes(ResourceId encoder, uint32_t stage, uint64_t index,
      rdcarray<byte> &data)
  { return m_DescriptorCoverage < 9 || RelocateDescriptorInlineShadow(encoder, stage, index, data); }
  bool RelocateDescriptorInlineShadow(ResourceId encoder, uint32_t stage, uint64_t index,
      rdcarray<byte> &data);
  uint32_t AnnotateDescriptorTable(void *object, const char *key,
                                  RENDERDOC_AnnotationType type, uint32_t width,
                                  const RENDERDOC_AnnotationValue *value);
  RDResult ScanDescriptorMetadata(RDCFile *rdc, int section, uint32_t diagnosticCoverage = 0);
  bool PrepareDescriptorTables();
  bool RestoreDescriptorTable(ResourceId buffer, const bytebuf &raw, uint64_t start = 0,
                                uint64_t size = ~0ULL);
  bool ValidDescriptorCPUWrite(ResourceId buffer, uint64_t start, uint64_t size) const;
  bool ApplyDescriptorCPUUpdate(ResourceId buffer, uint64_t start, const bytebuf &data);
  bool ValidateDescriptorFrame();
  bool ValidateDescriptorValues(ResourceId buffer, const bytebuf &raw);
  template <typename SerialiserType>
  bool Serialise_DeclareDescriptorBytes(SerialiserType &ser, ResourceId encoder, uint64_t index,
                                      uint64_t offset, uint64_t count, uint64_t stride);
  bool RelocateDescriptorBytes(ResourceId encoder, uint64_t index, rdcarray<byte> &data);

  void AddFrameCaptureRecordChunk(Chunk *chunk) { m_FrameCaptureRecord->AddChunk(chunk); }
  void RegisterCapturedFrameResource(ResourceId id)
  {
    SCOPED_LOCK(m_CapturedFrameResourcesLock);
    m_CapturedFrameResources.insert(id);
  }
  bool IsCapturedFrameResource(ResourceId id)
  {
    SCOPED_LOCK(m_CapturedFrameResourcesLock);
    return m_CapturedFrameResources.count(id) != 0;
  }
  void RegisterFramePlacementResource(ResourceId id, WrappedMTLHeap *heap)
  {
    m_FramePlacementResources[id] = heap;
  }
  bool CanReplayImplicitBufferAlias(ResourceId before, ResourceId after) const;
  bool CanReplayRetiredTextureAlias(ResourceId before, ResourceId after);
  uint64_t DescriptorPlacementAliasLimit() const
  {
    return m_DescriptorCoverage >= 42 ? 128ULL * 1024 : 64ULL * 1024;
  }
  bool SupportsPrivateDescriptorSources() const { return m_DescriptorCoverage >= 23; }
  bool SupportsTrackedAliasCreationWhileEncoding() const { return m_DescriptorCoverage >= 24; }
  void RecordReplayAliasable(ResourceId id) { m_ReplayAliasableResources.insert(id); }
  bool IsReplayResourceAliasable(ResourceId id) const { return m_ReplayAliasableResources.count(id) != 0; }
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
  bool EnqueueReplayCommandBuffer(WrappedMTLCommandBuffer *buffer);
  bool IsReplayCommandBufferCommitted(WrappedMTLCommandBuffer *buffer) const;
  bool WaitReplayCommandBuffer(WrappedMTLCommandBuffer *buffer, const char *reason = "captured CPU wait");
  bool ReplayCPUBufferUpdate(WrappedMTLBuffer *buffer, uint64_t start, const bytebuf &data,
      bool submissionSnapshot = false);
  bool RecordReplayBufferInitialContents(ResourceId id, const bytebuf &contents);
  bool RestoreReplayPrivateBufferInitialContents();
  void RegisterReplayTextureViewParent(ResourceId view, ResourceId parent)
  {
    m_ReplayTextureViewParents[view] = parent;
  }
  ResourceId GetReplayTextureViewParent(ResourceId view) const
  {
    auto parent = m_ReplayTextureViewParents.find(view);
    return parent == m_ReplayTextureViewParents.end() ? ResourceId() : parent->second;
  }
  bool HasReplayTextureInitialContents(ResourceId id) const
  {
    if(m_ReplayTextureInitialContents.count(id)) return true;
    auto parent = m_ReplayTextureViewParents.find(id);
    return parent != m_ReplayTextureViewParents.end() &&
           m_ReplayTextureInitialContents.count(parent->second);
  }
  bool RecordReplayTextureInitialContents(ResourceId id, const bytebuf &contents);
  bool RestoreReplayTextureInitialContents();
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
    if(encoder) MarkReplayCommandBufferEncoded();
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
    if(encoder) MarkReplayCommandBufferEncoded();
  }
  void SetReplayBlitCommandEncoder(WrappedMTLBlitCommandEncoder *encoder)
  {
    m_ReplayBlitCommandEncoder = encoder;
    if(encoder) MarkReplayCommandBufferEncoded();
  }
  void SetReplayAccelerationStructureCommandEncoder(
      WrappedMTLAccelerationStructureCommandEncoder *encoder)
  {
    m_ReplayAccelerationStructureCommandEncoder = encoder;
    if(encoder) MarkReplayCommandBufferEncoded();
  }
  void SetReplayComputeCommandEncoder(WrappedMTLComputeCommandEncoder *encoder)
  {
    m_ReplayComputeCommandEncoder = encoder;
    if(encoder) MarkReplayCommandBufferEncoded();
  }
  void MarkReplayCommandBufferEncoded();
  void MarkReplayCommandBufferCommitted();
  void AssignPendingReplayCPUBufferUpdates(WrappedMTLCommandBuffer *buffer);
  bool ApplyReplayCPUBufferUpdates(WrappedMTLCommandBuffer *buffer);
  bool ReplayMissingCopySubmissionPrefix(WrappedMTLCommandBuffer *buffer);

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
  void Present(WrappedMTLTexture *backBuffer, CA::MetalLayer *outputLayer, uint64_t commitEpoch);

  void CaptureClearSubmittedCmdBuffers();
  void CaptureCmdBufSubmit(MetalResourceRecord *record,
                           rdcarray<PendingCapturePresent> &presents);
  void ReleaseCapturedCommandBuffer(MetalResourceRecord *record);
  void EndCaptureFrame(ResourceId backbuffer);

  template <typename SerialiserType>
  bool Serialise_CaptureScope(SerialiserType &ser);
  template <typename SerialiserType>
  bool Serialise_BeginCaptureFrame(SerialiserType &ser);

  void AddResourceCurChunk(ResourceDescription &descr);

  bool ProcessChunk(ReadSerialiser &ser, MetalChunk chunk);
  RDResult ContextReplayLog(CaptureState readType, uint32_t endEventID,
                           ReplayLogType replayType, uint64_t diagnosticEndOffset = 0);
  bool FinishReplayCommands();
  bool RestoreReplayPurgedBuffers();
  bool ApplyTerminalReplayBufferPurges();
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
  std::set<ResourceId> m_ReplayAliasableResources;
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
  std::atomic<WrappedMTLTexture *> m_CapturedBackbuffer{NULL};
  Threading::CriticalSection m_CapturedPresentationLock;
  NS::Object *m_CapturedDirectTextureProxy = NULL;
  MTL::Texture *m_CapturedDirectTextureNative = NULL;
  CA::MetalDrawable *m_CapturedDirectDrawable = NULL;
  void ReleaseCapturedDirectPresentation();
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
    bool encoded = false;
  };
  std::map<ResourceId, ReplayCommandBufferState> m_ReplayCommandBuffers;
  rdcarray<ResourceId> m_ReplayCommandBufferOrder;
  rdcarray<ResourceId> m_ReplayCommandBufferQueueOrder;
  // A terminal Empty has no later reference in the frame stream. Execute it only after every
  // command buffer has completed, then restore NonVolatile before the next frame replay.
  std::set<ResourceId> m_TerminalFramePurgeableBuffers;
  std::set<ResourceId> m_PendingReplayBufferPurges;
  std::set<ResourceId> m_ReplayPurgedBuffers;
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
  bool ApplyFutureSharedAliasCPUUpdate(const CPUBufferUpdate &update);
  // Sourced preflight maps each automatic snapshot chunk to its following submission.
  std::map<uint64_t, ResourceId> m_DescriptorSubmissionSnapshotOwners;
  std::set<uint64_t> m_DescriptorInitialCPUValueOffsets;
  struct PartialCopySubmissionPlan {
    rdcarray<uint64_t> chunks;
    std::set<ResourceId> buffers;
    bool valid = true;
    uint32_t copies = 0;
    uint32_t signals = 0;
  };
  std::map<ResourceId, PartialCopySubmissionPlan> m_DescriptorPartialCopySubmissions;
  std::map<ResourceId, uint64_t> m_DescriptorFrameBufferBirthOffsets;
  rdcarray<ResourceId> m_DescriptorSubmissionOrder;
  std::map<ResourceId, rdcarray<CPUBufferUpdate>> m_ReplayCPUBufferUpdates;
  rdcarray<CPUBufferUpdate> m_PendingReplayCPUBufferUpdates;
  std::map<ResourceId, bytebuf> m_ReplayBufferInitialContents;
  std::set<ResourceId> m_ReplayBuffersWithCreationContents;
  struct GPUIdentity { uint32_t kind; uint64_t value; };
  std::map<ResourceId, GPUIdentity> m_ReplayGPUIdentities;
  rdcarray<DescriptorTable> m_DescriptorTables;
  std::map<ResourceId, bytebuf> m_DescriptorRawContents;
  uint32_t m_DescriptorCoverage = 0;
  Threading::CriticalSection m_DescriptorMetadataLock;
  struct DescriptorHistoryChunk { ResourceId buffer; Chunk *chunk; };
  rdcarray<DescriptorHistoryChunk> m_DescriptorHistory;
  rdcarray<Chunk *> m_DescriptorHistorySnapshot;
  std::set<ResourceId> m_DescriptorGPUWrittenBuffers;
  struct DescriptorFrameBuffer
  {
    ResourceId heap;
    uint64_t length, options, offset;
  };
  struct DescriptorFrameTexture
  {
    ResourceId heap;
    RDMTL::TextureDescriptor descriptor;
    uint64_t offset;
  };
  std::map<ResourceId, DescriptorFrameTexture> m_DescriptorFrameTextures;
  std::set<ResourceId> m_DescriptorDrawableTextures;
  std::map<ResourceId,uint64_t> m_DescriptorSubmissionInitialBuffers;
  // View ResourceIds keep their own native GPU identity. Their projected tiny
  // descriptor lives above; this edge establishes creation/lifetime dependency.
  std::map<ResourceId, ResourceId> m_DescriptorFrameTextureViewParents;
  std::set<ResourceId> m_DescriptorPreflightLiveTextures;
  std::map<ResourceId, DescriptorFrameBuffer> m_DescriptorFrameBuffers;
  std::map<ResourceId, ResourceId> m_DescriptorFrameViews;
  std::set<ResourceId> m_DescriptorPreflightLiveBuffers;
  std::set<ResourceId> m_DescriptorPreflightLiveTables;
  std::set<ResourceId> m_DescriptorPreflightAliasedBuffers;
  std::set<rdcpair<ResourceId, ResourceId>> m_ValidatedDescriptorBackingAliases;
  std::map<rdcpair<ResourceId, ResourceId>, std::set<ResourceId>> m_RetiredTextureAliasConsumers;
  std::map<rdcpair<ResourceId, ResourceId>, std::set<ResourceId>> m_DescriptorBackingAliasConsumers;
  std::set<ResourceId> m_DescriptorPreflightLiveViews;
  std::map<ResourceId, std::map<uint64_t, DescriptorTable>> m_DescriptorBytes;
  bool m_DescriptorPreflight = false;
  std::map<ResourceId, std::set<ResourceId>> m_DescriptorValidatedComputeResources;
  std::map<ResourceId, bytebuf> m_ReplayTextureInitialContents;
  std::map<ResourceId, ResourceId> m_ReplayTextureViewParents;
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
  Threading::CriticalSection m_CapturePendingGPULock;
  rdcarray<MTL::CommandBuffer *> m_CapturePendingGPU;
  bool m_FailedCaptureStart = false;
  MetalResourceRecord *m_FrameCaptureRecord = NULL;

  // record the command buffer records to insert them individually
  // (even if they were recorded locklessly in parallel)
  // queue submit order will enforce/display ordering, record order is not important
  Threading::CriticalSection m_CaptureCommandBuffersLock;
  std::map<ResourceId, rdcarray<MetalResourceRecord *>> m_CaptureCommandBuffersEnqueued;
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
