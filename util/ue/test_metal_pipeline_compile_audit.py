#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only interleaving, incomplete trace, and corruption checks."""
import json
import unittest
from metal_pipeline_compile_audit import audit_compile_trace


def line(phase, token=1, pid=100, **fields):
    record = dict(phase=phase, token=token)
    if phase == "begin":
        record.update(api="descriptor-sync", function_name_bytes="rayMain", label_bytes="pipeline",
                      thread=11, options=1, linked_functions=4, binary_functions=0,
                      private_functions=0, max_call_stack_depth=1)
    elif phase == "end":
        record.update(thread=12, success=True, elapsed_ms=50.25)
    record.update(fields)
    return "RDOC %06d: file.cpp(1) -Log - MetalComputeCompileTrace %s\n" % (pid, json.dumps(record))


class AuditTests(unittest.TestCase):
    def test_native_thread_identity_survives_async_callback_and_old_traces(self):
        result = audit_compile_trace([line("begin", api="descriptor-async", native_thread=123),
                                      line("submitted", native_thread=123),
                                      line("end", native_thread=456)])
        self.assertEqual(result["status"], "COMPLETE TRACE")
        self.assertEqual(result["completed"][0]["begin_native_thread"], 123)
        self.assertEqual(result["completed"][0]["end_native_thread"], 456)
        older = audit_compile_trace([line("begin"), line("end")])
        self.assertIsNone(older["completed"][0]["begin_native_thread"])
        self.assertEqual(audit_compile_trace([line("begin"), line("end", native_thread=True)])["status"],
                         "INVALID TRACE")

    def test_native_sync_success_and_error(self):
        result = audit_compile_trace([line("begin"), line("end"), line("begin", 2),
                                      line("end", 2, success=False)])
        self.assertEqual(result["status"], "COMPLETE TRACE")
        self.assertEqual(result["native_failed_count"], 1)
        self.assertEqual(result["completed"][0]["elapsed_ms"], 50.25)
        self.assertIn("NOT PROVEN", result["rt_dispatch"])

    def test_interleaved_threads_and_callback_before_submission_return(self):
        result = audit_compile_trace([line("begin", api="descriptor-async"),
            line("begin", 2), line("end"), line("submitted"), line("end", 2)])
        self.assertEqual(result["status"], "COMPLETE TRACE")
        self.assertEqual(result["completed_count"], 2)

    def test_pending_sync_submission_and_callback_distinct(self):
        result = audit_compile_trace([line("begin"), line("begin", 2, api="function-async"),
            line("begin", 3, api="function-options-async"), line("submitted", 3)])
        self.assertEqual(result["status"], "PENDING")
        self.assertEqual([p["waiting_for"] for p in result["pending"]],
                         ["native synchronous return", "native submission return", "native callback"])

    def test_same_tokens_from_distinct_processes(self):
        result = audit_compile_trace([line("begin"), line("begin", pid=200), line("end")])
        self.assertEqual(len(result["pending"]), 1)
        self.assertEqual(result["pending"][0]["pid"], 200)

    def test_escaped_unicode_control_and_quote_name_bytes(self):
        name = 'ray"\\\n光追'.encode("utf-8").decode("latin1")
        result = audit_compile_trace([line("begin", function_name_bytes=name)])
        self.assertEqual(result["pending"][0]["function_name"], 'ray"\\\n光追')

    def test_truncated_utf8_name_is_reported_with_replacement(self):
        result = audit_compile_trace([line("begin", function_name_bytes="\xe5\x85")])
        self.assertEqual(result["pending"][0]["function_name"], "\ufffd")

    def test_no_trace_is_not_a_pass(self):
        self.assertEqual(audit_compile_trace(["ordinary UE log\n"])["status"], "NOT OBSERVED")

    def test_bad_order_and_duplicate_records(self):
        for records in ([line("end")], [line("begin"), line("begin")],
                        [line("begin"), line("end"), line("end")],
                        [line("begin"), line("submitted")],
                        [line("begin", api="function-async"), line("submitted"), line("submitted")]):
            self.assertEqual(audit_compile_trace(records)["status"], "INVALID TRACE")

    def test_corruption_preserves_preceding_pending_call(self):
        for corrupt in ('MetalComputeCompileTrace {"token":',
                        line("end", success=1), line("end", elapsed_ms=float("nan")),
                        line("begin", 2, function_name_bytes=12), line("begin", 2, thread=-1),
                        line("begin", 2, api="unknown"), line("begin", token=True)):
            result = audit_compile_trace([line("begin"), corrupt])
            self.assertEqual(result["status"], "INVALID TRACE")
            self.assertEqual(len(result["pending"]), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
