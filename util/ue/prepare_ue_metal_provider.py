#!/usr/bin/env python3
"""Patch an isolated UE MetalRHI copy with our diagnostic provider hooks."""
import argparse
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--placement-heap-size-mib', type=int, choices=(32,64,128,256,512),
                        help='Optional diagnostic Mac placement block size in the isolated source only')
    args = parser.parse_args()
    engine, source = args.engine.resolve(), args.source.resolve()
    if source.is_relative_to(engine):
        parser.error('Refusing to patch the installed engine')
    if source.exists():
        parser.error('Use a fresh isolated source directory')
    repo = Path(__file__).resolve().parents[2]
    original = engine / 'Source/Runtime/Apple/MetalRHI'
    content = (original / 'Private/MetalBindlessDescriptors.cpp').read_text()
    replacements = [
        ('#include "MetalDynamicRHI.h"', '#include "MetalDynamicRHI.h"\n#include "RenderDocMetalDescriptorProvider.h"'),
        ('FMetalDescriptorHeap::~FMetalDescriptorHeap()\n{',
         'FMetalDescriptorHeap::~FMetalDescriptorHeap()\n{\n\tRenderDocMetalDescriptorProvider::Destroy(this);'),
        ('Manager = new FRHIHeapDescriptorAllocator(TypeMask, HeapSize / sizeof(IRDescriptorTableEntry), {});',
         'Manager = new FRHIHeapDescriptorAllocator(TypeMask, HeapSize / sizeof(IRDescriptorTableEntry), {});\n\tRenderDocMetalDescriptorProvider::Init(this, Device, ResourceHeap->GetCurrentBuffer(), TypeMask == ERHIDescriptorTypeMask::Sampler, HeapSize / sizeof(IRDescriptorTableEntry));'),
        ('\n\treturn Handle;\n}', '\n\tRenderDocMetalDescriptorProvider::Allocate(this, Handle);\n\treturn Handle;\n}'),
        ('\t\tManager->Free(DescriptorHandle);',
         '\t\tRenderDocMetalDescriptorProvider::Free(this, DescriptorHandle);\n\t\tManager->Free(DescriptorHandle);'),
        ('GetDescriptorMemory()[DescriptorIndex] = DescriptorData;',
         'GetDescriptorMemory()[DescriptorIndex] = DescriptorData;\n\tRenderDocMetalDescriptorProvider::CPUWrite(this, DescriptorHandle);'),
        ('Context.RHIDispatchComputeShader(1, 1, 1);',
         'Context.RHIDispatchComputeShader(1, 1, 1);\n\tRenderDocMetalDescriptorProvider::GPUValues(Device, Dest->GetCurrentBuffer(), PendingDescriptorUpdates, ProviderSources, DescriptorEntriesBuffer);'),
    ]
    before = 'StandardResources.Init(RHICmdList, GBindlessResourceDescriptorHeapCount * sizeof(IRDescriptorTableEntry));'
    if content.count(before) != 1:
        parser.error('Missing primary descriptor destination initialization')
    content = content.replace(before, before + '\n\tRenderDocMetalDescriptorProvider::MarkGPUWrites(Device, StandardResources.GetHeap()->GetCurrentBuffer());')
    # The plain and initialized overload both return Handle: only the plain one
    # owns allocation. Avoid emitting a second allocation after CPU initialization.
    for before, after in replacements:
        expected = 2 if before == '\n\treturn Handle;\n}' else 1
        if content.count(before) != expected:
            parser.error(f'UE source does not match expected 5.8.3 hook: {before[:80]}')
        content = content.replace(before, after, 1)
    content = content.replace('RenderDocMetalDescriptorProvider::CPUWrite(this, DescriptorHandle);',
        'RenderDocMetalDescriptorProvider::CPUWrite(this, DescriptorHandle, DescriptorData);')
    source_hooks = [
        ('IRDescriptorTableSetSampler(&DescriptorData, SamplerState, 0.0f);',
         'RenderDocMetalDescriptorProvider::Created(DescriptorData, SamplerState, 2);'),
        ('IRDescriptorTableSetTexture(&DescriptorData, Texture, 0.0f, 0u);',
         'RenderDocMetalDescriptorProvider::Created(DescriptorData, Texture, 1);'),
        ('IRDescriptorTableSetBuffer(&DescriptorData, Buffer->GetGPUAddress() + ExtraOffset, Buffer->GetLength());',
         'RenderDocMetalDescriptorProvider::Created(DescriptorData, Buffer->GetMTLBuffer(), 0, Buffer->GetOffset() + ExtraOffset);'),
        ('IRDescriptorTableSetBuffer(&DescriptorData, View.Buffer->GetGPUAddress() + View.Offset, View.Size);',
         'RenderDocMetalDescriptorProvider::Created(DescriptorData, View.Buffer->GetMTLBuffer(), 0, View.Buffer->GetOffset() + View.Offset);'),
        ('IRDescriptorTableSetBufferView(&DescriptorData, &BufferView);',
         'RenderDocMetalDescriptorProvider::Created(DescriptorData, View.Buffer->GetMTLBuffer(), 0, View.Buffer->GetOffset() + View.Offset, View.Texture.get());'),
    ]
    for before, hook in source_hooks:
        if content.count(before) != 1:
            parser.error('Missing exact descriptor factory hook: ' + before)
        content = content.replace(before, before + '\n\t' + hook)
    before = '\tRHICmdList.EnqueueLambda([this, UpdateType, Descriptor, DescriptorHandle](FRHICommandListBase& RHICmdList)\n\t{'
    after = '\tconst auto ProviderCreated = RenderDocMetalDescriptorProvider::TakeCreated(Descriptor);\n\tRHICmdList.EnqueueLambda([this, UpdateType, Descriptor, DescriptorHandle, ProviderCreated](FRHICommandListBase& RHICmdList)\n\t{\n\t\tRenderDocMetalDescriptorProvider::ScopeCreated ProviderScope(ProviderCreated);'
    if content.count(before) != 1:
        parser.error('Missing asynchronous descriptor update hook')
    content = content.replace(before, after)
    for call, context in [('Context.EnqueueDescriptorUpdate(DescriptorHandle, Descriptor);', '&Context'),
                          ('Context->EnqueueDescriptorUpdate(DescriptorHandle, Descriptor);', 'Context')]:
        if content.count(call) != 1:
            parser.error('Missing exact descriptor enqueue hook: ' + call)
        content = content.replace(call, 'RenderDocMetalDescriptorProvider::Queue(' + context + ', DescriptorHandle, Descriptor);\n\t\t' + call)
    before = '\tFMetalDescriptorHeap* Heap = new FMetalDescriptorHeap(Device, ERHIDescriptorTypeMask::Standard);'
    if content.count(before) != 1:
        parser.error('Missing pending descriptor flush hook')
    content = content.replace(before, '\tconst auto ProviderSources = RenderDocMetalDescriptorProvider::TakeQueue(&Context, PendingDescriptorUpdates);\n' + before)
    before = 'Heap->Init(RHICmdList, 256);'
    if content.count(before) != 1:
        parser.error('Missing exact temporary three-handle heap initialization')
    content = content.replace(before,
        '{\n\t\tRenderDocMetalDescriptorProvider::HeapTableCountScope ProviderCount(3);\n\t\t' + before + '\n\t}')
    before = 'FMemory::Memcpy(DescriptorEntriesBuffer->Contents(), PendingDescriptorUpdates.Descriptors.GetData(), NumDescriptors * sizeof(IRDescriptorTableEntry));'
    if content.count(before) != 1:
        parser.error('Missing exact temporary payload copy hook')
    content = content.replace(before, before + '\n\tRenderDocMetalDescriptorProvider::Payload(DescriptorEntriesBuffer.Get(), Device, DescriptorEntriesBuffer, ProviderSources);')
    before = '\n\t\tdelete Heap;'
    if content.count(before) != 1:
        parser.error('Missing exact deferred payload retirement hook')
    content = content.replace(before, '\n\t\tRenderDocMetalDescriptorProvider::Destroy(DescriptorEntriesBuffer.Get());' + before)
    def patch_file(relative, hooks):
        text = (original / relative).read_text()
        for before, after, expected in hooks:
            if text.count(before) != expected:
                parser.error(f'Missing exact source hook in {relative}: {before[:80]}')
            text = text.replace(before, after)
        return text
    static = patch_file('Private/MetalStaticSamplers.cpp', [
        ('#include "MetalDynamicRHI.h"', '#include "MetalDynamicRHI.h"\n#include "MetalBindlessDescriptors.h"\n#include "RenderDocMetalDescriptorProvider.h"', 1),
        ('memcpy(StaticSamplersTable->Contents(), SamplerTableContent, TableSize);',
         'memcpy(StaticSamplersTable->Contents(), SamplerTableContent, TableSize);\n\tRenderDocMetalDescriptorProvider::StaticTable(this, Device, StaticSamplersTable, StaticSamplers);', 1),
        ('FMetalDynamicRHI::Get().DeferredDelete(StaticSamplersTable);',
         'FMetalDynamicRHI::Get().DeferredDelete([ProviderOwner = this, ProviderBuffer = StaticSamplersTable]()\n\t{\n\t\tRenderDocMetalDescriptorProvider::Destroy(ProviderOwner);\n\t});', 1),
    ])
    cache_hooks = [
        ('#include "RHIUniformBufferUtilities.h"', '#include "RHIUniformBufferUtilities.h"\n#include "RenderDocMetalDescriptorProvider.h"', 1),
        ('FMetalStateCache::~FMetalStateCache()\n{', 'FMetalStateCache::~FMetalStateCache()\n{\n\tRenderDocMetalDescriptorProvider::InlineReset(this);', 1),
        ('void FMetalStateCache::Reset()\n{', 'void FMetalStateCache::Reset()\n{\n\tRenderDocMetalDescriptorProvider::InlineReset(this);', 1),
        ('CBVTable[Frequency][Index] = Buffer->GetGPUAddress();',
         'CBVTable[Frequency][Index] = Buffer->GetGPUAddress();\n\tRenderDocMetalDescriptorProvider::InlineSource(this, uint32(Frequency), Index, Buffer);', 2),
        ('VertexBufferVAs[Index].addr = SideBuffer->GetGPUAddress();',
         'VertexBufferVAs[Index].addr = SideBuffer->GetGPUAddress();\n\t\t\t\tRenderDocMetalDescriptorProvider::InlineSource(this, 0xffffffffu, Index, SideBuffer);', 1),
        ('VertexBufferVAs[Index].addr = VertexBuffers[Index].Buffer->GetGPUAddress() + Offset;',
         'VertexBufferVAs[Index].addr = VertexBuffers[Index].Buffer->GetGPUAddress() + Offset;\n\t\t\t\tRenderDocMetalDescriptorProvider::InlineSource(this, 0xffffffffu, Index, VertexBuffers[Index].Buffer, Offset);', 1),
    ]
    before = 'Encoder->SetShaderBytes(FunctionType, (const uint8*)CBVTable[Frequency], sizeof(uint64) * (Shader->Bindings.RSNumCBVs+1), kIRArgumentBufferBindPoint);'
    hook = 'RenderDocMetalDescriptorProvider::Inline(Device, this, uint32(Frequency), FunctionType == MTL::FunctionTypeKernel ? static_cast<void *>(Encoder->GetComputeCommandEncoder()) : static_cast<void *>(Encoder->GetRenderCommandEncoder()), RenderDocMetalDescriptorProvider::Stage(FunctionType), kIRArgumentBufferBindPoint, CBVTable[Frequency], Shader->Bindings.RSNumCBVs + 1, 8, StaticSamplersTable);\n\t'
    cache_hooks.append((before, hook + before, 1))
    for name, stage in [('Mesh', 4), ('Object', 3), ('Vertex', 1)]:
        before = f'Encoder->set{name}Bytes(VertexBufferVAs, sizeof(VertexBufferVAs), kIRVertexBufferBindPoint);'
        hook = f'RenderDocMetalDescriptorProvider::Inline(Device, this, 0xffffffffu, Encoder, {stage}, kIRVertexBufferBindPoint, VertexBufferVAs, UE_ARRAY_COUNT(VertexBufferVAs), sizeof(IRRuntimeVertexBuffer));\n\t\t'
        cache_hooks.append((before, hook + before, 1))
    cache = patch_file('Private/MetalStateCache.cpp', cache_hooks)
    before = '\n}\n\nvoid FMetalStateCache::CommitComputeResources(FMetalCommandEncoder* Compute)'
    if cache.count(before) != 1:
        parser.error('Missing end of all-stage render resource commit')
    declaration = '''
#if METAL_USE_METAL_SHADER_CONVERTER
    if(IsMetalBindlessEnabled()
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
       && !IsValidRef(GraphicsPSO->GeometryShader)
#endif
#if PLATFORM_SUPPORTS_MESH_SHADERS
       && !IsValidRef(GraphicsPSO->MeshShader) && !IsValidRef(GraphicsPSO->AmplificationShader)
#endif
      )
        RenderDocMetalDescriptorProvider::RenderWritesDeclared(Device, Raster->GetRenderCommandEncoder());
#endif
'''
    cache = cache.replace(before, declaration + before)
    command_hooks = [('#include "MetalRHIPrivate.h"',
                      '#include "MetalRHIPrivate.h"\n#include "RenderDocMetalDescriptorProvider.h"', 1)]
    for call in [
        'IRRuntimeDrawPrimitives(CurrentEncoder.GetRenderCommandEncoder(), TranslatePrimitiveType(PrimitiveType), BaseVertexIndex, NumVertices, NumInstances, 0);',
        'IRRuntimeDrawIndexedPrimitives(CurrentEncoder.GetRenderCommandEncoder(), TranslatePrimitiveType(PrimitiveType), NumIndices, IndexType, IndexBufferPtr->GetMTLBuffer(), BaseIndexLocation, NumInstances, BaseVertexIndex, FirstInstance);',
    ]:
        prefix = ('RenderDocMetalDescriptorProvider::DrawConstants(Device, CurrentEncoder.GetRenderCommandEncoder(), kIRArgumentBufferDrawArgumentsBindPoint, sizeof(IRRuntimeDrawParams));\n\t\t'
                  'RenderDocMetalDescriptorProvider::DrawConstants(Device, CurrentEncoder.GetRenderCommandEncoder(), kIRArgumentBufferUniformsBindPoint, sizeof(uint16));\n\t\t')
        command_hooks.append((call, prefix + call, 1))
    for call in [
        'IRRuntimeDrawPrimitives(CurrentEncoder.GetRenderCommandEncoder(), TranslatePrimitiveType(PrimitiveType), TheBackingBuffer->GetMTLBuffer(), TheBackingBuffer->GetOffset() + ArgumentOffset);',
        'IRRuntimeDrawIndexedPrimitives(CurrentEncoder.GetRenderCommandEncoder(), TranslatePrimitiveType(PrimitiveType), IndexBuffer->GetIndexType(), TheBackingIndexBuffer->GetMTLBuffer(), TheBackingIndexBuffer->GetOffset(), TheBackingBuffer->GetMTLBuffer(), TheBackingBuffer->GetOffset() + ArgumentOffset);',
    ]:
        prefix = 'RenderDocMetalDescriptorProvider::DrawConstants(Device, CurrentEncoder.GetRenderCommandEncoder(), kIRArgumentBufferUniformsBindPoint, sizeof(uint16));\n\t\t\t'
        command_hooks.append((call, prefix + call, 1))
    call = 'CurrentEncoder.GetRenderCommandEncoder()->setVertexBytes(&NullBuffer, sizeof(uint32), kIRArgumentBufferUniformsBindPoint);'
    prefix = 'RenderDocMetalDescriptorProvider::DrawConstants(Device, CurrentEncoder.GetRenderCommandEncoder(), kIRArgumentBufferUniformsBindPoint, sizeof(uint32));\n\t\t\t'
    command_hooks.append((call, prefix + call, 2))
    commands = patch_file('Private/MetalCommands.cpp', command_hooks)
    placement = None
    if args.placement_heap_size_mib is not None:
        placement = (original / 'Private/MetalBuffer.cpp').read_text()
        before = 'static constexpr uint32 METALHEAP_DEFAULT_BLOCKSIZE = 1024 << 19; // 512mb'
        if placement.count(before) != 1:
            parser.error('Missing exact UE5.8.3 Mac placement heap block size')
        placement = placement.replace(before,
            f'static constexpr uint32 METALHEAP_DEFAULT_BLOCKSIZE = {args.placement_heap_size_mib}u * 1024u * 1024u; // isolated diagnostic block size')
    shutil.copytree(original, source)
    for path in source.rglob('*'):
        if path.is_file():
            path.chmod(path.stat().st_mode | 0o200)
    (source / 'Private/MetalBindlessDescriptors.cpp').write_text(content)
    (source / 'Private/MetalStaticSamplers.cpp').write_text(static)
    (source / 'Private/MetalStateCache.cpp').write_text(cache)
    (source / 'Private/MetalCommands.cpp').write_text(commands)
    if placement is not None:
        (source / 'Private/MetalBuffer.cpp').write_text(placement)
    shutil.copyfile(repo / 'util/ue/metal_provider/RenderDocMetalDescriptorProvider.h',
                    source / 'Private/RenderDocMetalDescriptorProvider.h')
    shutil.copyfile(repo / 'renderdoc/api/app/renderdoc_app.h', source / 'Private/renderdoc_app.h')
    print(f'Prepared isolated provider source: {source}')
    print('Diagnostic records only. No coverage declaration or engine installation.')
    if placement is not None:
        print(f'Isolated Mac placement block size: {args.placement_heap_size_mib} MiB; resource larger than this still uses its required size')


if __name__ == '__main__':
    main()
