#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
DEMO_BUILD_DIR="${RENDERDOC_METAL_DEMO_BUILD_DIR:-${REPO_ROOT}/build-metal-demos}"
RENDERDOC_BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"
CAPTURE_DIR="${RENDERDOC_METAL_CAPTURE_DIR:-${REPO_ROOT}/captures/metal-smoke}"
DEMO_BIN="${REPO_ROOT}/bin/demos_x64"
RENDERDOC_LIB="${RENDERDOC_BUILD_DIR}/lib/librenderdoc.dylib"
RENDERDOCCMD="${RENDERDOC_BUILD_DIR}/bin/renderdoccmd"
OUTPUT_SMOKE="${RENDERDOC_BUILD_DIR}/metal_replay_output_smoke"
LIFECYCLE_SMOKE="${RENDERDOC_BUILD_DIR}/metal_replay_lifecycle_smoke"
TARGET_ARCH="$(uname -m)"

cmake -S "${REPO_ROOT}/util/test/demos" -B "${DEMO_BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build "${DEMO_BUILD_DIR}" -j "$(sysctl -n hw.ncpu)"

"${SCRIPT_DIR}/build_metal_dev_macos.sh"

mkdir -p "${CAPTURE_DIR}"
rm -f "${CAPTURE_DIR}/t00_capture.rdc" "${CAPTURE_DIR}/t01_capture.rdc" \
      "${CAPTURE_DIR}/t02_capture.rdc" "${CAPTURE_DIR}/t03_capture.rdc" \
      "${CAPTURE_DIR}/t04_capture.rdc" "${CAPTURE_DIR}/t05_capture.rdc" \
      "${CAPTURE_DIR}/t06_capture.rdc" "${CAPTURE_DIR}/t07_capture.rdc" \
      "${CAPTURE_DIR}/t08_capture.rdc" "${CAPTURE_DIR}/t09_capture.rdc" \
      "${CAPTURE_DIR}/t10_capture.rdc" "${CAPTURE_DIR}/t11_capture.rdc" \
      "${CAPTURE_DIR}/t12_capture.rdc" "${CAPTURE_DIR}/t13_capture.rdc" \
      "${CAPTURE_DIR}/t14_capture.rdc" "${CAPTURE_DIR}/t15_capture.rdc" \
      "${CAPTURE_DIR}/t16_capture.rdc" "${CAPTURE_DIR}/t17_capture.rdc" \
      "${CAPTURE_DIR}/t18_capture.rdc" "${CAPTURE_DIR}/t19_capture.rdc" \
      "${CAPTURE_DIR}/t20_capture.rdc" "${CAPTURE_DIR}/t21_capture.rdc" \
      "${CAPTURE_DIR}/t22_capture.rdc" "${CAPTURE_DIR}/t23_capture.rdc" \
      "${CAPTURE_DIR}/t24_capture.rdc" "${CAPTURE_DIR}/t25_capture.rdc" \
      "${CAPTURE_DIR}/t26_capture.rdc" "${CAPTURE_DIR}/t27_capture.rdc" \
      "${CAPTURE_DIR}/t28_capture.rdc" "${CAPTURE_DIR}/t29_capture.rdc" \
      "${CAPTURE_DIR}/t00.xml" "${CAPTURE_DIR}/t01.xml" "${CAPTURE_DIR}/t02.xml" \
      "${CAPTURE_DIR}/t03.xml" "${CAPTURE_DIR}/t04.xml" "${CAPTURE_DIR}/t05.xml" \
      "${CAPTURE_DIR}/t06.xml" "${CAPTURE_DIR}/t07.xml" "${CAPTURE_DIR}/t08.xml" \
      "${CAPTURE_DIR}/t09.xml" \
      "${CAPTURE_DIR}/t10.xml" "${CAPTURE_DIR}/t11.xml" "${CAPTURE_DIR}/t12.xml" \
      "${CAPTURE_DIR}/t13.xml" "${CAPTURE_DIR}/t14.xml" "${CAPTURE_DIR}/t15.xml" \
      "${CAPTURE_DIR}/t16.xml" "${CAPTURE_DIR}/t17.xml" "${CAPTURE_DIR}/t18.xml" \
      "${CAPTURE_DIR}/t19.xml" "${CAPTURE_DIR}/t20.xml" "${CAPTURE_DIR}/t21.xml" \
      "${CAPTURE_DIR}/t22.xml" "${CAPTURE_DIR}/t23.xml" \
      "${CAPTURE_DIR}/t24.xml" "${CAPTURE_DIR}/t25.xml" \
      "${CAPTURE_DIR}/t26.xml" "${CAPTURE_DIR}/t27.xml" \
      "${CAPTURE_DIR}/t28.xml" "${CAPTURE_DIR}/t29.xml" \
      "${CAPTURE_DIR}/t01_event_clear.ppm" "${CAPTURE_DIR}/t01_event_draw.ppm" \
      "${CAPTURE_DIR}/t01_event_rewind.ppm" "${CAPTURE_DIR}/t01_texture.dds" \
      "${CAPTURE_DIR}/t02_replay.ppm" "${CAPTURE_DIR}/t03_replay.ppm" \
      "${CAPTURE_DIR}/t04_replay.ppm" "${CAPTURE_DIR}/t05_replay.ppm" \
      "${CAPTURE_DIR}/t06_replay.ppm" "${CAPTURE_DIR}/t07_replay.ppm" \
      "${CAPTURE_DIR}/t08_replay.ppm" "${CAPTURE_DIR}/t09_replay.ppm" \
      "${CAPTURE_DIR}/t10_replay.ppm" "${CAPTURE_DIR}/t11_replay.ppm" \
      "${CAPTURE_DIR}/t12_replay.ppm" "${CAPTURE_DIR}/t13_replay.ppm" \
      "${CAPTURE_DIR}/t14_replay.ppm" "${CAPTURE_DIR}/t15_replay.ppm" \
      "${CAPTURE_DIR}/t16_replay.ppm" "${CAPTURE_DIR}/t17_replay.ppm" \
      "${CAPTURE_DIR}/t18_replay.ppm" "${CAPTURE_DIR}/t19_replay.ppm" \
      "${CAPTURE_DIR}/t20_replay.ppm" "${CAPTURE_DIR}/t21_replay.ppm" \
      "${CAPTURE_DIR}/t22_replay.ppm" "${CAPTURE_DIR}/t23_replay.ppm" \
      "${CAPTURE_DIR}/t24_replay.ppm" "${CAPTURE_DIR}/t25_replay.ppm" \
      "${CAPTURE_DIR}/t26_replay.ppm" "${CAPTURE_DIR}/t27_replay.ppm" \
      "${CAPTURE_DIR}/t28_replay.ppm" "${CAPTURE_DIR}/t29_replay.ppm" \
      "${CAPTURE_DIR}/t09_cube.dds" "${CAPTURE_DIR}/t10_mips.dds" \
      "${CAPTURE_DIR}/t11_filtered.dds" "${CAPTURE_DIR}/t12_argument_texture.dds" \
      "${CAPTURE_DIR}/t13_arguments.bin" "${CAPTURE_DIR}/t14_indices.bin" \
      "${CAPTURE_DIR}/t15_output.dds" "${CAPTURE_DIR}/t16_output.dds" \
      "${CAPTURE_DIR}/t17_output.dds" "${CAPTURE_DIR}/t18_storage.bin" \
      "${CAPTURE_DIR}/t19_storage.bin" "${CAPTURE_DIR}/t20_vertices.bin" \
      "${CAPTURE_DIR}/t21_arguments.bin" "${CAPTURE_DIR}/t22_packets.bin" \
      "${CAPTURE_DIR}/t23_indices.bin" "${CAPTURE_DIR}/t24_packets.bin" \
      "${CAPTURE_DIR}/t25_resources.bin" "${CAPTURE_DIR}/t26_vertices.bin" \
      "${CAPTURE_DIR}/t27_packets.bin" \
      "${CAPTURE_DIR}/t29_output.bin"

