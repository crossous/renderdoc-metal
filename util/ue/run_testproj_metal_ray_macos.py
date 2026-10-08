#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Isolated UE RT capture diagnostic; never enables default production capability."""
import argparse
import fcntl
import tempfile
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from ue_metal_capture_supervisor import run_launcher, write_json
from audit_ue_metal_ray_capture import analyse as analyse_ray_capture
from audit_ue_metal_ray_air import verify as verify_ray_air


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def diagnostic_editor_settings(text, window_size=(640, 480)):
    # MainFrame consumes saved RootWindow before the plugin's viewport resize.
    # CLI ResX/ResY alone did not constrain the saved maximized Retina window.
    for name, values in (
            ("/Script/UnrealEd.EditorLoadingSavingSettings", {"LoadLevelAtStartup": "ProjectDefault"}),
            ("RootWindow", {"WindowSize": "X=%d Y=%d" % tuple(window_size), "InitiallyMaximized": "False"})):
        pattern = r"(?ms)^\[" + re.escape(name) + r"\].*?(?=^\[|\Z)"
        section = re.search(pattern, text)
        replacement = section.group(0) if section else "[" + name + "]\n"
        for key, value in values.items():
            setting = r"(?m)^" + re.escape(key) + r"=.*$"
            line = key + "=" + value
            replacement = (re.sub(setting, line, replacement) if re.search(setting, replacement)
                           else replacement.rstrip("\n") + "\n" + line + "\n")
        text = re.sub(pattern, lambda _match: replacement, text) if section else text + "\n" + replacement
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", action="store_true", help="Launch bounded RT diagnostic (default: path check)")
    parser.add_argument("--project", type=Path, default=Path.home()/"Documents/Unreal Projects/Testproj/Testproj.uproject")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--trace-as-input-snapshots",action="store_true",help="Record finite captured AS input freeze diagnostics")
    parser.add_argument("--capture-delay",type=int,default=45,help="Plugin ticker delay before one-frame capture")
    parser.add_argument("--startup-timeout",type=int,default=360,help="Bound the editor initialization window")
    parser.add_argument("--lumen-only",action="store_true",help="Test Lumen HWRT without separate RT shadow workload")
    parser.add_argument('--small-shadows', action='store_true', help='Bound raster shadow cascades for the isolated small RT scene')
    parser.add_argument('--small-lumen-caches', action='store_true', help='Bound surface/radiance caches for the isolated RT scene while retaining HW query workloads')
    parser.add_argument('--rhi', type=Path, help='Isolated MetalRHI module override (never installed into Engine)')
    parser.add_argument('--viewport-size', type=int, nargs=2, default=(320,240),
                        help='Independent bounded viewport dimensions for capture validation')
    parser.add_argument('--window-size', type=int, nargs=2, default=(640,480),
                        help='Bounded editor window dimensions, independent of the captured viewport')
    parser.add_argument('--nanite-vsm', action='store_true',
                        help='Request and log Nanite and Virtual Shadow Maps; verify actual workloads separately')
    parser.add_argument('--camera', type=float, nargs=6, metavar=('X','Y','Z','PITCH','YAW','ROLL'),
                        help='Optional finite editor fixture camera, applied with the capture size')
    parser.add_argument('--scalability-quality', type=int, choices=(2,3,4),
                        help='UE public High/Epic/Cinematic quality tier; actual resource budgets still apply')
    parser.add_argument('--lossless-settings-library', type=Path,
                        help='Test-only preload helper for the public in-memory PNG setting')
    parser.add_argument('--native-viewport-output', action='store_true',
                        help='Save a same-frame Native full viewport BGRA observation; frame-end readback affects timing')
    parser.add_argument('--native-viewport-after-capture', action='store_true',
                        help='Read the retained same-frame viewport after capture ends; add no observation commands to the captured frame')
    parser.add_argument('--production-capabilities', action='store_true',
                        help='Use published device capabilities without the process-local RT probe')
    args = parser.parse_args()
    if args.native_viewport_after_capture and not args.native_viewport_output:
        parser.error('--native-viewport-after-capture requires --native-viewport-output')
    if not 1 <= args.capture_delay <= 60 or not 30 <= args.startup_timeout <= 360:
        parser.error("capture-delay must be 1..60 and startup-timeout 30..360 seconds")
    if any(n < 64 or n > 2048 for n in args.viewport_size):
        parser.error('viewport dimensions must be 64..2048 for this bounded diagnostic')
    if not (320 <= args.window_size[0] <= 2048 and 240 <= args.window_size[1] <= 2048):
        parser.error('window dimensions must be 320x240..2048x2048')
    if args.camera and not all(math.isfinite(n) for n in args.camera):
        parser.error('camera values must be finite')
    repo = Path(__file__).resolve().parents[2]
    build = (args.build_dir or repo/"build-macos-debug").resolve()
    work = (args.work_dir or build/"ue-ray-diagnostic").resolve()
    project = args.project.resolve()
    work.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in tuple(env):
        if key.startswith("RENDERDOC_METAL_") or key == "DYLD_INSERT_LIBRARIES":
            env.pop(key)
    rhi = (args.rhi or repo/"build-macos-debug/local-ue-module-probe/provider-build-render-indirect/libUnrealEditor-MetalRHI.dylib").resolve()
    env.update(UE_METAL_PROJECT=str(project), RENDERDOC_METAL_BUILD_DIR=str(build),
               UE_METAL_RHI_OVERRIDE=str(rhi),
               RENDERDOC_METAL_RAYTRACING_PROBE="0" if args.production_capabilities else "1",
               RENDERDOC_METAL_PIPELINE_COMPILE_TRACE="1",
               RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS="1",
               RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS="1",
               UE_METAL_AUTO_CAPTURE_DELAY_SECONDS=str(args.capture_delay), UE_METAL_EXIT_AFTER_CAPTURE="1",
               UE_METAL_FRAME_STALL_SECONDS="12", UE_METAL_POST_CAPTURE_TIMEOUT_SECONDS="20",
               UE_METAL_TIMEOUT_SECONDS=str(args.startup_timeout), UE_METAL_CAPTURE_VIEWPORT_WIDTH="320",
               UE_METAL_CAPTURE_VIEWPORT_HEIGHT="240", UE_METAL_CAPTURE_WINDOW_WIDTH="640",
               UE_METAL_CAPTURE_WINDOW_HEIGHT="480")
    env['UE_METAL_CAPTURE_VIEWPORT_WIDTH'],env['UE_METAL_CAPTURE_VIEWPORT_HEIGHT'] = map(str,args.viewport_size)
    env['UE_METAL_CAPTURE_WINDOW_WIDTH'],env['UE_METAL_CAPTURE_WINDOW_HEIGHT'] = map(str,args.window_size)
    if args.camera:
        env['UE_METAL_CAPTURE_CAMERA'] = ','.join(map(str,args.camera))
    if args.lossless_settings_library:
        config_library=args.lossless_settings_library.resolve(strict=True)
        env.update(UE_METAL_CONFIG_LIBRARY=str(config_library),RENDERDOC_CAPTURE_LOSSLESS_THUMBNAIL='1')
    if args.native_viewport_output:
        native_viewport = work / 'native-viewport.bgra'
        if native_viewport.exists():
            parser.error('Native viewport output already exists; use a fresh work directory')
        env['UE_METAL_NATIVE_VIEWPORT_OUTPUT'] = str(native_viewport)
        env['UE_METAL_NATIVE_VIEWPORT_AFTER_CAPTURE'] = '1' if args.native_viewport_after_capture else '0'
    if args.trace_as_input_snapshots:env["RENDERDOC_METAL_TRACE_AS_INPUT_SNAPSHOTS"]="1"
    if args.trace_as_input_snapshots:env["RENDERDOC_METAL_TRACE_HEAP_BIRTH"]="1"
    env["UE_METAL_EDITOR_ARGS"] = (
        "-ini:Engine:[SystemSettings]:r.RayTracing=1,[SystemSettings]:r.Lumen.HardwareRayTracing=1,"
        "[SystemSettings]:r.Lumen.HardwareRayTracing.Inline=1,[SystemSettings]:r.DynamicGlobalIlluminationMethod=1,"
        "[SystemSettings]:r.ReflectionMethod=1,"
        "[SystemSettings]:r.RayTracing.Shadows="+("0" if args.lumen_only else "1")+",[SystemSettings]:r.Nanite.Streaming.StreamingPoolSize=32,"
        "[SystemSettings]:r.Nanite.Streaming.NumInitialRootPages=256 "
        '-ExecCmds="r.SceneRenderTargetResizeMethod 0,r.SceneRenderTargetResizeMethodForceOverride 1,'
        'r.Editor.Viewport.ScreenPercentageMode.RealTime 0,r.Editor.Viewport.ScreenPercentage 100" '
        "-windowed -ResX=%d -ResY=%d" % tuple(args.window_size))
    if args.nanite_vsm:
        env["UE_METAL_EDITOR_ARGS"] = env["UE_METAL_EDITOR_ARGS"].replace(
            "[SystemSettings]:r.Nanite.Streaming.StreamingPoolSize=32",
            "[SystemSettings]:r.Nanite=1,[SystemSettings]:r.Shadow.Virtual.Enable=1,"
            "[SystemSettings]:r.Nanite.Streaming.StreamingPoolSize=32")
    quality_cvars = {name:args.scalability_quality for name in (
        'sg.ViewDistanceQuality','sg.AntiAliasingQuality','sg.ShadowQuality',
        'sg.GlobalIlluminationQuality','sg.ReflectionQuality','sg.PostProcessQuality',
        'sg.TextureQuality','sg.EffectsQuality','sg.FoliageQuality','sg.ShadingQuality')}
    if args.scalability_quality is not None:
        env['UE_METAL_EDITOR_ARGS'] = env['UE_METAL_EDITOR_ARGS'].replace(
            '-ini:Engine:', '-ini:Engine:'+','.join('[SystemSettings]:'+name+'='+str(value)
                                                   for name,value in quality_cvars.items())+',')
    if args.small_shadows:
        env["UE_METAL_EDITOR_ARGS"] = env["UE_METAL_EDITOR_ARGS"].replace(
            "[SystemSettings]:r.Nanite.Streaming.StreamingPoolSize=32",
            "[SystemSettings]:r.Shadow.MaxResolution=256,[SystemSettings]:r.Shadow.MaxCSMResolution=256,"
            "[SystemSettings]:r.Shadow.CSM.MaxCascades=1,[SystemSettings]:r.Nanite.Streaming.StreamingPoolSize=32")
    # Values come from the installed UE renderer's public cvars. Grid4 across
    # four clipmaps has 256 possible cells, below the 32² probe capacity; final
    # (8+2)*32 atlas is 320². Preserve Lumen HWRT/Inline switches and trace budget.
    small_cache_cvars = {
        "r.LumenScene.SurfaceCache.AtlasSize":512,
        "r.Lumen.ScreenProbeGather.RadianceCache.GridResolution":4,
        "r.Lumen.ScreenProbeGather.RadianceCache.ProbeResolution":8,
        "r.Lumen.ScreenProbeGather.RadianceCache.ProbeAtlasResolutionInProbes":32,
        "r.Lumen.TranslucencyVolume.RadianceCache.GridResolution":4,
        "r.Lumen.TranslucencyVolume.RadianceCache.ProbeResolution":8,
        "r.Lumen.TranslucencyVolume.RadianceCache.ProbeAtlasResolutionInProbes":32,
    }
    if args.small_lumen_caches:
        env["UE_METAL_EDITOR_ARGS"] = env["UE_METAL_EDITOR_ARGS"].replace(
            "[SystemSettings]:r.Nanite.Streaming.StreamingPoolSize=32",
            ",".join("[SystemSettings]:"+key+"="+str(value) for key,value in small_cache_cvars.items())+
            ",[SystemSettings]:r.Nanite.Streaming.StreamingPoolSize=32")
    # Print the actual runtime cvars to the retained UE log. These requests alone
    # never count as proof that the renderer dispatched a hardware ray workload.
    env["UE_METAL_EDITOR_ARGS"] = env["UE_METAL_EDITOR_ARGS"].replace(
        'r.Editor.Viewport.ScreenPercentage 100"',
        'r.Editor.Viewport.ScreenPercentage 100,r.RayTracing,r.Lumen.HardwareRayTracing,r.Lumen.HardwareRayTracing.Inline,r.DynamicGlobalIlluminationMethod,r.ReflectionMethod"')
    if args.small_shadows:
        env["UE_METAL_EDITOR_ARGS"] = env["UE_METAL_EDITOR_ARGS"].replace(
            'r.ReflectionMethod"', 'r.ReflectionMethod,r.Shadow.MaxResolution,r.Shadow.MaxCSMResolution,r.Shadow.CSM.MaxCascades"')
    if args.small_lumen_caches:
        printed = "r.Shadow.CSM.MaxCascades" if args.small_shadows else "r.ReflectionMethod"
        env["UE_METAL_EDITOR_ARGS"] = env["UE_METAL_EDITOR_ARGS"].replace(
            printed+'"', printed+','+','.join(small_cache_cvars)+'"')
    if args.nanite_vsm:
        env["UE_METAL_EDITOR_ARGS"] = env["UE_METAL_EDITOR_ARGS"].replace(
            '" -windowed', ',r.Nanite,r.Shadow.Virtual.Enable" -windowed')
    manifest = {"project":str(project), "rhi_override":str(rhi), "rhi_sha256":sha(rhi), "backend_sha256":sha(build/"lib/librenderdoc.dylib"),
                "diagnostic_compute_capability_requested":not args.production_capabilities,
                "published_capabilities_requested":args.production_capabilities,
                "production_capabilities_enabled":"DEVICE_QUERY_NOT_YET_VERIFIED",
                "AS_input_snapshot_trace_requested":args.trace_as_input_snapshots,
                "capture_delay_seconds":args.capture_delay,"startup_timeout_seconds":args.startup_timeout,
                "lumen_HWRT_requested":True,"separate_RT_shadows_requested":not args.lumen_only,
                "small_lumen_cache_limits_requested":small_cache_cvars if args.small_lumen_caches else None,
                "small_shadow_limits_requested":dict(max_resolution=256,max_CSM_resolution=256,max_cascades=1) if args.small_shadows else None,
                "replay":"NOT RUN", "rt_dispatch":"NOT YET PROVEN", "status":"CHECK",
                "execution":{"native_capture":"NOT_RUN", "replay":"NOT_RUN"},
                "output_comparison":{"status":"NOT_RUN", "scope":"complete UE Native versus replay"},
                "overall_acceptance":{"status":"INCOMPLETE", "reason":"capture alone does not validate replay outputs or enable ray tracing"}}
    manifest['viewport_size_requested']=args.viewport_size
    manifest['nanite_vsm_requested']=args.nanite_vsm
    manifest['fixture_camera_requested'] = args.camera
    manifest['scalability_quality_requested'] = args.scalability_quality
    if args.lossless_settings_library:
        manifest.update(lossless_config_library=str(config_library),lossless_config_library_sha256=sha(config_library),
                        lossless_thumbnail_requested=True)
    launcher = ["bash", str(repo/"util/ue/run_ue_metal_capture_macos.sh")]
    try:
        if not args.run:
            return subprocess.run(launcher+["--check"],env=env).returncode
        manifest.update(status="RUNNING", frame_stall_seconds=12, post_capture_timeout_seconds=20)
        write_json(work/"manifest.json", manifest)
        for name in ("UnrealEditor","qrenderdoc"):
            if subprocess.run(["pgrep","-x",name],stdout=subprocess.DEVNULL).returncode == 0:
                raise RuntimeError(name+" is already running; finish its GPU work first")
        settings = project.parent/"Saved/Config/MacEditor/EditorPerProjectUserSettings.ini"
        original_settings_sha = sha(settings) if settings.exists() else None
        text = settings.read_text() if settings.exists() else ""
        text = diagnostic_editor_settings(text, args.window_size)
        isolated = work/"EditorPerProjectUserSettings.ini"
        isolated.write_text(text)
        env["UE_METAL_EDITOR_ARGS"] += " " + shlex.quote("-EditorPerProjectUserSettingsINI="+str(isolated))
        manifest.update(startup_settings=str(isolated), user_settings_sha256=original_settings_sha,
                        startup_window=args.window_size, initially_maximized=False)
        live = project.parent/"Saved/RenderDocMetalCaptures/UE58_capture.rdc"
        previous = sha(live) if live.exists() else None
        if previous:
            preserved = live.with_name("UE58_original_"+previous+".rdc")
            if preserved.exists() and sha(preserved)!=previous:
                raise RuntimeError("Preserved original capture hash differs")
            if not preserved.exists(): shutil.copyfile(live,preserved)
        with (work/"launch.log").open("w") as log:
            launch_exit = run_launcher(launcher+["--run"],env,log,timeout=args.startup_timeout+40)
        manifest["launch_exit"] = launch_exit
        if original_settings_sha is not None and sha(settings)!=original_settings_sha:
            raise RuntimeError("Original editor settings changed")
        if launch_exit: raise RuntimeError("UE launch/capture failed; inspect launch.log and its session logs")
        if not live.exists() or sha(live)==previous:
            raise RuntimeError("No fresh RT diagnostic capture")
        if args.small_lumen_caches:
            match = re.search(r"^Session: (.+)$", (work/"launch.log").read_text(), re.M)
            if not match:raise RuntimeError("No retained UE session for runtime cvar proof")
            editor_log = (Path(match.group(1))/"ue-editor.log").read_text(errors="replace")
            expected_cvars = dict(small_cache_cvars, **{
                "r.RayTracing":1,"r.Lumen.HardwareRayTracing":1,
                "r.Lumen.HardwareRayTracing.Inline":1,
                "r.DynamicGlobalIlluminationMethod":1,"r.ReflectionMethod":1})
            observed = {}
            for key,value in expected_cvars.items():
                found = re.findall(re.escape(key)+r'\s*=\s*"(\d+)"\s+LastSetBy:\s*(\w+)', editor_log)
                observed[key] = {"value":int(found[-1][0]),"source":found[-1][1]} if found else None
            manifest["runtime_cache_cvars"] = observed
            if any(observed[key] is None or observed[key]["value"]!=value or
                   observed[key]["source"]!="SystemSettingsIni" for key,value in expected_cvars.items()):
                raise RuntimeError("UE did not apply the requested small cache/HWRT cvars")
        original = work/"original.rdc"; shutil.copyfile(live,original)
        manifest.update(capture=str(original),capture_sha256=sha(original))
        xml = work/"original.zip.xml"
        export_env=env.copy()
        export_env["RENDERDOC_DEBUG_LOG_FILE"]=str(work/"structured-export-renderdoc.log")
        with (work/"structured-export-renderdoc.log").open("w") as retained, (work/"structured-export.log").open("w") as export_log:
            fcntl.flock(retained.fileno(),fcntl.LOCK_SH)
            subprocess.run([str(build/"bin/renderdoccmd"),"convert","-f",str(original),
                            "-o",str(xml),"-c","zip.xml"],check=True,timeout=120,
                           env=export_env,stdout=export_log,stderr=subprocess.STDOUT)
        export_trace=work/"structured-export-renderdoc.log"
        diagnostics=(work/"structured-export.log").read_text(errors="replace")
        if export_trace.exists():diagnostics+=export_trace.read_text(errors="replace")
        if any(marker in diagnostics for marker in ("Assertion failed","OVERRUNNING CHUNK",
                "failed assertion","Unexpected Metal resource type","m_ResourceMap.empty")):
            raise RuntimeError("UE capture structured export has backend assertion/corruption diagnostics")
        chunks = ET.parse(xml).findall("./chunks/chunk")
        counts = {}
        for chunk in chunks:
            name = chunk.get("name","")
            if ("AccelerationStructure" in name or "IntersectionFunction" in name or
                "dispatch" in name): counts[name] = counts.get(name,0)+1
        ray_inventory=analyse_ray_capture(xml)
        write_json(work/"ray-workload-inventory.json",ray_inventory)
        manifest["ray_workload_inventory"]=str(work/"ray-workload-inventory.json")
        if ray_inventory["IR_dispatches"]:
            manifest["rt_dispatch"]="IR STRUCTURAL DISPATCH OBSERVED; OUTPUTS/COMPLETION/REPLAY UNVALIDATED"
        elif ray_inventory["inline_candidates"]:
            manifest["rt_dispatch"]="AS-BOUND COMPUTE CANDIDATE; SHADER/OUTPUTS/REPLAY UNVALIDATED"
        else:
            manifest["rt_dispatch"]="NOT PROVEN; ordinary/unidentified compute is insufficient"
        if args.lumen_only:
            ray_air=verify_ray_air(xml,work/"AIR-proof",ray_inventory)
            manifest["ray_AIR_proof"]=str(work/"AIR-proof/manifest.json")
            if ray_air["bound_ray_query_dispatches"]:
                manifest["rt_dispatch"]="BOUND HW RAY QUERY SHADERS WITH NONZERO GROUPS; OUTPUTS/REPLAY UNVALIDATED"
            manifest["ray_AIR_unvalidated_entries"]=sum(x["status"]=="UNVALIDATED" for x in ray_air["entries"])
            if args.small_lumen_caches and (not ray_air["bound_ray_query_dispatches"] or
                                           manifest["ray_AIR_unvalidated_entries"]):
                raise RuntimeError("Small cache capture lacks fully audited bound HW query dispatches")
        manifest["observed_chunks"] = counts
        manifest["status"] = "CAPTURED; RT use and replay require audit"
        manifest["execution"]["native_capture"] = "COMPLETED"
        if args.native_viewport_output:
            if not native_viewport.exists() or native_viewport.stat().st_size != args.viewport_size[0] * args.viewport_size[1] * 4:
                raise RuntimeError('Missing or wrong-size same-frame Native viewport observation')
            manifest['native_viewport_observation'] = {
                'path':str(native_viewport), 'sha256':sha(native_viewport),
                'dimensions':args.viewport_size, 'format':'UE FColor BGRA8 RCM_UNorm linearToGamma=false',
                'scope':('same retained viewport backing; ReadSurfaceData AFTER EndFrameCapture, before following draw; comparison not run' if args.native_viewport_after_capture else 'same complete viewport draw as capture; frame-end ReadPixels may affect scheduling; replay comparison not run'),
                'inside_capture':not args.native_viewport_after_capture}
        print(json.dumps(manifest,ensure_ascii=False,indent=2))
        return 0
    except (OSError,RuntimeError,subprocess.SubprocessError) as error:
        manifest["execution"]["native_capture"] = "FAILED"
        manifest.update(status="FAIL",error=str(error));print(str(error),file=sys.stderr);return 1
    finally:
        launch_log = work/"launch.log"
        if args.run and launch_log.exists():
            match = re.search(r"^Session: (.+)$", launch_log.read_text(), re.M)
            if match:
                session = Path(match.group(1))
                manifest["session"] = str(session)
                supervisor = session/"supervisor.json"
                if supervisor.exists():
                    manifest["supervisor"] = json.loads(supervisor.read_text())
                trace = session/"renderdoc.log"
                manifest["diagnostic_query_observed"] = trace.exists() and (
                    "Experimental Metal compute ray tracing probe enabled" in
                    trace.read_text(errors="replace"))
        current_sha = sha(build/"lib/librenderdoc.dylib")
        manifest["backend_end_sha256"] = current_sha
        if current_sha != manifest["backend_sha256"]:
            manifest.update(status="FAIL", error="Backend changed during UE diagnostic")
        write_json(work/"manifest.json", manifest)
        if current_sha != manifest["backend_sha256"]:
            raise SystemExit(1)


if __name__ == "__main__":
    # Share the finite sample gate lock; an editor capture cannot overlap any
    # local IR native/capture/replay run, even when launched from another chat.
    with (Path(tempfile.gettempdir())/"renderdoc-metal-ir-gpu-tests.lock").open("a") as lock:
        fcntl.flock(lock.fileno(),fcntl.LOCK_EX)
        sys.exit(main())
