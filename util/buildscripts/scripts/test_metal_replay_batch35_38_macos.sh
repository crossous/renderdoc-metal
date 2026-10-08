#!/bin/bash
# Verify existing captures without recapturing or opening qrenderdoc.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
CMD="${BUILD_DIR}/bin/renderdoccmd"
LAST_TEST="${RENDERDOC_METAL_LAST_TEST:-37}"
case "$LAST_TEST" in 37|39|41|43|46|47|48|49|50|52|53|56|57|59|60|61|62|63|64|65|66|67|68|69|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|92|94|95|96|97|98|99|100|101|102|103|104|105|106|107|108|109|110|111|112|113|114|115|116|117|118|119|120|121|122|123|124|125|126|127|128|129|130|131|132|135|136|137|138|139|140|141|142|143|144|145|146|147|148|149|150|151|152|153|157|160|162|163|164|165|166|167|168|169|170|171|172|173|174|175|176|177|178|179|180|181|182|183|184|185|186|187|188|189|190|191|192|193|194|195|196|197|198|199|200|201|202|203|204|205|206|207|208|209|210|211|212|213|214|215|216|217|218|219|220|221|222|223|224|225|226|227|228|229|230|231|232|233|234|235|236|237|238|239|240|241|242|243|244|245|246|247|248|249|250|251|252|253|254|255|256|257|258|259|260|261|262|263|264|265|266|267|268|269|270|271|272|273|274|275|276|277|278|279|280|281|282|283|284|285|286|287|288|289|290|291|292|293|294|295|296|297|298|299|300|301|302|303|304|305|306|307|308|309|310|311|312) ;; *) echo "LAST_TEST must be an existing supported batch from 37 through 312 (excluding expected-rejection fixtures)" >&2; exit 2 ;; esac
captures=()
draw_captures=()
for ((number=1; number<=LAST_TEST; number++)); do
  # T70/T133 are expected-rejection captures; T154/T155 require unavailable counters; T158 has no capture.
  if (( number == 70 || number == 133 || number == 154 || number == 155 || number == 158 )); then continue; fi
  printf -v fixture 't%02d' "$number"
  path="${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ ! -f "$path" ]]; then echo "Missing capture: $path" >&2; exit 1; fi
  captures+=("$path")
  if [[ "$number" != 35 ]]; then draw_captures+=("$path"); fi
done
captures+=("${CAPTURE_DIR}/t10_debug_capture.rdc")
draw_captures+=("${CAPTURE_DIR}/t10_debug_capture.rdc")
if [[ ! -f "${CAPTURE_DIR}/t10_debug_capture.rdc" ]]; then
  echo "Run phase38 capture batch first." >&2; exit 1
fi

if [[ "${RENDERDOC_METAL_SKIP_BUILD:-0}" != 1 ]]; then
  "${SCRIPT_DIR}/build_metal_dev_macos.sh"
fi
shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${BUILD_DIR}/start-hash.log"
for test in output lifecycle; do
  clang++ -std=c++17 -arch "$(uname -m)" -mmacosx-version-min=12.0 \
    -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
    "${REPO_ROOT}/util/test/metal/metal_replay_${test}_smoke.mm" \
    -L"${BUILD_DIR}/lib" -lrenderdoc -framework Cocoa -framework QuartzCore -framework Metal \
    -Wl,-rpath,"${BUILD_DIR}/lib" -o "${BUILD_DIR}/metal_replay_${test}_smoke"
done
for capture in "${captures[@]}"; do
  "${BUILD_DIR}/metal_replay_output_smoke" "$capture" \
    "${CAPTURE_DIR}/$(basename "$capture" .rdc)_batch35_38.ppm"
  "${CMD}" replay --loops 1 "$capture"
done
python3 "${REPO_ROOT}/util/test/metal/metal_render_command_creation_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t34_capture.rdc" "${CAPTURE_DIR}/t35_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_render_dynamic_state_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t36_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_blit_transfer_invalid.py" \
  "${CMD}" "${CAPTURE_DIR}/t37_capture.rdc"
invalid_count=71
if (( LAST_TEST >= 39 )); then
  python3 "${REPO_ROOT}/util/test/metal/metal_sampler_private_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t38_capture.rdc" "${CAPTURE_DIR}/t39_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_compute_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t28_capture.rdc" "${CAPTURE_DIR}/t29_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_compute_sampler_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t30_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_compute_batch_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t31_capture.rdc"
  for fixture in t32 t33; do
    python3 "${REPO_ROOT}/util/test/metal/metal_compute_indirect_invalid.py" \
      "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  done
  invalid_count=175
fi
if (( LAST_TEST >= 41 )); then
  # Also validate native Metal calls made by replay itself, including shared display/mesh shaders.
  for fixture in t01 t02 t09 t40 t41; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  done
  python3 "${REPO_ROOT}/util/test/metal/metal_compute_inline_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t40_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_blit_optimization_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t41_capture.rdc"
  invalid_count=239
