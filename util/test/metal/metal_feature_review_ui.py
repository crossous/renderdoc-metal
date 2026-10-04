# SPDX-License-Identifier: MIT
# Open any B465 capture and pass this file to qrenderdoc --ui-script.
# Checks real thumbnail widgets and resource following with public RenderDoc UI APIs.
import renderdoc as rd
import qrenderdoc as qrd
from pathlib import Path
import traceback

root = Path(pyrenderdoc.GetCaptureFilename()).resolve().parents[2]
log = root / 'build-macos-debug/metal-tile-features/ui-review.log'
helper = pyrenderdoc.Extensions().GetMiniQtHelper()
results = []

def record(text):
    results.append(text)
    log.write_text('\n'.join(results) + '\n')

def load(name, eid):
    pyrenderdoc.CloseCapture()
    pyrenderdoc.LoadCapture(str(root / f'captures/metal-features-b465/{name}_capture.rdc'),
                           rd.ReplayOptions(), '', False, True)
    assert pyrenderdoc.IsCaptureLoaded(), name
    pyrenderdoc.SetEventID([], eid, eid, True)

def check_features():
    try:
        for cycle in range(3):
            load('memoryless', 10)
            pyrenderdoc.ShowPipelineViewer()
            viewer = pyrenderdoc.GetPipelineViewer()
            viewer.SelectPipelineStage(qrd.PipelineStage.PixelShader)
            contents = helper.FindChildByName(viewer.Widget(), 'metalStageContents4')
            group = helper.FindChildByName(contents, 'metalShaderFeatures')
            assert contents and group and helper.IsWidgetVisible(group)
            last = helper.GetChild(contents, helper.GetNumChildren(contents) - 1)
            assert helper.GetWidgetName(last) == 'metalShaderFeatures'
            pyrenderdoc.SetEventID([], 16, 16, True)
            assert not helper.IsWidgetVisible(group), 'Plain entry must hide unused features'
            unused = helper.FindChildByName(viewer.Widget(), 'metalShowUnused')
            helper.SetWidgetChecked(unused, True)
            assert helper.IsWidgetVisible(group), 'Show Unused must reveal declarations'
            helper.SetWidgetChecked(unused, False)
            assert not helper.IsWidgetVisible(group)
            record(f'PASS cycle {cycle}: FS features last; plain hidden; Show Unused works')
        load('tile', 13)
        viewer = pyrenderdoc.GetPipelineViewer()
        viewer.SelectPipelineStage(qrd.PipelineStage.ComputeShader)
        contents = helper.FindChildByName(viewer.Widget(), 'metalStageContents9')
        last = helper.GetChild(contents, helper.GetNumChildren(contents) - 1)
        assert helper.GetWidgetName(last) == 'metalShaderFeatures' and helper.IsWidgetVisible(last)
        record('PASS Tile features last')
        load('metalfx-spatial', 6)
        pyrenderdoc.ShowTextureViewer()
        # Wait behind descriptor-name queries, then inspect on the UI thread.
        pyrenderdoc.Replay().AsyncInvoke(lambda controller: helper.InvokeOntoUIThread(check_fx))
    except Exception:
        record('FAIL\n' + traceback.format_exc())

def check_fx():
    try:
        viewer = pyrenderdoc.GetTextureViewer()
        for strip_name, slot, resource in [
            ('inputThumbs', 'MetalFX Input', 'MetalFX input 16x16'),
            ('outputThumbs', 'MetalFX Output', 'MetalFX output 32x32'),
        ]:
            strip = helper.FindChildByName(viewer.Widget(), strip_name)
            contents = helper.FindChildByName(strip, 'scrollAreaWidgetContents')
            visible = []
            for i in range(helper.GetNumChildren(contents)):
                thumb = helper.GetChild(contents, i)
                if thumb and helper.IsWidgetVisible(thumb):
                    name = helper.FindChildByName(thumb, 'descriptionLabel')
                    binding = helper.FindChildByName(thumb, 'slotLabel')
                    visible.append((helper.GetWidgetText(binding), helper.GetWidgetText(name)))
            assert any(label == slot and resource in name for label, name in visible), visible
            record(f'PASS {strip_name}: {visible}')
        pipe = pyrenderdoc.CurPipelineState()
        for kind, bindings in [
            (qrd.FollowType.ReadOnly, pipe.GetReadOnlyResources(rd.ShaderStage.Compute, True)),
            (qrd.FollowType.ReadWrite, pipe.GetReadWriteResources(rd.ShaderStage.Compute, True)),
        ]:
            assert len(bindings) == 1
            binding = bindings[0]
            viewer.ViewFollowedResource(kind, binding.access.stage,
                                        binding.access.index, binding.access.arrayElement)
            assert viewer.GetCurrentResource() == binding.descriptor.resource
        record('PASS MetalFX input/output following; UI assertions complete. Visual menu/layout approval remains manual.')
    except Exception:
        record('FAIL\n' + traceback.format_exc())

log.write_text('START\n')
helper.InvokeOntoUIThread(check_features)