"${DEMO_BIN}" Metal_Empty_Frame --frames 5
"${DEMO_BIN}" Metal_Simple_Triangle --frames 5
"${DEMO_BIN}" Metal_Indexed_Cube --frames 5
"${DEMO_BIN}" Metal_Textured_Quad --frames 5
"${DEMO_BIN}" Metal_Dynamic_Uniform --frames 5
"${DEMO_BIN}" Metal_Instanced_Mesh --frames 5
"${DEMO_BIN}" Metal_MRT_Blend --frames 5
"${DEMO_BIN}" Metal_Depth_Stencil --frames 5
"${DEMO_BIN}" Metal_MSAA_Resolve --frames 5
"${DEMO_BIN}" Metal_Texture_Subresources --frames 5
"${DEMO_BIN}" Metal_Blit_Operations --frames 5
"${DEMO_BIN}" Metal_Compute_Texture_Filter --frames 5
"${DEMO_BIN}" Metal_Argument_Buffer --frames 5
"${DEMO_BIN}" Metal_Indirect_Draw --frames 5
"${DEMO_BIN}" Metal_Indexed_Instancing --frames 5
"${DEMO_BIN}" Metal_Point_Line --frames 5
"${DEMO_BIN}" Metal_Vertex_Texture --frames 5
"${DEMO_BIN}" Metal_Batch_Texture --frames 5
"${DEMO_BIN}" Metal_Fragment_Storage_Buffer --frames 5
"${DEMO_BIN}" Metal_Vertex_Storage_Buffer --frames 5
"${DEMO_BIN}" Metal_Indirect_Command_Buffer --frames 5
"${DEMO_BIN}" Metal_Indexed_Indirect_Draw --frames 5
"${DEMO_BIN}" Metal_Multi_Command_ICB --frames 5
"${DEMO_BIN}" Metal_Indexed_ICB --frames 5
"${DEMO_BIN}" Metal_ICB_Reset_Reencode --frames 5
"${DEMO_BIN}" Metal_Mixed_ICB --frames 5
"${DEMO_BIN}" Metal_ICB_Inherit_Pipeline --frames 5
"${DEMO_BIN}" Metal_ICB_Inherit_Buffers --frames 5
"${DEMO_BIN}" Metal_Compute_Dispatch_Threads --frames 5
"${DEMO_BIN}" Metal_Compute_Buffer_Binding --frames 5

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t00" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Empty_Frame --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t01" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Simple_Triangle --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t02" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Indexed_Cube --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t03" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Textured_Quad --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t04" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Dynamic_Uniform --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t05" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Instanced_Mesh --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t06" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_MRT_Blend --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t07" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Depth_Stencil --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t08" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_MSAA_Resolve --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t09" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Texture_Subresources --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t10" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Blit_Operations --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t11" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Compute_Texture_Filter --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t12" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Argument_Buffer --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t13" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Indirect_Draw --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t14" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Indexed_Instancing --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t15" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Point_Line --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t16" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Vertex_Texture --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t17" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Batch_Texture --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t18" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Fragment_Storage_Buffer --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t19" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Vertex_Storage_Buffer --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t20" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Indirect_Command_Buffer --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t21" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Indexed_Indirect_Draw --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t22" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Multi_Command_ICB --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t23" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Indexed_ICB --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t24" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_ICB_Reset_Reencode --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t25" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_Mixed_ICB --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t26" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_ICB_Inherit_Pipeline --frames 8

RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t27" \
DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
  "${DEMO_BIN}" Metal_ICB_Inherit_Buffers --frames 8

for spec in '28 Metal_Compute_Dispatch_Threads' '29 Metal_Compute_Buffer_Binding'; do
  read -r index name <<< "$spec"
  RENDERDOC_METAL_CAPTURE_PATH="${CAPTURE_DIR}/t${index}" \
  DYLD_INSERT_LIBRARIES="${RENDERDOC_LIB}" \
    "${DEMO_BIN}" "$name" --frames 8
done

test -s "${CAPTURE_DIR}/t00_capture.rdc"
test -s "${CAPTURE_DIR}/t01_capture.rdc"
test -s "${CAPTURE_DIR}/t02_capture.rdc"
test -s "${CAPTURE_DIR}/t03_capture.rdc"
test -s "${CAPTURE_DIR}/t04_capture.rdc"
test -s "${CAPTURE_DIR}/t05_capture.rdc"
test -s "${CAPTURE_DIR}/t06_capture.rdc"
test -s "${CAPTURE_DIR}/t07_capture.rdc"
test -s "${CAPTURE_DIR}/t08_capture.rdc"
test -s "${CAPTURE_DIR}/t09_capture.rdc"
test -s "${CAPTURE_DIR}/t10_capture.rdc"
test -s "${CAPTURE_DIR}/t11_capture.rdc"
test -s "${CAPTURE_DIR}/t12_capture.rdc"
test -s "${CAPTURE_DIR}/t13_capture.rdc"
test -s "${CAPTURE_DIR}/t14_capture.rdc"
test -s "${CAPTURE_DIR}/t15_capture.rdc"
test -s "${CAPTURE_DIR}/t16_capture.rdc"
test -s "${CAPTURE_DIR}/t17_capture.rdc"
test -s "${CAPTURE_DIR}/t18_capture.rdc"
test -s "${CAPTURE_DIR}/t19_capture.rdc"
test -s "${CAPTURE_DIR}/t20_capture.rdc"
test -s "${CAPTURE_DIR}/t21_capture.rdc"
test -s "${CAPTURE_DIR}/t22_capture.rdc"
test -s "${CAPTURE_DIR}/t23_capture.rdc"
test -s "${CAPTURE_DIR}/t24_capture.rdc"
test -s "${CAPTURE_DIR}/t25_capture.rdc"
test -s "${CAPTURE_DIR}/t26_capture.rdc"
test -s "${CAPTURE_DIR}/t27_capture.rdc"
test -s "${CAPTURE_DIR}/t28_capture.rdc"
test -s "${CAPTURE_DIR}/t29_capture.rdc"

"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t00_capture.rdc" \
  -o "${CAPTURE_DIR}/t00.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t01_capture.rdc" \
  -o "${CAPTURE_DIR}/t01.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t02_capture.rdc" \
  -o "${CAPTURE_DIR}/t02.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t03_capture.rdc" \
  -o "${CAPTURE_DIR}/t03.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t04_capture.rdc" \
  -o "${CAPTURE_DIR}/t04.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t05_capture.rdc" \
  -o "${CAPTURE_DIR}/t05.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t06_capture.rdc" \
  -o "${CAPTURE_DIR}/t06.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t07_capture.rdc" \
  -o "${CAPTURE_DIR}/t07.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t08_capture.rdc" \
  -o "${CAPTURE_DIR}/t08.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t09_capture.rdc" \
  -o "${CAPTURE_DIR}/t09.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t10_capture.rdc" \
  -o "${CAPTURE_DIR}/t10.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t11_capture.rdc" \
  -o "${CAPTURE_DIR}/t11.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t12_capture.rdc" \
  -o "${CAPTURE_DIR}/t12.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t13_capture.rdc" \
  -o "${CAPTURE_DIR}/t13.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t14_capture.rdc" \
  -o "${CAPTURE_DIR}/t14.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t15_capture.rdc" \
  -o "${CAPTURE_DIR}/t15.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t16_capture.rdc" \
  -o "${CAPTURE_DIR}/t16.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t17_capture.rdc" \
  -o "${CAPTURE_DIR}/t17.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t18_capture.rdc" \
  -o "${CAPTURE_DIR}/t18.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t19_capture.rdc" \
  -o "${CAPTURE_DIR}/t19.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t20_capture.rdc" \
  -o "${CAPTURE_DIR}/t20.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t21_capture.rdc" \
  -o "${CAPTURE_DIR}/t21.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t22_capture.rdc" \
  -o "${CAPTURE_DIR}/t22.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t23_capture.rdc" \
  -o "${CAPTURE_DIR}/t23.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t24_capture.rdc" \
  -o "${CAPTURE_DIR}/t24.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t25_capture.rdc" \
  -o "${CAPTURE_DIR}/t25.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t26_capture.rdc" \
  -o "${CAPTURE_DIR}/t26.xml" -c xml
