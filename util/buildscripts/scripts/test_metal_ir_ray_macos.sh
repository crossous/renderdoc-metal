#!/bin/bash
# Serial finite converted-DXR native/capture/replay and corruption gate. No UE launch.
set -euo pipefail
cd "$(dirname "$0")/../../.."
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-$PWD/build-macos-debug}"
RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-$BUILD_DIR/metal-ir-ray}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-$PWD/captures/metal-ir-ray}"
mkdir -p "$RESULT_DIR" "$CAPTURE_DIR"
if [[ "${RENDERDOC_METAL_SKIP_BUILD:-0}" != 1 ]]; then
  cmake --build "$BUILD_DIR" --target renderdoccmd build-qrenderdoc -j 2 >"$RESULT_DIR/build.log" 2>&1
fi
python3 util/test/metal/metal_ir_ray_sample_gate.py --typed --build-dir "$BUILD_DIR" \
  --work-dir "$RESULT_DIR/ir" --capture-dir "$CAPTURE_DIR"
for mode in ue-pad pattern-pad null-hit null-miss ue-null-both ue-any-hit ue-any-hit-no-closest ue-any-hit-null-miss local-root ue-local-root-any-hit ue-local-root-null-hit ue-local-root-null-miss ue-local-root-any-hit-no-closest ue-local-root-six-samplers global-root ue-global-root-any-hit ue-global-root-null-miss ue-global-local-root-six-samplers ue-global-local-root-six-samplers-any-hit descriptor-heaps ue-descriptor-heaps-any-hit ue-descriptor-heaps-null-hit ue-descriptor-heaps-null-miss ue-descriptor-heaps-local-root-six-samplers ue-descriptor-heaps-local-root-six-samplers-any-hit ue-global-descriptor-heaps-local-root-six-samplers descriptor-heaps-heap-as ue-descriptor-heaps-heap-as-any-hit ue-descriptor-heaps-heap-as-null-hit ue-descriptor-heaps-heap-as-null-miss ue-descriptor-heaps-heap-as-local-root-six-samplers ue-descriptor-heaps-heap-as-local-root-six-samplers-any-hit ue-global-descriptor-heaps-heap-as-local-root-six-samplers descriptor-heaps-heap-as-only ue-descriptor-heaps-heap-as-only-local-root-six-samplers ue-indirect-tlas ue-indirect-tlas-empty ue-indirect-tlas-masked ue-indirect-tlas-private ue-indirect-tlas-private-masked ue-descriptor-heaps-heap-as-only-indirect-tlas ue-descriptor-heaps-heap-as-only-indirect-tlas-empty ue-descriptor-heaps-heap-as-only-indirect-tlas-masked ue-descriptor-heaps-heap-as-only-indirect-tlas-private ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private ue-descriptor-heaps-heap-as-only-local-root-six-samplers-indirect-tlas ue-descriptor-heaps-heap-as-only-indirect-tlas-any-hit ue-indirect-tlas-any-hit ue-indirect-tlas-null-hit ue-indirect-tlas-null-miss ue-indirect-tlas-private-frame-build ue-indirect-tlas-private-empty-frame-build ue-indirect-tlas-private-masked-frame-build ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build ue-descriptor-heaps-heap-as-only-indirect-tlas-private-empty-frame-build ue-descriptor-heaps-heap-as-only-local-root-six-samplers-indirect-tlas-private-frame-build ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build ue-indirect-tlas-scratch-offset ue-indirect-tlas-private-scratch-offset ue-indirect-tlas-empty-scratch-offset ue-indirect-tlas-private-masked-scratch-offset ue-indirect-tlas-private-frame-build-scratch-offset ue-indirect-tlas-private-empty-frame-build-scratch-offset ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build-scratch-offset ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build-scratch-offset ue-indirect-tlas-private-frame-build-geometry ue-indirect-tlas-private-frame-build-geometry-indexed ue-indirect-tlas-private-frame-build-geometry-new-target ue-indirect-tlas-private-frame-build-geometry-new-target-indexed ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build-geometry-new-target ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build-geometry-indexed ue-indirect-tlas-private-frame-build-geometry-multi-indexed ue-indirect-tlas-private-frame-build-geometry-new-target-multi-indexed ue-indirect-tlas-private-frame-build-geometry-multi64-indexed ue-indirect-tlas-private-frame-build-geometry-new-target-multi64-indexed ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build-geometry-new-target-multi-indexed ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build-geometry-multi-indexed; do
  python3 util/test/metal/metal_ir_ray_sample_gate.py --typed --mode "$mode" --build-dir "$BUILD_DIR" \
    --work-dir "$RESULT_DIR/$mode" --capture-dir "$CAPTURE_DIR/$mode"
done
python3 util/test/metal/metal_ir_ray_invalid.py "$BUILD_DIR/bin/renderdoccmd" \
  "$CAPTURE_DIR/ir-runtime_capture.rdc" --output "$RESULT_DIR/invalid" --oracle "$RESULT_DIR/ir/replay"
for mode in local-root ue-local-root-six-samplers; do
  python3 util/test/metal/metal_ir_ray_local_invalid.py "$BUILD_DIR/bin/renderdoccmd" \
    "$CAPTURE_DIR/$mode/ir-runtime_capture.rdc" --mode "$mode" \
    --output "$RESULT_DIR/local-invalid-$mode" --oracle "$RESULT_DIR/$mode/replay"
