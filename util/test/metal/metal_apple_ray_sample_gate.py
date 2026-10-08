#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned Apple sample: build, deterministic native oracle, optional capture probe.

Capture-probe does not certify replay or change published ray tracing flags.
See docs/metal-replay/RAYTRACING_ENABLEMENT.md for the remaining release gates.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import zipfile


URL = "https://docs-assets.developer.apple.com/published/ade36d76f1bb/AcceleratingRayTracingUsingMetal.zip"
SHA256 = "4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", choices=("size-queries", "native", "capture-probe", "replay"), default="native")
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    work = (args.work_dir or repo / "build-macos-debug/ray-samples/apple-basic").resolve()
    library = (args.library or repo / "build-macos-debug/lib/librenderdoc.dylib").resolve()
    work.mkdir(parents=True, exist_ok=True)
    results = work / "gate-results"
    results.mkdir(exist_ok=True)
    manifest = {"sample_url": URL, "sample_sha256": SHA256, "stage": args.stage,
                "seed": 1, "frames": 4, "width": 64, "height": 64, "checks": [],
                "replay": "NOT RUN", "ue_raytracing": "NOT RUN"}

    def run(name, command, seconds=120, injected=False):
        env = os.environ.copy()
        env.pop("DYLD_INSERT_LIBRARIES", None)
        env["MTL_DEBUG_LAYER"] = "1"
        env["RENDERDOC_DEBUG_LOG_FILE"] = str(results / (name + "-renderdoc.log"))
        if injected:
            env["DYLD_INSERT_LIBRARIES"] = str(library)
        log_path = results / (name + ".log")
        with log_path.open("w") as log:
            process = subprocess.Popen([str(x) for x in command], cwd=repo, env=env,
                                       stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                code = process.wait(timeout=seconds)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                code = 124
        manifest["checks"].append({"name": name, "exit_code": code, "log": str(log_path)})
        print(("PASS " if code == 0 else "FAIL ") + name, flush=True)
        if code:
            print(log_path.read_text(errors="replace")[-5000:], file=sys.stderr)
            raise RuntimeError(name + " failed")

    def size_queries(injected):
        query = work / "size-query"
        run("build-size-query", ["clang++", "-std=c++17", "-fobjc-arc",
                                repo / "util/test/metal/metal_ray_size_query.mm",
                                "-framework", "Foundation", "-framework", "Metal", "-o", query])
        run("native-size-query", [query], 60)
        if injected:
            manifest["backend_sha256"] = digest(library)
            run("injected-size-query", [query], 60, injected=True)
            def query_results(name):
                return [line for line in (results / name).read_text().splitlines()
                        if line.startswith(("primitive ", "instance "))]
            native_sizes = query_results("native-size-query.log")
            injected_sizes = query_results("injected-size-query.log")
            if len(native_sizes) != 5 or native_sizes != injected_sizes:
                raise RuntimeError("Injected size queries differ from the native driver")
            manifest["checks"].append({"name": "size-query-equality", "cases": 5,
                                       "results": native_sizes})
        unbound = work / "unbound-size-query"
        run("build-unbound-size-query", ["clang++", "-std=c++17", "-fobjc-arc",
            repo / "util/test/metal/metal_ray_unbound_size_query.mm",
            "-framework", "Foundation", "-framework", "Metal", "-o", unbound])
        run("native-unbound-size-query", [unbound], 60)
        if injected:
            run("injected-unbound-size-query", [unbound], 60, injected=True)
            def unbound_results(name):
                return [line for line in (results / name).read_text().splitlines()
                        if line.startswith(("unbound ", "private "))]
            native_sizes = unbound_results("native-unbound-size-query.log")
            injected_sizes = unbound_results("injected-unbound-size-query.log")
            if len(native_sizes) != 5 or native_sizes != injected_sizes:
                raise RuntimeError("Unbound/indirect size queries differ from the native driver")
            manifest["checks"].append({"name": "unbound-size-query-equality", "cases": 5,
                                       "results": native_sizes})

    try:
        if args.stage == "size-queries":
            size_queries(True)
            manifest["status"] = "PASS size-queries"
            return 0
        archive = work / "AcceleratingRayTracingUsingMetal.zip"
        if not archive.exists():
            run("download", ["curl", "-fLsS", "--max-time", "60", URL, "-o", archive], 75)
        if digest(archive) != SHA256:
            raise RuntimeError("Official archive changed: review provenance before updating the pin")
        source = work / "source"
        extract = not source.exists()
        if extract:
            source.mkdir()
        with zipfile.ZipFile(archive) as z:
            for entry in z.infolist():
                if entry.filename.split("/")[0] in (".git", "__MACOSX"):
                    continue
                target = source / entry.filename
                if not target.resolve().is_relative_to(source):
                    raise RuntimeError("Archive path escapes sample directory")
                if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                    raise RuntimeError("Unexpected symlink in sample archive")
                if extract:
                    z.extract(entry, source)
                if not entry.is_dir() and target.read_bytes() != z.read(entry):
                    raise RuntimeError("Official source changed: " + entry.filename)
        manifest["official_source_verified"] = True
        manifest["sample_license_sha256"] = digest(source / "LICENSE.txt")
        run("build-metal2", ["xcodebuild", "-project", source / "SimplePathTracer.xcodeproj",
                            "-scheme", "macOS - Metal2 - SimplePathTracer", "-configuration", "Debug",
                            "-derivedDataPath", work / "derived", "CODE_SIGNING_ALLOWED=NO", "build"], 600)
        renderer = source / "Renderer"
        runner = work / "runner"
        run("build-runner", ["clang++", "-std=c++17", "-fobjc-arc", "-DSUPPORTS_METAL_3=0",
                             "-I" + str(repo), "-I" + str(renderer),
                             repo / "util/test/metal/metal_apple_ray_sample_capture.mm",
                             renderer / "Renderer.mm", renderer / "Scene.mm", renderer / "Transforms.mm",
                             "-framework", "Foundation", "-framework", "Metal", "-framework", "MetalKit",
                             "-framework", "QuartzCore", "-o", runner])
        metallib = work / "derived/Build/Products/Debug/SimplePathTracer-Metal2.app/Contents/Resources/default.metallib"
        manifest["metallib_sha256"] = digest(metallib)
        manifest["runner_sha256"] = digest(runner)
        size_queries(args.stage in ("capture-probe", "replay"))
        for mode in ("triangles", "procedural"):
            for attempt in (1, 2):
                prefix = results / ("native-" + mode + "-" + str(attempt))
                run(prefix.name, [runner, metallib, prefix, mode, "native"], 60)
            first = results / ("native-" + mode + "-1.rgba32f")
            second = results / ("native-" + mode + "-2.rgba32f")
            if first.read_bytes() != second.read_bytes():
                raise RuntimeError(mode + ": native output is not repeatable on this device")
            manifest["checks"].append({"name": "repeatable-" + mode,
                                       "output_sha256": digest(first), "bytes": first.stat().st_size})
        if args.stage in ("capture-probe", "replay"):
            manifest["backend_sha256"] = digest(library)
            for mode in ("triangles", "procedural"):
                prefix = results / ("capture-" + mode)
                run(prefix.name, [runner, metallib, prefix, mode, "capture-probe"], 60, injected=True)
                if prefix.with_suffix(".rgba32f").read_bytes() != (results / ("native-" + mode + "-1.rgba32f")).read_bytes():
                    raise RuntimeError(mode + ": injected output differs from native oracle")
                captures = sorted(results.glob(prefix.name + "*.rdc"))
                if not captures:
                    raise RuntimeError(mode + ": no capture produced")
                manifest["checks"].append({"name": "capture-output-" + mode,
                                           "captures": [{"path": str(p), "sha256": digest(p)} for p in captures]})
        if args.stage == "replay":
            oracle = work / "replay"
            run("build-replay", ["clang++", "-std=c++17", "-DRENDERDOC_PLATFORM_APPLE",
                                "-I" + str(repo), repo / "util/test/metal/metal_apple_ray_sample_replay.cpp",
                                "-L" + str(library.parent), "-lrenderdoc",
                                "-Wl,-rpath," + str(library.parent), "-o", oracle])
            manifest["replay"] = "RUNNING"
            for mode in ("triangles", "procedural"):
                capture = results / ("capture-" + mode + "_capture.rdc")
                run("replay-output-" + mode, [oracle, capture, results / ("native-" + mode + "-1.rgba32f")], 300)
                run("replay-cli-" + mode, [repo / "build-macos-debug/bin/renderdoccmd",
                                          "replay", "--loops", "3", capture], 300)
            manifest["replay"] = "PASS"
        manifest["status"] = "PASS " + args.stage
    except (OSError, RuntimeError, zipfile.BadZipFile) as error:
        manifest["status"] = "FAIL"
        manifest["error"] = str(error)
        if manifest["replay"] == "RUNNING":
            manifest["replay"] = "FAIL"
        print(str(error), file=sys.stderr)
        return 1
    finally:
        record = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
        (results / "manifest.json").write_text(record)
        (results / (args.stage + "-manifest.json")).write_text(record)
    print("Manifest: " + str(results / "manifest.json"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
