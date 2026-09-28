# BATCH53：ICB GPU 操作、空命令与初值回退

2026-09-26。接通 **1 bridge / 3 旧 chunk**：单命令 reset，以及 blit ICB
reset/copy/optimize。后三项 bridge 原本已转发，不能重复算作 bridge 减少。
剩余 **bridge 149→148，旧 chunk 未处理分支 90→87**。追加 chunk1270 单命令 reset、
1271 不可重建初值诊断；Max1272。诊断 chunk 不算新增功能入口。
无 qrenderdoc GUI/Computer Use、无提交，保留全部既有修改及用户 UE 路线文件。

## Phase batch

1. 三项 blit 调用真正编码 native GPU 操作；记录资源依赖、非零范围、目的偏移，更新
   replay 命令元数据。支持跨 ICB copy 和同 ICB 不重叠 copy。单命令 reset 支持初始化
   清空/重编码；空命令有 execute 子项但不冒充 draw，复制后保留双 buffer 和 indexed 状态。
2. 每个 replay epoch 恢复 CPU 初始 ICB 命令，避免 GPU copy/reset 的结果污染后退；
   OnlyDraw 与 WithoutDraw 共用 epoch。真实 optimize 不能在同提交重复覆盖重叠区间，
   非法范围在调用 Metal 前拒绝，零长度合法。详见 PHASE53。
3. 仅支持 CPU 初始化的 Shared render ICB；创建阶段拒绝其他存储模式。捕获前已有 GPU
   写入且未通过完整 CPU reset 重建的初值，原生继续工作、离线明确拒绝。不支持通用
   GPU shader 生成 ICB、Private/compute ICB，也未改变现有 CPU 编辑的初始化记录模型，
   不声称支持帧内 CPU 重编码与 GPU 提交交错。
4. 修复旧 inheritance 异常输入缺失 render pipeline 时的原生崩溃。六份旧 ICB 异常
   脚本增加 30 秒超时及禁止信号退出，避免将崩溃误记为正常拒绝；T22/T24 两个空命令
   用例现在属于合法输入，转为三轮正例，未删除覆盖。

## Test batch（最终代码与构建实测）

一键：`bash util/buildscripts/scripts/test_metal_capture_batch53_macos.sh`。
仅重放：`RENDERDOC_METAL_LAST_TEST=53 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
最终完整日志：`/tmp/metal-batch53-final.log`。

1. demos、库/CLI/app 构建及内嵌库同步通过。T53 native Metal 验证层 12 帧、注入 capture
   12 帧、XML 及验证层 CLI 三轮通过。每帧四阶段：洋红 → 红绿蓝 → 红黑蓝 → 红黑蓝；
   native 每阶段检查三带中心像素。两 reset、两 copy、三 optimize；14 子项为 9 draw / 5 empty。
2. API 检查四父 execute 的范围、两种 PSO、slot0/1 offset16/0、UInt16 index offset4/
   size12、五个 80-byte 参数包及 padding、16-byte tint、被清空黄命令无 draw usage。
   四阶段及七个 GPU API EID 往返通过，包含最后阶段回到初始洋红。
3. 新增 **194 类异常 + 三种合法变体**（零长度 GPU 操作、移除优化、完整目标 reset）。
   旧 **70 类 ICB 异常 + 两种空命令正例**也纳入联合入口；此前 1066 类保留。
   合计 **1330 类异常**干净拒绝，无崩溃/超时。合法变体各三轮。
4. **54 captures API/CLI、540 次 lifecycle** 通过（T01–T53 + T10 marker，不含 T00），
   resident growth **2,228,224 bytes**，门限 64 MiB。**26 份 API Metal 验证层**通过，
   即之前 18 份加 T20/T22–T27/T53。旧正式 captures 未重录。
5. 独立真实 fixture `t53_preframe_unknown_capture.rdc` 加入捕获前 GPU copy：native
   验证层与注入各三帧通过，离线在 `unavailableInitialContents` 干净拒绝。
   此项不计入 54 份成功 capture 或 1330 类变异输入，也不列入 UI 打开清单。
6. bash 语法、Python 编译与 git diff --check 通过。计数方法：所有 *bridge.mm 中
   METAL_NOT_HOOKED() 为 148；metal_core.cpp 中 METAL_CHUNK_NOT_HANDLED() 为 88 处，
   减去宏定义后 87 个实际分支。未以注释或新增可处理 chunk 冲抵旧缺口。

## 最终版本与后续 QA

- 库/app 内嵌副本：`a5dcf57234590bedcd2f3b8f8764bb3a9aa9982fdf20dd0ca7c9760e511083aa`。
- T53：`6ad98408529a6a23355d92c03f7e0cac97e8a9bd451351ca9517ecb98fe60724`。
- GUI executable 未改：`3cc9c3b63507006794f87e219921454aaf50469bebb05f72b10f5c4b98b1a4bc`。

**T34–T53 与 T10 marker 共 21 份**待集中人工 GUI；PHASE35–53 和相应 batch 保持开放。
T53 的事件、空项、复制状态与四阶段画面已合并到 QA_CONSOLIDATED，不重复人工计算
自动已核对的数据。旧 T20–T27 已验结论保留，新行为集中验 T53。下一编号 T54/PHASE54。
