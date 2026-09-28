# 阶段 37：T36 render 动态状态

状态：实现和自动验证完成，用户 GUI L4 待验；阶段保持开放。

## 已实现

- `setViewports:count:` 与 `setScissorRects:count:` 捕获完整数组，replay 拒绝空数组
  和超过 16 项的输入；当前 Pipeline State 模型记录首项，真实 replay 使用完整数组。
- `setDepthClipMode:`、`setDepthBias:slopeScale:clamp:`、`setTriangleFillMode:` 和
  `setBlendColorRed:green:blue:alpha:` 均完成 bridge、序列化与真实 replay。
- command buffer 的 `pushDebugGroup/popDebugGroup`，以及 render encoder 的
  `pushDebugGroup/insertDebugSignpost/popDebugGroup` 完成捕获与结构化 replay 读取。
- depth clip/fill enum 在 replay 阶段严格校验；非法值稳定失败。
- BATCH38 扩展 visibility mode/offset、三类附件 store actions/options、textureBarrier。
  修复 visibility buffer 的 capture 引用；动态 store 同步 pass 摘要，提前 seek 可安全结束
  初始 Unknown 的 pass。新增 D32S8 raw readback/pick 验证附件实际结果。

## 自动证据

T36 用 z=2 的全屏三角形验证 `DepthClipModeClamp`，用 blend constant
`0.2/0.4/0.6/0.8` 与 scissor `64,48,272,204` 验证中心/角落像素。XML 精确断言
六个状态 chunk、五个调试标记 chunk、三个字符串和全部参数；Replay API 断言这些
structured chunks 以及 viewport/scissor state；扩展后 14 类非法数组/enum/visibility/store
capture 被拒绝。visibility count 精确为 55488，D32S8 全图 depth=0.75/stencil=23，
EndPass 为 C0/D/S Store。3-loop CLI replay 和包含 T34–T36 的 10 轮 lifecycle 通过；
最终联合回归与功能边界见 `BATCH38.md`。

Metal Pipeline State 现有公开模型没有 depth clip/bias/fill/blend constant 独立字段；本阶段
保证 API Inspector 与 GPU replay 正确，不把尚未实现的专用 UI 字段记为已完成。
