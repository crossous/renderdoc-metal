# 阶段 32：T31 compute 批量 texture/sampler/buffer 绑定

在 T30 的直接 sampler 基线上，T31 覆盖非零 slot range、空槽清除、批量绑定
与后续单槽覆盖。与 `PHASE31.md` 组成 `BATCH31-32.md`。状态：自动与 GUI L4
通过；GUI 导出入口经用户明确免除本轮复验，自动数据已核对。T30 公共事件树
复验已确认，阶段与批次已关闭。

## 验证清单

- L1：T31 native、capture/XML、Replay API action/state/descriptor/usage/readback/
  seek、输出与 buffer 原始字节、空槽/越界 range/无效 offset 和资源拒绝、
  CLI replay、本场景 lifecycle。
- L2 必跑：T30/T11/T28/T29、T03/T16/T17、T18/T19、T01。
- L2 条件：通用 argument buffer/descriptor reflection 改动加 T12；render-pass
  改动加 T06/T07/T08；blit 前后事件分组改动加 T10；初始内容/所有权改动加 T00/T09/T10；公共事件/ICB
  改动加 T20/T22/T23/T24/T25/T26/T27。其他影响先补编号。
- L4：批末用户检查 Event/API 中各批量调用的 EID/range、CS Pipeline 的
  texture/sampler/buffer 与空槽、Buffer/Texture/Resource 跳转、逐事件画面、
  HTML/CSV/raw/DDS 入口及 `No problems detected`。本轮用户明确免除 GUI 导出
  入口复验；自动 DDS/raw 数据已核对，其他 L4 项通过。
- L3 默认不跑；按批次触发条件升级时覆盖 T00–T31 全部场景及 CLI/lifecycle。

## P32.1 fixture/native

建立 `Metal_Compute_Batch_Binding`，使用非零起始 slot 的
`setTextures:withRange:`、`setSamplerStates:withRange:` 和
`setBuffers:offsets:withRange:`，含一个空槽、已声明未使用槽和随后单槽覆盖。
固定参考像素、buffer 原始字节和未触及区域，先证明 native 正确。

## P32.2 capture/replay/state

补齐三类批量调用的 bridge/wrapper/chunk/资源引用、GPU replay 与状态快照；
空槽必须清除先前绑定。核对 XML 参数、逐事件 seek、descriptor/usage 与无效
range/offset/resource 的明确拒绝。

## P32.3 标准 Viewer

自动核对 CS Pipeline 的 used/unused/empty slot、Buffer/Texture/Resource Viewer、
HTML/CSV/raw/DDS 内容；可见交互留给批末用户 L4。

## P32.4 批次收口

最终代码按 `BATCH31-32.md` 联合清单去重跑 L0/L1/L2、逐份 CLI 与 lifecycle；
按最终 captures 写合并 T30/T31 GUI 验收单，同时保留跨批次待验项。
收到各 T 明确通过反馈并满足其他门槛后才关闭对应阶段/批次。
