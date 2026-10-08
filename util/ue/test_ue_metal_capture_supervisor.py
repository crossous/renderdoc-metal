#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only failure injection for the editor diagnostic's ownership and deadlines."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import Mock, patch

from ue_metal_capture_supervisor import Progress, run_launcher, run_owned, stop_group
from run_testproj_metal_ray_macos import diagnostic_editor_settings

INIT = b"[2026.10.05-15.51.04:008][  0]LogInit: Engine is initialized. Leaving FEngineLoop::Init()\n"


class ProgressTests(unittest.TestCase):
    def test_startup_deadline(self):
        monitor = Progress(0, 10)
        monitor.feed(b"starting\n", 9)
        self.assertIsNone(monitor.reason(9))
        self.assertEqual(monitor.reason(10), "startup timeout")

    def test_split_lines_and_initialization_once(self):
        monitor = Progress(0, 10, post_init_seconds=5)
        monitor.feed(INIT[:40], 3)
        self.assertFalse(monitor.initialized)
        monitor.feed(INIT[40:], 4)
        monitor.feed(INIT, 7)
        self.assertEqual(monitor.post_init_deadline, 9)
        self.assertEqual(monitor.reason(9), "controlled capture timeout")

    def test_background_logs_cannot_hide_stall(self):
        monitor = Progress(0, 360, 12)
        monitor.feed(INIT, 0)
        monitor.feed(b"[timestamp][127]provider heap\n", 1)
        for now in range(2, 14):
            monitor.feed(b"[timestamp][127]audio/network/maintenance still logging\n", now)
        self.assertEqual(monitor.reason(13), "editor frame progress stalled")

    def test_advancing_and_wrapped_frames(self):
        monitor = Progress(0, 10, 12)
        monitor.feed(INIT, 0)
        for now, frame in ((11, 999), (22, 0), (33, 1)):
            monitor.feed(f"[timestamp][{frame}]frame\n".encode(), now)
            self.assertIsNone(monitor.reason(now))

    def test_unrelated_lines_do_not_count(self):
        monitor = Progress(0, 10, 12)
        monitor.feed(INIT, 0)
        monitor.feed(b"other [127] words\n", 11)
        self.assertEqual(monitor.reason(12), "editor frame progress stalled")

    def test_saved_precedes_deadline(self):
        monitor = Progress(0, 1)
        monitor.feed(b"LogRenderDocMetalCapture: Display: Capture saved: /capture.rdc\n", 2)
        self.assertEqual(monitor.reason(2), "capture saved")

    def test_manual_capture_does_not_close_editor(self):
        monitor = Progress(0, 10, stop_on_saved=False)
        monitor.feed(INIT, 1)
        monitor.feed(b"LogRenderDocMetalCapture: Display: Capture saved: /capture.rdc\n", 2)
        self.assertIsNone(monitor.reason(20))

    def test_shader_error(self):
        monitor = Progress(0, 10)
        monitor.feed(b"Shader compiler errors compiling global shaders\n", 1)
        self.assertEqual(monitor.reason(1), "global shader compilation failed")

    def test_darwin_eperm_requires_no_live_group_members(self):
        process = Mock(pid=123)
        with patch("ue_metal_capture_supervisor.os.killpg", side_effect=PermissionError), \
             patch("ue_metal_capture_supervisor.subprocess.run",
                   return_value=Mock(stdout=" 123 Z\n 456 S\n")):
            stop_group(process)
        process.wait.assert_called_once()

    def test_eperm_with_live_member_remains_failure(self):
        process = Mock(pid=123)
        with patch("ue_metal_capture_supervisor.os.killpg", side_effect=PermissionError), \
             patch("ue_metal_capture_supervisor.subprocess.run",
                   return_value=Mock(stdout=" 123 S\n")):
            with self.assertRaises(PermissionError):
                stop_group(process)

    def test_eperm_probe_allows_exiting_member_bounded_grace(self):
        process = Mock(pid=123)
        with patch("ue_metal_capture_supervisor.os.killpg",
                   side_effect=[None, PermissionError, PermissionError, ProcessLookupError]), \
             patch("ue_metal_capture_supervisor.active_group_members",
                   side_effect=[["123 S"], []]), \
             patch("ue_metal_capture_supervisor.time.sleep") as sleep:
            stop_group(process)
        sleep.assert_called_once_with(0.05)
        process.wait.assert_called_once()


