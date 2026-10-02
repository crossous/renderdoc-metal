#!/usr/bin/env python3
"""Local M2 Testproj capture, audited candidate preparation, and real GPU image verification."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

from prepare_ue_metal_replay_candidate import sha


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_mutually_exclusive_group(required=True)
    actions.add_argument('--check', action='store_true', help='Validate launch paths; no GPU replay')
    actions.add_argument('--capture', action='store_true', help='Capture automatically, exit UE, prepare and verify')
    actions.add_argument('--replay', type=Path, help='Prepare and verify an original capture without recapturing')
    actions.add_argument('--verify', type=Path, help='Verify an already prepared candidate without promoting it')
    actions.add_argument('--ui', type=Path, help='Open an already prepared capture in qrenderdoc')
    parser.add_argument('--project', type=Path, default=Path.home() / 'Documents/Unreal Projects/Testproj/Testproj.uproject')
    parser.add_argument('--build-dir', type=Path, default=root / 'build-macos-debug')
    parser.add_argument('--rhi', type=Path, default=root / 'build-macos-debug/local-ue-module-probe/provider-build-render-indirect/libUnrealEditor-MetalRHI.dylib')
    args = parser.parse_args()
    build, project = args.build_dir.resolve(), args.project.resolve()
    env = os.environ.copy()
    for key in tuple(env):
        if key.startswith('RENDERDOC_METAL_'):
            env.pop(key)
    env.update(UE_METAL_PROJECT=str(project), RENDERDOC_METAL_BUILD_DIR=str(build),
               UE_METAL_RHI_OVERRIDE=str(args.rhi.resolve()),
               RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS='1',
               RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS='1',
               UE_METAL_AUTO_CAPTURE_DELAY_SECONDS='45', UE_METAL_EXIT_AFTER_CAPTURE='1',
               UE_METAL_TIMEOUT_SECONDS='360', UE_METAL_CAPTURE_VIEWPORT_WIDTH='320',
               UE_METAL_CAPTURE_VIEWPORT_HEIGHT='240', UE_METAL_CAPTURE_WINDOW_WIDTH='640',
               UE_METAL_CAPTURE_WINDOW_HEIGHT='480')
    env.setdefault('UE_METAL_EDITOR_ARGS', '-ini:Engine:[SystemSettings]:r.Nanite.Streaming.StreamingPoolSize=32,[SystemSettings]:r.Nanite.Streaming.NumInitialRootPages=256 -ExecCmds="r.SceneRenderTargetResizeMethod 0,r.SceneRenderTargetResizeMethodForceOverride 1,r.Shadow.Virtual.Enable 0,r.Lumen.DiffuseIndirect.Allow 0,r.Lumen.Reflections.Allow 0,r.ShadowQuality 0,r.VolumetricFog 0,r.SSR.Quality 0,r.Editor.Viewport.ScreenPercentageMode.RealTime 0,r.Editor.Viewport.ScreenPercentage 100" -windowed -ResX=640 -ResY=480')
    launcher = ['bash', str(root / 'util/ue/run_ue_metal_capture_macos.sh')]
    if args.check:
        return subprocess.run(launcher + ['--check'], env=env).returncode
    # Do not overlap heavy UE replay with an editor or another qrenderdoc replay.
    for name in ('UnrealEditor', 'qrenderdoc'):
        if subprocess.run(['pgrep', '-x', name], stdout=subprocess.DEVNULL).returncode == 0:
            parser.error('Close ' + name + ' before starting another capture/replay process')
    if args.ui:
        ui = build / 'bin/qrenderdoc.app/Contents/MacOS/qrenderdoc'
        embedded = ui.parent.parent / 'lib/librenderdoc.dylib'
        if sha(embedded) != sha(build / 'lib/librenderdoc.dylib'):
            parser.error('qrenderdoc embedded library differs from the validated build')
        # Keep the launcher alive until the viewer exits, including under task
        # runners that clean up child processes when a command finishes.
        return subprocess.run([str(ui), str(args.ui.resolve())], env=env).returncode
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    session = build / 'local-m2-descriptor-replay' / ('testproj-' + stamp)
    session.mkdir(parents=True)
    library_sha = sha(build / 'lib/librenderdoc.dylib')
    manifest = {'session': str(session), 'library_sha256': library_sha,
                'targeted': 'not run by this workflow', 'full_regression': 'not run by this workflow',
                'real_ue': 'pending', 'ui': 'not run by this workflow'}
    try:
        if args.capture:
            # MainFrame reads RootWindow before the capture-size timer. A saved
            # maximized Retina window can allocate large persistent histories
            # even when the captured viewport is resized later. Use UE's native
            # INI destination override so this run never rewrites user settings.
            settings = project.parent / 'Saved/Config/MacEditor/EditorPerProjectUserSettings.ini'
            text = settings.read_text() if settings.exists() else ''
            settings_sha = sha(settings) if settings.exists() else None
            section = '[RootWindow]\nWindowSize=X=640 Y=480\nInitiallyMaximized=False\n\n'
            pattern = r'(?ms)^\[RootWindow\]\s*\n.*?(?=^\[|\Z)'
            text = re.sub(pattern, section, text) if re.search(pattern, text) else text + '\n' + section
            isolated_settings = session / 'EditorPerProjectUserSettings.ini'
            isolated_settings.write_text(text)
            env['UE_METAL_EDITOR_ARGS'] += ' ' + shlex.quote('-EditorPerProjectUserSettingsINI=' + str(isolated_settings))
            manifest.update(startup_settings=str(isolated_settings), user_settings_sha256=settings_sha)
            live = project.parent / 'Saved/RenderDocMetalCaptures/UE58_capture.rdc'
            previous_sha = sha(live) if live.exists() else None
            if previous_sha:
                preserved = live.with_name('UE58_original_' + previous_sha + '.rdc')
                if preserved.exists() and sha(preserved) != previous_sha:
                    raise RuntimeError('Existing preserved capture hash differs')
                if not preserved.exists():
                    shutil.copyfile(live, preserved)
            with (session / 'capture-launch.log').open('w') as log:
                subprocess.run(launcher + ['--run'], env=env, stdout=log,
                               stderr=subprocess.STDOUT, check=True, timeout=400)
            if settings_sha is not None and sha(settings) != settings_sha:
                raise RuntimeError('Original editor settings changed during isolated launch')
            if not live.exists() or sha(live) == previous_sha:
                raise RuntimeError('No new capture was produced')
            original = session / 'original.rdc'
            shutil.copyfile(live, original)
        else:
            original = (args.replay or args.verify).resolve()
        manifest.update(original=str(original), original_sha256=sha(original))
        if args.verify:
            candidate = original
            audit = session / 'candidate-export'
            audit.mkdir()
            with (audit / 'export.log').open('w') as log:
                subprocess.run([str(build / 'bin/renderdoccmd'), 'convert', '-f', str(candidate),
                                '-o', str(audit / 'original.zip.xml'), '-c', 'zip.xml'],
                               env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=240)
        else:
            candidate, audit = session / 'replay.rdc', session / 'audit'
            with (session / 'prepare.log').open('w') as log:
                subprocess.run([sys.executable, str(root / 'util/ue/prepare_ue_metal_replay_candidate.py'),
                                '--capture', str(original), '--output', str(candidate),
                                '--build-dir', str(build), '--audit-dir', str(audit)],
                               env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
        images = session / 'images'; images.mkdir()
        probe = session / 'image-probe'
        subprocess.run(['clang++', '-std=c++17', '-DRENDERDOC_PLATFORM_APPLE', '-I' + str(root),
                        str(root / 'util/ue/ue_metal_replay_image_probe.cpp'), '-L' + str(build / 'lib'),
                        '-lrenderdoc', '-Wl,-rpath,' + str(build / 'lib'), '-o', str(probe)], check=True)
        # Normal OpenCapture; no diagnostic coverage/prefix flags are enabled.
        with (images / 'normal-replay.log').open('w') as log:
            subprocess.run([str(probe), str(candidate), str(images)], env=dict(env, MTL_DEBUG_LAYER='1'),
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=360)
        phases = [json.loads((images / (phase + '-presented.json')).read_text())
                  for phase in ('loaded', 'replay-0', 'replay-1')]
        if phases[0] != phases[1] or phases[0] != phases[2]:
            # File names intentionally contain phase; compare actual descriptions.
            shapes = [[{k:v for k,v in entry.items() if k != 'file'} for entry in phase] for phase in phases]
            if shapes[0] != shapes[1] or shapes[0] != shapes[2]:
                raise RuntimeError('Presented target descriptors differ across resets')
        hashes = [[sha(images / e['file']) for e in phase] for phase in phases]
        if hashes[0] != hashes[1] or hashes[0] != hashes[2]:
            raise RuntimeError('First real GPU discrepancy: full replay bytes differ across EID0 resets')
        thumb = ET.parse(audit / 'original.zip.xml').getroot().find('header/thumbnail')
        thumbnail_probe = session / 'thumbnail-probe'
        subprocess.run(['clang++', '-std=c++17', '-I' + str(root),
                        str(root / 'util/ue/ue_metal_replay_thumbnail_match.cpp'),
                        str(root / 'renderdoc/3rdparty/jpeg-compressor/jpge.cpp'),
                        '-o', str(thumbnail_probe)], check=True)
        if len(phases[0]) != 1 or not phases[0][0]['bgra'] or phases[0][0]['special']:
            raise RuntimeError('Thumbnail comparison currently requires one BGRA8 presented target')
        entry = phases[0][0]; jpeg = images / 'replayed-native-thumb.jpg'
        subprocess.run([str(thumbnail_probe), str(images / entry['file']), str(entry['width']),
                        str(entry['height']), thumb.get('width'), thumb.get('height'), str(jpeg)], check=True)
        with zipfile.ZipFile(audit / 'original.zip') as archive:
            captured_thumb = archive.read(thumb.text)
        exact = jpeg.read_bytes() == captured_thumb
        manifest.update(candidate=str(candidate), candidate_sha256=sha(candidate),
                        native_presented_sha256=hashes[0], thumbnail_exact_bytes=exact,
                        thumbnail_sha256=hashlib.sha256(captured_thumb).hexdigest())
        if not exact:
            raise RuntimeError('First real GPU image discrepancy: replay thumbnail differs from original capture')
        if sha(build / 'lib/librenderdoc.dylib') != library_sha or sha(original) != manifest['original_sha256']:
            raise RuntimeError('Capture or replay library changed during verification')
        manifest['real_ue'] = 'PASS normal open, two EID0 resets, identical Native full images, exact original JPEG thumbnail'
        print(json.dumps(manifest, indent=2), flush=True)
        print('Open UI: python3 util/ue/run_testproj_metal_replay_macos.py --ui ' + str(candidate), flush=True)
        return 0
    except Exception as error:
        manifest['real_ue'] = 'FAILED: ' + str(error)
        raise
    finally:
        (session / 'results.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    sys.exit(main())
