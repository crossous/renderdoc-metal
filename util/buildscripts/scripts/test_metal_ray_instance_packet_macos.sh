#!/bin/bash
# BLAS/TLAS argument packets and missing/unbuilt/wrong-kind resource rejection.
set -euo pipefail
cd "$(dirname "$0")/../../.."
export RENDERDOC_METAL_RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-$PWD/build-macos-debug/metal-ray-b500}"
export RENDERDOC_METAL_CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-$PWD/captures/metal-ray-b500}"
export RENDERDOC_METAL_PACKET_TLAS=1
export RENDERDOC_METAL_PACKET_EXTENDED_NEGATIVES=1
bash util/buildscripts/scripts/test_metal_ray_packet_macos.sh
