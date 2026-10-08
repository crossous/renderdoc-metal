#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Read opt-in native compute compile traces; pending calls are not RT validation."""
import argparse
import json
from pathlib import Path
import re

MARKER = "MetalComputeCompileTrace "
APIS = {"function-sync", "function-options-sync", "descriptor-sync",
        "function-async", "function-options-async", "descriptor-async"}


def decoded_name(value):
    # The native logger escapes each UTF-8 byte as JSON U+00xx, including truncation.
    if type(value) is not str:
        raise ValueError("invalid name bytes")
    return value.encode("latin1").decode("utf-8", errors="replace")


def audit_compile_trace(lines):
    begins, endings, submissions = {}, {}, set()
    errors = []
    for number, line in enumerate(lines, 1):
        if MARKER not in line:
            continue
        prefix, payload = line.split(MARKER, 1)
        match = re.search(r"RDOC\s+(\d+):", prefix)
        pid = int(match.group(1)) if match else 0
        try:
            record = json.loads(payload)
            token, phase = record["token"], record["phase"]
            if type(token) is not int or token <= 0:
                raise ValueError("invalid token")
            # B496's thread is a pthread pointer. B497 additionally records the
            # native numeric ID used by macOS sample/spindump reports. Older
            # traces remain readable without inventing a native thread ID.
            if "native_thread" in record and (type(record["native_thread"]) is not int or
                                               record["native_thread"] < 0):
                raise ValueError("invalid native_thread")
            key = pid, token
            if phase == "begin":
                if key in begins:
                    raise ValueError("duplicate begin")
                if record["api"] not in APIS:
                    raise ValueError("unknown API")
                for name in ("thread", "options", "linked_functions", "binary_functions",
                             "private_functions", "max_call_stack_depth"):
                    if type(record[name]) is not int or record[name] < 0:
                        raise ValueError("invalid " + name)
                record["function_name"] = decoded_name(record["function_name_bytes"])
                record["label"] = decoded_name(record["label_bytes"])
                record.update(pid=pid, begin_line=number)
                begins[key] = record
            elif phase == "submitted":
                if key not in begins or key in submissions or not begins[key]["api"].endswith("async"):
                    raise ValueError("unmatched/duplicate/non-async submission")
                # Native implementations may invoke the callback before returning.
                submissions.add(key)
            elif phase == "end":
                if key not in begins or key in endings:
                    raise ValueError("unmatched or duplicate end")
                elapsed = record["elapsed_ms"]
                if (type(record["success"]) is not bool or type(elapsed) not in (int, float) or
                        not 0 <= elapsed < float("inf")):
                    raise ValueError("invalid native result/duration")
                endings[key] = record
            else:
                raise ValueError("unknown phase")
        except (KeyError, TypeError, ValueError, UnicodeError) as error:
            errors.append({"line": number, "error": str(error)})
    pending = []
    for key, begin in begins.items():
        if key not in endings:
            item = dict(begin)
            item["waiting_for"] = ("native callback" if key in submissions else
                                   "native submission return" if begin["api"].endswith("async") else
                                   "native synchronous return")
            pending.append(item)
    completed = [{"pid": key[0], "token": key[1], "api": begins[key]["api"],
                  "function_name": begins[key]["function_name"], "success": end["success"],
                  "begin_native_thread": begins[key].get("native_thread"),
                  "end_native_thread": end.get("native_thread"),
                  "elapsed_ms": end["elapsed_ms"]} for key, end in endings.items()]
    return {"status": "INVALID TRACE" if errors else "PENDING" if pending else
            "COMPLETE TRACE" if begins else "NOT OBSERVED",
            "begin_count": len(begins), "completed_count": len(endings),
            "native_failed_count": sum(not end["success"] for end in endings.values()),
            "pending": pending, "completed": completed, "errors": errors,
            "rt_dispatch": "NOT PROVEN BY COMPILE TRACE"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    with args.log.open(errors="replace") as trace:
        report = audit_compile_trace(trace)
    data = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.write_text(data)
    else:
        print(data, end="")
    return 1 if report["errors"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
