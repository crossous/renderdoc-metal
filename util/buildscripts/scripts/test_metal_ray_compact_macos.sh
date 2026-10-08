#!/bin/bash
# Bounded serial update/copy snapshot checks, including the previous ray update gate.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-$REPO_ROOT/build-macos-debug}"
RESULT_DIR="${RENDERDOC_METAL_RESULT_DIR:-$BUILD_DIR/metal-ray-b475}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-$REPO_ROOT/captures/metal-ray-b475}"
cd "$REPO_ROOT"
mkdir -p "$RESULT_DIR" "$CAPTURE_DIR"
run() {
  local name="$1" timeout="$2"
  shift 2
  if ! python3 - "$timeout" "$@" > "$RESULT_DIR/$name.log" 2>&1 <<'PY'
import os, signal, subprocess, sys
p = subprocess.Popen(sys.argv[2:], start_new_session=True)
try:
    code = p.wait(timeout=int(sys.argv[1]))
except subprocess.TimeoutExpired:
    os.killpg(p.pid, signal.SIGKILL)
    p.wait()
    sys.exit(124)
sys.exit(code if code >= 0 else 1)
PY
  then tail -40 "$RESULT_DIR/$name.log" >&2; return 1; fi
  echo "PASS $name"
}
run updates 2400 env RENDERDOC_METAL_RESULT_DIR="$RESULT_DIR/updates" \
  RENDERDOC_METAL_CAPTURE_DIR="$CAPTURE_DIR/updates" \
  bash util/buildscripts/scripts/test_metal_ray_refit_macos.sh
modes=(background-compact background-indexed-compact background-formatted-compact \
  background-tlas-compact background-tlas-indexed-child-compact background-compact-copy \
  background-compact-rebuilt background-refittable-indexed-compact background-indexed-compact-frame \
  background-tlas-indexed-repeated-child-compact background-indexed-compact-copy background-tlas-compact-copy)
for mode in "${modes[@]}"; do
  run "$mode-native" 60 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/updates/basics/capture" "$CAPTURE_DIR/$mode-native" "$mode"
  run "$mode-capture" 60 env MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$BUILD_DIR/lib/librenderdoc.dylib" \
    "$RESULT_DIR/updates/basics/capture" "$CAPTURE_DIR/$mode" "$mode"
  run "$mode-inspection" 180 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/updates/basics/replay" "$CAPTURE_DIR/${mode}_capture.rdc" "$mode"
  run "$mode-cli" 120 "$BUILD_DIR/bin/renderdoccmd" replay --loops 3 "$CAPTURE_DIR/${mode}_capture.rdc"
done
run compact-initial-invalid 600 python3 util/test/metal/metal_ray_compact_initial_invalid.py \
  "$BUILD_DIR/bin/renderdoccmd" "$CAPTURE_DIR/background-compact_capture.rdc" "$CAPTURE_DIR/updates/basics/background_capture.rdc"
files=(); for mode in "${modes[@]}"; do files+=("$CAPTURE_DIR/${mode}_capture.rdc"); done
run lifecycle 240 env MTL_DEBUG_LAYER=1 "$RESULT_DIR/updates/basics/lifecycle" "${files[@]}" 10
run as-compatibility 1200 env RENDERDOC_METAL_CAPTURE_DIR="$REPO_ROOT/captures/metal-smoke" \
  bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh \
  t32 t33 t136 t137 t138 t139 t142 t143 t145 t146 t147 t149 t150 t152 t153 t157 t160 t162 t163 t165 t166 t167 t168 t169 t170 t171 t172 t173 t174 t175 t176 t177 t178 t179 t180 t181 t182 t183 t184 t185 t186 t187 t188 t189 t190 t191 t192 t193 t194 t195 t196 t197 t198 t199 t200 t201 t202 t203 t204 t205 t206 t207 t208 t209 t210 t211 t212 t213 t214 t215 t219 t221 t222 t223 t224 t225 t226 t227 t228 t229 t230 t231 t232 t233 t234 t235 t236 t237 t238 t239 t240 t241 t242 t243 t245 t246 t248 t249 t251 t252 t253 t254 t255 t256 t258 t259 t260 t261 t262 t263 t264 t265 t266 t267 t268 t269 t270 t271 t272 t273 t274 t275 t276 t277 t278 t279 t280 t281 t282 t283 t284 t285 t286 t287 t288 t289 t290 t291 t292 t293 t298
shasum -a 256 "$BUILD_DIR/lib/librenderdoc.dylib" "$BUILD_DIR/bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib" \
  "$CAPTURE_DIR/background-compact_capture.rdc" "$CAPTURE_DIR/background-tlas-indexed-repeated-child-compact_capture.rdc" > "$RESULT_DIR/hashes.log"
echo "PASS compact/version directed gate; logs $RESULT_DIR; not full regression or GUI acceptance"
