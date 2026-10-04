# SPDX-License-Identifier: MIT
# Open tile_capture.rdc on startup and pass this file with --ui-script.
# Uses the public CaptureContext UI API (PythonShell does not define __file__).
import renderdoc as rd
import qrenderdoc as qrd
from pathlib import Path
import os

root=Path(pyrenderdoc.GetCaptureFilename()).resolve().parents[2]
log=root/'build-macos-debug/metal-tile-features/ui-smoke.log'
helper=pyrenderdoc.Extensions().GetMiniQtHelper()

def inspect():
    completed=[]
    pyrenderdoc.CloseCapture()
    # Rapid capture switches exercise queued thumbnail callbacks as well as stage changes.
    for cycle in range(3):
        for name,eid,stage in [
            ('tile',13,qrd.PipelineStage.ComputeShader),
            ('memoryless',10,qrd.PipelineStage.PixelShader),
            ('metalfx-spatial',6,qrd.PipelineStage.ComputeShader),
        ]:
            path=root/f'captures/metal-features-b465/{name}_capture.rdc'
            pyrenderdoc.LoadCapture(str(path),rd.ReplayOptions(),'',False,True)
            pyrenderdoc.ShowPipelineViewer()
            pyrenderdoc.SetEventID([],eid,eid,True)
            pyrenderdoc.GetPipelineViewer().SelectPipelineStage(stage)
            completed.append((cycle,name,eid))
            log.write_text(repr(completed))
            pyrenderdoc.CloseCapture()
    final=os.environ.get('METAL_FEATURE_UI_FINAL','tile')
    eid,stage={
        'tile':(13,qrd.PipelineStage.ComputeShader),
        'memoryless':(10,qrd.PipelineStage.PixelShader),
        'metalfx-spatial':(6,qrd.PipelineStage.ComputeShader),
    }[final]
    pyrenderdoc.LoadCapture(str(root/f'captures/metal-features-b465/{final}_capture.rdc'),rd.ReplayOptions(),'',False,True)
    pyrenderdoc.ShowPipelineViewer()
    pyrenderdoc.SetEventID([],eid,eid,True)
    pyrenderdoc.GetPipelineViewer().SelectPipelineStage(stage)
    log.write_text(f'PASS 9 GUI capture switches; {final} EID {eid} selected. Layout still needs visual inspection.\n'+repr(completed))

helper.InvokeOntoUIThread(inspect)
