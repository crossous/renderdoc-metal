#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Own and bound one editor diagnostic, including its separate process group."""
import datetime
import fcntl
import json
import os
from pathlib import Path
import re
import shlex
import signal
import subprocess
import sys
import time
from metal_pipeline_compile_audit import audit_compile_trace


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


class Progress:
    def __init__(self, started, startup_seconds, stall_seconds=0, post_init_seconds=None,
                 stop_on_saved=True):
        self.startup_deadline = started + startup_seconds
        self.stall_seconds = stall_seconds
        self.post_init_seconds = post_init_seconds
        self.initialized = False
        self.post_init_deadline = None
        self.frame = None
        self.last_frame_time = started
        self.tail = b""
        self.saved = False
        self.shader_error = False
        self.stop_on_saved = stop_on_saved

    def feed(self, output, now):
        data = self.tail + output
        # Only parse complete lines; unrelated growing logs at the same frame must
        # not conceal a blocked render/game loop. Counter wrap is also progress.
        lines = data.split(b"\n")
        self.tail = lines.pop()
        for line in lines:
            if b"Shader compiler errors compiling global shaders" in line:
                self.shader_error = True
            if not self.initialized and b"Engine is initialized. Leaving FEngineLoop::Init()" in line:
                self.initialized = True
                self.last_frame_time = now
                if self.post_init_seconds is not None:
                    self.post_init_deadline = now + self.post_init_seconds
            match = re.match(rb"^\[[^]\r\n]+\]\[\s*(\d+)\]", line)
            if match and self.initialized:
                frame = int(match.group(1))
                if self.frame != frame:
                    self.frame = frame
                    self.last_frame_time = now
            if b"LogRenderDocMetalCapture: Display: Capture saved:" in line:
                self.saved = True

    def reason(self, now):
        if self.shader_error:
            return "global shader compilation failed"
        if self.saved and self.stop_on_saved:
            return "capture saved"
        if not self.initialized and now >= self.startup_deadline:
            return "startup timeout"
        if self.initialized:
            if self.stall_seconds and now - self.last_frame_time >= self.stall_seconds:
                return "editor frame progress stalled"
            if self.post_init_deadline is not None and now >= self.post_init_deadline:
                return "controlled capture timeout"
        return None


def active_group_members(pgid):
    members = subprocess.run(["ps", "-axo", "pgid=,stat="], check=True,
                             capture_output=True, text=True, timeout=1)
    return [line for line in members.stdout.splitlines()
            if len(line.split()) == 2 and line.split()[0] == str(pgid) and
            not line.split()[1].startswith("Z")]


def stop_group(process, grace=2):
    # Even if the leader has exited, its owned children may still be alive.
    for sig in (signal.SIGTERM, signal.SIGKILL):
        try:
            os.killpg(process.pid, sig)
        except ProcessLookupError:
            break
        except PermissionError:
            if active_group_members(process.pid):
                raise
            break
        if sig == signal.SIGTERM:
            limit = time.monotonic() + grace
            while time.monotonic() < limit:
                process.poll()  # reap the leader, without forgetting its group
                try:
                    os.killpg(process.pid, 0)
                except ProcessLookupError:
                    break
                except PermissionError:
                    # Darwin can return EPERM for a group containing only an
                    # exiting/zombie member. Give an exiting live member the
                    # same bounded grace period as a successful signal probe.
                    # EPERM itself is never proof of exit: any member surviving
                    # grace is checked again before the SIGKILL fallback.
                    if not active_group_members(process.pid):
                        break
                time.sleep(0.05)
    process.wait(timeout=2)


