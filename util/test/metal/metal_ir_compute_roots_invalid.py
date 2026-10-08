#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject damaged converted-compute mixed root/table closures before GPU submission."""
import argparse, copy, fcntl, hashlib, json, os, signal, struct, subprocess, tempfile
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--work-dir',type=Path,required=True)
    p.add_argument('--oracle',type=Path,required=True);p.add_argument('--hit',type=int,required=True)
    p.add_argument('--case',action='append',help='Run selected development cases after a fixture correction')
    p.add_argument('--sampler-outside-library',type=Path,
                   help='Compiled seven-sampler library for pre-submit rejection only; never execute natively')
    a=p.parse_args();r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=a.work_dir.resolve();w.mkdir(parents=True,exist_ok=True)
    original=ET.parse(a.source);f=lambda c,n:c.find('./*[@name="'+n+'"]')
    with zipfile.ZipFile(a.source.with_suffix('')) as z:blobs={n:z.read(n) for n in z.namelist()}
    named=lambda tree,n:[c for c in tree.find('./chunks') if c.get('name')==n]
    rootName='MTLComputePipelineState::DeclareIRComputeRoot';heapName='MTLComputePipelineState::DeclareIRComputeHeapEntry'
    assert len(named(original,rootName)) in (5,6) and len(named(original,heapName)) in (1,2,3)
    textureWrites=[c for c in named(original,heapName) if f(c,'kind').text in ('4','8')]
    currentWrites=any(f(c,'kind').text=='8' for c in textureWrites)
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    m=dict(status='RUNNING',scope='B544 development closure checks, not final integrated acceptance',backend_sha256=sha(b/'lib/librenderdoc.dylib'),checks=[],bad_groups=0,preGPU_rejections=0,legal_controls=0)
    if a.sampler_outside_library:m['outside_sampler_library_sha256']=sha(a.sampler_outside_library)
    def run(tag,cmd,expected):
        env=os.environ.copy()
        for k in tuple(env):
            if k.startswith('RENDERDOC_') or k=='DYLD_INSERT_LIBRARIES':env.pop(k)
        env.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
        if tag.startswith('sampler-consumer-outside'):env['RENDERDOC_METAL_TRACE_UNIFORM_PROOFS']='1'
        if tag.startswith('selector-buffer-'):env['RENDERDOC_METAL_TRACE_UNIFORM_PROOFS']='1'
        with (w/(tag+'.log')).open('w') as log,(w/(tag+'-renderdoc.log')).open('a') as keep:
            fcntl.flock(keep,fcntl.LOCK_SH);child=subprocess.Popen(list(map(str,cmd)),env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try:code=child.wait(timeout=45)
            except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait();code=124
        text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
        bad=['Assertion failed','failed assertion','OVERRUNNING CHUNK','Unexpected Metal resource type','m_ResourceMap.empty','m_ResourceRecords.empty']
        if expected:bad+=['Metal replay wait begin']
        hits=[marker for marker in bad if marker in text];ok=code==expected and not hits
        if tag in ('sampler-consumer-outside-API','sampler-consumer-outside-CLI'):
            ok &= 'MetalUniformConsumer ' in text and 'sampler=7 unknownSampler=1' in text
        if tag.startswith('selector-buffer-') and tag.endswith(('-API','-CLI')):
            ok &= 'MetalUniformConsumer ' in text and 'bufferWrite=1' in text
        m['checks'].append(dict(tag=tag,exit=code,passed=ok,diagnostic_hits=hits));(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');print(tag,code,flush=True)
        if not ok:raise RuntimeError(tag+text[-2200:])
    cases=['root-missing','root-duplicate','root-frame','root-pipeline-zero','root-offset-align','root-offset-outside','root-kind','root-count-zero','root-count-over','root-bytes','sampler-kind','sampler-count','sampler-bytes','range-missing','range-output-AS','heap-duplicate','heap-sampler-namespace','heap-kind','heap-bytes','heap-AS-slot','heap-output-slot','heap-outside','texture-source-missing','texture-source-kind','sampler-source-offset','sampler-slot-type','inline-table-source-output','legal-original']
    firstBinding=next(c for c in named(original,'MTLCommandEncoder::DescriptorInlineBinding') if f(c,'entry').text=='0')
    frameTarget=f(firstBinding,'resource').text
    frameCopies=[c for c in named(original,'MTLBlitCommandEncoder::copyFromBuffer') if f(c,'destinationBuffer').text==frameTarget]
    largeCreation=next((c for c in named(original,'MTLDevice::newBufferWithBytes') if f(c,'Buffer').text==frameTarget and int(f(c,'length').text)>65536),None)
    if largeCreation is not None:
        cases=cases[:-1]+['CBV-creation-empty','CBV-creation-short','CBV-backing-over-limit','CBV-read-over-limit','legal-original']
    frameSnapshots=[c for c in named(original,'Internal_MTLBufferModifyCPUContents') if f(c,'Buffer').text==frameTarget]
    if frameSnapshots:
        cases=cases[:-1]+['CBV-snapshot-missing','CBV-snapshot-short','CBV-snapshot-outside','CBV-snapshot-private','legal-original']
    if frameCopies:
        cases=cases[:-1]+['CBV-copy-missing','CBV-copy-short','CBV-copy-tail','CBV-copy-zero','CBV-copy-self','CBV-copy-private-source','CBV-copy-late-submit','legal-split-copy','legal-original']
        if currentWrites:
            cases=cases[:-1]+['selector-AS-untyped','selector-AS-outside','selector-UAV-SRV-role',
                'selector-secondary-SRV-role','selector-UAV-numeric-family','selector-SRV-UAV-role','legal-original']
        if not textureWrites:
            cases=cases[:-1]+['selector-buffer-write-outside','selector-buffer-write-overflow',
                             'selector-buffer-AS-role','selector-buffer-SRV-role','legal-original']
    queryDecl=named(original,'MTLComputePipelineState::DeclareRayQueryHeapDispatch')[0]
    output=f(queryDecl,'output').text
    outputInitial=next((c for c in named(original,'Internal::Initial Contents') if f(c,'id').text==output),None)
    if outputInitial is not None:
        cases=cases[:-1]+['output-initial-missing']+(['output-initial-short'] if f(outputInitial,'Contents') is not None else [])+['output-declaration-alias','output-identity-missing','legal-original']
    if textureWrites:
        cases=cases[:-1]+['texture-write-missing','texture-write-duplicate','texture-write-role','texture-write-namespace','texture-write-index','texture-write-bytes','texture-output-slot-type','texture-output-source','texture-output-readonly','legal-original']
    if textureWrites:
        cases=cases[:-1]+['texture-input-initial-missing','texture-input-write-residency','texture-input-no-read-usage','legal-original']
    inputBinding=next(c for c in named(original,'MTLBuffer::DescriptorSlotBinding') if f(c,'offset').text=='72' and f(c,'kind').text=='1')
    inputTexture=f(inputBinding,'resource').text
    if textureWrites and any(f(c,'Texture').text==inputTexture for c in named(original,'MTLHeap::newTexture(offset)')):
        cases=cases[:-1]+['texture-input-heap-zero','texture-input-heap-no-room','texture-input-heap-offset','legal-original']
    reflectionName='MTLComputePipelineState::CaptureIRComputeReflection'
    if named(original,reflectionName):
        cases=cases[:-1]+['reflection-duplicate','reflection-frame','reflection-pipeline-zero',
            'reflection-pipeline-buffer','reflection-empty','reflection-oversize',
            'ABI-root-binding','ABI-root-rebound-buffer','ABI-threadgroup','ABI-root-length','ABI-binding-bool',
            'ABI-binding-alias','ABI-root-overlap','ABI-root-type','ABI-query-claim',
            'ABI-resource-claim','legal-runtime-ABI','legal-original']
    if currentWrites:
        cases=cases[:-1]+['multi-secondary-role-missing','multi-secondary-role-read','multi-secondary-initial-missing',
            'multi-secondary-identity-missing','multi-secondary-readonly','multi-rebind-source-missing',
            'multi-rebind-generation','multi-rebind-source-alias','multi-producer-late-submit','legal-original']
    texelCreation=next((c for c in named(original,'MTLBuffer::newTextureWithDescriptor') if f(c,'Texture').text==output),None)
    texelInitial=None
    if texelCreation is not None:
        texelParent=f(texelCreation,'Buffer').text
        texelInitial=next(c for c in named(original,'Internal::Initial Contents') if f(c,'id').text==texelParent)
        cases=cases[:-1]+['texel-parent-initial-missing','texel-parent-initial-short','texel-offset-align',
                         'texel-offset-outside','texel-parent-SRV-alias','texel-parent-CBV-alias','texel-row-short','texel-height','texel-readonly','texel-numeric-family','legal-original']
    if a.sampler_outside_library:
        cases=cases[:-1]+['sampler-consumer-outside','legal-original']
    if a.case:
        if not set(a.case).issubset(cases):p.error('selected case is unavailable for this capture')
        cases=[case for case in cases if case in a.case]
    m['selected_cases']=cases
    for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
    try:
        for tag in cases:
            tree=copy.deepcopy(original);ch=tree.find('./chunks');roots=named(tree,rootName);root=roots[0];sampler=roots[-1];entry=next(c for c in named(tree,heapName) if f(c,'kind').text=='1')
            blobPatches={};libraryPatch=None
            assert f(sampler,'kind').text=='3'
            if tag=='sampler-consumer-outside':
                library=next(c for c in ch if c.get('name','').startswith('MTLDevice::newLibrary') and f(c,'data') is not None)
                data=a.sampler_outside_library.read_bytes();assert data
                libraryPatch=(int(f(library,'data').text),data)
                f(library,'data').set('byteLength',str(len(data)));library.set('length',str(len(data)+4096))
            elif tag.startswith('texel-'):
                view=next(c for c in named(tree,'MTLBuffer::newTextureWithDescriptor') if f(c,'Texture').text==output)
                desc=f(view,'descriptor');parent=f(view,'Buffer').text
                initial=next(c for c in named(tree,'Internal::Initial Contents') if f(c,'id').text==parent)
                if tag=='texel-parent-initial-missing':ch.remove(initial)
                elif tag=='texel-parent-initial-short':f(initial,'Contents').set('byteLength',str(len(blobs[f'{int(f(initial,"Contents").text):06}'])-1))
                elif tag=='texel-parent-SRV-alias':
                    readBinding=next(c for c in named(tree,'MTLBuffer::DescriptorSlotBinding') if f(c,'offset').text=='96' and f(c,'kind').text=='1')
                    readView=next(c for c in named(tree,'MTLBuffer::newTextureWithDescriptor') if f(c,'Texture').text==f(readBinding,'resource').text)
                    f(view,'Buffer').text=f(readView,'Buffer').text
                    # Keep the aliased parent alive at view creation, so rejection
                    # exercises read/write overlap rather than a forward reference.
                    birth=next(c for c in named(tree,'MTLDevice::newBufferWithLength') if f(c,'Buffer').text==f(readView,'Buffer').text)
                    ch.remove(birth);ch.insert(list(ch).index(view),birth)
                elif tag=='texel-parent-CBV-alias':f(view,'Buffer').text=frameTarget
                elif tag=='texel-offset-align':f(view,'offset').text='1'
                elif tag=='texel-offset-outside':f(view,'offset').text=str(128*1024*1024)
                elif tag=='texel-row-short':f(view,'bytesPerRow').text='256'
                elif tag=='texel-height':f(desc,'height').text='2'
                elif tag=='texel-readonly':f(desc,'usage').text='1'
                elif tag=='texel-numeric-family':f(desc,'pixelFormat').text='55'
                else:raise AssertionError(tag)
            elif tag.startswith('selector-'):
                rootEntry=0 if tag in ('selector-AS-untyped','selector-AS-outside','selector-SRV-UAV-role') else 1
                binding=next(c for c in named(tree,'MTLCommandEncoder::DescriptorInlineBinding') if f(c,'entry').text==str(rootEntry))
                target=f(binding,'resource').text;at=int(f(binding,'memberOffset').text)
                cp=next(c for c in named(tree,'MTLBlitCommandEncoder::copyFromBuffer') if f(c,'destinationBuffer').text==target)
                source=f(cp,'sourceBuffer').text
                creation=next(c for c in named(tree,'MTLDevice::newBufferWithBytes') if f(c,'Buffer').text==source)
                selector,value={'selector-buffer-write-outside':(4,8),'selector-buffer-write-overflow':(4,4294967295),
                    'selector-buffer-AS-role':(0,1),'selector-buffer-SRV-role':(0,3),
                    'selector-AS-untyped':(0,0),'selector-AS-outside':(0,1000),
                    'selector-UAV-SRV-role':(0,3),'selector-secondary-SRV-role':(8,3),
                    'selector-UAV-numeric-family':(0,4),'selector-SRV-UAV-role':(12,2)}[tag]
                sourceAt=int(f(cp,'sourceOffset').text)+at+selector-int(f(cp,'destinationOffset').text)
                blobPatches[int(f(creation,'initialData').text)]=(sourceAt,value)
                # CPU publication snapshots supersede creation bytes at commit.
                # Mutate every authoritative version of this upload scalar;
                # otherwise the intended damaged selector never reaches replay.
                for snapshot in named(tree,'Internal_MTLBufferModifyCPUContents'):
                    if f(snapshot,'Buffer').text!=source:continue
                    start=int(f(snapshot,'start').text);size=int(f(snapshot,'size').text)
                    if start<=sourceAt and sourceAt+4<=start+size:
                        blobPatches[int(f(snapshot,'data').text)]=(sourceAt-start,value)
                for initial in named(tree,'Internal::Initial Contents'):
                    if f(initial,'id').text==source and f(initial,'Contents') is not None:
                        blobPatches[int(f(initial,'Contents').text)]=(sourceAt,value)
            elif tag.startswith('ABI-') or tag=='legal-runtime-ABI':
                fact=named(tree,reflectionName)[0]
                compiler=json.loads(f(fact,'reflection').text)
                payload=dict(Origin='MetalIRRuntimeBindings',ShaderType='Compute',RootBindPoint=2,
                    ResourceHeapBindPoint=0,SamplerHeapBindPoint=1,StaticSamplerCount=6,
                    TopLevelArgumentBuffer=compiler['TopLevelArgumentBuffer'],state=compiler['state'])
                if tag=='ABI-root-binding':payload['RootBindPoint']=3
                elif tag=='ABI-root-rebound-buffer':
                    setter=copy.deepcopy(named(tree,'MTLComputeCommandEncoder::setBuffer')[0])
                    f(setter,'index').text='2'
                    call=next(c for c in ch if c.get('name','').startswith('MTLComputeCommandEncoder::dispatch'))
                    ch.insert(list(ch).index(call),setter)
                elif tag=='ABI-threadgroup':payload['state']['tg_size']=[32,1,1]
                elif tag=='ABI-root-length':
                    payload['TopLevelArgumentBuffer'].pop(-2)
                    payload['TopLevelArgumentBuffer'][-1]['EltOffset']-=8
                elif tag=='ABI-binding-bool':payload['RootBindPoint']=True
                elif tag=='ABI-binding-alias':payload['SamplerHeapBindPoint']=0
                elif tag=='ABI-root-overlap':payload['TopLevelArgumentBuffer'][-1]['EltOffset']=0
                elif tag=='ABI-root-type':payload['TopLevelArgumentBuffer'][0]['Type']='Table'
                elif tag=='ABI-query-claim':payload['UsesRayQuery']=True
                elif tag=='ABI-resource-claim':payload['UsedResources']=[]
                else:assert tag=='legal-runtime-ABI'
                f(fact,'reflection').text=json.dumps(payload,separators=(',',':'))
            elif tag.startswith('multi-'):
                secondary=next(c for c in named(tree,heapName) if f(c,'index').text=='4')
                slotBindings=[c for c in named(tree,'MTLBuffer::DescriptorSlotBinding') if f(c,'offset').text=='96' and f(c,'kind').text=='1']
                texture=f(slotBindings[0],'resource').text
                if tag=='multi-secondary-role-missing':ch.remove(secondary)
                elif tag=='multi-secondary-role-read':f(secondary,'kind').text='1'
                elif tag=='multi-secondary-initial-missing':
                    ch.remove(next(c for c in named(tree,'Internal::Initial Contents') if f(c,'id').text==texture))
                elif tag=='multi-secondary-identity-missing':
                    ch.remove(next(c for c in named(tree,'MTLResource::CaptureGPUIdentity') if f(c,'resource').text==texture))
                elif tag=='multi-secondary-readonly':
                    creation=next(c for c in ch if f(c,'Texture') is not None and f(c,'Texture').text==texture and f(c,'descriptor') is not None)
                    f(f(creation,'descriptor'),'usage').text='1'
                elif tag=='multi-rebind-source-missing':ch.remove(slotBindings[1])
                elif tag=='multi-rebind-generation':
                    events=[c for c in named(tree,'MTLBuffer::DescriptorSlotEvent') if f(c,'offset').text=='96' and f(c,'event').text=='2']
                    f(events[1],'generation').text='2'
                elif tag=='multi-rebind-source-alias':
                    reads=[c for c in named(tree,'MTLBuffer::DescriptorSlotBinding') if f(c,'offset').text=='72' and f(c,'kind').text=='1']
                    f(slotBindings[1],'resource').text=f(reads[1],'resource').text
                elif tag=='multi-producer-late-submit':
                    dispatches=named(tree,'MTLComputeCommandEncoder::dispatchThreadgroups(indirect)')
                    commands=[]
                    for dispatch in dispatches[:2]:
                        enc=f(dispatch,'ComputeCommandEncoder').text
                        birth=next(c for c in named(tree,'MTLCommandBuffer::computeCommandEncoder') if f(c,'ComputeCommandEncoder').text==enc)
                        commands.append(f(birth,'CommandBuffer').text)
                    first=next(c for c in named(tree,'MTLCommandBuffer::commit') if f(c,'CommandBuffer').text==commands[0])
                    second=next(c for c in named(tree,'MTLCommandBuffer::commit') if f(c,'CommandBuffer').text==commands[1])
                    ch.remove(first)
                    for wait in list(named(tree,'MTLCommandBuffer::waitUntilCompleted')):
                        if f(wait,'CommandBuffer').text==commands[0]:ch.remove(wait)
                    ch.insert(list(ch).index(second)+1,first)
                else:raise AssertionError(tag)
            elif tag.startswith('reflection-'):
                fact=named(tree,reflectionName)[0]
                if tag=='reflection-duplicate':ch.insert(list(ch).index(fact)+1,copy.deepcopy(fact))
                elif tag=='reflection-frame':
                    ch.remove(fact);scope=next(c for c in ch if c.get('id')=='5');ch.insert(list(ch).index(scope)+1,fact)
                elif tag=='reflection-pipeline-zero':f(fact,'pipeline').text='0'
                elif tag=='reflection-pipeline-buffer':f(fact,'pipeline').text=frameTarget
                elif tag=='reflection-empty':f(fact,'reflection').text=''
                else:f(fact,'reflection').text='x'*65537;fact.set('length','66000')
            elif tag.startswith('texture-input-'):
                binding=next(c for c in named(tree,'MTLBuffer::DescriptorSlotBinding') if f(c,'offset').text=='72' and f(c,'kind').text=='1')
                texture=f(binding,'resource').text
                if tag.startswith('texture-input-heap-'):
                    creation=next(c for c in named(tree,'MTLHeap::newTexture(offset)') if f(c,'Texture').text==texture)
                    heap=next(c for c in named(tree,'MTLDevice::newHeapWithDescriptor') if f(c,'Heap').text==f(creation,'Heap').text)
                    if tag=='texture-input-heap-zero':f(heap,'size').text='0'
                    # Native heaps can round their requested capacity upward.
                    # Use an aligned offset beyond every supported heap instead
                    # of assuming a smaller request invalidates this child.
                    elif tag=='texture-input-heap-no-room':f(creation,'offset').text=str(1024*1024*1024)
                    else:f(creation,'offset').text='1'
                elif tag=='texture-input-initial-missing':
                    initial=next(c for c in named(tree,'Internal::Initial Contents') if f(c,'id').text==texture);ch.remove(initial)
                elif tag=='texture-input-write-residency':
                    for c in named(tree,'MTLComputeCommandEncoder::useResource'):
                        if f(c,'resource').text==texture:f(c,'usageValue').text='2'
                else:
                    creation=next(c for c in ch if f(c,'Texture') is not None and f(c,'Texture').text==texture and f(c,'descriptor') is not None)
                    f(f(creation,'descriptor'),'usage').text='2'
            elif tag.startswith('texture-write-'):
                write=next(c for c in named(tree,heapName) if f(c,'kind').text in ('4','8'))
                if tag=='texture-write-missing':ch.remove(write)
                elif tag=='texture-write-duplicate':ch.insert(list(ch).index(write),copy.deepcopy(write))
                else:
                    key,value={'texture-write-role':('kind','1'),'texture-write-namespace':('heap','1'),'texture-write-index':('index','3'),'texture-write-bytes':('bytes','4')}[tag];f(write,key).text=value
            elif tag.startswith('texture-output-'):
                if tag=='texture-output-slot-type':
                    for c in named(tree,'MTLBuffer::DescriptorSlotEvent'):
                        if f(c,'offset').text=='48' and f(c,'descriptorType').text=='5':f(c,'descriptorType').text='4'
                elif tag=='texture-output-source':
                    write=next(c for c in named(tree,'MTLBuffer::DescriptorSlotBinding') if f(c,'offset').text=='48' and f(c,'kind').text=='1')
                    read=next(c for c in named(tree,'MTLBuffer::DescriptorSlotBinding') if f(c,'offset').text=='72' and f(c,'kind').text=='1')
                    f(write,'resource').text=f(read,'resource').text
                else:
                    creation=next(c for c in ch if f(c,'Texture') is not None and f(c,'descriptor') is not None and f(c,'Texture').text==output)
                    f(f(creation,'descriptor'),'usage').text='1'
            elif tag.startswith('output-'):
                if tag in ('output-initial-missing','output-initial-short'):
                    initial=next(c for c in named(tree,'Internal::Initial Contents') if f(c,'id').text==output)
                    if tag=='output-initial-missing':ch.remove(initial)
                    else:f(initial,'Contents').set('byteLength',str(int(f(initial,'Contents').get('byteLength'))-1))
                elif tag=='output-declaration-alias':
                    query=named(tree,'MTLComputePipelineState::DeclareRayQueryHeapDispatch')[0];f(query,'output').text=f(query,'heap').text
                else:
                    identity=next(c for c in ch if 'GPUIdentity' in c.get('name','') and f(c,'resource') is not None and f(c,'resource').text==output)
                    ch.remove(identity)
            elif tag.startswith('CBV-snapshot-'):
                snapshots=[c for c in named(tree,'Internal_MTLBufferModifyCPUContents') if f(c,'Buffer').text==frameTarget]
                for snapshot in snapshots:
                    if tag=='CBV-snapshot-missing':ch.remove(snapshot)
                    elif tag=='CBV-snapshot-short':f(snapshot,'size').text='15';f(snapshot,'data').set('byteLength','15')
                    elif tag=='CBV-snapshot-outside':f(snapshot,'start').text='2097152'
                if tag=='CBV-snapshot-private':
                    creation=next(c for c in named(tree,'MTLDevice::newBufferWithLength') if f(c,'Buffer').text==frameTarget)
                    f(creation,'options').text='32'
            elif tag in ('CBV-creation-empty','CBV-creation-short','CBV-backing-over-limit','CBV-read-over-limit'):
                creation=next(c for c in named(tree,'MTLDevice::newBufferWithBytes') if f(c,'Buffer').text==frameTarget)
                if tag.startswith('CBV-creation-'):
                    payload=f(creation,'initialData');size=0 if tag=='CBV-creation-empty' else int(f(creation,'length').text)-1
                    # Give the importer an internally consistent blob. Rejection
                    # must come from the backend's missing/partial initial state.
                    payload.set('byteLength',str(size))
                elif tag=='CBV-backing-over-limit':f(creation,'length').text=str(128*1024*1024+1)
                else:f(named(tree,'MTLComputePipelineState::DeclareRayQueryHeapCBVRoot')[0],'bytes').text='65537'
            elif tag.startswith('CBV-copy-') or tag=='legal-split-copy':
                cp=next(c for c in named(tree,'MTLBlitCommandEncoder::copyFromBuffer') if f(c,'destinationBuffer').text==frameTarget)
                if tag=='CBV-copy-missing':ch.remove(cp)
                elif tag in ('CBV-copy-short','CBV-copy-tail','CBV-copy-zero'):f(cp,'size').text={'CBV-copy-short':'15','CBV-copy-tail':'79','CBV-copy-zero':'0'}[tag]
                elif tag=='CBV-copy-self':f(cp,'sourceBuffer').text=f(cp,'destinationBuffer').text
                elif tag=='CBV-copy-private-source':
                    # Contributions are Private; their values need an independent
                    # typed producer proof, never CPU-readable upload inference.
                    f(cp,'sourceBuffer').text=f(named(tree,'MTLBuffer::DeclareRayASHeader')[0],'contributions').text
                    f(cp,'size').text='4'
                elif tag=='CBV-copy-late-submit':
                    enc=f(cp,'BlitCommandEncoder').text
                    birth=next(c for c in ch if c.get('name')=='MTLCommandBuffer::blitCommandEncoder' and f(c,'BlitCommandEncoder').text==enc)
                    cmd=f(birth,'CommandBuffer').text
                    commit=next(c for c in named(tree,'MTLCommandBuffer::commit') if f(c,'CommandBuffer').text==cmd)
                    dispatch=named(tree,'MTLComputeCommandEncoder::dispatchThreadgroups(indirect)')[0]
                    encoder=f(dispatch,'ComputeCommandEncoder').text
                    birth=next(c for c in named(tree,'MTLCommandBuffer::computeCommandEncoder') if f(c,'ComputeCommandEncoder').text==encoder)
                    consumer=next(c for c in named(tree,'MTLCommandBuffer::commit') if f(c,'CommandBuffer').text==f(birth,'CommandBuffer').text)
                    # Consumer order is proven at commit, not inferred from the
                    # producer's earlier encoding in the API stream.
                    ch.remove(commit)
                    for wait in list(named(tree,'MTLCommandBuffer::waitUntilCompleted')):
                        if f(wait,'CommandBuffer').text==cmd:ch.remove(wait)
                    ch.insert(list(ch).index(consumer)+1,commit)
                else:
                    other=copy.deepcopy(cp);total=int(f(cp,'size').text);half=total//2
                    f(cp,'size').text=str(half)
                    f(other,'sourceOffset').text=str(int(f(cp,'sourceOffset').text)+half)
                    f(other,'destinationOffset').text=str(int(f(cp,'destinationOffset').text)+half)
                    f(other,'size').text=str(total-half)
                    ch.insert(list(ch).index(cp)+1,other)
            elif tag=='root-missing':ch.remove(root)
            elif tag in ('root-duplicate','heap-duplicate'):
                item=root if tag=='root-duplicate' else entry;ch.insert(list(ch).index(item),copy.deepcopy(item))
            elif tag=='root-frame':
                ch.remove(root);scope=next(c for c in ch if c.get('id')=='5');ch.insert(list(ch).index(scope)+1,root)
            elif tag.startswith('root-'):
                key,value={'root-pipeline-zero':('pipeline','0'),'root-offset-align':('offset','1'),'root-offset-outside':('offset','128'),'root-kind':('kind','1'),'root-count-zero':('count','0'),'root-count-over':('count','65'),'root-bytes':('bytes','17')}[tag];f(root,key).text=value
            elif tag.startswith('sampler-') and tag in ('sampler-kind','sampler-count','sampler-bytes'):
                key,value={'sampler-kind':('kind','2'),'sampler-count':('count','5'),'sampler-bytes':('bytes','143')}[tag];f(sampler,key).text=value
            elif tag=='range-missing':ch.remove(named(tree,'MTLComputePipelineState::DeclareRayQueryHeapCBVRoot')[0])
            elif tag=='range-output-AS':f(named(tree,'MTLComputePipelineState::DeclareRayQueryHeapCBVRoot')[0],'outputSlot').text='24'
            elif tag.startswith('heap-'):
                key,value={'heap-sampler-namespace':('heap','1'),'heap-kind':('kind','2'),'heap-bytes':('bytes','4'),'heap-AS-slot':('index','1'),'heap-output-slot':('index','2'),'heap-outside':('index','683')}[tag];f(entry,key).text=value
            elif tag.startswith('texture-source-'):
                binding=next(c for c in named(tree,'MTLBuffer::DescriptorSlotBinding') if f(c,'offset').text=='72' and f(c,'kind').text=='1')
                if tag=='texture-source-missing':ch.remove(binding)
                else:f(binding,'kind').text='2'
            elif tag=='sampler-source-offset':
                binding=next(c for c in named(tree,'MTLBuffer::DescriptorSlotBinding') if f(c,'kind').text=='2');f(binding,'memberOffset').text='8'
            elif tag=='sampler-slot-type':
                for c in named(tree,'MTLBuffer::DescriptorSlotEvent'):
                    if f(c,'descriptorType').text=='7':f(c,'descriptorType').text='0'
            elif tag=='inline-table-source-output':
                query=named(tree,'MTLComputePipelineState::DeclareRayQueryHeapDispatch')[0]
                for c in named(tree,'MTLCommandEncoder::DescriptorInlineBinding'):
                    if int(f(c,'entry').text)==len(roots)-1:f(c,'resource').text=f(query,'output').text
            else:assert tag=='legal-original'
            padding=4096 if tag.startswith('ABI-') or tag=='legal-runtime-ABI' else 128
            for c in ch:c.set('length',str(int(c.get('length','0'))+padding))
            xml=w/(tag+'.zip.xml');tree.write(xml,encoding='utf-8',xml_declaration=True)
            with zipfile.ZipFile(xml.with_suffix(''),'w',zipfile.ZIP_DEFLATED) as z:
                for n,data in blobs.items():
                    if libraryPatch and int(n)==libraryPatch[0]:data=libraryPatch[1]
                    if int(n) in blobPatches:
                        at,value=blobPatches[int(n)]
                        mutable=bytearray(data);struct.pack_into('<I',mutable,at,value);data=bytes(mutable)
                    if tag.startswith('CBV-creation-') and int(n)==int(f(largeCreation,'initialData').text):
                        data=b'' if tag=='CBV-creation-empty' else data[:-1]
                    if tag=='CBV-snapshot-short' and any(int(n)==int(f(c,'data').text) for c in frameSnapshots):data=data[:15]
                    if tag=='texel-parent-initial-short' and int(n)==int(f(texelInitial,'Contents').text):data=data[:-1]
                    if tag=='output-initial-short' and int(n)==int(f(outputInitial,'Contents').text):data=data[:-1]
                    z.writestr(n,data)
            cap=w/(tag+'.rdc');run(tag+'-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',cap,'-c','rdc'],0)
            if tag in ('legal-original','legal-split-copy','legal-runtime-ABI'):
                run(tag+'-API',[a.oracle,cap,a.hit,132],0);run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','3',cap],0);m['legal_controls']+=1
            else:
                run(tag+'-API',[b/'metal-ray-b534/final-short/opener',cap],4);run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1)
                m['bad_groups']+=1;m['preGPU_rejections']+=2
        assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m['status']='PASS DEVELOPMENT NEGATIVE CHECKS'
    except Exception as ex:m.update(status='FAIL',error=str(ex));raise
    finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')


if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);main()
