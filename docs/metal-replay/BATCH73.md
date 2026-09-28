# BATCH73：Render heap residency 声明

2026-09-27。接通 `MTLRenderCommandEncoder` 的 `useHeap`、`useHeaps` 及两种
`stages` 重载：四个 bridge 和四个旧 chunk 都记录 heap 身份、render encoder 与阶段，
在离线回放中向真实 Metal encoder 重发。保持原生调用顺序和父资源引用；单个入口只接收
一个 heap，批量入口最多32个，坏身份/类型/阶段/encoder明确拒绝。T70 GPU indirect
ICB range 的未解决边界不变。原始剩余标记 **92 bridge / 55旧chunk**；若按功能缺口
计入 T70，则为 **93 / 56**。

T73使用同一自动 Private tracked heap 的1×1纹理，在三个提交的 draw 前分别执行
单 heap、批量 heap、两种 fragment-stage 声明。五次 Metal Validation 原生运行、
截帧、四 chunk/Heap ID/阶段 XML 核对、三轮 CLI replay、API 像素/事件回退、
22种损坏声明、T64/T72定向回放均通过。入口：
`bash util/buildscripts/scripts/test_metal_capture_batch73_macos.sh`，日志
`/tmp/metal-batch73-targeted.log`。

最终集中终端回归：**73份成功 capture API/CLI、1969畸形输入、730次 lifecycle**，
resident growth 1015808 bytes；`/tmp/metal-batch73-full.log`。库及app内嵌库 SHA-256
`57efba093c5337d648b5580edb8113cc85639a7b7a3e688145ff37ae2b79436f`，
T73 capture SHA-256 `2bff846250a6954de6ffe4b391cd77afaf9f99d0eb0f2deaf3801ee44b3bc5b6`。
未运行 GUI/Computer Use，未提交/推送；T73加入累计人工QA清单。
