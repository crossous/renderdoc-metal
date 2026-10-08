#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject damaged typed indirect query contracts, and verify native per-use mismatches."""
import argparse,copy,fcntl,hashlib,json,os,signal,subprocess,tempfile,xml.etree.ElementTree as ET,zipfile
from pathlib import Path


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--work-dir',type=Path,required=True)
    p.add_argument('--oracle',type=Path,required=True);p.add_argument('--hit',type=int,required=True)
    a=p.parse_args();r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=a.work_dir.resolve();w.mkdir(parents=True,exist_ok=True)
    original=ET.parse(a.source);f=lambda c,n:c.find('./*[@name="'+n+'"]')
    with zipfile.ZipFile(a.source.with_suffix('')) as archive:blobs={n:archive.read(n) for n in archive.namelist()}
    names=lambda tree,n:[c for c in tree.find('./chunks') if c.get('name')==n]
    proofName='MTLComputeCommandEncoder::CaptureIndirectArguments';countName='MTLDevice::CaptureComputeIndirectArgumentsCount'
    # Read the actual count-chunk name instead of guessing its command owner.
    counts=[c for c in original.find('./chunks') if c.get('name')==countName]
    assert len(counts)==1;countName=counts[0].get('name')
    dispatchName='MTLComputeCommandEncoder::dispatchThreadgroups(indirect)';proofs=names(original,proofName);assert len(proofs)==3
    widths=[int(f(c,'groups')[0].text) for c in proofs];assert widths==[1,132,0]
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    m={'status':'RUNNING','backend_sha256':sha(b/'lib/librenderdoc.dylib'),'checks':[],'preGPU_groups':0,'preGPU_rejections':0,'execution_mismatch_groups':0,'execution_mismatch_rejections':0,'legal_controls':0}
    def run(tag,args,code,gpu=False):
        e=os.environ.copy()
        for k in tuple(e):
            if k.startswith('RENDERDOC_') or k=='DYLD_INSERT_LIBRARIES':e.pop(k)
        e.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_INDIRECT_REPLAY='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
        with (w/(tag+'-renderdoc.log')).open('a') as keep,(w/(tag+'.log')).open('w') as log:
            fcntl.flock(keep,fcntl.LOCK_SH);child=subprocess.Popen(list(map(str,args)),env=e,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try:c=child.wait(timeout=60)
            except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait();c=124
        text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
        markers=['Assertion failed','failed assertion','OVERRUNNING CHUNK','File and decompress stream readers do not support seeking','Unexpected Metal resource type','m_ResourceMap.empty','m_ResourceRecords.empty']
        if code and not gpu:markers+=['Metal replay wait begin']
        hits=[x for x in markers if x in text];ok=c==code and not hits
        if code and gpu:ok &= 'Metal replay wait end' in text and 'Metal compute indirect execution-point arguments do not match capture' in text
        m['checks'].append(dict(tag=tag,exit=c,passed=ok,diagnostic_hits=hits,GPU_execution_mismatch=gpu));(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');print(tag,c,flush=True)
        if not ok:raise RuntimeError(tag+text[-2200:])
    cases=['missing-evidence','count-zero','count-short','count-large','missing-first','missing-middle','missing-last','duplicate-first','proof-frame','proof-ordinal','proof-encoder-zero','proof-encoder-unknown','proof-command','proof-buffer-zero','proof-buffer-header','proof-offset-align','proof-offset-end','proof-groups-short','proof-groups-long','proof-width-overflow','proof-width-capacity','proof-y','proof-z','dispatch-buffer-zero','dispatch-buffer-header','dispatch-buffer-output','dispatch-offset-align','dispatch-offset-end','dispatch-offset-mismatch','threads-zero','threads-over','threads-y','threads-z','query-output-arguments','proof-argument-storage','proof-argument-untracked','mismatch-first','mismatch-middle','mismatch-last','legal-original']
    for name in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',name],stdout=subprocess.DEVNULL).returncode!=0
    try:
        for tag in cases:
            tree=copy.deepcopy(original);ch=tree.find('./chunks');proofs=names(tree,proofName);dispatches=names(tree,dispatchName)
            count=names(tree,countName)[0];d=dispatches[0];proof=proofs[0]
            header=names(tree,'MTLBuffer::DeclareRayASHeader')[0];query=names(tree,'MTLComputePipelineState::DeclareRayQueryHeapDispatch')[0]
            if tag=='missing-evidence':
                for c in proofs+[count]:ch.remove(c)
            elif tag.startswith('count-'):f(count,'count').text={'count-zero':'0','count-short':'2','count-large':'65537'}[tag]
            elif tag.startswith('missing-'):ch.remove(proofs[{'missing-first':0,'missing-middle':1,'missing-last':2}[tag]])
            elif tag=='duplicate-first':ch.insert(list(ch).index(proof),copy.deepcopy(proof))
            elif tag=='proof-frame':
                ch.remove(proof);scope=next(c for c in ch if c.get('id')=='5');ch.insert(list(ch).index(scope)+1,proof)
            elif tag in ['proof-ordinal','proof-encoder-zero','proof-encoder-unknown','proof-command','proof-buffer-zero','proof-buffer-header','proof-offset-align','proof-offset-end']:
                field,value={'proof-ordinal':('ordinal','1'),'proof-encoder-zero':('encoder','0'),'proof-encoder-unknown':('encoder','99999999'),'proof-command':('command',f(header,'buffer').text),'proof-buffer-zero':('buffer','0'),'proof-buffer-header':('buffer',f(header,'buffer').text),'proof-offset-align':('offset','1'),'proof-offset-end':('offset','64')}[tag];f(proof,field).text=value
            elif tag=='proof-groups-short':f(proof,'groups').remove(f(proof,'groups')[-1])
            elif tag=='proof-groups-long':ET.SubElement(f(proof,'groups'),'uint',typename='uint32_t',width='4').text='1'
            elif tag in ['proof-width-overflow','proof-width-capacity','proof-y','proof-z']:
                i,v={'proof-width-overflow':(0,4294967295),'proof-width-capacity':(0,133),'proof-y':(1,2),'proof-z':(2,2)}[tag];f(proof,'groups')[i].text=str(v)
            elif tag.startswith('dispatch-buffer-'):f(d,'indirectBuffer').text='0' if tag.endswith('zero') else f(header,'buffer').text if tag.endswith('header') else f(query,'output').text
            elif tag.startswith('dispatch-offset-'):f(d,'indirectBufferOffset').text={'dispatch-offset-align':'1','dispatch-offset-end':'64','dispatch-offset-mismatch':str(4 if int(f(d,'indirectBufferOffset').text)!=4 else 8)}[tag]
            elif tag.startswith('threads-'):
                f(f(d,'threadsPerGroup'),'width' if tag in ['threads-zero','threads-over'] else 'height' if tag=='threads-y' else 'depth').text={'threads-zero':'0','threads-over':'2048','threads-y':'2','threads-z':'2'}[tag]
            elif tag=='query-output-arguments':f(query,'output').text=f(proof,'buffer').text
            elif tag.startswith('proof-argument-'):
                alloc=next(c for c in ch if c.get('name')=='MTLHeap::newBuffer(offset)' and f(c,'Buffer').text==f(proof,'buffer').text)
                f(alloc,'options').text='512' if tag.endswith('storage') else '288'
            elif tag.startswith('mismatch-'):
                # Modify only bounded capture evidence. Native GPU inputs remain
                # the original 1/132/0, with a full 528-word output allocation.
                i={'mismatch-first':0,'mismatch-middle':1,'mismatch-last':2}[tag];f(proofs[i],'groups')[0].text=str([2,131,1][i])
            else:assert tag=='legal-original'
            for c in ch:c.set('length',str(int(c.get('length','0'))+128))
            xml=w/(tag+'.zip.xml');tree.write(xml,encoding='utf-8',xml_declaration=True)
            with zipfile.ZipFile(xml.with_suffix(''),'w',zipfile.ZIP_DEFLATED) as z:
                for n,data in blobs.items():z.writestr(n,data)
            cap=w/(tag+'.rdc');run(tag+'-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',cap,'-c','rdc'],0)
            if tag=='legal-original':
                run(tag+'-API',[a.oracle,cap,a.hit,132],0);run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','3',cap],0);m['legal_controls']+=1
            else:
                gpu=tag.startswith('mismatch-');run(tag+'-API',[b/'metal-ray-b534/final-short/opener',cap],4,gpu);run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1,gpu)
                m['execution_mismatch_groups' if gpu else 'preGPU_groups']+=1;m['execution_mismatch_rejections' if gpu else 'preGPU_rejections']+=2
        assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m['status']='PASS'
    except Exception as ex:m.update(status='FAIL',error=str(ex));raise
    finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')


if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);main()
