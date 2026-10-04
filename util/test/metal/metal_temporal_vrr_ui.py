# SPDX-License-Identifier: MIT
# Real GUI widgets and public UI APIs; no simulated screenshots.
import renderdoc as rd
import qrenderdoc as qrd
from pathlib import Path
import traceback
root=Path(pyrenderdoc.GetCaptureFilename()).resolve().parents[2]
log=root/'build-macos-debug/metal-temporal-vrr/ui.log'
helper=pyrenderdoc.Extensions().GetMiniQtHelper()
results=[]
def record(s):
    results.append(s);log.write_text('\n'.join(results)+'\n')
def load(path,eid):
    pyrenderdoc.CloseCapture()
    pyrenderdoc.LoadCapture(str(root/path),rd.ReplayOptions(),'',False,True)
    assert pyrenderdoc.IsCaptureLoaded(),path
    pyrenderdoc.SetEventID([],eid,eid,True)
def texts(widget):
    values=[helper.GetWidgetText(widget)]
    for i in range(helper.GetNumChildren(widget)):
        child=helper.GetChild(widget,i)
        if child:values+=texts(child)
    return values
def begin():
    try:
        for cycle in range(3):
            load('captures/metal-features-b467/vrr_capture.rdc',7)
            pyrenderdoc.ShowPipelineViewer()
            viewer=pyrenderdoc.GetPipelineViewer()
            viewer.SelectPipelineStage(qrd.PipelineStage.Rasterizer)
            contents=helper.FindChildByName(viewer.Widget(),'metalStageContents3')
            group=helper.FindChildByName(contents,'metalVRRState')
            assert group and helper.IsWidgetVisible(group)
            values=texts(group)
            assert 'Yes' in values and '400 × 300' in values and '1' in values,values
            record(f'PASS cycle {cycle}: VRR visible in RS, enabled/logical size/layer count')
            load('captures/metal-features-b467/temporal-missing-history_capture.rdc',17)
            viewer=pyrenderdoc.GetPipelineViewer()
            viewer.SelectPipelineStage(qrd.PipelineStage.ComputeShader)
            contents=helper.FindChildByName(viewer.Widget(),'metalStageContents10')
            group=helper.FindChildByName(contents,'metalFXTemporalState')
            assert group and helper.IsWidgetVisible(group)
            assert any('Pre-capture history unavailable' in t for t in texts(group))
            record(f'PASS cycle {cycle}: Temporal pre-capture history warning visible')
        load('captures/metal-features-b467/temporal_capture.rdc',17)
        pyrenderdoc.ShowTextureViewer()
        pyrenderdoc.Replay().AsyncInvoke(lambda controller:helper.InvokeOntoUIThread(check_temporal))
    except Exception:record('FAIL\n'+traceback.format_exc())
def check_temporal(attempt=0):
    try:
        viewer=pyrenderdoc.GetTextureViewer()
        expected={'MetalFX Color','MetalFX Depth','MetalFX Motion Vectors','MetalFX Exposure','MetalFX Reactive Mask'}
        for name,slots in [('inputThumbs',expected),('outputThumbs',{'MetalFX Output'})]:
            strip=helper.FindChildByName(viewer.Widget(),name)
            contents=helper.FindChildByName(strip,'scrollAreaWidgetContents')
            visible=[]
            for i in range(helper.GetNumChildren(contents)):
                thumb=helper.GetChild(contents,i)
                if thumb and helper.IsWidgetVisible(thumb):
                    label=helper.FindChildByName(thumb,'slotLabel')
                    desc=helper.FindChildByName(thumb,'descriptionLabel')
                    visible.append((helper.GetWidgetText(label),helper.GetWidgetText(desc)))
            if not slots <= {label for label,desc in visible} and attempt < 4:
                # OnCaptureLoaded queues another OnEventChanged; wait behind its name query too.
                pyrenderdoc.Replay().AsyncInvoke(lambda controller:helper.InvokeOntoUIThread(lambda:check_temporal(attempt+1)))
                return
            assert slots <= {label for label,desc in visible},visible
            record(f'PASS {name}: {visible}')
        pipe=pyrenderdoc.CurPipelineState()
        for kind,bindings in [(qrd.FollowType.ReadOnly,pipe.GetReadOnlyResources(rd.ShaderStage.Compute,True)),
                              (qrd.FollowType.ReadWrite,pipe.GetReadWriteResources(rd.ShaderStage.Compute,True))]:
            assert len(bindings)==(5 if kind==qrd.FollowType.ReadOnly else 1)
            for b in bindings:
                viewer.ViewFollowedResource(kind,b.access.stage,b.access.index,b.access.arrayElement)
                assert viewer.GetCurrentResource()==b.descriptor.resource
        pv=pyrenderdoc.GetPipelineViewer();pv.SelectPipelineStage(qrd.PipelineStage.ComputeShader)
        group=helper.FindChildByName(pv.Widget(),'metalFXTemporalState')
        assert any('History reconstructed from captured calls' in t for t in texts(group))
        record('PASS Temporal parameters, all six resource follows and exact-history status; visual approval remains manual')
    except Exception:record('FAIL\n'+traceback.format_exc())
log.write_text('START\n')
helper.InvokeOntoUIThread(begin)