"${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t27_capture.rdc" \
  -o "${CAPTURE_DIR}/t27.xml" -c xml

rg -q 'driver id="11">Metal<' "${CAPTURE_DIR}/t00.xml"
rg -q 'name="MTLCommandBuffer::presentDrawable"' "${CAPTURE_DIR}/t00.xml"
rg -q 'name="MTLDevice::newLibraryWithSource"' "${CAPTURE_DIR}/t01.xml"
rg -q 'name="FunctionName" typename="NSString" important="true">vs_main<' \
  "${CAPTURE_DIR}/t01.xml"
rg -q 'name="FunctionName" typename="NSString" important="true">fs_main<' \
  "${CAPTURE_DIR}/t01.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="96"' \
  "${CAPTURE_DIR}/t01.xml"
rg -q 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t01.xml"
rg -q 'name="vertexCount" typename="uint64_t" width="8" important="true">3<' \
  "${CAPTURE_DIR}/t01.xml"
rg -q 'name="stride" typename="uint64_t" width="8">28<' "${CAPTURE_DIR}/t02.xml"
rg -q 'string="MTLVertexFormatFloat3">30<' "${CAPTURE_DIR}/t02.xml"
rg -q 'string="MTLVertexFormatFloat4">31<' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="depthAttachmentPixelFormat".*string="MTLPixelFormatDepth32Float">252<' \
  "${CAPTURE_DIR}/t02.xml"
rg -q 'name="depthCompareFunction".*string="MTLCompareFunctionLess">1<' \
  "${CAPTURE_DIR}/t02.xml"
rg -q 'name="depthWriteEnabled" typename="bool">true<' "${CAPTURE_DIR}/t02.xml"
rg -q 'string="MTLWindingCounterClockwise">1<' "${CAPTURE_DIR}/t02.xml"
rg -q 'string="MTLCullModeBack">2<' "${CAPTURE_DIR}/t02.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::setScissorRect"' "${CAPTURE_DIR}/t02.xml")" = "2"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawIndexedPrimitives"' "${CAPTURE_DIR}/t02.xml")" = "2"
test "$(rg -c 'name="indexCount" typename="uint64_t" width="8" important="true">36<' \
  "${CAPTURE_DIR}/t02.xml")" = "2"
rg -q 'name="indexType".*string="MTLIndexTypeUInt16">0<' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="indexType".*string="MTLIndexTypeUInt32">1<' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="224"' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="72"' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="144"' "${CAPTURE_DIR}/t02.xml"
rg -q 'name="MTLDevice::newTextureWithDescriptor"' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="pixelFormat".*string="MTLPixelFormatRGBA8Unorm">70<' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="width" typename="uint64_t" width="8">4<' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="height" typename="uint64_t" width="8">4<' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="MTLTexture::replaceRegion"' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="contents" typename="Byte Buffer" important="true" byteLength="64"' \
  "${CAPTURE_DIR}/t03.xml"
rg -q 'name="MTLDevice::newSamplerStateWithDescriptor"' "${CAPTURE_DIR}/t03.xml"
test "$(rg -c 'string="MTLSamplerMinMagFilterNearest">0<' "${CAPTURE_DIR}/t03.xml")" = "2"
test "$(rg -c 'string="MTLSamplerAddressModeClampToEdge">0<' "${CAPTURE_DIR}/t03.xml")" = "3"
rg -q 'name="MTLRenderCommandEncoder::setFragmentTexture"' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="texture" typename="MTLTexture" width="8" important="true">17<' \
  "${CAPTURE_DIR}/t03.xml"
rg -q 'name="MTLRenderCommandEncoder::setFragmentSamplerState"' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="sampler" typename="MTLSamplerState" width="8" important="true">18<' \
  "${CAPTURE_DIR}/t03.xml"
rg -q 'string="MTLPrimitiveTypeTriangleStrip">4<' "${CAPTURE_DIR}/t03.xml"
rg -q 'name="vertexCount" typename="uint64_t" width="8" important="true">4<' \
  "${CAPTURE_DIR}/t03.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="512"' \
  "${CAPTURE_DIR}/t04.xml"
rg -q 'name="MTLRenderCommandEncoder::setFragmentBuffer"' "${CAPTURE_DIR}/t04.xml"
rg -q 'name="MTLRenderCommandEncoder::setFragmentBufferOffset"' "${CAPTURE_DIR}/t04.xml"
rg -q 'name="offset" typename="uint64_t" width="8">256<' "${CAPTURE_DIR}/t04.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t04.xml")" = "2"
rg -q 'name="stride" typename="uint64_t" width="8">8<' "${CAPTURE_DIR}/t05.xml"
rg -q 'name="stride" typename="uint64_t" width="8">24<' "${CAPTURE_DIR}/t05.xml"
rg -q 'string="MTLVertexStepFunctionPerInstance">2<' "${CAPTURE_DIR}/t05.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="24"' \
  "${CAPTURE_DIR}/t05.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="96"' \
  "${CAPTURE_DIR}/t05.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::setVertexBuffer"' "${CAPTURE_DIR}/t05.xml")" = "2"
rg -q 'name="instanceCount" typename="uint64_t" width="8">3<' "${CAPTURE_DIR}/t05.xml"
rg -q 'name="baseInstance" typename="uint64_t" width="8">1<' "${CAPTURE_DIR}/t05.xml"
rg -q 'name="pixelFormat".*string="MTLPixelFormatBGRA8Unorm">80<' \
  "${CAPTURE_DIR}/t06.xml"
rg -q 'name="pixelFormat".*string="MTLPixelFormatRGBA8Unorm">70<' \
  "${CAPTURE_DIR}/t06.xml"
rg -q 'name="sourceRGBBlendFactor".*string="MTLBlendFactorSourceAlpha">4<' \
  "${CAPTURE_DIR}/t06.xml"
rg -q 'name="destinationRGBBlendFactor".*string="MTLBlendFactorOneMinusSourceAlpha">5<' \
  "${CAPTURE_DIR}/t06.xml"
rg -q 'name="writeMask".*string="MTLColorWriteMaskAll">15<' "${CAPTURE_DIR}/t06.xml"
rg -q 'name="writeMask".*MTLColorWriteMaskBlue.*MTLColorWriteMaskGreen.*MTLColorWriteMaskRed">14<' \
  "${CAPTURE_DIR}/t06.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="240"' \
  "${CAPTURE_DIR}/t06.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t06.xml")" = "2"
rg -q 'name="depthAttachmentPixelFormat".*string="MTLPixelFormatDepth32Float_Stencil8">260<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="stencilAttachmentPixelFormat".*string="MTLPixelFormatDepth32Float_Stencil8">260<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="stencilFailureOperation".*string="MTLStencilOperationZero">1<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="depthFailureOperation".*string="MTLStencilOperationIncrementClamp">3<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="depthStencilPassOperation".*string="MTLStencilOperationReplace">2<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="readMask" typename="uint32_t" width="4">63<' "${CAPTURE_DIR}/t07.xml"
rg -q 'name="writeMask" typename="uint32_t" width="4">255<' "${CAPTURE_DIR}/t07.xml"
rg -q 'name="MTLRenderCommandEncoder::setStencilFrontReferenceValue"' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="frontReferenceValue" typename="uint32_t" width="4" important="true">9<' \
  "${CAPTURE_DIR}/t07.xml"
