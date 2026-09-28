# BATCH38 后续人工 QA（本轮未执行）

与 `QA_BATCH35-37.md` 合并在后续 chat 检查即可。当前待验 T34/T35/T36/T37，以及
T10_debug 的三个新 marker；原 T10 已通过的 GUI 不撤销。

## 固定版本

仓库 `/Users/crossous/Developer/renderdoc-metal`；app 为
`build-macos-debug/bin/qrenderdoc.app`。最终库与 app 内嵌库相同：`47e0635e8736…`。

| capture（均在 captures/metal-smoke） | SHA-256 前缀 |
| --- | --- |
| t34_capture.rdc | e6ee77deb50a |
| t35_capture.rdc | 7fbed8c95df6 |
| t36_capture.rdc（新增 visibility/store/barrier） | 436b3262fef5 |
| t37_capture.rdc | f87cbbc8ca00 |
| t10_debug_capture.rdc | 2d4a8d609bae |

T36 重录后事件号已经变化，以 API 名称定位，不沿用早期截图的 EID。

## T36 增量

1. 保留旧单的 viewport/scissor/depth clamp/blend 检查。额外确认 API Inspector 中
   visibility `mode=2, offset=0`，color/depth/stencil store `1`、options `0` 和 textureBarrier。
2. 选择 draw/EndPass，8-byte visibility buffer 按 uint64 显示 `55488`；EndPass 摘要
   包含 `C0=Store, D=Store, S=Store`。中心 RGBA 约 `0.2,0.4,0.6,0.8`，角落黑色。
3. 前后切换 BeginPass、store setter、draw、EndPass，确认不崩溃。D32S8 的整图 raw
   readback/pick 已由终端验证；本批没有新增深度显示 shader，不把深度缩略图成功显示
   当成已实现或必要通过条件。若 UI 能触发 picking，可记录 R=0.75、G=23/255 的结果。

## T37 新场景

1. 清空 Event Browser 筛选，检查 fill、两次 buffer→texture、whole copy、range copy、
   两次 texture→buffer，最后一个 draw；API Inspector 可见 pitched transfers/readback marker。
   Marker 当前是普通 API 事件，不要求折叠分组。
2. API Inspector 核对上传 offset=256、row=256、image=512、size=3×2×1；回读 offsets
   512/1536；range copy 为 source slice1/mip1 → destination slice0/mip1、counts1/1。
3. 在 readback buffer 按 hex/uint8 查看：fill 后全 `a5`；首次 readback 后 offset512
   起为 `40 80 c0 ff 41 80 c0 ff 42 80 c0 ff`，下一行在 offset768（G=81）；第二次
   同样内容从1536开始。其它 padding 保持 `a5`。回到 fill 再到第二次 copy 数据可重复。
4. 目的纹理选择 slice0/mip1（8×4），只有 x=1..3、y=1..2 的六个 texel 非零；最终
   backbuffer 是 RGBA `64,128,192,255` 对应纯色。确认资源链接、参数、画面与状态栏。

## T10 marker 最短复验

打开独立的 `t10_debug_capture.rdc`，API Inspector 中确认
`T10 blit debug group`、`T10 blit operations` 和 pop；原 blit 画面不变，不要求重做旧 T10
全部人工项目。

反馈可写 `T34/T35/T36/T37 和 T10 marker 全部符合`，或写 T 编号、调用名、实际现象。
未经反馈，本单所有 GUI 项都保持待验。自动测试说明和限制见 `BATCH38.md`。
