# 批次 38：blit 传输与 render 附件状态补齐

状态：终端自动验证通过；GUI L4 延后，批次保持开放。2026-09-26，本轮未启动
qrenderdoc、未使用 Computer Use。此前 BATCH35–37 的待验项继续保留。

## 实现范围

- T36 扩展八项 render 入口：visibility result、color/depth/stencil 的 store action
  和 store options、textureBarrier。动态附件状态同步到 pass 描述和 EndPass 摘要。
- 修复 visibilityResultBuffer 未标记 frame reference 导致 capture 缺失资源、native
  replay 崩溃的问题。enabled mode 在 replay 前检查 buffer、8-byte alignment 和范围。
- 对延迟设置 StoreAction 的 pass，native replay 初始 Unknown 暂用 Store，允许用户
  seek 到 setter 之前；逻辑描述仍保留原值，setter 再更新最终状态。
- T37 新增 buffer→texture、texture→buffer、整纹理复制、slice/mip 范围复制四条真实
  replay 路径，包含 capture 资源引用、Copy actions、subresource 和资源 usage。
  ObjC 无 options/显式 options 两种调用统一记录现有 `_options` chunk。
- 接通三条已有 blit debug marker 的结构化 replay 路由；T10 增量 capture 使用独立
  `t10_debug_capture.rdc`，原 `t10_capture.rdc` 不覆盖，旧版本仍可通过回归。
- 补上 D32S8 Replay API raw readback 和 PickPixel。depth/stencil 分别 blit 回读后打包为
  RenderDoc 的 8-byte 像素（float depth + stencil byte + 三字节零 padding）；拾取返回
  R=depth、G=stencil/255、A=1。不宣称深度纹理可视化 shader 已实现。

## 自动测试 batch

1. `test_metal_capture_phase37_macos.sh`：T36 native、capture/XML、Replay API、3-loop CLI；
   最终异常脚本 14 类输入被干净拒绝（含缺失 visibility buffer）；三份 capture × 10
   lifecycle 通过，记录的 resident growth 311,296 bytes。
2. `test_metal_capture_phase38_macos.sh`：T37/T10_debug native、capture/XML、Replay API、
   逐份 3-loop CLI；最终 T37 异常脚本 43 类输入被干净拒绝；独立空帧 + 两份 draw
   capture × 10 lifecycle 通过，记录的 resident growth 262,144 bytes。
3. `test_metal_replay_batch35_38_macos.sh`：在最终库上重用 T01–T37 与 T10_debug 共
   38 份 capture，逐份 Replay API + 1-loop CLI；T34/T35 14 类、T36 14 类、T37 43 类，
   合计 71 类异常输入；38 captures × 10 次打开/关闭 lifecycle，脚本最终轮次 resident
   growth **2,064,384 bytes**（门限 64 MiB，先前手工同范围为 835,584 bytes）。T35 放首位满足
   harness 的 no-draw 约定，其余 37 份要求存在 draw。不重录旧 capture，不执行 GUI。

T36 GPU 断言：中心 RGBA `0.2,0.4,0.6,0.8`、角落黑色、visibility count `55,488`、
EndPass `C0=Store, D=Store, S=Store`，整张 400×300 D32S8 每像素 depth `0.75`、stencil
`23`、零 padding。T37 逐字节核对 4096-byte readback（含未触及的 `0xa5` padding），
两个 offset 512/1536、row pitch 256、两次回退/前进、array slice/mip 转换、未触及 texels，
最终像素 RGBA `64,128,192,255`。

本轮扩大旧 capture replay 回归，是因为修改了 pass 状态与共享纹理读回；不是重新执行
全部历史 native/recapture/GUI，也不改变旧批次的人工验收结论。

## 当前审计

- bridge `METAL_NOT_HOOKED()`：216 → 194 → **186**。
- `metal_core.cpp` 实际未处理 chunk：165 → 143 → **128**。
- 本增量减少 8 bridge、15 chunk；整个当前开发批累计减少 30/37。另有 D32S8 API 能力
  和两个 bug 修复，不体现在标记计数中。宏定义自身不算未处理分支。
- 两条未带 options 的历史 chunk 枚举仍保留未处理标记；当前 bridge 并不发出它们，
  不能为了让计数好看而声称它们有独立序列化覆盖。

## 明确边界

- T37 验证的是单采样 RGBA8 2D array、非零 slice/mip/origin、带 padding 的双向传输。
  linear transfer 目前明确拒绝 packed/compressed/depth-stencil 格式和非零 blit options。
  3D/其他普通颜色格式虽然部分公共路径可处理，但本批没有专项 native fixture 证据。
- Store fixture 覆盖 Unknown→Store 和 options=None；MSAA store/resolve 与 custom sample
  选项组合不由本批背书。textureBarrier 验证调用捕获/重放，未模拟复杂读后写 hazard。
- Debug group/signpost 保留字符串与 API 事件，不新增 Event Browser 嵌套分组能力。
- D32S8 readback/picking 不等于完整 Texture Viewer depth/stencil 显示；MSAA、depth remap、
  D16/D24 和颜色格式扩展不在本次范围。已有 InitialContentsList 警告/initial-state 骨架
  未在本轮解决，真实引擎 capture 兼容性仍需独立验证。
- heap/event/counter/parallel render/ray tracing/mesh/function table 仍是后续独立工作。

后续 UI 项见 `QA_BATCH38.md`；T34–T36 原验收说明见 `QA_BATCH35-37.md`。
只有收到用户明确反馈后才能关闭相关 PHASE/BATCH。