rg -q 'name="backReferenceValue" typename="uint32_t" width="4" important="true">5<' \
  "${CAPTURE_DIR}/t07.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::setStencilReferenceValue"' \
  "${CAPTURE_DIR}/t07.xml")" = "5"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t07.xml")" = "5"
rg -q 'name="textureType".*string="MTLTextureType2DMultisample">4<' \
  "${CAPTURE_DIR}/t08.xml"
rg -q 'name="sampleCount" typename="uint64_t" width="8">4<' "${CAPTURE_DIR}/t08.xml"
rg -q 'name="rasterSampleCount" typename="uint64_t" width="8">4<' \
  "${CAPTURE_DIR}/t08.xml"
rg -q 'name="resolveTexture" typename="MTLTexture" width="8">[1-9][0-9]*<' \
  "${CAPTURE_DIR}/t08.xml"
rg -q 'name="storeAction".*string="MTLStoreActionMultisampleResolve">2<' \
  "${CAPTURE_DIR}/t08.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="216"' \
  "${CAPTURE_DIR}/t08.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t08.xml")" = "3"
rg -q 'name="textureType".*string="MTLTextureType2D">2<' "${CAPTURE_DIR}/t09.xml"
rg -q 'name="textureType".*string="MTLTextureType2DArray">3<' "${CAPTURE_DIR}/t09.xml"
rg -q 'name="textureType".*string="MTLTextureTypeCube">5<' "${CAPTURE_DIR}/t09.xml"
rg -q 'name="mipmapLevelCount" typename="uint64_t" width="8">3<' \
  "${CAPTURE_DIR}/t09.xml"
rg -q 'name="arrayLength" typename="uint64_t" width="8">3<' "${CAPTURE_DIR}/t09.xml"
test "$(rg -c 'name="MTLTexture::replaceRegion"' "${CAPTURE_DIR}/t09.xml")" = "12"
test "$(rg -c 'name="slice" typename="uint64_t" width="8" important="true"' \
  "${CAPTURE_DIR}/t09.xml")" = "9"
test "$(rg -c 'name="MTLRenderCommandEncoder::setFragmentTexture"' \
  "${CAPTURE_DIR}/t09.xml")" = "3"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t09.xml")" = "1"
rg -q 'name="MTLCommandBuffer::blitCommandEncoder"' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="MTLBlitCommandEncoder::copyFromBuffer"' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="sourceOffset" typename="uint64_t" width="8">8<' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="destinationOffset" typename="uint64_t" width="8">0<' \
  "${CAPTURE_DIR}/t10.xml"
rg -q 'name="size" typename="uint64_t" width="8">32<' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="MTLBlitCommandEncoder::fillBuffer"' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="location" typename="uint64_t" width="8">16<' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="length" typename="uint64_t" width="8">16<' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="value" typename="uint8_t" width="1" important="true">96<' \
  "${CAPTURE_DIR}/t10.xml"
rg -q 'name="MTLBlitCommandEncoder::copyFromTexture"' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="sourceSize" typename="MTLSize"' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="width" typename="uint64_t" width="8">8<' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="height" typename="uint64_t" width="8">8<' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="MTLBlitCommandEncoder::generateMipmapsForTexture"' \
  "${CAPTURE_DIR}/t10.xml"
rg -q 'name="MTLBlitCommandEncoder::endEncoding"' "${CAPTURE_DIR}/t10.xml"
rg -q 'name="MTLDevice::newComputePipelineStateWithFunction"' "${CAPTURE_DIR}/t11.xml"
rg -q 'name="FunctionName" typename="NSString" important="true">filter_main<' \
  "${CAPTURE_DIR}/t11.xml"
rg -q 'name="MTLCommandBuffer::computeCommandEncoder"' "${CAPTURE_DIR}/t11.xml"
rg -q 'name="MTLComputeCommandEncoder::setComputePipelineState"' "${CAPTURE_DIR}/t11.xml"
test "$(rg -c 'name="MTLComputeCommandEncoder::setTexture"' "${CAPTURE_DIR}/t11.xml")" = "2"
rg -q 'name="MTLComputeCommandEncoder::dispatchThreadgroups"' "${CAPTURE_DIR}/t11.xml"
rg -q 'name="MTLComputeCommandEncoder::endEncoding"' "${CAPTURE_DIR}/t11.xml"
rg -q 'name="contents" typename="Byte Buffer" important="true" byteLength="256"' \
  "${CAPTURE_DIR}/t11.xml"
rg -q 'name="MTLFunction::newArgumentEncoderWithBufferIndex"' "${CAPTURE_DIR}/t12.xml"
rg -q 'name="bufferIndex" typename="uint64_t" width="8" important="true">0<' \
  "${CAPTURE_DIR}/t12.xml"
rg -q 'name="MTLArgumentEncoder::setArgumentBuffer"' "${CAPTURE_DIR}/t12.xml"
rg -q 'name="MTLArgumentEncoder::setTexture"' "${CAPTURE_DIR}/t12.xml"
rg -q 'name="index" typename="uint64_t" width="8" important="true">0<' \
  "${CAPTURE_DIR}/t12.xml"
rg -q 'name="MTLArgumentEncoder::setSamplerState"' "${CAPTURE_DIR}/t12.xml"
rg -q 'name="index" typename="uint64_t" width="8" important="true">1<' \
  "${CAPTURE_DIR}/t12.xml"
rg -q 'name="MTLRenderCommandEncoder::setFragmentBuffer"' "${CAPTURE_DIR}/t12.xml"
rg -q 'name="MTLRenderCommandEncoder::useResource"' "${CAPTURE_DIR}/t12.xml"
rg -q 'name="MTLRenderCommandEncoder::drawPrimitives\(indirect\)"' "${CAPTURE_DIR}/t13.xml"
rg -q 'name="indirectBuffer" typename="MTLBuffer" width="8" important="true">[1-9][0-9]*<' \
  "${CAPTURE_DIR}/t13.xml"
rg -q 'name="indirectBufferOffset" typename="uint64_t" width="8" important="true">16<' \
  "${CAPTURE_DIR}/t13.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="48"' \
  "${CAPTURE_DIR}/t13.xml"
rg -q 'name="MTLRenderCommandEncoder::drawIndexedPrimitives"' "${CAPTURE_DIR}/t14.xml"
rg -q 'name="indexCount" typename="uint64_t" width="8" important="true">3<' \
  "${CAPTURE_DIR}/t14.xml"
rg -q 'name="indexBufferOffset" typename="uint64_t" width="8" important="true">4<' \
  "${CAPTURE_DIR}/t14.xml"
rg -q 'name="instanceCount" typename="uint64_t" width="8" important="true">2<' \
  "${CAPTURE_DIR}/t14.xml"
rg -q 'name="baseVertex" typename="int64_t" width="8" important="true">1<' \
  "${CAPTURE_DIR}/t14.xml"
rg -q 'name="baseInstance" typename="uint64_t" width="8" important="true">1<' \
  "${CAPTURE_DIR}/t14.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="12"' \
  "${CAPTURE_DIR}/t14.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::drawPrimitives"' "${CAPTURE_DIR}/t15.xml")" = "3"
rg -q 'name="primitiveType".*string="MTLPrimitiveTypePoint">0<' "${CAPTURE_DIR}/t15.xml"
rg -q 'name="primitiveType".*string="MTLPrimitiveTypeLine">1<' "${CAPTURE_DIR}/t15.xml"
rg -q 'name="primitiveType".*string="MTLPrimitiveTypeLineStrip">2<' "${CAPTURE_DIR}/t15.xml"
for start in 1 3 6; do
  rg -q "name=\"vertexStart\" typename=\"uint64_t\" width=\"8\">${start}<" \
    "${CAPTURE_DIR}/t15.xml"