def run_owned(command, env, session, startup_seconds, stall_seconds=0,
              post_init_seconds=None, sample=True, stop_on_saved=True):
    record = {"status": "STARTING", "command": command, "rt_dispatch": "NOT PROVEN",
              "supervisor_pid": os.getpid(), "started_utc": datetime.datetime.now(
                  datetime.timezone.utc).isoformat(), "startup_seconds": startup_seconds,
              "stall_seconds": stall_seconds, "post_init_seconds": post_init_seconds}
    status_path = session / "supervisor.json"
    write_json(status_path, record)
    interrupted = []
    previous_handlers = {}
    process = None
    retained_log = None
    progress = Progress(time.monotonic(), startup_seconds, stall_seconds, post_init_seconds,
                        stop_on_saved)
    checked_bytes = 0
    reason = None
    code = 1
    try:
        for sig in (signal.SIGTERM, signal.SIGINT):
            previous_handlers[sig] = signal.signal(sig, lambda value, _frame: interrupted.append(value))
        # RenderDoc unlinks its log on clean exit when it can take an exclusive
        # lock. Hold the normal shared lock through owned-process cleanup/audit.
        retained_log = (session / "renderdoc.log").open("a")
        fcntl.flock(retained_log.fileno(), fcntl.LOCK_SH)
        with (session / "ue-stdout.log").open("wb") as log:
            process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                                       start_new_session=True)
            record.update(status="RUNNING", editor_pid=process.pid, process_group=process.pid)
            write_json(status_path, record)
            print(f"Editor PID: {process.pid}", flush=True)
            while True:
                editor_log = session / "ue-editor.log"
                if editor_log.exists():
                    with editor_log.open("rb") as editor_output:
                        if editor_log.stat().st_size < checked_bytes:
                            checked_bytes = 0
                            progress.tail = b""
                        editor_output.seek(checked_bytes)
                        output = editor_output.read(1024 * 1024)
                        checked_bytes += len(output)
                    progress.feed(output, time.monotonic())
                reason = ("supervisor interrupted" if interrupted else progress.reason(time.monotonic()))
                record.update(initialized=progress.initialized, last_frame=progress.frame,
                              updated_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                              frame_stale_seconds=round(time.monotonic() - progress.last_frame_time, 2))
                write_json(status_path, record)
                if reason:
                    code = 0 if reason == "capture saved" else (65 if progress.shader_error else 124)
                    break
                exit_code = process.poll()
                if exit_code is not None:
                    reason = "editor exited"
                    code = exit_code if exit_code >= 0 else 1
                    break
                time.sleep(0.2)
            # Take a bounded stack before stopping only this editor. Never signal
            # the shared system compiler, WindowServer, or unrelated editor PIDs.
            if code and sample and process.poll() is None and sys.platform == "darwin":
                with (session / "abort-sample.log").open("wb") as log:
                    try:
                        subprocess.run(["/usr/bin/sample", str(process.pid), "1", "10", "-file",
                                        str(session / "abort-sample.txt")], stdout=log,
                                       stderr=subprocess.STDOUT, timeout=2)
                    except (OSError, subprocess.TimeoutExpired) as error:
                        record["sample_error"] = str(error)
            print(f"UE diagnostic: {reason}; closing owned editor group.", flush=True)
    finally:
        try:
            if process is not None:
                try:
                    stop_group(process)
                except Exception as error:
                    record["cleanup_error"] = str(error)
                    code = 1
                    raise
                record["editor_exit"] = process.returncode
        finally:
            trace_path = session / "renderdoc.log"
            if trace_path.exists():
                try:
                    with trace_path.open(errors="replace") as trace:
                        audit = audit_compile_trace(trace)
                    audit_path = session / "compile-trace.json"
                    write_json(audit_path, audit)
                    record["compile_trace"] = {"path": str(audit_path), "status": audit["status"],
                                               "pending_count": len(audit["pending"]),
                                               "completed_count": audit["completed_count"]}
                except (OSError, ValueError) as error:
                    record["compile_trace_error"] = str(error)
            record.update(status="CAPTURE SAVED" if code == 0 and progress.saved else "STOPPED",
                          reason=reason or "supervisor exception", exit_code=code)
            write_json(status_path, record)
            for sig, handler in previous_handlers.items():
                signal.signal(sig, handler)
            if retained_log is not None:
                retained_log.close()
    return code


def run_launcher(command, env, log, timeout):
    """Give the supervisor time to clean its editor group on outer timeout."""
    process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        return process.wait(timeout=timeout)
    finally:
        stop_group(process, grace=6)


def main():
    editor, project, library, capture_dir, session, timeout, extra_args = sys.argv[1:]
    session = Path(session)
    env = os.environ.copy()
    env.update(DYLD_INSERT_LIBRARIES=library, DYLD_PRINT_LIBRARIES="1",
               RENDERDOC_METAL_LIBRARY=library, RENDERDOC_CAPFILE=str(Path(capture_dir) / "UE58"),
               RENDERDOC_DEBUG_LOG_FILE=str(session / "renderdoc.log"))
    config_library = env.get("UE_METAL_CONFIG_LIBRARY")
    if config_library:
        config_library = Path(config_library).resolve(strict=True)
        if ":" in str(config_library):
            raise ValueError("Capture config library path cannot contain a dyld separator")
        env["DYLD_INSERT_LIBRARIES"] += ":" + str(config_library)
    override = env.get("UE_METAL_RHI_OVERRIDE")
    if override:
        engine_root = Path(editor).parents[5]
        env["DYLD_LIBRARY_PATH"] = ":".join(map(str, [Path(override).resolve().parent,
            engine_root / "Binaries/Mac", engine_root / "Binaries/ThirdParty/Apple/MetalShaderConverter/Mac"]))
    command = [editor, project] + shlex.split(extra_args) + ["-Metal", "-NoSplash",
        "-ddc=" + env.get("UE_METAL_DDC_MODE", "InstalledNoZenLocalFallback"),
        "-ABSLOG=" + str(session / "ue-editor.log")]
    controlled = env.get("UE_METAL_EXIT_AFTER_CAPTURE") == "1"
    post_init = (float(env.get("UE_METAL_AUTO_CAPTURE_DELAY_SECONDS", "45")) +
                 float(env.get("UE_METAL_POST_CAPTURE_TIMEOUT_SECONDS", "180"))) if controlled else None
    print("Editor command:", " ".join(shlex.quote(arg) for arg in command), flush=True)
    return run_owned(command, env, session, float(timeout),
                     float(env.get("UE_METAL_FRAME_STALL_SECONDS", "0")), post_init,
                     stop_on_saved=controlled)


if __name__ == "__main__":
    sys.exit(main())
