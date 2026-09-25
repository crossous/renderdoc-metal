#!/usr/bin/env python3
"""Reject malformed compute sampler bindings."""
import pathlib
import sys
from metal_compute_invalid import check
import tempfile


def main():
    command, capture = map(pathlib.Path, sys.argv[1:3])
    sampler = "MTLComputeCommandEncoder::setSamplerState"
    cases = [
        ("invalid-slot", [(sampler, 0, "index", 16)]),
        ("missing-sampler", [(sampler, 0, "sampler", 999999)]),
    ]
    with tempfile.TemporaryDirectory(prefix="metal-sampler-invalid-") as temp:
        check(command, capture, "t30", cases, pathlib.Path(temp))
    print("T30 invalid compute sampler captures rejected: 2 cases")


if __name__ == "__main__":
    main()
