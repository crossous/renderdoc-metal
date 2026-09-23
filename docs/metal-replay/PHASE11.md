# 阶段 11 细化计划：T10 buffer/texture blit

T09 已把 mip、2D array 与 cube 的确定性子资源上传、读取、标准 Texture Viewer 选择和
Pipeline 绑定连成一条纵向链路。T10 聚焦常见 blit encoder 数据流：buffer/texture copy、
buffer fill 与 mipmap generation。不要扩大到 compute、compressed texture 或多命令缓冲同步。

当前状态：P11.1-P11.4 已完成；T00-T10 最终代码完整回归、逐份 CLI replay、生命周期与最新
qrenderdoc 实机验收均通过。

## P11.1：确定性 T10 fixture

- 新增 `Metal_Blit_Operations`：使用固定字节模式的 shared buffer、RGBA8 source/destination
  texture 与 mipmapped destination texture。
- 每种操作留下可读取、可在最终 draw 中采样的独立结果；先验证未注入 native 输出。
- 记录 blit encoder 起止、操作顺序、拷贝范围与预期像素/字节，不依赖未定义内容。

验收：native 结果稳定，buffer copy/fill、texture copy 和 generated mips 可区分。

## P11.2：capture/replay 和事件语义

- 按 fixture 精确补齐 `MTLBlitCommandEncoder` 的创建、copy/fill、generateMipmaps、endEncoding
  chunk、序列化和真实 GPU replay。
- 明确 buffer offset/length、texture mip/slice/origin/size 的边界检查；保持旧 T03/T09 upload
  路径不退化。
- 逐操作建立可 seek 的 action/event，验证 blit 前后数据与最终采样输出。

验收：structured XML、`GetBufferData()`/`GetTextureData()` 与 GPU 输出吻合；事件前进/回退正确。

## P11.3：通用状态与 UI

- 让 blit 的源/目标资源引用、usage 和 event 名称进入标准 Event Browser/Resource Inspector；
  不新增 Metal 专用数据查看器。
- 使用标准 Buffer Viewer、Texture Viewer 的 mip/slice 与保存路径核对结果。

验收：qrenderdoc 的事件、资源跳转和数据视图与自动 smoke 一致。

## P11.4：阶段收口

- 开发中仅跑 T10 及相关 T03/T09 定向验证；结束时一次完整 T00-T10 回归、CLI replay 和
  最新 qrenderdoc 实机验收。
- 同步 `PLAN.md`、`STATUS.md`、`TEST_MATRIX.md`、`DECISIONS.md`、`HANDOFF.md`，留下下一阶段入口。

验收：完整回归和 lifecycle 通过，qrenderdoc 状态栏为 `No problems detected`。

## 收口记录（2026-09-23）

- P11.1：`Metal_Blit_Operations` 原生自检通过。64-byte source/destination buffer、8x8 RGBA8
  四象限纹理与四级 mip chain 分别留下可读取结果；最终 draw 显示橙/灰/红/橄榄四条色带。
- P11.2：blit encoder 起止、offset 8→0 的 32-byte buffer copy、16-byte `0x60` fill、
  mip0/slice0 的 8x8 texture copy 和 mip generation 已完成 capture、结构化 XML、真实 GPU replay。
  replay 在执行前检查 buffer 范围和 texture mip/slice/origin/size/format；EID 2→3→2 的 buffer
  数据往返、texture copy 和 mip1/mip3 读取由 smoke 断言。
- P11.3：每项 blit 有独立 action/event 和源/目标 resource link；`GetUsage()` 暴露
  `CopySrc/CopyDst/Clear/GenMips`。标准 Resource Inspector 从 API 参数跳到 Buffer 18 与
  Texture 20，并显示 `Copy - Dest`、`Clear`、`Generate Mips`；Buffer Viewer 在 EID 2/3 显示
  橙色复制值与 `0x60` 填充值，Texture Viewer 显示 Texture 20 四象限和 Texture 21 mip3
  `(134,132,88,255)`。UI 保存的 `t10_mips_ui_final.dds` 与自动产物逐字节相同，均为 468 字节。
- P11.4：初次 T00-T10 完整回归通过；通用事件分组/UI 修正后以最终代码复验，
  `/tmp/t10-final-regression-post-ui.log` 中 11 份 capture 各 10 次 lifecycle resident growth 为
  1,277,952 字节；`/tmp/t10-final-cli-replay-post-ui.log` 中 T00-T10 各 `--loops 1` 通过。
  最新 qrenderdoc 使用与正式 capture SHA-256 相同的 `/tmp/t10-ui-capture-final.rdc`，
  Event Browser 的组名为 `Copy/Clear Pass #1`，含 EID 1-6 blit 子事件；EID 8 输出四条色带，
  状态栏为 `No problems detected`。SHA-256 为
  `3ef992a4151228d6aa6d70947a58c47a6e879d2c3e3e21b3feed20eaf0181667`。
- 下一阶段入口：`PHASE12.md` P12.1 / T11 compute texture filter。

## 当前不在本切片内

- compute dispatch、argument buffer、indirect command、fence/event。
- compressed/depth/stencil/3D texture blit、大规模跨 queue 同步与任意应用注入。