done
for mode in global-root ue-global-local-root-six-samplers; do
  python3 util/test/metal/metal_ir_ray_global_invalid.py "$BUILD_DIR/bin/renderdoccmd" \
    "$CAPTURE_DIR/$mode/ir-runtime_capture.rdc" --mode "$mode" \
    --output "$RESULT_DIR/global-invalid-$mode" --oracle "$RESULT_DIR/$mode/replay"
done
for mode in descriptor-heaps ue-global-descriptor-heaps-local-root-six-samplers; do
  python3 util/test/metal/metal_ir_ray_heap_invalid.py "$BUILD_DIR/bin/renderdoccmd" \
    "$CAPTURE_DIR/$mode/ir-runtime_capture.rdc" --mode "$mode" \
    --output "$RESULT_DIR/heap-invalid-$mode" --oracle "$RESULT_DIR/$mode/replay"
done
for mode in descriptor-heaps-heap-as ue-global-descriptor-heaps-heap-as-local-root-six-samplers descriptor-heaps-heap-as-only; do
  python3 util/test/metal/metal_ir_ray_heap_as_invalid.py "$BUILD_DIR/bin/renderdoccmd" \
    "$CAPTURE_DIR/$mode/ir-runtime_capture.rdc" --mode "$mode" \
    --output "$RESULT_DIR/heap-AS-invalid-$mode" --oracle "$RESULT_DIR/$mode/replay"
done
for mode in ue-indirect-tlas-private ue-indirect-tlas-empty ue-indirect-tlas-private-masked ue-descriptor-heaps-heap-as-only-indirect-tlas-private ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private; do
  python3 util/test/metal/metal_ir_ray_indirect_invalid.py "$BUILD_DIR/bin/renderdoccmd" \
    "$CAPTURE_DIR/$mode/ir-runtime_capture.rdc" --mode "$mode" \
    --output "$RESULT_DIR/indirect-invalid-$mode" --oracle "$RESULT_DIR/$mode/replay"
done
for mode in ue-indirect-tlas-private-frame-build ue-indirect-tlas-private-empty-frame-build ue-descriptor-heaps-heap-as-only-indirect-tlas-private-empty-frame-build ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build; do
  python3 util/test/metal/metal_ir_ray_frame_invalid.py "$BUILD_DIR/bin/renderdoccmd" \
    "$CAPTURE_DIR/$mode/ir-runtime_capture.rdc" --mode "$mode" \
    --output "$RESULT_DIR/frame-invalid-$mode" --oracle "$RESULT_DIR/$mode/replay"
done
for mode in ue-indirect-tlas-private-frame-build-scratch-offset ue-indirect-tlas-private-empty-frame-build-scratch-offset ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build-scratch-offset ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build-scratch-offset; do
  python3 util/test/metal/metal_ir_ray_frame_invalid.py "$BUILD_DIR/bin/renderdoccmd" \
    "$CAPTURE_DIR/$mode/ir-runtime_capture.rdc" --mode "$mode" \
    --output "$RESULT_DIR/frame-offset-invalid-$mode" --oracle "$RESULT_DIR/$mode/replay"
done
for mode in ue-indirect-tlas-private-frame-build-geometry ue-indirect-tlas-private-frame-build-geometry-indexed ue-indirect-tlas-private-frame-build-geometry-new-target ue-indirect-tlas-private-frame-build-geometry-new-target-indexed ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build-geometry-new-target ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build-geometry-indexed ue-indirect-tlas-private-frame-build-geometry-multi-indexed ue-indirect-tlas-private-frame-build-geometry-new-target-multi-indexed ue-indirect-tlas-private-frame-build-geometry-multi64-indexed ue-indirect-tlas-private-frame-build-geometry-new-target-multi64-indexed ue-descriptor-heaps-heap-as-only-indirect-tlas-private-frame-build-geometry-new-target-multi-indexed ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private-frame-build-geometry-multi-indexed; do
  python3 util/test/metal/metal_ir_ray_frame_invalid.py "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/$mode/ir-runtime_capture.rdc" --mode "$mode" --output "$RESULT_DIR/geometry-invalid-$mode" --oracle "$RESULT_DIR/$mode/replay"
done
python3 util/test/metal/metal_initial_list_close_gate.py "$BUILD_DIR/bin/renderdoccmd" \
  "$CAPTURE_DIR/ue-indirect-tlas-private/ir-runtime_capture.rdc" \
  --output "$RESULT_DIR/initial-list-close" --oracle "$RESULT_DIR/ue-indirect-tlas-private/replay"
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. -L"$BUILD_DIR/lib" -lrenderdoc \
  -Wl,-rpath,"$BUILD_DIR/lib" util/test/metal/metal_ray_failed_close_smoke.mm \
  -framework Foundation -framework Metal -framework QuartzCore -o "$RESULT_DIR/failed-close"
"$RESULT_DIR/failed-close" "$CAPTURE_DIR/ue-indirect-tlas-private/ir-runtime_capture.rdc" \
  "$RESULT_DIR/initial-list-close/oversized-list-count.rdc" \
  "$RESULT_DIR/indirect-invalid-ue-indirect-tlas-private/unknown-kind.rdc" \
  "$RESULT_DIR/frame-offset-invalid-ue-indirect-tlas-private-empty-frame-build-scratch-offset/scratch-offset-aligned-overflow.rdc" 10
echo 'PASS seventy-eight converted IR modes and local/global-root/descriptor-heap/heap-AS/indirect-TLAS/frame-AS/scratch-offset/geometry corruption gates, immutable shader roles, nullable shaders/unused pad, allocation-order relocation, events/EID0 and clean malformed rejection. UE/release gate remains separate.'
