# 批次 35–37：render 绑定、命令创建与动态状态

本文件保留首轮 22 对入口的检查点。后续增量、最终库/计数与联合回归见 `BATCH38.md`；
T36 已增加 visibility/store/barrier 与 D32S8 API，当前异常脚本为 14 类，最终未接通
计数为 186/128。GUI 清单已同步新版 capture，请将两个 QA batch 合并执行。

状态：T34/T35/T36 功能与终端自动验证已完成；按用户要求，本轮没有启动
qrenderdoc 或使用 Computer Use。三项 GUI L4 均登记在 `QA_PENDING.md`，因此
PHASE35–37 和本批保持开放。一次性人工步骤见 `QA_BATCH35-37.md`。

## 功能边界

1. T34：接通 render encoder 的 vertex/fragment inline bytes、buffer 数组绑定和
   vertex buffer offset 更新，覆盖资源 usage 与 Pipeline State 绑定。
2. T35：接通有限容量 command queue、unretained/descriptor command buffer、
   direct dispatch type、compute pass descriptor 创建入口与 `waitUntilScheduled`。
3. T36：接通 viewport/scissor 数组、depth clip、depth bias、triangle fill 和
   blend constant 六项 render 动态状态，并接通 command buffer/render encoder 的
   五项 debug group/signpost 入口。

本批没有把 heap、shared event、counter、parallel render、ray tracing、tile shader
或 function table 伪装成支持；这些仍按剩余标记审计分层推进。

## 最终自动验证

- `test_metal_capture_batch35_36_macos.sh`：T34/T35 native、capture/XML、Replay API、
  逐份 3-loop CLI replay、14 类畸形 capture 拒绝，以及两 capture × 10 轮 lifecycle。
- `test_metal_capture_phase37_macos.sh`：T36 native、capture/XML、Replay API、3-loop
  CLI replay、5 类畸形 capture 拒绝，以及 T34/T35/T36 × 10 轮 lifecycle；最终
  最后一轮 resident growth 606,208 bytes。
- T34 最终像素、四个 buffer offset、inline/batch structured chunks 和 usage 均断言；
  T35 两个 dispatch 的 pipeline/output offset、`waitUntilScheduled` structured chunk
  与最终 `17,17` 均断言；T36 的
  debug group/signpost 字符串、viewport/scissor state、blend/depth-clamp/scissor
  输出均断言。
- `renderdoc`、`renderdoccmd` 和 qrenderdoc app 均由终端构建成功；`git diff --check`
  通过。没有执行 GUI 交互验证。
- 共享路径定向回归 T01/T02/T04/T07/T17 的 Replay API 与逐份 CLI 均通过；T00、
  这五份旧 capture、T34/T36 的 8 captures × 10 lifecycle 通过，growth 245,760 bytes。

## 标记审计结果

本轮开始时 bridge 中有 216 个 `METAL_NOT_HOOKED()`，`metal_core.cpp` 有 165 个
实际 `METAL_CHUNK_NOT_HANDLED()` 分支。T34/T35/T36 首轮共接通 22 对入口，当时分别为
194 和 143。计数只是范围指标；本批只把可捕获、可重放并可自动判定的入口记为完成。

## 关闭条件

用户后续按 `QA_BATCH35-37.md` 在同一 qrenderdoc 进程检查三份正式 capture；逐项
明确符合后，更新 `QA_PENDING.md` 并关闭三份 PHASE 与本批。T34 inline bytes 和
T36 部分动态状态目前主要由 API Inspector 与输出验证，若期望它们在 Pipeline State
拥有独立可视字段，应作为 UI state-model 增量开发，而不是误报现有页面已展示。
