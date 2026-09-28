# BATCH54–56：兼容重载、Function 创建与顶层 Argument encoder

2026-09-26，连续功能族波次。起点148 bridge / 87旧chunk，Max1272；集中基线BATCH53。
不启动GUI/Computer Use、不提交、不改用户UE路线。以下为计划，结果随实际验证更新。

| 族 | 范围/边界 | Fixture | 必跑旧T | 断言/负例 | 当前状态 |
| --- | --- | --- | --- | --- | --- |
| A 重载兼容 | indexed instanced无base重载；两个无options旧blit chunk，后者不算新bridge | T54复用T14场景变体；T37派生旧格式 | T02/T14/T21/T25/T37 | draw参数/绑定偏移/像素/回退；legacy字段布局、非法范围/身份 | 定向通过：native/capture各6帧、36负例/2正例；1 bridge/3旧chunk |
| B Function创建 | constants/descriptor同步异步；不含intersection；scalar常量、options0、无archives | T55 | T12/T47/T48/T51 | 特化结果/失败回调/释放/快照/资源依赖；身份/类型/长度 | 定向通过：native/capture各6帧，74负例/1正例、14capture验证层/CLI、50 lifecycle；4 bridge/2旧chunk；新增1272/1273 |
| C Argument encoder | texture/sampler批量设置；带reflection创建；顶层资源数组、单Shared packet offset0 | T56扩展T12族 | T12/T17/T31/T49 | 两个不同资源、非零range、空绑定/部分覆盖、反射/descriptor/usage/pixel/回退；slot/类型/范围 | 定向通过：native/capture各6帧、38负例、14capture验证层/API/CLI；3 bridge，复用已有chunk |

族间跑定向入口的9份代表capture及受影响前族；涉及所有权/初值时追加定向lifecycle。
第3族后集中回归（包含本波新增测试）；影响无法局部界定则提前。失败先修复，不用成功
打开代替数据正确。UI新增项集中登记，最终GUI确认前本批保持开放。

## 实际范围与格式

- 共接通8 bridge、5旧chunk；148→140 / 87→82。新async chunk1272/1273，Max1274；
  原有chunk编号不变，非删除标记或no-op计数。C的批量setter规范化为逐成员1234/1235，
  reflection重载复用1043；不额外计为旧chunk减少。
- A：1147短indexed instanced序列化不包含baseVertex/baseInstance，回放显式归零；
  legacy1207/1209真实缺少options字段，不能套用1208/1210布局。T54保留index offset4，
  vertex bindings offsets8/24、instances2，验证红/蓝两实例及raw index/seek。
- B：通过公开setter/init/copy/reset观察FunctionConstantValues；类簇由公开alloc/init探针
  获取实际类，不硬编码私有类名/存储。未知或未观察对象不伪造快照。按index/range/name
  保存有序写入，保留Metal本身的覆盖规则；同步/异步descriptor和constants均做快照。
  scalar Bool/Char/UChar/Short/UShort/Half/Int/UInt/Long/ULong/Float；options0，无archives。
  异步native原始error/包装身份/借用引用保留，离线不执行应用回调。
- C本波**收窄**：原buffer/constants计划涉及初始化raw bytes、1198 CPU更新、跨提交/seek
  的GPU地址重定位，不能仅补bridge。留下一波独立端到端实现，未移除其标记。
  本次只覆盖帧前CPU编码的顶层texture/sampler；资源数组展开成有名的逐id反射条目，
  当前描述符空间每buffer32成员，越界或成员类型不符先于Metal拒绝。嵌套、buffer成员、
  constants、arrayElement多packet、帧内资源重编码/GPU生成、其他stage扩展未宣称支持。

## 验证节奏与可复现入口

先完成A/B/C各自定向断言和必要负例；B额外5×10 lifecycle通过，growth360448bytes。
每族后最多跑14份定向capture，不同步多份phase交接文档；最后一次集中gate已通过。
详细临时日志 `/tmp/metal-wave54-*`、`/tmp/metal-wave55-*`、`/tmp/metal-wave56-*`。

```sh
# 只重放现有57份capture；全量负例/验证层/API/CLI/lifecycle，并同步app内嵌库，不启动GUI
RENDERDOC_METAL_LAST_TEST=56 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
# 从三份新fixture的native/capture开始，再做同一集中gate；不重录旧正式capture
bash util/buildscripts/scripts/test_metal_capture_batch54_56_macos.sh
# 开发途中定向入口，不能替代上方gate
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t54 t55 t56
```

本波新增36+74+38=148异常，另A两合法兼容变体、B重复常量写入合法变体。
T56自身覆盖数组空绑定/部分覆盖后恢复、零长度range；payload核对零range不产生额外写入。
三份fixture均native/capture各6帧，T55含失败回调、copy/reset和异步后修改/释放；
T56含reflection非空与NULL输出指针。GUI只登记最小可见增量，不重复自动数值验证。

## 最终集中关口（2026-09-26）

- 57 captures（T01–T56 + T10debug）API数据/状态/像素/seek与逐份CLI通过；
  1478异常、29份API Metal验证层、57×10 lifecycle通过，resident growth **1671168 bytes**。
  `/tmp/metal-wave54-56-final.log`；另3份旧源码应用新录API/CLI兼容通过，日志
  `/tmp/metal-wave54-56-source-compat.log`。旧正式capture未重录。
- bash语法、三个新Python脚本编译、git diff检查通过。新capture入口复用已执行的native/
  capture及gate命令，未再重复执行整套入口。未启动GUI/Computer Use，未提交。
- 库与app内嵌库SHA-256：`71cf518d1e5ba3995283c1e5f6d1ce6cbbc7f065794f880fe444a72bb8ec2a88`。
- GUI executable未改：`3cc9c3b63507006794f87e219921454aaf50469bebb05f72b10f5c4b98b1a4bc`。
- T54：`7fa91d16ce14dd5f841d9c3da46c52fc644a2a49c77a7a3ce20bd7456344ca41`。
- T55：`59cd50ecb3f1e33e43c5d5c5bf047dfe3a08d4fd56be6890bd340721ebb570f5`。
- T56：`9cb0d420ac9c959eeaec987e1d4104b987030539b3beba0fc69d25de726438f6`。

UI待验T34–T56 + T10 marker共24份，见QA_PENDING/QA_CONSOLIDATED；本批未关闭。