done
rg -q 'name="initialData" typename="Byte Buffer" byteLength="264"' "${CAPTURE_DIR}/t15.xml"
rg -q 'name="MTLRenderCommandEncoder::setVertexTexture"' "${CAPTURE_DIR}/t16.xml"
rg -q 'name="MTLRenderCommandEncoder::setVertexSamplerState"' "${CAPTURE_DIR}/t16.xml"
rg -q 'name="texture" typename="MTLTexture" width="8" important="true">[1-9][0-9]*<' \
  "${CAPTURE_DIR}/t16.xml"
rg -q 'name="sampler" typename="MTLSamplerState" width="8" important="true">[1-9][0-9]*<' \
  "${CAPTURE_DIR}/t16.xml"
rg -q 'name="vertexCount" typename="uint64_t" width="8" important="true">24<' \
  "${CAPTURE_DIR}/t16.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="384"' "${CAPTURE_DIR}/t16.xml"
for method in setVertexTextures setVertexSamplerStates setFragmentTextures setFragmentSamplerStates; do
  rg -q "name=\"MTLRenderCommandEncoder::${method}\"" "${CAPTURE_DIR}/t17.xml"
done
rg -q 'name="initialData" typename="Byte Buffer" byteLength="400"' "${CAPTURE_DIR}/t17.xml"
test "$(rg -c '<uint typename="uint8_t" width="1">0</uint>' "${CAPTURE_DIR}/t17.xml")" = "4"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="640"' "${CAPTURE_DIR}/t18.xml"
rg -q 'name="MTLRenderCommandEncoder::setFragmentBuffer"' "${CAPTURE_DIR}/t18.xml"
rg -q 'name="offset" typename="uint64_t" width="8">256<' "${CAPTURE_DIR}/t18.xml"
rg -q 'name="index" typename="uint64_t" width="8" important="true">3<' "${CAPTURE_DIR}/t18.xml"
rg -q 'name="initialData" typename="Byte Buffer" byteLength="768"' "${CAPTURE_DIR}/t19.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::setVertexBuffer"' "${CAPTURE_DIR}/t19.xml")" = "2"
rg -q 'name="offset" typename="uint64_t" width="8">256<' "${CAPTURE_DIR}/t19.xml"
rg -q 'name="offset" typename="uint64_t" width="8">320<' "${CAPTURE_DIR}/t19.xml"
rg -q 'name="index" typename="uint64_t" width="8" important="true">4<' "${CAPTURE_DIR}/t19.xml"
rg -q 'name="index" typename="uint64_t" width="8" important="true">6<' "${CAPTURE_DIR}/t19.xml"
rg -q 'name="MTLRenderCommandEncoder::executeCommandsInBuffer"' "${CAPTURE_DIR}/t20.xml"
rg -q 'name="MTLIndirectRenderCommand::drawPrimitives"' "${CAPTURE_DIR}/t20.xml"
rg -q 'name="MTLRenderCommandEncoder::drawIndexedPrimitives"' "${CAPTURE_DIR}/t21.xml"
rg -q 'name="indexBufferOffset" typename="uint64_t" width="8" important="true">4<' "${CAPTURE_DIR}/t21.xml"
rg -q 'name="indirectBufferOffset" typename="uint64_t" width="8" important="true">16<' "${CAPTURE_DIR}/t21.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::executeCommandsInBuffer"' "${CAPTURE_DIR}/t22.xml")" = "2"
test "$(rg -c 'name="MTLIndirectRenderCommand::drawPrimitives"' "${CAPTURE_DIR}/t22.xml")" = "3"
rg -q 'name="MTLIndirectRenderCommand::drawIndexedPrimitives"' "${CAPTURE_DIR}/t23.xml"
rg -q 'name="indexType" typename="MTLIndexType" width="8" important="true" string="MTLIndexTypeUInt16"' "${CAPTURE_DIR}/t23.xml"
rg -q 'name="indexBufferOffset" typename="uint64_t" width="8" important="true">4<' "${CAPTURE_DIR}/t23.xml"
rg -q 'name="baseVertex" typename="int64_t" width="8" important="true">1<' "${CAPTURE_DIR}/t23.xml"
rg -q 'name="baseInstance" typename="uint64_t" width="8" important="true">1<' "${CAPTURE_DIR}/t23.xml"
rg -q 'name="MTLIndirectCommandBuffer::resetWithRange"' "${CAPTURE_DIR}/t24.xml"
rg -q 'name="location" typename="uint64_t" width="8">1<' "${CAPTURE_DIR}/t24.xml"
rg -q 'name="length" typename="uint64_t" width="8">1<' "${CAPTURE_DIR}/t24.xml"
test "$(rg -c 'name="MTLIndirectRenderCommand::drawPrimitives"' "${CAPTURE_DIR}/t24.xml")" = "4"
test "$(rg -c 'name="MTLRenderCommandEncoder::executeCommandsInBuffer"' "${CAPTURE_DIR}/t24.xml")" = "3"
rg -q 'name="commandTypes".*string="MTL::IndirectCommandType\(3\)">3<' "${CAPTURE_DIR}/t25.xml"
rg -q 'name="MTLIndirectRenderCommand::drawPrimitives"' "${CAPTURE_DIR}/t25.xml"
rg -q 'name="MTLIndirectRenderCommand::drawIndexedPrimitives"' "${CAPTURE_DIR}/t25.xml"
rg -q 'name="indexBufferOffset" typename="uint64_t" width="8" important="true">4<' "${CAPTURE_DIR}/t25.xml"
rg -q 'name="baseVertex" typename="int64_t" width="8" important="true">1<' "${CAPTURE_DIR}/t25.xml"
rg -q 'name="baseInstance" typename="uint64_t" width="8" important="true">1<' "${CAPTURE_DIR}/t25.xml"
for index in 28 29; do
  "${RENDERDOCCMD}" convert -f "${CAPTURE_DIR}/t${index}_capture.rdc" \
    -o "${CAPTURE_DIR}/t${index}.xml" -c xml
done
rg -q 'name="MTLComputeCommandEncoder::dispatchThreads"' "${CAPTURE_DIR}/t28.xml"
rg -q 'name="MTLComputeCommandEncoder::setBuffer"' "${CAPTURE_DIR}/t29.xml"
python3 - "${CAPTURE_DIR}/t28.xml" "${CAPTURE_DIR}/t29.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET

t28, t29 = [ET.parse(path).getroot() for path in sys.argv[1:]]
def chunks(root, name):
    return [node for node in root.findall('./chunks/chunk') if node.get('name') == name]
dispatch = chunks(t28, 'MTLComputeCommandEncoder::dispatchThreads')
assert len(dispatch) == 1
assert [int(node.text) for node in dispatch[0].find("./struct[@name='grid']")] == [7, 5, 1]
assert [int(node.text) for node in dispatch[0].find("./struct[@name='threadsPerGroup']")] == [4, 3, 1]
bindings = chunks(t29, 'MTLComputeCommandEncoder::setBuffer')
assert len(bindings) == 2
assert [(int(node.find("./uint[@name='index']").text),
         int(node.find("./uint[@name='offset']").text)) for node in bindings] == [(2, 32), (4, 64)]
PY

rg -q 'name="inheritPipelineState" typename="bool" important="true">true<' "${CAPTURE_DIR}/t26.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::executeCommandsInBuffer"' "${CAPTURE_DIR}/t26.xml")" = "2"
rg -q 'name="inheritBuffers" typename="bool" important="true">true<' "${CAPTURE_DIR}/t27.xml"
rg -q 'name="maxVertexBufferBindCount" typename="uint64_t" width="8" important="true">0<' "${CAPTURE_DIR}/t27.xml"
test "$(rg -c 'name="MTLRenderCommandEncoder::executeCommandsInBuffer"' "${CAPTURE_DIR}/t27.xml")" = "2"

