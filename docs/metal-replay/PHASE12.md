# 阶段 12 细化计划：T11 compute texture filter

T10 已关闭常见 blit 数据流、逐操作事件与标准资源查看。T11 聚焦一次 compute dispatch 把固定
RGBA8 输入纹理写入另一张纹理，再由 render draw 采样输出；保持单 command buffer、直接资源绑定和
可确定性验证的线程网格，不扩展到 argument buffer、indirect dispatch 或跨 queue 同步。

当前状态：P12.1-P12.4 已完成，T00-T11 全量 L3 与最新 qrenderdoc L4 均通过；下一项为
`PHASE13.md` P13.1。详见 `STATUS.md`。

## P12.1：确定性 compute fixture

- 在 `util/test/demos/metal/` 新增 `Metal_Compute_Texture_Filter`，并接入
  `util/test/demos/CMakeLists.txt`；使用固定 8x8 RGBA8 图案、独立可写目标纹理和简单逐像素变换。
- 选择明确的 threadgroup size 与 dispatch 网格，包含易识别的边界像素；compute 后用现有 render
  路径采样目标纹理，先验证未注入 native 结果。
- 不在本 fixture 中混入 blit、argument buffer、indirect dispatch、fence/event 或多 queue。

验收：native 输出与 CPU 参考字节一致，dispatch 输入/输出和最终 draw 结果可区分。

## P12.2：capture、replay 与事件

- 按 fixture 需要，在 `renderdoc/driver/metal/metal_device.*`、`metal_command_buffer.*`、
  `metal_core.cpp` 和相应 Objective-C bridge 中补齐 compute pipeline 创建、compute encoder 创建、
  pipeline/texture binding、dispatch 与 endEncoding 的包装和序列化；先盘点现有 skeleton 再确定
  新增 wrapper 文件及资源所有权。
- 在 replay 中重建真实 compute encoder/pipeline，保持 compute→render 资源可见；为 dispatch 建立
  可 seek action/event 和输出资源 usage，拒绝无效的线程网格与越界绑定。
- 使用 structured XML 与 `GetTextureData()`、最终 GPU 输出对照 CPU 参考，验证 dispatch 前后与
  前进/回退，定向检查 T03/T09/T10 的 texture 与 blit 路径。

验收：capture 参数、event、目标纹理原始数据和最终画面一致，事件切换稳定。

## P12.3：通用状态与标准 UI

- 扩展 `MetalPipe::State`、通用 descriptor/reflection 和 qrenderdoc Metal Pipeline 页面中 T11 所需的
  compute pipeline、shader 入口与直接 texture binding；只展示有 capture/replay 证据的字段。
- 在标准 Event Browser、Resource Inspector 和 Texture Viewer 核对 dispatch、读写资源 usage、
  输出 texel、资源跳转与保存路径，不新增 Metal 专用 compute 数据查看器。

验收：UI 的 shader、绑定、事件与纹理内容与自动 smoke 一致，状态栏无错误。

## P12.4：阶段收口

- 开发中只跑 T11 和受影响的 T03/T09/T10 定向验证；功能齐备后一次 T00-T11 完整回归、逐份
  CLI replay/lifecycle，以及一次最新 qrenderdoc 实机验收。
- 同步 `PLAN.md`、`STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md`、`HANDOFF.md`，留下 T12 入口。

验收：完整回归通过，qrenderdoc 状态栏为 `No problems detected`。

## 收口证据（2026-09-23）

- `util/buildscripts/scripts/test_metal_capture_macos.sh`：T00-T11 原生运行、capture/XML、专项
  output smoke、逐份 CLI replay 与 12×10 lifecycle 全通过，resident growth 999,424 字节；日志
  `/tmp/t11-final-regression.log`。
- T11 dispatch 前/后/回退纹理为全零/CPU 参考 swizzle/全零；最终 draw 与 384-byte DDS 一致。
- 最新 qrenderdoc：Event Browser EID 1-3 compute、EID 5 draw；CS Pipeline 的 `filter_main`、
  Texture 19 只读、Texture 20 读写，标准 Texture Viewer/Resource Inspector 跳转、首像素拾取、
  final draw、DDS/HTML 保存均通过；状态栏 `No problems detected`。

## 当前不在本切片内

- argument buffer、indirect command/dispatch、ICB、fence/event、跨 queue 同步。
- 3D、compressed、depth/stencil、整数/浮点纹理及任意应用注入。