fi
if (( LAST_TEST >= 43 )); then
  clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
    "${REPO_ROOT}/util/test/metal/metal_barrier_usage_replay.cpp" \
    -L"${BUILD_DIR}/lib" -lrenderdoc -Wl,-rpath,"${BUILD_DIR}/lib" \
    -o "${BUILD_DIR}/metal_barrier_usage_replay"
  for fixture in t42 t43; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_barrier_usage_replay" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc"
  done
  for fixture in t12 t19 t42 t43; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  done
  python3 "${REPO_ROOT}/util/test/metal/metal_resource_barrier_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t42_capture.rdc" "${CAPTURE_DIR}/t43_capture.rdc"
  invalid_count=342
fi
if (( LAST_TEST >= 46 )); then
  for fixture in t44 t45 t46; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  done
  python3 "${REPO_ROOT}/util/test/metal/metal_fence_present_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t44_capture.rdc" "${CAPTURE_DIR}/t45_capture.rdc" \
    "${CAPTURE_DIR}/t46_capture.rdc"
  invalid_count=429
fi
if (( LAST_TEST >= 47 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t47_capture.rdc" "${CAPTURE_DIR}/t47_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_pipeline_variants_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t47_capture.rdc"
  invalid_count=513
fi
if (( LAST_TEST >= 48 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t48_capture.rdc" "${CAPTURE_DIR}/t48_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_binary_library_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t48_capture.rdc"
  invalid_count=577
fi
if (( LAST_TEST >= 49 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t49_capture.rdc" "${CAPTURE_DIR}/t49_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_command_handlers_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t49_capture.rdc"
  invalid_count=664
fi
if (( LAST_TEST >= 50 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t50_capture.rdc" "${CAPTURE_DIR}/t50_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_texture_readback_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t50_capture.rdc"
  invalid_count=840
fi
if (( LAST_TEST >= 52 )); then
  for fixture in t51 t52; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  done
  python3 "${REPO_ROOT}/util/test/metal/metal_async_event_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t51_capture.rdc" "${CAPTURE_DIR}/t52_capture.rdc"
  invalid_count=1066
fi
if (( LAST_TEST >= 53 )); then
  for fixture in t20 t22 t23 t24 t25 t26 t27 t53; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  done
  python3 "${REPO_ROOT}/util/test/metal/metal_icb_operations_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t53_capture.rdc"
  for entry in icb:20 multi_icb:22 indexed_icb:23 icb_reset:24 mixed_icb:25; do
    python3 "${REPO_ROOT}/util/test/metal/metal_${entry%:*}_invalid.py" \
      "${CMD}" "${CAPTURE_DIR}/t${entry#*:}_capture.rdc"
  done
  python3 "${REPO_ROOT}/util/test/metal/metal_icb_inheritance_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t26_capture.rdc" "${CAPTURE_DIR}/t27_capture.rdc" \
    "${CAPTURE_DIR}/t20_capture.rdc"
  # 194 new GPU ICB cases, plus 70 existing ICB negatives; two old empty cases are now positives.
  invalid_count=1330
fi
if (( LAST_TEST >= 56 )); then
  for fixture in t54 t55 t56; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  done
  python3 "${REPO_ROOT}/util/test/metal/metal_overload_compat_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t54_capture.rdc" "${CAPTURE_DIR}/t37_capture.rdc" \
    "${BUILD_DIR}/metal_replay_output_smoke"
  python3 "${REPO_ROOT}/util/test/metal/metal_function_variants_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t55_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_argument_batch_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t56_capture.rdc"
  invalid_count=1478
fi
if (( LAST_TEST >= 57 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t57_capture.rdc" "${CAPTURE_DIR}/t57_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_argument_data_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t57_capture.rdc" "${BUILD_DIR}/metal_replay_output_smoke"
  invalid_count=1639
fi
if (( LAST_TEST >= 59 )); then
  for fixture in t58 t59; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  done
  python3 "${REPO_ROOT}/util/test/metal/metal_texture_views_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t58_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_buffer_texture_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t59_capture.rdc"
  invalid_count=1730
fi
if (( LAST_TEST >= 60 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t60_capture.rdc" "${CAPTURE_DIR}/t60_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_device_argument_encoder_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t60_capture.rdc"
  invalid_count=1759
fi
if (( LAST_TEST >= 61 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t61_capture.rdc" "${CAPTURE_DIR}/t61_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_buffer_no_copy_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t61_capture.rdc"
  invalid_count=1772
fi
if (( LAST_TEST >= 62 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t62_capture.rdc" "${CAPTURE_DIR}/t62_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_purgeable_state_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t62_capture.rdc"
  invalid_count=1784
fi
if (( LAST_TEST >= 63 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t63_capture.rdc" "${CAPTURE_DIR}/t63_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_vertex_dynamic_stride_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t63_capture.rdc"
  invalid_count=1832
fi
if (( LAST_TEST >= 64 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t64_capture.rdc" "${CAPTURE_DIR}/t64_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_shared_texture_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t64_capture.rdc"
  invalid_count=1860
fi
if (( LAST_TEST >= 65 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t65_capture.rdc" "${CAPTURE_DIR}/t65_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_shared_event_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t65_capture.rdc"
  invalid_count=1872
fi
if (( LAST_TEST >= 66 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t66_capture.rdc" "${CAPTURE_DIR}/t66_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_tessellation_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t66_capture.rdc"
  invalid_count=1883
fi
if (( LAST_TEST >= 67 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t67_capture.rdc" "${CAPTURE_DIR}/t67_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_tessellation_variants_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t67_capture.rdc"
  invalid_count=1896
fi
if (( LAST_TEST >= 68 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t68_capture.rdc" "${CAPTURE_DIR}/t68_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_shared_texture_handle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t68_capture.rdc"
  invalid_count=1903
fi
if (( LAST_TEST >= 69 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t69_capture.rdc" "${CAPTURE_DIR}/t69_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_shared_event_handle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t69_capture.rdc"
  invalid_count=1917
fi
if (( LAST_TEST >= 71 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t71_capture.rdc" "${CAPTURE_DIR}/t71_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_heap_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t71_capture.rdc"
  invalid_count=1930
fi
if (( LAST_TEST >= 72 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t72_capture.rdc" "${CAPTURE_DIR}/t72_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_heap_texture_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t72_capture.rdc"
  invalid_count=1947
fi
if (( LAST_TEST >= 73 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t73_capture.rdc" "${CAPTURE_DIR}/t73_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_heap_use_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t73_capture.rdc"
  invalid_count=1969
fi
if (( LAST_TEST >= 74 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t74_capture.rdc" "${CAPTURE_DIR}/t74_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_tile_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t74_capture.rdc"
  invalid_count=1997
fi
if (( LAST_TEST >= 75 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t75_capture.rdc" "${CAPTURE_DIR}/t75_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_tile_sample_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t75_capture.rdc"
  invalid_count=2019
fi
if (( LAST_TEST >= 76 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t76_capture.rdc" "${CAPTURE_DIR}/t76_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_tile_memory_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t76_capture.rdc"
  invalid_count=2027
fi
if (( LAST_TEST >= 77 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t77_capture.rdc" "${CAPTURE_DIR}/t77_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_tile_async_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t77_capture.rdc"
  invalid_count=2035
fi
if (( LAST_TEST >= 78 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t78_capture.rdc" "${CAPTURE_DIR}/t78_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t78_capture.rdc"
  invalid_count=2052
fi
if (( LAST_TEST >= 79 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t79_capture.rdc" "${CAPTURE_DIR}/t79_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_bindings_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t79_capture.rdc"
  invalid_count=2068
fi
if (( LAST_TEST >= 80 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t80_capture.rdc" "${CAPTURE_DIR}/t80_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_resources_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t80_capture.rdc"
  invalid_count=2096
fi
if (( LAST_TEST >= 81 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t81_capture.rdc" "${CAPTURE_DIR}/t81_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_threads_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t81_capture.rdc"
  invalid_count=2104
fi
if (( LAST_TEST >= 82 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t82_capture.rdc" "${CAPTURE_DIR}/t82_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_async_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t82_capture.rdc"
  invalid_count=2112
fi
if (( LAST_TEST >= 83 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t83_capture.rdc" "${CAPTURE_DIR}/t83_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_object_mesh_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t83_capture.rdc"
  invalid_count=2128
fi
if (( LAST_TEST >= 84 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t84_capture.rdc" "${CAPTURE_DIR}/t84_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_object_bindings_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t84_capture.rdc"
  invalid_count=2141
fi
if (( LAST_TEST >= 85 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t85_capture.rdc" "${CAPTURE_DIR}/t85_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_object_resources_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t85_capture.rdc"
  invalid_count=2169
fi
if (( LAST_TEST >= 86 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t86_capture.rdc" "${CAPTURE_DIR}/t86_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_object_memory_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t86_capture.rdc"
  invalid_count=2174
fi
if (( LAST_TEST >= 87 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t87_capture.rdc" "${CAPTURE_DIR}/t87_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_object_async_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t87_capture.rdc"
  invalid_count=2182
fi
if (( LAST_TEST >= 88 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t88_capture.rdc" "${CAPTURE_DIR}/t88_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_indirect_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t88_capture.rdc"
  invalid_count=2193
fi
if (( LAST_TEST >= 89 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t89_capture.rdc" "${CAPTURE_DIR}/t89_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_indirect_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t89_capture.rdc"
  invalid_count=2204
fi
if (( LAST_TEST >= 90 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t90_capture.rdc" "${CAPTURE_DIR}/t90_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_indirect_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t90_capture.rdc"
  invalid_count=2215
fi
if (( LAST_TEST >= 92 )); then
  for fixture in t91 t92; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  done
  python3 "${REPO_ROOT}/util/test/metal/metal_object_threadgroup_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t91_capture.rdc" "${CAPTURE_DIR}/t92_capture.rdc"
  invalid_count=2231
fi
if (( LAST_TEST >= 94 )); then
  for fixture in t93 t94; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  done
  python3 "${REPO_ROOT}/util/test/metal/metal_object_threadgroup_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t93_capture.rdc" "${CAPTURE_DIR}/t94_capture.rdc"
  invalid_count=2247
fi
if (( LAST_TEST >= 95 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t95_capture.rdc" "${CAPTURE_DIR}/t95_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_rate_map_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t95_capture.rdc"
  invalid_count=2265
fi
if (( LAST_TEST >= 96 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t96_capture.rdc" "${CAPTURE_DIR}/t96_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_rate_map_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t96_capture.rdc"
  invalid_count=2283
fi
if (( LAST_TEST >= 97 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t97_capture.rdc" "${CAPTURE_DIR}/t97_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_rate_map_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t97_capture.rdc"
  invalid_count=2305
fi
if (( LAST_TEST >= 98 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t98_capture.rdc" "${CAPTURE_DIR}/t98_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_rate_map_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t98_capture.rdc"
  invalid_count=2331
fi
if (( LAST_TEST >= 99 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t99_capture.rdc" "${CAPTURE_DIR}/t99_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t99_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_rate_map_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t99_capture.rdc"
  invalid_count=2376
fi
if (( LAST_TEST >= 100 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t100_capture.rdc" "${CAPTURE_DIR}/t100_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_async_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t100_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_rate_map_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t100_capture.rdc"
  invalid_count=2412
fi
if (( LAST_TEST >= 101 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t101_capture.rdc" "${CAPTURE_DIR}/t101_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_counter_stage_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t101_capture.rdc"
  invalid_count=2434
fi
if (( LAST_TEST >= 102 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t102_capture.rdc" "${CAPTURE_DIR}/t102_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_binding_encoder_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t102_capture.rdc"
  invalid_count=2447
fi
if (( LAST_TEST >= 103 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t103_capture.rdc" "${CAPTURE_DIR}/t103_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_counter_stage_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t103_capture.rdc"
  invalid_count=2469
fi
if (( LAST_TEST >= 104 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t104_capture.rdc" "${CAPTURE_DIR}/t104_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_library_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t104_capture.rdc"
  invalid_count=2488
fi
if (( LAST_TEST >= 105 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t105_capture.rdc" "${CAPTURE_DIR}/t105_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_multi_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t105_capture.rdc"
  invalid_count=2502
fi
if (( LAST_TEST >= 106 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t106_capture.rdc" "${CAPTURE_DIR}/t106_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_preload_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t106_capture.rdc"
  invalid_count=2508
fi
if (( LAST_TEST >= 107 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t107_capture.rdc" "${CAPTURE_DIR}/t107_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_render_preload_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t107_capture.rdc"
  invalid_count=2513
fi
if (( LAST_TEST >= 108 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t108_capture.rdc" "${CAPTURE_DIR}/t108_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_render_preload_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t108_capture.rdc"
  invalid_count=2518
fi
if (( LAST_TEST >= 109 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t109_capture.rdc" "${CAPTURE_DIR}/t109_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_render_preload_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t109_capture.rdc"
  invalid_count=2523
fi
if (( LAST_TEST >= 110 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t110_capture.rdc" "${CAPTURE_DIR}/t110_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_library_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t110_capture.rdc"
  invalid_count=2542
fi
if (( LAST_TEST >= 111 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t111_capture.rdc" "${CAPTURE_DIR}/t111_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_library_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t111_capture.rdc"
  invalid_count=2561
fi
if (( LAST_TEST >= 112 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t112_capture.rdc" "${CAPTURE_DIR}/t112_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_render_preload_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t112_capture.rdc"
  invalid_count=2566
fi
if (( LAST_TEST >= 113 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t113_capture.rdc" "${CAPTURE_DIR}/t113_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_render_preload_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t113_capture.rdc"
  invalid_count=2571
fi
if (( LAST_TEST >= 114 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t114_capture.rdc" "${CAPTURE_DIR}/t114_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_preload_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t114_capture.rdc"
  invalid_count=2577
fi
if (( LAST_TEST >= 115 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t115_capture.rdc" "${CAPTURE_DIR}/t115_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_render_preload_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t115_capture.rdc"
  invalid_count=2582
fi
if (( LAST_TEST >= 116 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t116_capture.rdc" "${CAPTURE_DIR}/t116_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_heap_placement_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t116_capture.rdc"
  invalid_count=2594
fi
if (( LAST_TEST >= 117 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t117_capture.rdc" "${CAPTURE_DIR}/t117_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_heap_placement_texture_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t117_capture.rdc"
  invalid_count=2607
fi
if (( LAST_TEST >= 118 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t118_capture.rdc" "${CAPTURE_DIR}/t118_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_nested_argument_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t118_capture.rdc"
  invalid_count=2625
fi
if (( LAST_TEST >= 119 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t119_capture.rdc" "${CAPTURE_DIR}/t119_validation.ppm"
fi
if (( LAST_TEST >= 120 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t120_capture.rdc" "${CAPTURE_DIR}/t120_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_visible_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t120_capture.rdc"
  invalid_count=2643
fi
if (( LAST_TEST >= 121 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t121_capture.rdc" "${CAPTURE_DIR}/t121_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_visible_function_table_range_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t121_capture.rdc"
  invalid_count=2648
fi
if (( LAST_TEST >= 122 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t122_capture.rdc" "${CAPTURE_DIR}/t122_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_vertex_visible_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t122_capture.rdc"
  invalid_count=2656
fi
if (( LAST_TEST >= 123 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t123_capture.rdc" "${CAPTURE_DIR}/t123_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_visible_function_table_range_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t123_capture.rdc"
  invalid_count=2661
fi
if (( LAST_TEST >= 124 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t124_capture.rdc" "${CAPTURE_DIR}/t124_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_compute_visible_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t124_capture.rdc"
  invalid_count=2677
fi
if (( LAST_TEST >= 125 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t125_capture.rdc" "${CAPTURE_DIR}/t125_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_visible_function_table_range_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t125_capture.rdc"
  invalid_count=2682
fi
if (( LAST_TEST >= 126 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t126_capture.rdc" "${CAPTURE_DIR}/t126_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_argument_visible_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t126_capture.rdc"
  invalid_count=2689
fi
if (( LAST_TEST >= 127 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t127_capture.rdc" "${CAPTURE_DIR}/t127_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_argument_visible_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t127_capture.rdc"
  invalid_count=2696
fi
if (( LAST_TEST >= 128 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t128_capture.rdc" "${CAPTURE_DIR}/t128_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_tile_visible_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t128_capture.rdc"
  invalid_count=2706
fi
if (( LAST_TEST >= 129 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t129_capture.rdc" "${CAPTURE_DIR}/t129_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_visible_function_table_range_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t129_capture.rdc"
  invalid_count=2711
fi
if (( LAST_TEST >= 130 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t130_capture.rdc" "${CAPTURE_DIR}/t130_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_tile_visible_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t130_capture.rdc"
  invalid_count=2721
fi
if (( LAST_TEST >= 131 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t131_capture.rdc" "${CAPTURE_DIR}/t131_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_heap_alias_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t131_capture.rdc"
  invalid_count=2724
fi
if (( LAST_TEST >= 132 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t132_capture.rdc" "${CAPTURE_DIR}/t132_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_heap_alias_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t132_capture.rdc"
  invalid_count=2727
fi
if (( LAST_TEST >= 134 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t134_capture.rdc" "${CAPTURE_DIR}/t134_validation.ppm"
fi
if (( LAST_TEST >= 135 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t135_capture.rdc" "${CAPTURE_DIR}/t135_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t135_capture.rdc"
  invalid_count=2745
fi
if (( LAST_TEST >= 136 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t136_capture.rdc" "${CAPTURE_DIR}/t136_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t136_capture.rdc"
  invalid_count=2764
fi
if (( LAST_TEST >= 137 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t137_capture.rdc" "${CAPTURE_DIR}/t137_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t137_capture.rdc"
  invalid_count=2782
fi
if (( LAST_TEST >= 138 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t138_capture.rdc" "${CAPTURE_DIR}/t138_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t138_capture.rdc"
  invalid_count=2800
fi
if (( LAST_TEST >= 139 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t139_capture.rdc" "${CAPTURE_DIR}/t139_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t139_capture.rdc"
  invalid_count=2822
fi
if (( LAST_TEST >= 140 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t140_capture.rdc" "${CAPTURE_DIR}/t140_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t140_capture.rdc"
  invalid_count=2854
fi
if (( LAST_TEST >= 141 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t141_capture.rdc" "${CAPTURE_DIR}/t141_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t141_capture.rdc"
  invalid_count=2882
fi
if (( LAST_TEST >= 142 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t142_capture.rdc" "${CAPTURE_DIR}/t142_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t142_capture.rdc"
  invalid_count=2903
fi
if (( LAST_TEST >= 143 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t143_capture.rdc" "${CAPTURE_DIR}/t143_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t143_capture.rdc"
  invalid_count=2926
fi
if (( LAST_TEST >= 144 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t144_capture.rdc" "${CAPTURE_DIR}/t144_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t144_capture.rdc"
  invalid_count=2950
fi
if (( LAST_TEST >= 145 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t145_capture.rdc" "${CAPTURE_DIR}/t145_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t145_capture.rdc"
  invalid_count=2979
fi
if (( LAST_TEST >= 146 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t146_capture.rdc" "${CAPTURE_DIR}/t146_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t146_capture.rdc"
  invalid_count=3008
fi
if (( LAST_TEST >= 147 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t147_capture.rdc" "${CAPTURE_DIR}/t147_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t147_capture.rdc"
  invalid_count=3037
fi
if (( LAST_TEST >= 148 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t148_capture.rdc" "${CAPTURE_DIR}/t148_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t148_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_intersection_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t148_capture.rdc"
  invalid_count=3086
fi
if (( LAST_TEST >= 149 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t149_capture.rdc" "${CAPTURE_DIR}/t149_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t149_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_intersection_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t149_capture.rdc"
  invalid_count=3135
fi
if (( LAST_TEST >= 150 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t150_capture.rdc" "${CAPTURE_DIR}/t150_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t150_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_intersection_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t150_capture.rdc"
  invalid_count=3184
fi
if (( LAST_TEST >= 151 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t151_capture.rdc" "${CAPTURE_DIR}/t151_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t151_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_intersection_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t151_capture.rdc"
  invalid_count=3235
fi
if (( LAST_TEST >= 152 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t152_capture.rdc" "${CAPTURE_DIR}/t152_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t152_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_intersection_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t152_capture.rdc"
  invalid_count=3286
fi
if (( LAST_TEST >= 153 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t153_capture.rdc" "${CAPTURE_DIR}/t153_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t153_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_intersection_function_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t153_capture.rdc"
  invalid_count=3337
fi
if (( LAST_TEST >= 157 )); then
  for fixture in t156 t157; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
    python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
      "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
    python3 "${REPO_ROOT}/util/test/metal/metal_opaque_intersection_invalid.py" \
      "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  done
  invalid_count=3429
fi
if (( LAST_TEST >= 160 )); then
  for fixture in t159 t160; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
    python3 "${REPO_ROOT}/util/test/metal/metal_intersection_buffer_invalid.py" \
      "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  done
  invalid_count=$((invalid_count + 18))
fi
if (( LAST_TEST >= 162 )); then
  for fixture in t161 t162; do
    MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
      "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
    python3 "${REPO_ROOT}/util/test/metal/metal_intersection_visible_invalid.py" \
      "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  done
  invalid_count=$((invalid_count + 16))
fi
if (( LAST_TEST >= 163 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t163_capture.rdc" "${CAPTURE_DIR}/t163_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_intersection_visible_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t163_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_function_table_residency_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t163_capture.rdc"
  invalid_count=$((invalid_count + 14))
fi
if (( LAST_TEST >= 164 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t164_capture.rdc" "${CAPTURE_DIR}/t164_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_as_pass_descriptor_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t164_capture.rdc"
  invalid_count=$((invalid_count + 10))
fi
if (( LAST_TEST >= 165 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t165_capture.rdc" "${CAPTURE_DIR}/t165_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t165_capture.rdc"
  invalid_count=$((invalid_count + 13))
fi
if (( LAST_TEST >= 166 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t166_capture.rdc" "${CAPTURE_DIR}/t166_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_descriptor_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t166_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t166_capture.rdc"
  invalid_count=$((invalid_count + 18))
fi
if (( LAST_TEST >= 167 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t167_capture.rdc" "${CAPTURE_DIR}/t167_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_indexed_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t167_capture.rdc"
  invalid_count=$((invalid_count + 12))
fi
if (( LAST_TEST >= 168 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t168_capture.rdc" "${CAPTURE_DIR}/t168_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_indexed_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t168_capture.rdc"
  invalid_count=$((invalid_count + 12))
fi
if (( LAST_TEST >= 169 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t169_capture.rdc" "${CAPTURE_DIR}/t169_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_descriptor_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t169_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_indexed_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t169_capture.rdc"
  invalid_count=$((invalid_count + 17))
fi
if (( LAST_TEST >= 170 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t170_capture.rdc" "${CAPTURE_DIR}/t170_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_indexed_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t170_capture.rdc"
  invalid_count=$((invalid_count + 12))
fi
if (( LAST_TEST >= 171 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t171_capture.rdc" "${CAPTURE_DIR}/t171_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_descriptor_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t171_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_indexed_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t171_capture.rdc"
  invalid_count=$((invalid_count + 17))
fi
if (( LAST_TEST >= 172 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t172_capture.rdc" "${CAPTURE_DIR}/t172_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_indexed_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t172_capture.rdc"
  invalid_count=$((invalid_count + 12))
fi
if (( LAST_TEST >= 173 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t173_capture.rdc" "${CAPTURE_DIR}/t173_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t173_capture.rdc"
  invalid_count=$((invalid_count + 13))
fi
if (( LAST_TEST >= 174 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t174_capture.rdc" "${CAPTURE_DIR}/t174_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_descriptor_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t174_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t174_capture.rdc"
  invalid_count=$((invalid_count + 18))
fi
if (( LAST_TEST >= 175 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t175_capture.rdc" "${CAPTURE_DIR}/t175_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t175_capture.rdc"
  invalid_count=$((invalid_count + 13))
fi
if (( LAST_TEST >= 176 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t176_capture.rdc" "${CAPTURE_DIR}/t176_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t176_capture.rdc"
  invalid_count=$((invalid_count + 13))
fi
if (( LAST_TEST >= 177 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t177_capture.rdc" "${CAPTURE_DIR}/t177_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_descriptor_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t177_capture.rdc"
  python3 "${REPO_ROOT}/util/test/metal/metal_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t177_capture.rdc"
  invalid_count=$((invalid_count + 18))
fi
if (( LAST_TEST >= 178 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t178_capture.rdc" "${CAPTURE_DIR}/t178_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t178_capture.rdc"
  invalid_count=$((invalid_count + 18))
fi
if (( LAST_TEST >= 179 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t179_capture.rdc" "${CAPTURE_DIR}/t179_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t179_capture.rdc"
  invalid_count=$((invalid_count + 18))
fi
if (( LAST_TEST >= 180 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t180_capture.rdc" "${CAPTURE_DIR}/t180_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t180_capture.rdc"
  invalid_count=$((invalid_count + 18))
fi
if (( LAST_TEST >= 181 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t181_capture.rdc" "${CAPTURE_DIR}/t181_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t181_capture.rdc"
  invalid_count=$((invalid_count + 18))
fi
if (( LAST_TEST >= 182 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t182_capture.rdc" "${CAPTURE_DIR}/t182_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t182_capture.rdc"
  invalid_count=$((invalid_count + 20))
fi
for fixture in t183 t184 t185; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  invalid_count=$((invalid_count + 33))
done
for fixture in t186 t187 t188; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  case "$fixture" in
    t186) invalid_count=$((invalid_count + 20)) ;;
    t187) invalid_count=$((invalid_count + 22)) ;;
    t188) invalid_count=$((invalid_count + 23)) ;;
  esac
done
for fixture in t189 t190 t191; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t191 ]]; then
    invalid_count=$((invalid_count + 33))
  else
    invalid_count=$((invalid_count + 32))
  fi
done
for fixture in t192 t193; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t192 ]]; then
    invalid_count=$((invalid_count + 25))
  else
    invalid_count=$((invalid_count + 29))
  fi
done
for fixture in t194 t195; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t194 ]]; then
    invalid_count=$((invalid_count + 32))
  else
    invalid_count=$((invalid_count + 30))
  fi
done
if (( LAST_TEST >= 196 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t196_capture.rdc" "${CAPTURE_DIR}/t196_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t196_capture.rdc"
  invalid_count=$((invalid_count + 26))
fi
for fixture in t197 t198 t199; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  case "$fixture" in
    t197) invalid_count=$((invalid_count + 29)) ;;
    t198) invalid_count=$((invalid_count + 31)) ;;
    t199) invalid_count=$((invalid_count + 32)) ;;
  esac
done
if (( LAST_TEST >= 200 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t200_capture.rdc" "${CAPTURE_DIR}/t200_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t200_capture.rdc"
  invalid_count=$((invalid_count + 45))
fi
if (( LAST_TEST >= 201 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t201_capture.rdc" "${CAPTURE_DIR}/t201_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t201_capture.rdc"
  invalid_count=$((invalid_count + 47))
fi
for fixture in t202 t203; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t202 ]]; then
    invalid_count=$((invalid_count + 45))
  else
    invalid_count=$((invalid_count + 47))
  fi
done
for fixture in t204 t205 t206 t207 t208; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  case "$fixture" in
    t207) invalid_count=$((invalid_count + 52)) ;;
    t208) invalid_count=$((invalid_count + 34)) ;;
    *) invalid_count=$((invalid_count + 33)) ;;
  esac
done
for fixture in t209 t210 t211 t212; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t211 ]]; then
    invalid_count=$((invalid_count + 23))
  else
    invalid_count=$((invalid_count + 24))
  fi
done
for fixture in t213 t214 t215; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t213 ]]; then
    invalid_count=$((invalid_count + 28))
  else
    invalid_count=$((invalid_count + 29))
  fi
done
for fixture in t216 t217; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_dynamic_library_url_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t216 ]]; then
    invalid_count=$((invalid_count + 11))
  else
    invalid_count=$((invalid_count + 13))
  fi
done
for fixture in t218 t219 t220 t221 t222 t223 t224; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_compute_intersection_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t223 || "$fixture" == t224 ]]; then
    invalid_count=$((invalid_count + 19))
  elif [[ "$fixture" == t220 || "$fixture" == t221 || "$fixture" == t222 ]]; then
    invalid_count=$((invalid_count + 20))
  else
    invalid_count=$((invalid_count + 18))
  fi
done
for fixture in t225 t226; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_indexed_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  invalid_count=$((invalid_count + 15))
done
for fixture in t227 t228 t229 t230; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_indexed_opaque_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  invalid_count=$((invalid_count + 18))
done
for fixture in t231 t232 t233; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_triangle_table_offset_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  invalid_count=$((invalid_count + 17))
done
for fixture in t234 t235 t236; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_compute_intersection_table_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  invalid_count=$((invalid_count + 22))
done
for fixture in t237 t238 t239; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_triangle_table_offset_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  invalid_count=$((invalid_count + 16))
done
for fixture in t240 t241 t242 t243; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_refit_no_duplicate_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  invalid_count=$((invalid_count + 21))
done
for fixture in t244 t245 t246 t247 t248 t249 t250; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_formatted_triangle_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if (( 10#${fixture#t} >= 248 )); then
    invalid_count=$((invalid_count + 25))
  else
    invalid_count=$((invalid_count + 20))
  fi
done
for fixture in t251 t252 t253 t254 t255; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_formatted_refit_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  invalid_count=$((invalid_count + 34))
done
for fixture in t256 t257 t258 t259 t260 t261; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_box_refit_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t260 ]]; then
    invalid_count=$((invalid_count + 48))
  else
    invalid_count=$((invalid_count + 40))
  fi
done
for fixture in t262 t263 t264 t265; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  case "$fixture" in
    t262|t265)
      python3 "${REPO_ROOT}/util/test/metal/metal_acceleration_structure_invalid.py" \
        "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
      if [[ "$fixture" == t262 ]]; then invalid_count=$((invalid_count + 27));
      else invalid_count=$((invalid_count + 33)); fi ;;
    t263)
      python3 "${REPO_ROOT}/util/test/metal/metal_formatted_refit_invalid.py" \
        "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
      invalid_count=$((invalid_count + 34)) ;;
    t264)
      python3 "${REPO_ROOT}/util/test/metal/metal_refit_no_duplicate_invalid.py" \
        "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
      invalid_count=$((invalid_count + 21)) ;;
  esac
done
for fixture in t266 t267 t268 t269 t270 t271 t272 t273 t274 t275 t276 t277 t278 t279 t280 t281 t282 t283 t284 t285 t286 t287 t288 t289; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_indexed_refit_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  invalid_count=$((invalid_count + 50))
done
for fixture in t290 t291 t292 t293; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t293 ]]; then invalid_count=$((invalid_count + 32));
  else invalid_count=$((invalid_count + 23)); fi
done
for fixture in t294 t295 t296 t297; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_multiple_distinct_instance_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t294 || "$fixture" == t296 ]]; then
    invalid_count=$((invalid_count + 17))
  else invalid_count=$((invalid_count + 19)); fi
done
if (( LAST_TEST >= 298 )); then
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/t298_capture.rdc" "${CAPTURE_DIR}/t298_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_instance_acceleration_structure_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t298_capture.rdc"
  invalid_count=$((invalid_count + 23))
fi
for fixture in t299 t300; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  MTL_DEBUG_LAYER=1 "${BUILD_DIR}/metal_replay_output_smoke" \
    "${CAPTURE_DIR}/${fixture}_capture.rdc" "${CAPTURE_DIR}/${fixture}_validation.ppm"
  python3 "${REPO_ROOT}/util/test/metal/metal_multiple_distinct_instance_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  invalid_count=$((invalid_count + 20))
done
if (( LAST_TEST >= 301 )); then
  python3 "${REPO_ROOT}/util/test/metal/metal_binary_archive_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t301_capture.rdc"
  invalid_count=$((invalid_count + 19))
fi
if (( LAST_TEST >= 302 )); then
  python3 "${REPO_ROOT}/util/test/metal/metal_binary_archive_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t302_capture.rdc"
  invalid_count=$((invalid_count + 19))
fi
if (( LAST_TEST >= 303 )); then
  python3 "${REPO_ROOT}/util/test/metal/metal_stitched_library_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t303_capture.rdc"
  invalid_count=$((invalid_count + 17))
fi
for fixture in t305 t306 t307 t308; do
  if (( LAST_TEST < 10#${fixture#t} )); then break; fi
  python3 "${REPO_ROOT}/util/test/metal/metal_binary_archive_mutation_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/${fixture}_capture.rdc"
  if [[ "$fixture" == t308 ]]; then
    invalid_count=$((invalid_count + 44))
  elif [[ "$fixture" == t307 ]]; then
    invalid_count=$((invalid_count + 43))
  else
    invalid_count=$((invalid_count + 30))
  fi
done
if (( LAST_TEST >= 304 )); then
  python3 "${REPO_ROOT}/util/test/metal/metal_stitched_library_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t304_capture.rdc"
  invalid_count=$((invalid_count + 17))
fi
if (( LAST_TEST >= 309 )); then
  python3 "${REPO_ROOT}/util/test/metal/metal_tile_archive_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t309_capture.rdc"
  invalid_count=$((invalid_count + 25))
fi
if (( LAST_TEST >= 310 )); then
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_archive_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t310_capture.rdc"
  invalid_count=$((invalid_count + 34))
fi
if (( LAST_TEST >= 311 )); then
  python3 "${REPO_ROOT}/util/test/metal/metal_tile_archive_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t311_capture.rdc"
  invalid_count=$((invalid_count + 31))
fi
if (( LAST_TEST >= 312 )); then
  python3 "${REPO_ROOT}/util/test/metal/metal_mesh_archive_invalid.py" \
    "${CMD}" "${CAPTURE_DIR}/t312_capture.rdc"
  invalid_count=$((invalid_count + 40))
fi
"${BUILD_DIR}/metal_replay_lifecycle_smoke" "${CAPTURE_DIR}/t35_capture.rdc" \
  "${draw_captures[@]}" 10
# Legacy cumulative constants above predate legal nil AS unbinding. Those ten formerly
# rejected zero-AS mutations are now positive behavior, covered by T183-T185.
if (( LAST_TEST >= 141 )); then invalid_count=$((invalid_count - 1)); fi
if (( LAST_TEST >= 142 )); then invalid_count=$((invalid_count - 1)); fi
if (( LAST_TEST >= 143 )); then invalid_count=$((invalid_count - 1)); fi
if (( LAST_TEST >= 144 )); then invalid_count=$((invalid_count - 1)); fi
if (( LAST_TEST >= 145 )); then invalid_count=$((invalid_count - 2)); fi
if (( LAST_TEST >= 146 )); then invalid_count=$((invalid_count - 2)); fi
if (( LAST_TEST >= 147 )); then invalid_count=$((invalid_count - 2)); fi
# The multi-instance negative suite now also mutates the count beyond the supported cap.
if (( LAST_TEST >= 143 )); then invalid_count=$((invalid_count + 1)); fi
# The T36 post-draw texture-barrier rejection was added after the cumulative ledger above.
invalid_count=$(( invalid_count + 1 ))
# Device argument scalar arrayLength0 is legal; the two former T60 zero-count rejections
# are now covered positively by the official ray sample's native scalar descriptors.
if (( LAST_TEST >= 60 )); then invalid_count=$((invalid_count - 2)); fi
echo "Metal combined replay regression passed: ${#captures[@]} captures, ${invalid_count} malformed cases, $(( ${#captures[@]} * 10 )) lifecycle opens."

shasum -a 256 "${BUILD_DIR}/lib/librenderdoc.dylib" >"${BUILD_DIR}/end-hash.log"
cmp "${BUILD_DIR}/start-hash.log" "${BUILD_DIR}/end-hash.log"