clang++ -std=c++17 -arch "${TARGET_ARCH}" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_output_smoke.mm" \
  -L"${RENDERDOC_BUILD_DIR}/lib" -lrenderdoc \
  -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${RENDERDOC_BUILD_DIR}/lib" -o "${OUTPUT_SMOKE}"

clang++ -std=c++17 -arch "${TARGET_ARCH}" -mmacosx-version-min=12.0 \
  -DRENDERDOC_PLATFORM_APPLE -I"${REPO_ROOT}" \
  "${REPO_ROOT}/util/test/metal/metal_replay_lifecycle_smoke.mm" \
  -L"${RENDERDOC_BUILD_DIR}/lib" -lrenderdoc \
  -framework Cocoa -framework QuartzCore -framework Metal \
  -Wl,-rpath,"${RENDERDOC_BUILD_DIR}/lib" -o "${LIFECYCLE_SMOKE}"

"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t00_capture.rdc" "${CAPTURE_DIR}/t00_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t01_capture.rdc" "${CAPTURE_DIR}/t01_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t01_capture.rdc" \
  "${CAPTURE_DIR}/t01_event_clear.ppm" \
  "${CAPTURE_DIR}/t01_event_draw.ppm" \
  "${CAPTURE_DIR}/t01_event_rewind.ppm" \
  "${CAPTURE_DIR}/t01_texture.dds"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t02_capture.rdc" "${CAPTURE_DIR}/t02_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t03_capture.rdc" "${CAPTURE_DIR}/t03_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t04_capture.rdc" "${CAPTURE_DIR}/t04_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t05_capture.rdc" "${CAPTURE_DIR}/t05_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t06_capture.rdc" "${CAPTURE_DIR}/t06_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t07_capture.rdc" "${CAPTURE_DIR}/t07_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t08_capture.rdc" "${CAPTURE_DIR}/t08_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t09_capture.rdc" "${CAPTURE_DIR}/t09_replay.ppm" \
  "${CAPTURE_DIR}/t09_cube.dds"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t10_capture.rdc" "${CAPTURE_DIR}/t10_replay.ppm" \
  "${CAPTURE_DIR}/t10_mips.dds"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t11_capture.rdc" "${CAPTURE_DIR}/t11_replay.ppm" \
  "${CAPTURE_DIR}/t11_filtered.dds"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t12_capture.rdc" "${CAPTURE_DIR}/t12_replay.ppm" \
  "${CAPTURE_DIR}/t12_argument_texture.dds"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t13_capture.rdc" "${CAPTURE_DIR}/t13_replay.ppm" \
  "${CAPTURE_DIR}/t13_arguments.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t14_capture.rdc" "${CAPTURE_DIR}/t14_replay.ppm" \
  "${CAPTURE_DIR}/t14_indices.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t15_capture.rdc" "${CAPTURE_DIR}/t15_replay.ppm" \
  "${CAPTURE_DIR}/t15_output.dds"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t16_capture.rdc" "${CAPTURE_DIR}/t16_replay.ppm" \
  "${CAPTURE_DIR}/t16_output.dds"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t17_capture.rdc" "${CAPTURE_DIR}/t17_replay.ppm" \
  "${CAPTURE_DIR}/t17_output.dds"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t18_capture.rdc" "${CAPTURE_DIR}/t18_replay.ppm" \
  "${CAPTURE_DIR}/t18_storage.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t19_capture.rdc" "${CAPTURE_DIR}/t19_replay.ppm" \
  "${CAPTURE_DIR}/t19_storage.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t20_capture.rdc" "${CAPTURE_DIR}/t20_replay.ppm" \
  "${CAPTURE_DIR}/t20_vertices.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t21_capture.rdc" "${CAPTURE_DIR}/t21_replay.ppm" \
  "${CAPTURE_DIR}/t21_arguments.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t22_capture.rdc" "${CAPTURE_DIR}/t22_replay.ppm" \
  "${CAPTURE_DIR}/t22_packets.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t23_capture.rdc" "${CAPTURE_DIR}/t23_replay.ppm" \
  "${CAPTURE_DIR}/t23_indices.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t24_capture.rdc" "${CAPTURE_DIR}/t24_replay.ppm" \
  "${CAPTURE_DIR}/t24_packets.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t25_capture.rdc" "${CAPTURE_DIR}/t25_replay.ppm" \
  "${CAPTURE_DIR}/t25_resources.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t26_capture.rdc" "${CAPTURE_DIR}/t26_replay.ppm" \
  "${CAPTURE_DIR}/t26_vertices.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t27_capture.rdc" "${CAPTURE_DIR}/t27_replay.ppm" \
  "${CAPTURE_DIR}/t27_packets.bin"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t28_capture.rdc" "${CAPTURE_DIR}/t28_replay.ppm"
"${OUTPUT_SMOKE}" "${CAPTURE_DIR}/t29_capture.rdc" "${CAPTURE_DIR}/t29_replay.ppm" \
  "${CAPTURE_DIR}/t29_output.bin"
python3 "${REPO_ROOT}/util/test/metal/metal_compute_invalid.py" \
  "${RENDERDOCCMD}" "${CAPTURE_DIR}/t28_capture.rdc" \
  "${CAPTURE_DIR}/t29_capture.rdc"

python3 "${REPO_ROOT}/util/test/metal/metal_icb_invalid.py" \
  "${RENDERDOCCMD}" "${CAPTURE_DIR}/t20_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_indexed_indirect_invalid.py" \
  "${RENDERDOCCMD}" "${CAPTURE_DIR}/t21_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_multi_icb_invalid.py" \
  "${RENDERDOCCMD}" "${CAPTURE_DIR}/t22_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_indexed_icb_invalid.py" \
  "${RENDERDOCCMD}" "${CAPTURE_DIR}/t23_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_icb_reset_invalid.py" \
  "${RENDERDOCCMD}" "${CAPTURE_DIR}/t24_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_mixed_icb_invalid.py" \
  "${RENDERDOCCMD}" "${CAPTURE_DIR}/t25_capture.rdc"
python3 "${REPO_ROOT}/util/test/metal/metal_icb_inheritance_invalid.py" \
  "${RENDERDOCCMD}" "${CAPTURE_DIR}/t26_capture.rdc" \
  "${CAPTURE_DIR}/t27_capture.rdc" "${CAPTURE_DIR}/t20_capture.rdc"

read_rgb()
{
  local image="$1"
  local x="$2"
  local y="$3"
  local offset=$((15 + (y * 640 + x) * 3))
  dd if="${image}" bs=1 skip="${offset}" count=3 2>/dev/null | xxd -p
}

