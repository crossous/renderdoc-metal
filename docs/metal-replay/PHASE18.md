# 阶段 18 细化计划：T17 texture/sampler 批量绑定

T16 直接 vertex texture/sampler 已关闭，下一步覆盖 `setVertexTextures`、
`setVertexSamplerStates` 与 fragment 对应批量入口，重点验证 slot range、空槽、资源引用和
used/unused 过滤。保持现有直接绑定和 argument buffer 语义。

当前状态：P18.1-P18.4 全部通过，T17 已关闭；下一项为 `PHASE19.md` P19.1。

## 实现与验证记录

- `Metal_Batch_Texture` 的 VS slot 1-3 与 FS slot 3-5 批量范围分别包含被清空的旧绑定、有效采样
  资源和静态未使用资源。两个 2×2 RGBA8 texture、Clamp/Repeat sampler 与 400-byte vertex buffer
  形成可区分 stage/slot/sampler 的四象限输出；未注入 native BGRA readback 通过。
- 四个批量入口已接通 ObjC bridge、wrapper、chunk、frame reference、GPU replay 和事件状态；空槽
  显式清除旧绑定。XML、VS/FS reflection、通用 descriptor 的 used/unused、资源字节、usage、
  clear/draw/回退像素和 DDS 自动断言通过。越界 range、长度不符、缺失 texture/sampler 的四份
  派生 RDC 均在相应 chunk 被拒绝；T03/T12/T16 定向 replay 通过。
- `/tmp/t17-final-regression.log` 记录 T00-T17 native/capture/XML/output/state、逐份 CLI replay
  与 18×10 lifecycle 全部通过（resident growth 1,015,808 bytes）。首次收口运行发现 T10 的旧 usage
  断言把额外的真实 `PS_Resource` 记录当作失败；修正为检查所需 blit usage 存在后，全量重跑通过。
- 正式 `captures/metal-smoke/t17_capture.rdc` SHA-256 为
  `8ebbdc2c22aadfa2aecea33f91ec066a90ff48fe20df557828784f539955c3a3`。
- 最新 qrenderdoc 重启并打开正式 `t17_capture.rdc`，实机核对 EID 2 Event/API、VS slot 2/3、
  FS slot 4/5、Show Unused/Show Empty、空槽 1/3、Mesh VS Input、400-byte 标准 Buffer Viewer、
  Texture/Resource 跳转、`VS/FS - Texture` usage、四象限输出和 `No problems detected`。
  UI 与自动 DDS 均为 480128 bytes，SHA-256 同为
  `14337ebbdfa628be6f188328444d3db46a1816ccc0170c98be1edf7bcb634178`；
  `t17_pipeline_state_standard.html` 已导出并包含 VS/FS 资源和空槽。

## P18.1：fixture 与 native 语义

- 建立确定性的 T17，使用非零起始 slot 的批量 vertex 与 fragment 纹理/采样器绑定；包含空槽与
  已声明但未使用的 slot，并以不同颜色区域区分 stage、slot 和 sampler 状态。
- 固定原生像素、texture 字节、slot range 与 clear/draw 事件边界。

验收：未注入运行与 CPU 参考一致，错绑 slot 会改变可检查的输出。

## P18.2：capture/replay/state

- 接通批量 API bridge、wrapper、序列化、frame reference、GPU replay 与 event snapshot；
  范围中的空槽须显式更新状态，不保留旧绑定。
- 定向断言 XML、越界 range/缺失资源诊断、usage 与 clear/draw/回退像素。

验收：T17 capture/replay 与 native 一致，T03/T12/T16 直接绑定路径保持正确。

## P18.3：标准 Viewer

- 核对 VS/FS Pipeline、通用 descriptor/reflection、资源跳转、Texture Viewer、Buffer Viewer 与
  HTML/DDS export；used/unused 与 Show Empty Items 对相同 slot 一致。

验收：自动 smoke 与最新 qrenderdoc 的 EID、资源和像素一致。

## P18.4：阶段收口

- 开发中只运行 T17 与受影响的 T03/T12/T16 定向验证；阶段末运行一次 T00-T17 全量回归和
  一次最新 qrenderdoc 实机验收。
- 同步 README/STATUS/PLAN/HANDOFF/TEST_MATRIX/DECISIONS，留下下一阶段任务。

验收：完整回归通过，qrenderdoc 状态栏为 `No problems detected`。
