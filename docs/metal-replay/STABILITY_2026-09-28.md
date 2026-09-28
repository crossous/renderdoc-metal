# 2026-09-28 GPU panic 渐进复测记录

目标：在不运行 `LAST_TEST=300` 累计压力脚本、不启动 GUI 的条件下，
区分单份正常回放与长批次/畸形输入/重复释放的风险。使用最终库
`ac97012d33b5…`，每组 `MTL_DEBUG_LAYER=1` API readback + CLI单次回放，
检查 `/Library/Logs/DiagnosticReports/Retired` 的新panic。

| 捕获编号 | 数量 | 定向日志目录后缀 |
| --- | ---: | --- |
| T01–20 | 20 | `OMaGUS` |
| T21–40 | 20 | `c0qf0s` |
| T41–69 | 29 | `AhAoo7` |
| T71–90 | 20 | `bEcHXN` |
| T91–110 | 20 | `S93P1P` |
| T111–132 | 22 | `qnvmrZ` |
| T134 | 1 | `PsYzcv` |
| T135–153 | 19 | `NEWvxe` |
| T156–157 | 2 | `I6adb6` |
| T159–180 | 22 | `TEXZ6l` |
| T181–200 | 20 | `qB5y2F` |
| T201–220 | 20 | `MrB0EW` |
| T221–240 | 20 | `ezoM8V` |
| T241–260 | 20 | `MQ2Hmv` |
| T261–280 | 20 | `IHRi4y` |
| T281–300 | 20 | `RTdRPO` |

合计295份正常截帧均通过。T70/T133是预期拒绝，T154/T155/T158
无正常捕获；`t10_debug`不计在上述295份。分组均在
`build-macos-debug/metal-targeted.<后缀>/`保留日志。

全部295份正常截帧均已分组做至少10次生命周期打开，另加T10_debug。
每组最多约30份捕获，各在独立进程执行，不等于295份×10在**同一进程**。
T01–20、T21–40、T41–69、T71–90、T91–110、T111–132、
T134+T10_debug、T135–153+T156–157、T159–180、T181–200、
T201–220、T221–240、T241–260、T261–280、T281–300全部通过；
各组resident增长均小于64 MiB。T35基准和若干哨兵重复覆盖，
T01/T09/T53/T144/T300各CLI×5通过。

畸形输入也按功能族单独执行，每族后检查新panic：T34–53的命令创建、
动态状态、blit、sampler、compute、barrier、fence/present、pipeline、
binary library、command handler、texture readback、异步事件及ICB共
**1331例**；随后T60–62设备argument encoder/no-copy/purgeable共54例，
T64/65/68/69共享资源及handle共54例，T71–73 heap共52例。
T54–59重载/函数变体/argument data/纹理视图共407例，T63/66/67
动态stride和细分共72例，T74–90的tile/mesh/object/间接绘制共246例，
T91–100的object threadgroup/rate-map/mesh结合共197例，T101–118的
counter/binding/dynamic library/heap placement/nested argument共213例，
T120–132的visible function table与heap alias共102例。其后又单独
覆盖T135–153的AS build/copy/refit/TLAS/IFT共589例，以及T156–163的
opaque/嵌套表/驻留共136例。
即本轮各脚本报告的异常输入数量之和**3453例**，均按预期拒绝且无
进程崩溃/卡死；部分套件还各有合法变体正例。T88–90脚本原先把三份
捕获都打印为“T88”；T93/T94原先误打印“T91–T92”，本轮均修正输出
标签并重跑确认。T101–118的213例由T101–115共170例及T116–118
共43例构成。此数为独立
套件的报告数量之和，**不是**完整累计脚本的去重总数，且与累计脚本
内部硬编码计数相差1例；后续以各套件实际输出为准。期间始终只有
06:26那份`IOGPUResource` panic。

归因核查：panic头记录本地时间06:26:38，快照中出现`renderdoccmd`；
但可找到的`/private/tmp/metal-batch157-full.log`创建于06:29:41、
最后写入06:30:26，**晚于**panic。它停在T71不能据此指认为
06:26触发点。该时间窗的统一日志没有给出可用的`renderdoccmd`事件。
因此“测试可能触发内核断言”仍是风险判断，不能定位到某个capture，
更不能判定为应用释放顺序错误或纯系统限制。

结论限于**正常截帧分组单次回放、有界生命周期循环与上述分组负例**。
未运行剩余累计负例、全部295份×10同一进程、UI或UE实际截帧；不能
据此归因06:26 panic，也不能称完整压力回归通过。下一步继续按功能族
扩大负例；任何新panic即停止并记录最后一组。鉴于长时累计批次的
触发点未知，暂不把它作为无人值守的默认回归。