test "$(read_rgb "${CAPTURE_DIR}/t00_replay.ppm" 320 240)" = "14335c"
test "$(read_rgb "${CAPTURE_DIR}/t01_replay.ppm" 10 10)" = "14141a"
test "$(read_rgb "${CAPTURE_DIR}/t01_replay.ppm" 320 240)" != "14141a"
test "$(read_rgb "${CAPTURE_DIR}/t01_event_clear.ppm" 320 240)" = "14141a"
test "$(read_rgb "${CAPTURE_DIR}/t01_event_draw.ppm" 320 240)" != "14141a"
test "$(read_rgb "${CAPTURE_DIR}/t01_event_rewind.ppm" 320 240)" = "14141a"
test -s "${CAPTURE_DIR}/t01_texture.dds"
test "$(read_rgb "${CAPTURE_DIR}/t02_replay.ppm" 10 10)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t02_replay.ppm" 160 240)" != "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t02_replay.ppm" 480 240)" != "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t03_replay.ppm" 10 10)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t03_replay.ppm" 200 150)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t03_replay.ppm" 440 150)" = "10e030"
test "$(read_rgb "${CAPTURE_DIR}/t03_replay.ppm" 200 330)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t03_replay.ppm" 440 330)" = "f0d020"
test "$(read_rgb "${CAPTURE_DIR}/t04_replay.ppm" 160 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t04_replay.ppm" 480 240)" = "10df30"
test "$(read_rgb "${CAPTURE_DIR}/t05_replay.ppm" 144 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t05_replay.ppm" 320 240)" = "10df30"
test "$(read_rgb "${CAPTURE_DIR}/t05_replay.ppm" 496 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t06_replay.ppm" 32 32)" = "8c292e"
test "$(read_rgb "${CAPTURE_DIR}/t06_replay.ppm" 320 240)" = "6d572e"
test "$(read_rgb "${CAPTURE_DIR}/t07_replay.ppm" 160 240)" = "10df30"
test "$(read_rgb "${CAPTURE_DIR}/t07_replay.ppm" 480 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t08_replay.ppm" 16 16)" = "080a0f"
test "$(read_rgb "${CAPTURE_DIR}/t08_replay.ppm" 160 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t08_replay.ppm" 320 240)" = "10df30"
test "$(read_rgb "${CAPTURE_DIR}/t08_replay.ppm" 480 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 26 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 80 240)" = "10e030"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 133 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 186 240)" = "f0d020"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 240 240)" = "e030c0"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 293 240)" = "20d0e0"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 346 240)" = "ff8020"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 400 240)" = "8020ff"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 453 240)" = "20ff80"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 506 240)" = "ff4080"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 560 240)" = "80ff20"
test "$(read_rgb "${CAPTURE_DIR}/t09_replay.ppm" 613 240)" = "2080ff"
test -s "${CAPTURE_DIR}/t09_cube.dds"
test "$(read_rgb "${CAPTURE_DIR}/t10_replay.ppm" 80 240)" = "ff8020"
test "$(read_rgb "${CAPTURE_DIR}/t10_replay.ppm" 240 240)" = "606060"
test "$(read_rgb "${CAPTURE_DIR}/t10_replay.ppm" 400 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t10_replay.ppm" 560 240)" = "868458"
test -s "${CAPTURE_DIR}/t10_mips.dds"
test -s "${CAPTURE_DIR}/t11_filtered.dds"
test "$(read_rgb "${CAPTURE_DIR}/t12_replay.ppm" 200 150)" = "f82818"
test "$(read_rgb "${CAPTURE_DIR}/t12_replay.ppm" 440 150)" = "18d838"
test "$(read_rgb "${CAPTURE_DIR}/t12_replay.ppm" 200 330)" = "2048f8"
test "$(read_rgb "${CAPTURE_DIR}/t12_replay.ppm" 440 330)" = "e8c828"
test -s "${CAPTURE_DIR}/t12_argument_texture.dds"
test "$(read_rgb "${CAPTURE_DIR}/t13_replay.ppm" 144 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t13_replay.ppm" 320 240)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t13_replay.ppm" 496 240)" = "1840ff"
test "$(xxd -p "${CAPTURE_DIR}/t13_arguments.bin" | tr -d '\n')" = \
  "03000000020000000100000001000000"
test "$(read_rgb "${CAPTURE_DIR}/t14_replay.ppm" 144 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t14_replay.ppm" 320 240)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t14_replay.ppm" 496 240)" = "1840ff"
test "$(xxd -p "${CAPTURE_DIR}/t14_indices.bin" | tr -d '\n')" = "000001000200"
test "$(read_rgb "${CAPTURE_DIR}/t20_replay.ppm" 144 240)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t20_replay.ppm" 320 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t20_replay.ppm" 496 240)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t21_replay.ppm" 144 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t21_replay.ppm" 320 240)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t21_replay.ppm" 496 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t22_replay.ppm" 192 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t22_replay.ppm" 320 240)" = "1020ff"
test "$(read_rgb "${CAPTURE_DIR}/t22_replay.ppm" 448 240)" = "1020ff"
test "$(read_rgb "${CAPTURE_DIR}/t22_replay.ppm" 560 60)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t23_replay.ppm" 144 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t23_replay.ppm" 320 240)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t23_replay.ppm" 496 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t24_replay.ppm" 96 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t24_replay.ppm" 320 240)" = "10ff20"
test "$(read_rgb "${CAPTURE_DIR}/t24_replay.ppm" 544 240)" = "1020ff"
test "$(read_rgb "${CAPTURE_DIR}/t24_replay.ppm" 240 280)" = "06090e"
test "$(read_rgb "${CAPTURE_DIR}/t25_replay.ppm" 112 240)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t25_replay.ppm" 432 240)" = "10ff20"
test "$(read_rgb "${CAPTURE_DIR}/t25_replay.ppm" 550 240)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t25_replay.ppm" 320 240)" = "06090e"
test "$(xxd -p "${CAPTURE_DIR}/t21_arguments.bin" | tr -d '\n')" = \
  "0300000002000000010000000100000001000000"
test "$(read_rgb "${CAPTURE_DIR}/t15_replay.ppm" 160 120)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t15_replay.ppm" 320 240)" = "06090e"
test -s "${CAPTURE_DIR}/t15_output.dds"
test "$(read_rgb "${CAPTURE_DIR}/t16_replay.ppm" 160 120)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t16_replay.ppm" 480 120)" = "10e030"
test "$(read_rgb "${CAPTURE_DIR}/t16_replay.ppm" 160 360)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t16_replay.ppm" 480 360)" = "f0d020"
test -s "${CAPTURE_DIR}/t16_output.dds"
test "$(read_rgb "${CAPTURE_DIR}/t17_replay.ppm" 160 120)" = "002010"
test "$(read_rgb "${CAPTURE_DIR}/t17_replay.ppm" 480 120)" = "100030"
test "$(read_rgb "${CAPTURE_DIR}/t17_replay.ppm" 160 360)" = "1840ff"
test "$(read_rgb "${CAPTURE_DIR}/t17_replay.ppm" 480 360)" = "f0d000"
test -s "${CAPTURE_DIR}/t17_output.dds"
test "$(read_rgb "${CAPTURE_DIR}/t18_replay.ppm" 160 120)" = "2040ff"
test "$(read_rgb "${CAPTURE_DIR}/t18_replay.ppm" 480 120)" = "efcf00"
test "$(read_rgb "${CAPTURE_DIR}/t18_replay.ppm" 160 360)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t18_replay.ppm" 480 360)" = "10df30"
test "$(stat -f %z "${CAPTURE_DIR}/t18_storage.bin")" = "640"
test "$(read_rgb "${CAPTURE_DIR}/t19_replay.ppm" 160 120)" = "2040ff"
test "$(read_rgb "${CAPTURE_DIR}/t19_replay.ppm" 480 120)" = "efcf00"
test "$(read_rgb "${CAPTURE_DIR}/t19_replay.ppm" 160 360)" = "ff2010"
test "$(read_rgb "${CAPTURE_DIR}/t19_replay.ppm" 480 360)" = "10df30"
test "$(stat -f %z "${CAPTURE_DIR}/t19_storage.bin")" = "768"
test "$(stat -f %z "${CAPTURE_DIR}/t20_vertices.bin")" = "152"
test "$(stat -f %z "${CAPTURE_DIR}/t21_arguments.bin")" = "20"
test "$(stat -f %z "${CAPTURE_DIR}/t22_packets.bin")" = "312"
test "$(xxd -p "${CAPTURE_DIR}/t23_indices.bin" | tr -d '\n')" = "000001000200"
test "$(stat -f %z "${CAPTURE_DIR}/t24_packets.bin")" = "416"
test "$(stat -f %z "${CAPTURE_DIR}/t25_resources.bin")" = "252"
test "$(stat -f %z "${CAPTURE_DIR}/t26_vertices.bin")" = "24"
test "$(stat -f %z "${CAPTURE_DIR}/t27_packets.bin")" = "208"