class SettingsTests(unittest.TestCase):
    def test_saved_maximized_window_and_other_keys_preserved(self):
        source = ("[RootWindow]\nWindowSize=X=1728 Y=1020\nInitiallyMaximized=True\nWindowPosition=X=5 Y=6\n"
                  "[Other]\nKeep=yes\n[/Script/UnrealEd.EditorLoadingSavingSettings]\nLoadLevelAtStartup=LastOpened\n")
        result = diagnostic_editor_settings(source)
        self.assertIn("WindowSize=X=640 Y=480", result)
        self.assertIn("InitiallyMaximized=False", result)
        self.assertIn("WindowPosition=X=5 Y=6", result)
        self.assertIn("[Other]\nKeep=yes", result)
        self.assertIn("LoadLevelAtStartup=ProjectDefault", result)
        self.assertEqual(diagnostic_editor_settings(result), result)
        self.assertIn("InitiallyMaximized=True", source)

    def test_missing_sections_and_last_line_without_newline(self):
        for source in ("", "[RootWindow]\nInitiallyMaximized=True", "[Other]\nKeep=yes"):
            result = diagnostic_editor_settings(source)
            self.assertIn("WindowSize=X=640 Y=480", result)
            self.assertIn("InitiallyMaximized=False", result)
            self.assertIn("LoadLevelAtStartup=ProjectDefault", result)
            self.assertEqual(diagnostic_editor_settings(result), result)


class OwnershipTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.session = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def run_fixture(self, source, **kwargs):
        return run_owned([sys.executable, "-c", source], os.environ.copy(), self.session,
                         startup_seconds=0.4, sample=False, **kwargs)

    def record(self):
        return json.loads((self.session / "supervisor.json").read_text())

    def assert_dead(self, pid):
        probe = subprocess.run(["ps", "-p", str(pid), "-o", "stat="], capture_output=True, text=True)
        self.assertTrue(probe.returncode or probe.stdout.strip().startswith("Z"), probe.stdout)

    def test_startup_timeout_kills_ignoring_child(self):
        code = self.run_fixture("import signal,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);time.sleep(60)")
        self.assertEqual(code, 124)
        self.assertEqual(self.record()["reason"], "startup timeout")
        self.assert_dead(self.record()["editor_pid"])

    def test_exit_code_and_unrelated_process_preserved(self):
        unrelated = subprocess.Popen([sys.executable, "-c", "import time;time.sleep(60)"], start_new_session=True)
        try:
            self.assertEqual(self.run_fixture("raise SystemExit(17)"), 17)
            self.assertIsNone(unrelated.poll())
        finally:
            unrelated.terminate(); unrelated.wait(timeout=2)

    def test_clean_exit_keeps_log_that_would_be_unlinked_without_shared_holder(self):
        log = self.session / "renderdoc.log"
        source = ("import fcntl;from pathlib import Path;"
                  f"p=Path({str(log)!r});f=p.open('a');f.write('native compile evidence\\n');f.flush();"
                  "\ntry:\n fcntl.flock(f.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)\n p.unlink()"
                  "\nexcept BlockingIOError:\n pass\n")
        # Reproduce RenderDoc's normal-close deletion without launching Metal.
        subprocess.run([sys.executable, "-c", source], check=True)
        self.assertFalse(log.exists())
        self.assertEqual(self.run_fixture(source), 0)
        self.assertEqual(log.read_text(), "native compile evidence\n")

    def test_leader_exit_does_not_leave_descendant_running(self):
        child_pid = self.session / "descendant.pid"
        child = "import signal,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);time.sleep(60)"
        source = ("import subprocess,sys,time;from pathlib import Path;"
                  f"p=subprocess.Popen([sys.executable,'-c',{child!r}]);"
                  f"Path({str(child_pid)!r}).write_text(str(p.pid));time.sleep(0.15)")
        try:
            self.assertEqual(self.run_fixture(source), 0)
            self.assert_dead(int(child_pid.read_text()))
        finally:
            if child_pid.exists():
                try:
                    os.kill(int(child_pid.read_text()), signal.SIGKILL)
                except ProcessLookupError:
                    pass

    def test_frame_stall_with_active_background_log(self):
        source = ("import time;from pathlib import Path;"
                  f"p=Path({str(self.session / 'ue-editor.log')!r});p.write_bytes({INIT!r});"
                  "\nwhile True:\n with p.open('ab') as f:f.write(b'[timestamp][127]network heartbeat\\n')\n time.sleep(0.05)")
        self.assertEqual(self.run_fixture(source, stall_seconds=0.4), 124)
        self.assertEqual(self.record()["reason"], "editor frame progress stalled")
        self.assert_dead(self.record()["editor_pid"])

    def test_abort_preserves_pending_compile_identity(self):
        compile_begin = dict(phase="begin", token=1, api="descriptor-sync", thread=23,
                             function_name_bytes="rayShaderHash", label_bytes="RT pipeline",
                             options=1, linked_functions=4, binary_functions=0,
                             private_functions=0, max_call_stack_depth=1)
        (self.session / "renderdoc.log").write_text(
            "RDOC 000123: MetalComputeCompileTrace " + json.dumps(compile_begin) + "\n")
        source = ("import time;from pathlib import Path;"
                  f"Path({str(self.session / 'ue-editor.log')!r}).write_bytes({INIT!r});time.sleep(60)")
        self.assertEqual(self.run_fixture(source, stall_seconds=0.4), 124)
        self.assertEqual(self.record()["compile_trace"]["status"], "PENDING")
        report = json.loads((self.session / "compile-trace.json").read_text())
        self.assertEqual(report["pending"][0]["function_name"], "rayShaderHash")
        self.assertEqual(report["pending"][0]["thread"], 23)

    def test_invalid_compile_trace_does_not_hide_owned_process_cleanup(self):
        (self.session / "renderdoc.log").write_text('MetalComputeCompileTrace {"token":\n')
        self.assertEqual(self.run_fixture("import time;time.sleep(60)"), 124)
        self.assertEqual(self.record()["compile_trace"]["status"], "INVALID TRACE")
        self.assert_dead(self.record()["editor_pid"])

    def test_saved_closes_owned_group(self):
        source = ("import time;from pathlib import Path;"
                  f"Path({str(self.session / 'ue-editor.log')!r}).write_bytes("
                  "b'LogRenderDocMetalCapture: Display: Capture saved: /fixture.rdc\\n');time.sleep(60)")
        self.assertEqual(self.run_fixture(source), 0)
        self.assertEqual(self.record()["status"], "CAPTURE SAVED")
        self.assert_dead(self.record()["editor_pid"])

    def test_outer_timeout_cleans_separate_editor_group(self):
        helper = Path(__file__).parent.resolve()
        source = (f"import sys;sys.path.insert(0,{str(helper)!r});"
                  "from ue_metal_capture_supervisor import run_owned;from pathlib import Path;import os;"
                  f"run_owned([sys.executable,'-c','import time;time.sleep(60)'],os.environ.copy(),Path({str(self.session)!r}),"
                  "60,sample=False)")
        with (self.session / "outer.log").open("wb") as log:
            with self.assertRaises(subprocess.TimeoutExpired):
                run_launcher([sys.executable, "-c", source], os.environ.copy(), log, timeout=0.8)
        self.assertEqual(self.record()["reason"], "supervisor interrupted")
        self.assert_dead(self.record()["editor_pid"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
