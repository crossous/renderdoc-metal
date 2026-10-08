#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Create an isolated finite UE Lumen test scene; map generation uses NullRHI."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--capture-plugin', type=Path, required=True)
    parser.add_argument('--engine', type=Path, default=Path('/Users/Shared/Epic Games/UE_5.8/Engine'))
    parser.add_argument('--run', action='store_true', help='Generate the map with a bounded NullRHI commandlet')
    args = parser.parse_args()
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    project_dir = work/'project'
    if project_dir.exists():
        raise RuntimeError('Project directory already exists; use a fresh work directory to preserve its files')
    plugin = args.capture_plugin.resolve()
    engine = args.engine.resolve()
    editor = engine/'Binaries/Mac/UnrealEditor-Cmd.app/Contents/MacOS/UnrealEditor-Cmd'
    if not editor.is_file():
        editor = engine/'Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
    required = [plugin/'RenderDocMetalCapture.uplugin', plugin/'Binaries/Mac/UnrealEditor.modules',
                plugin/'Binaries/Mac/libUnrealEditor-RenderDocMetalCapture.dylib', engine/'Build/Build.version', editor]
    if not all(p.is_file() for p in required):
        raise RuntimeError('Missing matching engine or prebuilt capture plugin')
    (project_dir/'Config').mkdir(parents=True)
    copied = project_dir/'Plugins/RenderDocMetalCapture'
    shutil.copytree(plugin, copied, ignore=shutil.ignore_patterns('Intermediate', '.DS_Store'))
    project = project_dir/'MetalRayScene.uproject'
    project.write_text(json.dumps(dict(FileVersion=3, EngineAssociation='5.8',
        Description='Isolated finite Metal Lumen ray-query validation', DisableEnginePluginsByDefault=True, Plugins=[
            dict(Name=n, Enabled=True) for n in ('RenderDocMetalCapture', 'PythonScriptPlugin', 'EditorScriptingUtilities')]), indent=2)+'\n')
    (project_dir/'Config/DefaultEngine.ini').write_text('''[/Script/EngineSettings.GameMapsSettings]
EditorStartupMap=/Game/MetalRayScene.MetalRayScene
GameDefaultMap=/Game/MetalRayScene.MetalRayScene

[/Script/Engine.RendererSettings]
r.RayTracing=True
r.SkinCache.CompileShaders=True
r.DynamicGlobalIlluminationMethod=1
r.ReflectionMethod=1
r.GenerateMeshDistanceFields=True
r.Lumen.HardwareRayTracing=True
r.Lumen.HardwareRayTracing.Inline=True
r.RayTracing.Shadows=False

[/Script/MacTargetPlatform.MacTargetSettings]
-TargetedRHIs=SF_METAL_SM5
+TargetedRHIs=SF_METAL_SM6

[SystemSettings]
r.Streaming.PoolSize=64
r.Nanite.Streaming.StreamingPoolSize=32
r.Nanite.Streaming.NumInitialRootPages=256
''')
    script = work/'generate_scene.py'
    script.write_text('''import json
from pathlib import Path
import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert level.new_level('/Game/MetalRayScene')
cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
plane = unreal.load_asset('/Engine/BasicShapes/Plane.Plane')
assert cube and plane
entries = []
for name, mesh, location, scale in [
    ('QueryFloor', plane, (0, 0, 0), (8, 8, 1)),
    ('QueryCubeA', cube, (0, 0, 60), (1, 1, 1)),
    ('QueryCubeB', cube, (160, 0, 80), (1, 1, 1.5))]:
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    assert actor
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    entries.append(dict(name=name, asset=mesh.get_path_name(), location=location, scale=scale))
light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 300), unreal.Rotator(-45, -30, 0))
assert light
light.set_actor_label('QuerySun')
light.light_component.set_editor_property('intensity', 10.0)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 200))
assert sky
sky.set_actor_label('QuerySky')
assert level.save_current_level()
result = dict(status='MAP SAVED; GPU AND RAY QUERY NOT YET VALIDATED', mesh_actors=entries,
              light_actors=['QuerySun', 'QuerySky'], map='/Game/MetalRayScene')
Path(__file__).with_name('scene-manifest.json').write_text(json.dumps(result, indent=2)+'\\n')
unreal.log('PASS isolated Metal ray scene saved; three meshes and two lights')
''')
    manifest = dict(status='PREPARED', project=str(project), engine_version=json.loads(required[3].read_text()),
                    dependencies={str(p):sha(p) for p in required}, GPU='NOT RUN', ray_query='NOT VALIDATED',
                    original_project_modified=False)
    try:
        if args.run:
            env = os.environ.copy()
            for key in tuple(env):
                if key.startswith(('RENDERDOC_', 'UE_METAL_')) or key == 'DYLD_INSERT_LIBRARIES':
                    env.pop(key)
            command = [str(editor), str(project), '-run=pythonscript', '-script='+str(script),
                       '-NullRHI', '-unattended', '-nosplash', '-nosound', '-stdout',
                       '-FullStdOutLogOutput', '-ddc=InstalledNoZenLocalFallback', '-NoShaderCompile']
            with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
                fcntl.flock(lock, fcntl.LOCK_EX)
                for name in ('UnrealEditor', 'qrenderdoc'):
                    if subprocess.run(['pgrep', '-x', name], stdout=subprocess.DEVNULL).returncode == 0:
                        raise RuntimeError(name+' is already running')
                with (work/'NullRHI-generation.log').open('w') as log:
                    child = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                    try:
                        code = child.wait(timeout=120)
                    except subprocess.TimeoutExpired:
                        os.killpg(child.pid, signal.SIGKILL)
                        child.wait()
                        code = 124
            manifest['commandlet_exit'] = code
            diagnostics = (work/'NullRHI-generation.log').read_text(errors='replace')
            if code or 'PASS isolated Metal ray scene saved' not in diagnostics or any(s in diagnostics for s in
                    ('Assertion failed', 'Fatal error:', 'Traceback (most recent call last)')):
                raise RuntimeError('NullRHI scene generation failed; inspect retained log')
            scene = project_dir/'Content/MetalRayScene.umap'
            if not scene.is_file() or not (work/'scene-manifest.json').is_file():
                raise RuntimeError('Map or scene evidence missing')
            manifest.update(status='NULLRHI MAP GENERATION PASS; GPU/RT NOT RUN', map_sha256=sha(scene),
                            map_size=scene.stat().st_size)
        manifest['generated_files'] = {str(p.relative_to(work)):sha(p) for p in
            [project, project_dir/'Config/DefaultEngine.ini', script]}
    except Exception as ex:
        manifest.update(status='FAIL', error=str(ex))
        raise
    finally:
        (work/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(manifest['status'], project)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