"${LIFECYCLE_SMOKE}" "${CAPTURE_DIR}/t00_capture.rdc" \
  "${CAPTURE_DIR}/t01_capture.rdc" "${CAPTURE_DIR}/t02_capture.rdc" \
  "${CAPTURE_DIR}/t03_capture.rdc" "${CAPTURE_DIR}/t04_capture.rdc" \
  "${CAPTURE_DIR}/t05_capture.rdc" "${CAPTURE_DIR}/t06_capture.rdc" \
  "${CAPTURE_DIR}/t07_capture.rdc" "${CAPTURE_DIR}/t08_capture.rdc" \
  "${CAPTURE_DIR}/t09_capture.rdc" "${CAPTURE_DIR}/t10_capture.rdc" \
  "${CAPTURE_DIR}/t11_capture.rdc" "${CAPTURE_DIR}/t12_capture.rdc" \
  "${CAPTURE_DIR}/t13_capture.rdc" "${CAPTURE_DIR}/t14_capture.rdc" \
  "${CAPTURE_DIR}/t15_capture.rdc" "${CAPTURE_DIR}/t16_capture.rdc" \
  "${CAPTURE_DIR}/t17_capture.rdc" "${CAPTURE_DIR}/t18_capture.rdc" \
  "${CAPTURE_DIR}/t19_capture.rdc" "${CAPTURE_DIR}/t20_capture.rdc" \
  "${CAPTURE_DIR}/t21_capture.rdc" "${CAPTURE_DIR}/t22_capture.rdc" \
  "${CAPTURE_DIR}/t23_capture.rdc" "${CAPTURE_DIR}/t24_capture.rdc" \
  "${CAPTURE_DIR}/t25_capture.rdc" "${CAPTURE_DIR}/t26_capture.rdc" \
  "${CAPTURE_DIR}/t27_capture.rdc" \
  "${CAPTURE_DIR}/t28_capture.rdc" "${CAPTURE_DIR}/t29_capture.rdc" 10

for index in 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29; do
  "${RENDERDOCCMD}" replay --loops 1 "${CAPTURE_DIR}/t${index}_capture.rdc"
done

echo "Metal capture smoke test passed."
echo "T00: ${CAPTURE_DIR}/t00_capture.rdc"
echo "T01: ${CAPTURE_DIR}/t01_capture.rdc"
echo "T02: ${CAPTURE_DIR}/t02_capture.rdc"
echo "T03: ${CAPTURE_DIR}/t03_capture.rdc"
echo "T04: ${CAPTURE_DIR}/t04_capture.rdc"
echo "T05: ${CAPTURE_DIR}/t05_capture.rdc"
echo "T06: ${CAPTURE_DIR}/t06_capture.rdc"
echo "T07: ${CAPTURE_DIR}/t07_capture.rdc"
echo "T08: ${CAPTURE_DIR}/t08_capture.rdc"
echo "T09: ${CAPTURE_DIR}/t09_capture.rdc"
echo "T10: ${CAPTURE_DIR}/t10_capture.rdc"
echo "T11: ${CAPTURE_DIR}/t11_capture.rdc"
echo "T12: ${CAPTURE_DIR}/t12_capture.rdc"
echo "T13: ${CAPTURE_DIR}/t13_capture.rdc"
echo "T14: ${CAPTURE_DIR}/t14_capture.rdc"
echo "T15: ${CAPTURE_DIR}/t15_capture.rdc"
echo "T16: ${CAPTURE_DIR}/t16_capture.rdc"
echo "T17: ${CAPTURE_DIR}/t17_capture.rdc"
echo "T18: ${CAPTURE_DIR}/t18_capture.rdc"
echo "T19: ${CAPTURE_DIR}/t19_capture.rdc"
echo "T20: ${CAPTURE_DIR}/t20_capture.rdc"
echo "T21: ${CAPTURE_DIR}/t21_capture.rdc"
echo "T22: ${CAPTURE_DIR}/t22_capture.rdc"
echo "T23: ${CAPTURE_DIR}/t23_capture.rdc"
echo "T24: ${CAPTURE_DIR}/t24_capture.rdc"
echo "T25: ${CAPTURE_DIR}/t25_capture.rdc"
echo "T26: ${CAPTURE_DIR}/t26_capture.rdc"
echo "T27: ${CAPTURE_DIR}/t27_capture.rdc"
echo "T28: ${CAPTURE_DIR}/t28_capture.rdc"
echo "T29: ${CAPTURE_DIR}/t29_capture.rdc"
echo "T00 replay: ${CAPTURE_DIR}/t00_replay.ppm"
echo "T01 replay: ${CAPTURE_DIR}/t01_replay.ppm"
echo "T01 event replay: clear -> draw -> clear verified for 10 cycles"
echo "T01 shader entry/stage/MSL reflection verified"
echo "T01 pipeline/shaders/topology/vertex buffer/color target state verified"
echo "T01 texture readback/pixel picking/DDS save verified"
echo "T02 indexed state, clear/draw/rewind images, buffer data, generic VS input, and mesh preview verified"
echo "T03 texture upload/readback/pixel picking, sampler, bindings, and sampled output verified"
echo "T04 dynamic uniform bytes, fragment buffer offset/reflection, event replay, and output verified"
echo "T05 multi-buffer instancing, base instance, generic VS input, mesh preview, and output verified"
echo "T06 MRT targets, per-attachment blending/write masks, event replay, and output verified"
echo "T07 combined depth/stencil, front/back state, dynamic references, event replay, and output verified"
echo "T08 4x MSAA attachment, explicit resolve, sample state, event replay, and output verified"
echo "T09 mip/array/cube upload, descriptors, readback, display, picking, save, and output verified"
echo "T10 buffer copy/fill, texture copy, generated mips, event seek, usage, save, and output verified"
echo "T11 compute dispatch, read/write texture descriptors, event seek, save, and output verified"
echo "T12 direct argument-buffer texture/sampler references, event seek, save, and output verified"
echo "T13 indirect action/usage/16-byte argument range, event seek, save, and output verified"
echo "T14 indexed instancing/base vertex/index offset, mesh, usage, event seek, and output verified"
echo "T15 Point/Line/Line Strip action, topology, vertex start, mesh, seek, DDS, and output verified"
echo "T16 vertex texture/sampler binding, reflection, VS descriptors, usage, seek, DDS, and output verified"
echo "T17 vertex/fragment batch texture/sampler ranges, empty slots, descriptors, usage, seek, DDS, and output verified"
echo "T18 fragment storage-buffer slot/range, reflection, descriptors, usage, raw export, seek, and output verified"
echo "T19 vertex storage-buffer slot/range, reflection, descriptors, usage, raw export, seek, and output verified"
echo "T20 ICB execute/draw actions, range, IA/Mesh, resource usage, raw export, seek, and output verified"
echo "T21 indexed indirect arguments/indexStart/base vertex/instance, exact IA ranges, usage, invalid captures, seek, and output verified"
echo "T22 multi-command ICB nonzero range, ordered actions, per-command IA/Mesh, usage, invalid captures, seek, and output verified"
echo "T23 indexed ICB index offset/base vertex/instance, exact IA/Mesh, usage, invalid captures, seek, and output verified"
echo "T24 ICB reset/re-encode range, replacement state, old-resource invalidation, invalid captures, seek, and output verified"
echo "T25 mixed non-indexed/indexed ICB actions, exact IA/Mesh, usage, invalid captures, seek, and output verified"
echo "T26 inherited pipeline and T27 inherited buffers, exact draw state, usage, invalid captures, seek, and output verified"
echo "T28 dispatchThreads and T29 compute buffer binding, seek, resource data, and invalid captures verified"
echo "T00-T29 CLI replay, lifecycle, and unsupported-interface stability verified"
