# 阶段 13 细化计划：T12 直接 argument buffer 资源引用

T11 的 compute texture filter 完成后，T12 聚焦单层、直接编码的 argument buffer：固定 RGBA8
texture 与 sampler 经一个 argument buffer 交给 shader，再绘制可辨认图案。先盘点上游 Metal
argument encoder 与当前 buffer/pipeline reflection 骨架，不提前承诺嵌套 argument buffer、数组资源、
间接命令或任意应用注入。

当前状态：P13.1-P13.4 已完成；T00-T12 L3 与最新 qrenderdoc L4 均通过，阶段已关闭。

## P13.1：确定性 fixture 与 API 盘点

- 新增 `util/test/demos/metal/` 下的 T12 最小 fixture，并接入 demos CMake；固定输入纹理、
  sampler、argument buffer 布局和最终采样像素，先验证未注入 native 结果。
- 在 `renderdoc/driver/metal/` 检查 argument encoder 创建、`setArgumentBuffer`、texture/sampler
  写入、render/compute buffer binding、资源引用与反射的现状，明确本 fixture 的最小支持面。

验收：native 输出与 CPU/固定像素参考一致，argument buffer 中资源身份与绑定槽位可确定。

## P13.2：capture、replay 与生命周期

- 只补 fixture 使用的 argument encoder/argument buffer 调用、序列化和 replay；确保被间接引用的
  texture/sampler 进入 capture，并在 replay 中按真实 argument layout 重建。
- 将资源引用关联到 action/event，验证 event seek、最终采样结果和错误参数的显式拒绝。

验收：structured XML、资源引用、最终图像与前进/回退结果一致。

## P13.3：标准状态与 Viewer

- 在通用 descriptor/reflection 与 Metal Pipeline 页面展示有证据的 argument buffer 及其资源成员；
  资源行进入标准 Buffer/Texture Viewer 或 Resource Inspector，不建旁路查看器。
- 验证 Event Browser、绑定跳转、纹理像素、保存与状态栏。

验收：UI 和自动 smoke 对同一资源、槽位和最终内容一致。

## P13.4：阶段收口

- 开发中只跑 T12 与受影响的 T03/T04/T11 定向验证；阶段末运行一次 T00-T12 全量回归、
  逐份 CLI replay/lifecycle 和一次最新 qrenderdoc 实机验收。
- 同步 `PLAN.md`、`STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md`、`HANDOFF.md`，留下 T13 入口。

验收：完整回归通过，qrenderdoc 状态栏为 `No problems detected`。

## 当前不在本切片内

- 嵌套或数组 argument buffer、bindless/heap、indirect draw/dispatch、ICB、跨 queue 同步。
- 3D、compressed、depth/stencil、整数/浮点纹理及任意应用注入。

## 实施结果（2026-09-23）

- `Metal_Argument_Buffer` 原生运行通过四个固定 BGRA 像素检查；布局固定为 fragment buffer 0、
  texture `id(0)`、sampler `id(1)`。
- 新增 argument encoder wrapper/bridge/resource/chunk 路由；编码调用作为目标 buffer 的依赖序列保存，
  replay 不复制不可移植的 GPU 地址字节。`useResource` 同步 capture/replay 并显式跟踪间接纹理。
- `MetalPipe::ArgumentBuffer`、shader struct-member reflection 与通用 descriptor 查询暴露真实 buffer、
  texture、sampler；Metal Pipeline 的标准 RDTree 继续进入通用 Buffer/Texture Viewer 和 Resource
  Inspector。
- T12 smoke 已验证 XML、resource ID、usage、descriptor/reflection、clear→draw→clear/forward seek、
  四象限像素、64-byte texture readback 与 192-byte DDS。T03/T04/T11 定向验证通过。
- `/tmp/t12-final-regression.log` 记录 T00-T12 完整回归、13×10 lifecycle（resident growth
  671,744 bytes）和逐份 CLI replay 通过。正式 capture SHA-256 为
  `d13c1ef93e76340e7c530d5a753bb5f74fd0a2b967be4699dc2cff1f0726150f`。
- 最新 qrenderdoc L4 使用正式 `t12_capture.rdc` 完成：Event Browser 展开到 EID 2
  `drawPrimitives(4)`，Texture Viewer 显示正确四象限；FS Pipeline 显示 slot 0 `Buffer 20`、slot 0
  `Texture 17`、slot 1 `Sampler 18`，三者分别进入标准 Buffer Viewer、Texture Viewer 与 Resource
  Inspector。UI 保存的 `/tmp/t12_argument_texture_ui.dds` 为 192 bytes，SHA-256 与自动 DDS 相同；
  Pipeline HTML 保存到 `captures/metal-smoke/t12_pipeline_state_standard.html` 并包含 `fs_main` 与三项
  资源绑定，状态栏为 `No problems detected`。
