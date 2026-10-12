// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv)
{
  if(argc!=5)return 1;
  unsigned count=atoi(argv[2]),offset=atoi(argv[3]);bool unretained=atoi(argv[4]);
  if(!count || count>8192 || !offset || offset>64)return 1;
  @autoreleasepool {
    auto device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    const bool computeReadWrite=getenv("RENDERDOC_METAL_SAMPLE_COMPUTE_TEXTURE_READ_WRITE")!=nullptr;
    const bool computeTexture=getenv("RENDERDOC_METAL_SAMPLE_RETIRE_COMPUTE_TEXTURE")!=nullptr;
    auto library=[device newLibraryWithSource:@"#include <metal_stdlib>\nusing namespace metal;\nkernel void input_consumer(device const uchar *input [[buffer(0)]], device uint *output [[buffer(1)]], constant uint &count [[buffer(2)]], uint i [[thread_position_in_grid]]) { if(i<count) output[i]=uint(input[i])^(i*17u+5u); }\nkernel void texture_consumer(device const uchar *input [[buffer(0)]], device uint *output [[buffer(1)]], constant uint &count [[buffer(2)]], texture2d<float,access::write> image [[texture(0)]], uint i [[thread_position_in_grid]]) { if(i<count) output[i]=uint(input[i])^(i*17u+5u); if(i<image.get_width()*image.get_height()) image.write(float4(1,0,0,1),uint2(i%image.get_width(),i/image.get_width())); }\nkernel void readwrite_texture_consumer(device const uchar *input [[buffer(0)]], device uint *output [[buffer(1)]], constant uint &count [[buffer(2)]], texture2d<float,access::read_write> image [[texture(0)]], uint i [[thread_position_in_grid]]) { if(i<count) output[i]=uint(input[i])^(i*17u+5u); if(i<image.get_width()*image.get_height()) image.write(image.read(uint2(i%image.get_width(),i/image.get_width()))+float4(1,0,0,1),uint2(i%image.get_width(),i/image.get_width())); }" options:nil error:&error];
    MTLComputePipelineDescriptor *descriptor=[MTLComputePipelineDescriptor new];descriptor.computeFunction=[library newFunctionWithName:computeTexture?(computeReadWrite?@"readwrite_texture_consumer":@"texture_consumer"):@"input_consumer"];descriptor.label=@"NativeInputSnapshotSample";
    auto pipeline=[device newComputePipelineStateWithDescriptor:descriptor options:MTLPipelineOptionArgumentInfo|MTLPipelineOptionBufferTypeInfo reflection:nil error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    auto queue=[device newCommandQueue];std::vector<unsigned char> initial(count+offset+31);
    for(unsigned i=0;i<initial.size();i++)initial[i]=(i*23u+77u)&255u;
    std::vector<uint32_t> expected(count+16,0xcdcdcdcdU);
    auto staging=[device newBufferWithBytes:initial.data() length:initial.size() options:MTLResourceStorageModeShared];
    auto input=[device newBufferWithLength:initial.size() options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked];
    input.label=@"Native input snapshot source";
    auto output=[device newBufferWithBytes:expected.data() length:expected.size()*4 options:MTLResourceStorageModeShared];output.label=@"Native input snapshot output";
    auto params=[device newBufferWithBytes:&count length:4 options:MTLResourceStorageModeShared];params.label=@"Native input snapshot count";
    uint32_t groups[3]={(count+31)/32,1,1};auto args=[device newBufferWithBytes:groups length:12 options:MTLResourceStorageModeShared];
    auto upload=[queue commandBuffer];auto blit=[upload blitCommandEncoder];[blit copyFromBuffer:staging sourceOffset:0 toBuffer:input destinationOffset:0 size:initial.size()];[blit endEncoding];[upload commit];[upload waitUntilCompleted];
    if(upload.error)return 3;
    RENDERDOC_API_1_7_0 *api=nullptr;auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(get && get(eRENDERDOC_API_Version_1_7_0,(void**)&api)!=1)return 3;
    const unsigned interleaveCompute=getenv("RENDERDOC_METAL_SAMPLE_RETIRE_COMPUTE")?atoi(getenv("RENDERDOC_METAL_SAMPLE_RETIRE_COMPUTE")):0;
    if(interleaveCompute>2 || (computeTexture && interleaveCompute!=2))return 1;
    const unsigned renderWidth=getenv("RENDERDOC_METAL_SAMPLE_RETIRE_RENDER_WIDTH")?atoi(getenv("RENDERDOC_METAL_SAMPLE_RETIRE_RENDER_WIDTH")):0;
    const unsigned renderHeight=getenv("RENDERDOC_METAL_SAMPLE_RETIRE_RENDER_HEIGHT")?atoi(getenv("RENDERDOC_METAL_SAMPLE_RETIRE_RENDER_HEIGHT")):13;
    const bool renderHalf=getenv("RENDERDOC_METAL_SAMPLE_RETIRE_RENDER_HALF")!=nullptr;
    const bool renderView=getenv("RENDERDOC_METAL_SAMPLE_RETIRE_RENDER_VIEW")!=nullptr;
    if(renderWidth>1024 || !renderHeight || renderHeight>1024)return 1;
    const bool sameTableTextureHeap=getenv("RENDERDOC_METAL_SAMPLE_TEXTURE_SAME_TABLE_HEAP")!=nullptr;
    const bool bufferTexture=getenv("RENDERDOC_METAL_SAMPLE_COMPUTE_BUFFER_TEXTURE")!=nullptr;
    const unsigned bufferUnits=getenv("RENDERDOC_METAL_SAMPLE_BUFFER_TEXTURE_OFFSET_UNITS")?atoi(getenv("RENDERDOC_METAL_SAMPLE_BUFFER_TEXTURE_OFFSET_UNITS")):3;
    const unsigned bufferPadding=getenv("RENDERDOC_METAL_SAMPLE_BUFFER_TEXTURE_ROW_PADDING")?atoi(getenv("RENDERDOC_METAL_SAMPLE_BUFFER_TEXTURE_ROW_PADDING")):1;
    if(bufferTexture && (!computeTexture || renderView || !bufferUnits || bufferUnits>16 || bufferPadding>16))return 1;
    id<MTLBuffer> textureParent=nil;NSUInteger textureOffset=0,texturePitch=0;
    std::vector<unsigned char> textureParentInitial,textureParentExpected;
    id<MTLTexture> renderBase=nil,renderResult=nil;id<MTLHeap> textureHeap=nil;
    std::vector<unsigned char> renderExpected;
    if(renderWidth){
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:renderHalf?MTLPixelFormatRGBA16Float:MTLPixelFormatRGBA8Unorm width:renderWidth*(renderView?2:1) height:renderHeight*(renderView?2:1) mipmapped:renderView];
      td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|(computeTexture?MTLTextureUsageShaderWrite:0);
      const unsigned pixelBytes=renderHalf?8:4;
      if(bufferTexture){
        td.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
        const NSUInteger alignment=std::max([device minimumLinearTextureAlignmentForPixelFormat:td.pixelFormat],[device minimumTextureBufferAlignmentForPixelFormat:td.pixelFormat]);
        if(!alignment)return 2;
        textureOffset=alignment*bufferUnits;
        texturePitch=((renderWidth*pixelBytes+alignment-1)/alignment+bufferPadding)*alignment;
        textureParentInitial.assign(textureOffset+texturePitch*renderHeight+alignment*2,0xa7);
        for(unsigned y=0;y<renderHeight;y++)memset(textureParentInitial.data()+textureOffset+y*texturePitch,0,renderWidth*pixelBytes);
        textureParent=[device newBufferWithBytes:textureParentInitial.data() length:textureParentInitial.size() options:MTLResourceStorageModeShared];
        textureParent.label=@"Native retirement compute texture parent";
        renderBase=[textureParent newTextureWithDescriptor:td offset:textureOffset bytesPerRow:texturePitch];
      }else if(getenv("RENDERDOC_METAL_SAMPLE_COMPUTE_HEAP_TEXTURE")){
        if(!computeTexture)return 1;
        const unsigned units=getenv("RENDERDOC_METAL_SAMPLE_TEXTURE_HEAP_OFFSET_UNITS")?atoi(getenv("RENDERDOC_METAL_SAMPLE_TEXTURE_HEAP_OFFSET_UNITS")):3;
        if(!units || units>64)return 1;
        const auto layout=[device heapTextureSizeAndAlignWithDescriptor:td];
        if(!layout.size || !layout.align)return 2;
        auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=td.storageMode;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
        hd.size=units*layout.align+layout.size+layout.align*2;
        textureHeap=[device newHeapWithDescriptor:hd];
        renderBase=[textureHeap newTextureWithDescriptor:td offset:units*layout.align];
      }else renderBase=[device newTextureWithDescriptor:td];
      if(!renderBase)return 2;
      for(unsigned level=0;!bufferTexture && level<renderBase.mipmapLevelCount;level++){
        unsigned w=std::max(1u,unsigned(renderBase.width)>>level),h=std::max(1u,unsigned(renderBase.height)>>level);
        std::vector<unsigned char> zero(w*h*pixelBytes,0);
        [renderBase replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:level withBytes:zero.data() bytesPerRow:w*pixelBytes];
      }
      renderResult=renderView?[renderBase newTextureViewWithPixelFormat:td.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(1,1) slices:NSMakeRange(0,1)]:renderBase;
      if(!renderResult)return 2;
      renderResult.label=computeTexture?@"Native retirement unrelated compute texture":@"Native retirement unrelated render output";
      renderExpected.resize(renderWidth*renderHeight*pixelBytes);
      for(size_t i=0;i<renderExpected.size();i+=pixelBytes){
        if(renderHalf){const uint16_t red[]={0x3c00,0,0,0x3c00};memcpy(renderExpected.data()+i,red,8);}
        else {renderExpected[i]=255;renderExpected[i+3]=255;}
      }
    }
    if(computeTexture && !renderWidth)return 1;
    id<MTLBuffer> computeResult=nil;
    if(interleaveCompute){computeResult=[device newBufferWithBytes:expected.data() length:expected.size()*4 options:MTLResourceStorageModeShared];computeResult.label=@"Native retirement unrelated compute output";}
    const bool staleBirth = getenv("RENDERDOC_METAL_SAMPLE_STALE_BIRTH") != nullptr;
    const bool submitBeforeRetire = getenv("RENDERDOC_METAL_SAMPLE_SUBMIT_BEFORE_RETIRE") != nullptr;
    const bool earlyRetire=getenv("RENDERDOC_METAL_SAMPLE_EARLY_RETIRE_LABEL")!=nullptr;
    const bool tableHeap = getenv("RENDERDOC_METAL_SAMPLE_TABLE_HEAP") != nullptr;
    const unsigned unrelatedCopy = getenv("RENDERDOC_METAL_SAMPLE_UNRELATED_COPY") ? atoi(getenv("RENDERDOC_METAL_SAMPLE_UNRELATED_COPY")) : 0;
    const unsigned copyGrouping = getenv("RENDERDOC_METAL_SAMPLE_COPY_GROUPING") ? atoi(getenv("RENDERDOC_METAL_SAMPLE_COPY_GROUPING")) : 0;
    const unsigned copyOffset=getenv("RENDERDOC_METAL_SAMPLE_COPY_OFFSET")?atoi(getenv("RENDERDOC_METAL_SAMPLE_COPY_OFFSET")):0;
    const unsigned copyBytes=getenv("RENDERDOC_METAL_SAMPLE_COPY_BYTES")?atoi(getenv("RENDERDOC_METAL_SAMPLE_COPY_BYTES")):24;
    const unsigned copyTo=getenv("RENDERDOC_METAL_SAMPLE_COPY_TO")?atoi(getenv("RENDERDOC_METAL_SAMPLE_COPY_TO")):0;
    const char *fillText=getenv("RENDERDOC_METAL_SAMPLE_UNRELATED_FILL");
    const unsigned fillValue=fillText?strtoul(fillText,nullptr,0):0;
    const unsigned fillOffset=getenv("RENDERDOC_METAL_SAMPLE_FILL_OFFSET")?atoi(getenv("RENDERDOC_METAL_SAMPLE_FILL_OFFSET")):0;
    const unsigned fillBytes=getenv("RENDERDOC_METAL_SAMPLE_FILL_BYTES")?atoi(getenv("RENDERDOC_METAL_SAMPLE_FILL_BYTES")):24;
    if(fillText && (!unrelatedCopy || fillValue>255 || !fillBytes || fillOffset>24 || fillBytes>24-fillOffset))return 1;
    const bool lateMetadata=getenv("RENDERDOC_METAL_SAMPLE_LATE_METADATA")!=nullptr;
    const bool copyMetadata=getenv("RENDERDOC_METAL_SAMPLE_COPY_METADATA")!=nullptr;
    const bool copyPointer=getenv("RENDERDOC_METAL_SAMPLE_COPY_POINTER")!=nullptr;
    const bool resultTable=getenv("RENDERDOC_METAL_SAMPLE_RESULT_TABLE")!=nullptr;
    const bool resultHeap=getenv("RENDERDOC_METAL_SAMPLE_RESULT_HEAP")!=nullptr;
    const bool resultAlias=getenv("RENDERDOC_METAL_SAMPLE_RESULT_ALIAS")!=nullptr;
    const unsigned pointerOffset=getenv("RENDERDOC_METAL_SAMPLE_POINTER_OFFSET")?atoi(getenv("RENDERDOC_METAL_SAMPLE_POINTER_OFFSET")):3;
    if(copyPointer && (!unrelatedCopy || pointerOffset>=64))return 1;
    if(unrelatedCopy>2 || copyGrouping>2 || !copyBytes || copyOffset>24 || copyBytes>24-copyOffset || copyTo>24 || copyBytes>24-copyTo)return 1;
    std::vector<unsigned char> unrelatedExpected(24,0xa7);
    const bool typedAlias = getenv("RENDERDOC_METAL_SAMPLE_TYPED_ALIAS") != nullptr;
    id<MTLHeap> birthHeap=nil;id<MTLBuffer> carrier=nil,typedTable=nil,staleSource=nil;
    id<MTLHeap> sharedHeap=nil,resultPhysicalHeap=nil;id<MTLBuffer> copySource=nil,copyResult=nil,copyWriter=nil,transportSource=nil,resultLiveSource=nil;
    const NSUInteger resultOffset=resultTable?24:0;
    if(sameTableTextureHeap && (!staleBirth || !tableHeap || !computeTexture || !renderBase || bufferTexture))return 1;
    if(resultTable && !unrelatedCopy)return 1;
    if((resultHeap || resultAlias) && !resultTable)return 1;
    const unsigned slotOffset=offset%2?24:0;
    const unsigned staleSlots=getenv("RENDERDOC_METAL_SAMPLE_STALE_SLOTS")?atoi(getenv("RENDERDOC_METAL_SAMPLE_STALE_SLOTS")):1;
    if(!staleSlots || staleSlots>4096)return 1;
    auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d){
      if(!api)return 0U;RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void*)device,(__bridge void*)object,key,eRENDERDOC_UInt64,4,&v);
    };
    if(staleBirth){
      auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
      hd.size=[device heapBufferSizeAndAlignWithLength:initial.size() options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked].size;
      birthHeap=[device newHeapWithDescriptor:hd];carrier=[birthHeap newBufferWithLength:initial.size() options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:0];
      if(!carrier)return 8;
      if(getenv("RENDERDOC_METAL_SAMPLE_READ_BIRTH_ALIAS"))
        carrier.label=@"Native input physical initial carrier";
      auto cb=[queue commandBuffer];auto be=[cb blitCommandEncoder];[be copyFromBuffer:staging sourceOffset:0 toBuffer:carrier destinationOffset:0 size:initial.size()];[be endEncoding];[cb commit];[cb waitUntilCompleted];if(cb.error)return 8;
      staleSource=[device newBufferWithLength:64 options:MTLResourceStorageModeShared];typedTable=[device newBufferWithLength:(staleSlots+2)*24 options:MTLResourceStorageModeShared];memset(typedTable.contents,0,(staleSlots+2)*24);
      if(tableHeap || typedAlias){
        auto opt=MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked;
        const unsigned tableLength=(staleSlots+2)*24;
        auto tl=[device heapBufferSizeAndAlignWithLength:tableLength options:opt];
        auto ol=[device heapBufferSizeAndAlignWithLength:expected.size()*4 options:opt];
        const NSUInteger tableAt=tl.align*3;
        const NSUInteger outputAt=((tableAt+tl.size+ol.align-1)/ol.align+2)*ol.align;
        MTLTextureDescriptor *textureDescriptor=nil;
        MTLSizeAndAlign textureLayout={0,1};NSUInteger textureAt=0;
        if(sameTableTextureHeap){
          textureDescriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:renderHalf?MTLPixelFormatRGBA16Float:MTLPixelFormatRGBA8Unorm width:renderWidth*(renderView?2:1) height:renderHeight*(renderView?2:1) mipmapped:renderView];
          textureDescriptor.storageMode=MTLStorageModeShared;textureDescriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
          textureLayout=[device heapTextureSizeAndAlignWithDescriptor:textureDescriptor];
          const unsigned textureUnits=getenv("RENDERDOC_METAL_SAMPLE_TEXTURE_HEAP_OFFSET_UNITS")?atoi(getenv("RENDERDOC_METAL_SAMPLE_TEXTURE_HEAP_OFFSET_UNITS")):3;
          if(!textureLayout.size || !textureLayout.align || !textureUnits || textureUnits>64)return 1;
          textureAt=((outputAt+ol.size+textureLayout.align-1)/textureLayout.align+textureUnits)*textureLayout.align;
        }
        auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModeShared;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=sameTableTextureHeap?textureAt+textureLayout.size:outputAt+ol.size;
        sharedHeap=[device newHeapWithDescriptor:hd];
        if(sameTableTextureHeap){
          renderBase=[sharedHeap newTextureWithDescriptor:textureDescriptor offset:textureAt];if(!renderBase)return 8;
          for(unsigned level=0;level<renderBase.mipmapLevelCount;level++){
            unsigned w=std::max(1u,unsigned(renderBase.width)>>level),h=std::max(1u,unsigned(renderBase.height)>>level);
            std::vector<unsigned char> zero(w*h*(renderHalf?8:4),0);
            [renderBase replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:level withBytes:zero.data() bytesPerRow:w*(renderHalf?8:4)];
          }
          renderResult=renderView?[renderBase newTextureViewWithPixelFormat:textureDescriptor.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(1,1) slices:NSMakeRange(0,1)]:renderBase;
          if(!renderResult)return 8;renderResult.label=@"Native retirement unrelated compute texture";
        }
        typedTable=[sharedHeap newBufferWithLength:tableLength options:opt offset:tableAt];
        output=[sharedHeap newBufferWithLength:expected.size()*4 options:opt offset:outputAt];
        if(!typedTable || !output)return 8;
        output.label=@"Native input snapshot output";memset(typedTable.contents,0,tableLength);memcpy(output.contents,expected.data(),expected.size()*4);
        if(typedAlias && count+offset>tableLength)return 8;
      }
      uint64_t packet[3]={staleSource.gpuAddress,0,64};for(unsigned row=0;row<staleSlots;row++)memcpy((char*)typedTable.contents+slotOffset+row*24,packet,24);
      if(typedAlias){initial.resize(typedTable.length);memcpy(initial.data(),typedTable.contents,initial.size());}
      if(api){RENDERDOC_AnnotationValue v={};v.uint32=65;
        if(api->SetObjectAnnotation((__bridge void*)device,(__bridge void*)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v) ||
          annotation(typedTable,"metal.descriptorTable",2,0,staleSlots+2,24))return 8;
        for(unsigned row=0;row<staleSlots;row++){unsigned at=slotOffset+row*24;
          if(annotation(typedTable,"metal.descriptorSlotEvent",at,row+1,0,4) || annotation(typedTable,"metal.descriptorSlotEvent",at,row+1,2,4) || annotation(typedTable,"metal.descriptorSlotBinding",at,0,(uint64_t)(__bridge void*)staleSource,0))return 8;
        }
      }
      // A second table or the unused tail row of this table is ordinary zero
      // data for a byte copy, independent of the last stale slot's generation.
      if(unrelatedCopy){
        copySource=unrelatedCopy==1?typedTable:[device newBufferWithLength:72 options:MTLResourceStorageModeShared];
        copyResult=[device newBufferWithLength:resultTable?72:24 options:MTLResourceStorageModeShared];copyResult.label=@"Native retirement unrelated copy output";
        if(resultHeap || resultAlias){
          const auto options=MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked;
          auto layout=[device heapBufferSizeAndAlignWithLength:72 options:options];
          auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModeShared;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=layout.align*3+layout.size;
          resultPhysicalHeap=[device newHeapWithDescriptor:hd];copyResult=[resultPhysicalHeap newBufferWithLength:72 options:options offset:layout.align*3];if(!copyResult)return 8;
          copyResult.label=@"Native retirement unrelated copy output";
        }
        if(resultTable){
          memset(copyResult.contents,0,72);memset(unrelatedExpected.data(),0,24);
          resultLiveSource=[device newBufferWithLength:64 options:MTLResourceStorageModeShared];
          resultLiveSource.label=@"Native ordinary write adjacent live source";
          const uint64_t packet[]={resultLiveSource.gpuAddress,0,64};memcpy(copyResult.contents,packet,24);
          if(annotation(copyResult,"metal.descriptorTable",2,0,3,24) ||
             annotation(copyResult,"metal.descriptorSlotEvent",0,1,0,4) ||
             annotation(copyResult,"metal.descriptorSlotEvent",0,1,2,4) ||
             annotation(copyResult,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void*)resultLiveSource,0))return 8;
        }
        copyWriter=copyResult;
        if(resultAlias){
          copyWriter=[resultPhysicalHeap newBufferWithLength:72 options:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked offset:copyResult.heapOffset];if(!copyWriter)return 8;
          copyResult.label=@"Native ordinary adjacent live table";
          copyWriter.label=@"Native retirement unrelated copy output";
        }
        if(unrelatedCopy==2){memset(copySource.contents,0,72);if(annotation(copySource,"metal.descriptorTable",2,0,3,24))return 8;}
        const NSUInteger from=unrelatedCopy==1?(staleSlots+1)*24:24;
        copySource.label=@"Native transport source table";
        if(copyPointer){
          transportSource=[device newBufferWithLength:64 options:MTLResourceStorageModeShared];transportSource.label=@"Native transport referenced resource";
          for(unsigned i=0;i<64;i++)((unsigned char*)transportSource.contents)[i]=(i*37u+11u)&255u;
          const uint64_t address=transportSource.gpuAddress+pointerOffset;memcpy((char*)copySource.contents+from,&address,8);
        }
        if(copyMetadata && !lateMetadata){const uint64_t value=0x7361646f706d6574ULL;memcpy((char*)copySource.contents+from+16,&value,8);}
        if(copyPointer && (annotation(copySource,"metal.descriptorSlotEvent",from,staleSlots+17,0,0) ||
           annotation(copySource,"metal.descriptorSlotEvent",from,staleSlots+17,2,0) ||
           annotation(copySource,"metal.descriptorSlotBinding",from,0,(uint64_t)(__bridge void*)transportSource,pointerOffset)))return 8;
        if(!resultTable)memset(copyResult.contents,0xa7,24);
        if(fillText)memset(unrelatedExpected.data()+fillOffset,fillValue,fillBytes);
        memcpy(unrelatedExpected.data()+copyTo,(char*)copySource.contents+from+copyOffset,copyBytes);
      }
      staleSource=nil;
    }
    if(api){api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
    id<MTLBuffer> born=nil;
    if(staleBirth){
      if(typedAlias){
        // Different logical object, same physical typed bytes. Frame birth
        // avoids the separate background Shared-alias support limitation.
        input=[sharedHeap newBufferWithLength:typedTable.length options:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked offset:typedTable.heapOffset];
        if(!input)return 8;input.label=@"Native input snapshot source";
      }
      // Physical restoration metadata is interspersed before the exact free.
      // Neither birth nor retirement interprets the stale pointer as GPU input.
      // The optional consumer below reads the ordinary pre-frame carrier bytes
      // through the frame-born physical alias, independently of that table.
      born=[birthHeap newBufferWithLength:carrier.length options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:0];if(!born)return 8;
      if(getenv("RENDERDOC_METAL_SAMPLE_READ_BIRTH_ALIAS")){
        if(typedAlias)return 1;
        input.label=@"Native input snapshot unused source";
        input=born;input.label=@"Native input snapshot source";
      }
      for(unsigned row=0;row+1<staleSlots;row++)if(annotation(typedTable,"metal.descriptorSlotEvent",slotOffset+row*24,row+1,1,4))return 8;
    }
    if(staleBirth && earlyRetire){
      if(annotation(typedTable,"metal.descriptorSlotEvent",slotOffset+(staleSlots-1)*24,staleSlots,1,4) ||
         annotation(typedTable,"metal.descriptorSlotEvent",slotOffset,staleSlots+1,0,4) ||
         annotation(typedTable,"metal.descriptorSlotEvent",slotOffset,staleSlots+1,1,4))return 8;
    }
    auto command=unretained?[queue commandBufferWithUnretainedReferences]:[queue commandBuffer];
    if(earlyRetire)command.label=@"Unrelated metadata between retirement and copy";
    if(staleBirth && interleaveCompute){
      auto encoder=[command computeCommandEncoder];
      [encoder setComputePipelineState:pipeline];[encoder setBuffer:input offset:offset atIndex:0];
      [encoder setBuffer:computeResult offset:0 atIndex:1];[encoder setBuffer:params offset:0 atIndex:2];
      [encoder useResource:input usage:MTLResourceUsageRead];[encoder useResource:computeResult usage:MTLResourceUsageWrite];
      if(computeTexture){
        if(getenv("RENDERDOC_METAL_SAMPLE_COMPUTE_TEXTURE_ARRAY_BINDING")) [encoder setTextures:(const id<MTLTexture>[]){renderResult} withRange:NSMakeRange(0,1)];
        else [encoder setTexture:renderResult atIndex:0];
        [encoder useResource:renderResult usage:MTLResourceUsageWrite];
      }
      if(interleaveCompute==2)[encoder dispatchThreadgroups:MTLSizeMake((std::max(count,computeTexture?renderWidth*renderHeight:0u)+31)/32,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
      [encoder endEncoding];
    }
    id<MTLCommandBuffer> unrelatedCommand=nil;
    if(staleBirth && renderWidth && !computeTexture){
      auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=renderResult;
      pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].clearColor=MTLClearColorMake(1,0,0,1);pass.colorAttachments[0].storeAction=MTLStoreActionStore;
      auto encoder=[command renderCommandEncoderWithDescriptor:pass];
      if(!encoder)return 4;
      [encoder setVertexBuffer:input offset:0 atIndex:7];[encoder setFragmentBuffer:params offset:0 atIndex:11];
      [encoder setViewport:(MTLViewport){0,0,double(renderWidth),double(renderHeight),0,1}];
      [encoder useResource:input usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
      [encoder setVertexBuffer:nil offset:0 atIndex:7];
      [encoder endEncoding];
    }
    if(staleBirth && unrelatedCopy){
      unrelatedCommand=copyGrouping==0?command:(unretained?[queue commandBufferWithUnretainedReferences]:[queue commandBuffer]);
      auto be=[unrelatedCommand blitCommandEncoder];
      if(fillText)[be fillBuffer:copyWriter range:NSMakeRange(resultOffset+fillOffset,fillBytes) value:fillValue];
      const NSUInteger from=unrelatedCopy==1?(staleSlots+1)*24:24;
      [be copyFromBuffer:copySource sourceOffset:from+copyOffset toBuffer:copyWriter destinationOffset:resultOffset+copyTo size:copyBytes];
      [be endEncoding];
      if(copyGrouping==1){
        [unrelatedCommand commit];
        if(getenv("RENDERDOC_METAL_SAMPLE_WAIT_BEFORE_RETIRE")){
          [unrelatedCommand waitUntilCompleted];[unrelatedCommand waitUntilCompleted];
          if(unrelatedCommand.error || unrelatedCommand.status!=MTLCommandBufferStatusCompleted)return 4;
        }
      }
    }
    if(staleBirth){auto be=[command blitCommandEncoder];[be copyFromBuffer:input sourceOffset:offset toBuffer:output destinationOffset:0 size:count];[be endEncoding];}
    else {auto encoder=[command computeCommandEncoder];
    [encoder setComputePipelineState:pipeline];[encoder setBuffer:input offset:offset atIndex:0];[encoder setBuffer:output offset:0 atIndex:1];[encoder setBuffer:params offset:0 atIndex:2];
    [encoder useResources:(const id<MTLResource>[]){input,params,args} count:3 usage:MTLResourceUsageRead];[encoder useResource:output usage:MTLResourceUsageWrite];
    [encoder dispatchThreadgroupsWithIndirectBuffer:args indirectBufferOffset:0 threadsPerThreadgroup:MTLSizeMake(32,1,1)];
    [encoder endEncoding];}
    if(staleBirth && unrelatedCopy && lateMetadata){
      if(copyGrouping==1 || !copyMetadata)return 1;
      const NSUInteger from=unrelatedCopy==1?(staleSlots+1)*24:24;
      const uint64_t value=0x7361646f706d6574ULL;memcpy((char*)copySource.contents+from+16,&value,8);
      memcpy(unrelatedExpected.data()+copyTo,(char*)copySource.contents+from+copyOffset,copyBytes);
    }
    if(staleBirth && submitBeforeRetire)[command commit];
    if(staleBirth && !earlyRetire){
      // A valid ordinary background-input copy has been encoded, without
      // consuming the table. Exercise retirement on both sides of submission.
      if(annotation(typedTable,"metal.descriptorSlotEvent",slotOffset+(staleSlots-1)*24,staleSlots,1,4) ||
         annotation(typedTable,"metal.descriptorSlotEvent",slotOffset,staleSlots+1,0,4) ||
         annotation(typedTable,"metal.descriptorSlotEvent",slotOffset,staleSlots+1,1,4))return 8;
    }
    if(staleBirth && unrelatedCopy && copyGrouping==2)[unrelatedCommand commit];
    if(!(staleBirth && submitBeforeRetire))[command commit];
    [command waitUntilCompleted];if(command.error || command.status!=MTLCommandBufferStatusCompleted)return 4;
    if(staleBirth && unrelatedCopy && copyGrouping!=0){[unrelatedCommand waitUntilCompleted];if(unrelatedCommand.error)return 4;}
    if(staleBirth && unrelatedCopy){if(memcmp((char*)copyResult.contents+resultOffset,unrelatedExpected.data(),24))return 6;}
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 5;
    if(staleBirth)memcpy(expected.data(),initial.data()+offset,count);
    else for(unsigned i=0;i<count;i++)expected[i]=uint32_t(initial[i+offset])^(i*17u+5u);
    if(memcmp(output.contents,expected.data(),expected.size()*4))return 6;
    auto write=[&](const char *suffix,const void *data,size_t length){std::string name=std::string(argv[1])+suffix;FILE *f=fopen(name.c_str(),"wbx");if(!f)return false;bool ok=fwrite(data,1,length,f)==length;fclose(f);return ok;};
    if(staleBirth && unrelatedCopy && !write(".unrelated-output.bin",unrelatedExpected.data(),unrelatedExpected.size()))return 7;
    if(copyPointer || resultTable){
      const uint64_t layout[]={unrelatedCopy==1?(staleSlots+1)*24ULL:24ULL,copyOffset,copyTo,copyBytes,pointerOffset};
      if(resultTable){const uint64_t tableLayout[]={layout[0],layout[1],layout[2],layout[3],layout[4],resultOffset,0};if(!write(".unrelated-layout.bin",tableLayout,sizeof(tableLayout)))return 7;}
      else if(!write(".unrelated-layout.bin",layout,sizeof(layout)))return 7;
    }
    if(staleBirth && interleaveCompute){
      std::vector<uint32_t> computeExpected(count+16,0xcdcdcdcdU);
      if(interleaveCompute==2)for(unsigned i=0;i<count;i++)computeExpected[i]=uint32_t(initial[i+offset])^(i*17u+5u);
      if(memcmp(computeResult.contents,computeExpected.data(),computeExpected.size()*4) ||
         !write(".compute-output.bin",computeExpected.data(),computeExpected.size()*4))return 6;
    }
    if(staleBirth && renderWidth){
      std::vector<unsigned char> actual(renderExpected.size());
      if(bufferTexture){
        const unsigned rowBytes=renderWidth*(renderHalf?8:4);
        textureParentExpected=textureParentInitial;
        for(unsigned y=0;y<renderHeight;y++){
          memcpy(actual.data()+y*rowBytes,(char*)textureParent.contents+textureOffset+y*texturePitch,rowBytes);
          memcpy(textureParentExpected.data()+textureOffset+y*texturePitch,renderExpected.data()+y*rowBytes,rowBytes);
        }
        if(memcmp(textureParent.contents,textureParentExpected.data(),textureParentExpected.size()) ||
           !write(".texture-parent-initial.bin",textureParentInitial.data(),textureParentInitial.size()) ||
           !write(".texture-parent-output.bin",textureParentExpected.data(),textureParentExpected.size()))return 6;
      }else [renderResult getBytes:actual.data() bytesPerRow:renderWidth*(renderHalf?8:4) fromRegion:MTLRegionMake2D(0,0,renderWidth,renderHeight) mipmapLevel:0];
      if(actual!=renderExpected || !write(computeTexture?".compute-texture-output.bin":".render-output.bin",actual.data(),actual.size()))return 6;
    }
    if(!write(".input.bin",initial.data(),initial.size()) || !write(".output.bin",expected.data(),expected.size()*4))return 7;
    printf("Native complete output and padding MATCH count=%u offset=%u unretained=%d\n",count,offset,unretained);
  }
}
