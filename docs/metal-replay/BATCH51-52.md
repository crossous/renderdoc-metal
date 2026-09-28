# BATCH51–52：异步创建与 Event 同步

2026-09-26。新增 **9 bridge**：六种异步 Library/PSO 创建、newEvent、signal、wait。
bridge **158 → 149**，达到小于 150 的目标；实际旧 chunk 未处理分支 **93 → 90**。
异步创建追加六个可回放 chunk 1264–1269，Max1270；Event 复用旧 1032/1062/1063。
这是已接通的受限功能路径，不代表 Metal 完整支持；边界见 PHASE51 / PHASE52。
未启动 qrenderdoc/Computer Use、未提交，保留既有 dirty 修改及用户 UE 路线文件。

## Phase batch

1. **T51**：原生异步回调返回包装资源，保留 error/reflection、descriptor 快照和父资源
   生命周期，失败结果不建资源；离线只重建成功结果。异步 source 仅支持 options=nil。
   PSO/Event 扩展独立 native 所有权；共享 source serializer 加校验并补设备类型初始化。
2. **T52**：真实 Event signal/wait replay、先 signal 后 wait 校验、逐 replay epoch 重建
   事件防止 seek 残留值。覆盖两队列三提交，不支持 future-signal waits、SharedEvent
   或外部事件状态，也没有通用多队列调度重构。

## Test batch（最终构建实测）

一键：`bash util/buildscripts/scripts/test_metal_capture_batch51_52_macos.sh`。
仅重放：`RENDERDOC_METAL_LAST_TEST=52 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
最终完整日志：`/tmp/metal-batch51-52-final.log`。

1. 库/CLI/app/demos 构建与内嵌库同步通过；两 fixture 各 native Metal 验证层 12 帧、
   注入 capture 12 帧、XML 检查及验证层 3-loop CLI。T51 每次七个 completion（六成功/
   一非法 MSL 失败），T52 每次 72 个 signal/wait，native 与 capture 数据均断言。
2. T51 API 核对 3 dispatch / 2 draw、五 PSO 身份、shader 反射/bindings/usage、428-byte
   数据及 44-byte padding、descriptor 快照、左右半屏与回退。最终 RGBA31/83/127/255。
3. T52 API 核对两 Sync 资源、六条同步 API、两队列三提交、444-byte output 的 96 uint
   与 60-byte padding、同步点/dispatch/draw 事件回退，最终 RGBA51/83/115/255。
4. 新增 **226 类异常 = T51 104 + T52 122**，每例有 30 秒超时，干净拒绝且无崩溃/挂起；
   四个合法变体各 3-loop 通过。旧 840 类保留，联合 **1066 类**全部通过。
5. **53 captures API/CLI、1066 类异常、530 次 lifecycle** 全通过；resident growth
   **1,736,704 bytes**，门限 64 MiB。18 份 API Metal 验证层（旧 16 + T51/T52）。
   正式 T01–T50 未重录；另新录 triangle/argument buffer/pipeline variants 三份兼容
   capture，验证 PSO/library/function 所有权变化后的源库路径，API/验证层 CLI 均通过。
6. bash 语法、Python 编译、git diff --check 通过。计数方法：所有 *bridge.mm 中
   METAL_NOT_HOOKED() 为 149；metal_core.cpp 中 METAL_CHUNK_NOT_HANDLED() 共 91 处，
   减去宏定义后为 90 个真实分支。没有把注释修改或新增可处理 chunk 算作旧分支减少。

## 最终版本 / 后续 QA

- 库/app 内嵌副本：`8dbbe2d60146c78cb1b929196b3570d020fa4f7faa5fa54670d8566c2cea4ef6`。
- T51：`e0fc940f6ce0ca5e3003b4bd8da4f34c16e16c809249aa5043c340f6a10d2fec`。
- T52：`e0116d679d5fa79eadc565e5280a3f271b9e7d28938366ed52aff8d3ab82637f`。
- GUI executable 未改：`3cc9c3b63507006794f87e219921454aaf50469bebb05f72b10f5c4b98b1a4bc`。

**T34–T52 及 T10 marker 共 20 份**待集中人工 GUI，PHASE35–52 与相应 batch 保持开放。
新项并入 QA_CONSOLIDATED；数值/回调/异常已自动核对，人工仅查 UI 可见差异，不重复
执行测试程序或旧已验功能。下一编号 T53/PHASE53。
