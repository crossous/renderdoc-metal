# SPDX-License-Identifier: MIT
# Verify real GUI event seeks using its replay controller and leave the first draw selected.
from pathlib import Path
import traceback
import renderdoc as rd

root = Path(pyrenderdoc.GetCaptureFilename()).resolve().parents[2]
log = root / 'build-macos-debug/metal-future-shared/ui.log'
helper = pyrenderdoc.Extensions().GetMiniQtHelper()
steps = [13, 16, 21, 13, 0, 13]
results = []


def record(value):
    results.append(value)
    log.write_text('\n'.join(results) + '\n')


def seek(index=0):
    try:
        eid = steps[index]
        pyrenderdoc.SetEventID([], eid, eid, True)

        def check(controller):
            try:
                assert controller.GetFatalErrorStatus().OK()
                target = next(r.resourceId for r in controller.GetResources()
                              if r.name == 'Future Shared regression target')
                if eid in (13, 21):
                    pixels = bytes(controller.GetTextureData(target, rd.Subresource(0, 0, 0)))
                    assert pixels == bytes([64, 128, 191, 255]) * (32 * 32)
                def complete():
                    record(f'PASS real GUI EID {eid}: fatal OK' +
                           (' and draw pixels checked' if eid in (13, 21) else ''))
                    if index + 1 < len(steps):
                        seek(index + 1)
                    else:
                        pyrenderdoc.ShowTextureViewer()
                        pyrenderdoc.GetTextureViewer().ViewTexture(target, rd.CompType.Typeless, True)
                        record('PASS real GUI forward/reverse/reset; left at first draw EID13')
                helper.InvokeOntoUIThread(complete)
            except Exception:
                message = traceback.format_exc()
                helper.InvokeOntoUIThread(lambda: record('FAIL\n' + message))
        pyrenderdoc.Replay().AsyncInvoke(check)
    except Exception:
        record('FAIL\n' + traceback.format_exc())


log.write_text('START\n')
helper.InvokeOntoUIThread(seek)
