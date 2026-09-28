# BATCH49：回调接通、CPU更新与事件回退

2026-09-26。新增 **2 bridge / 2旧chunk**，剩余 **160 bridge / 96 chunk**。
T49覆盖scheduled/completed；同时修复其暴露的commit前CPU快照与部分重放/回退问题。
范围和边界见PHASE49。未运行qrenderdoc/Computer Use、未提交，未改用户UE路线。

## Test batch

一键：`bash util/buildscripts/scripts/test_metal_capture_batch49_macos.sh`。
只重放：`RENDERDOC_METAL_LAST_TEST=49 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
最终一键完整日志 `/tmp/metal-batch49-final.log`（含主回归和源码兼容复验）。

1. 构建库/CLI/app/demos；native Metal验证层12帧、注入capture12帧，各96个回调的
   身份/次数/状态/闭包释放及GPU数据验证通过。回放验证层3-loop CLI通过。
2. XML八条注册：两个command buffer各两scheduled和两completed，无应用block/指针
   payload；callback参数41/67/101的9-byte差异逐字节核对。
3. Replay API检查两个dispatch、三个fill、一个draw；12-byte参数从7/11/13变为41/67/101再
   回退，覆盖非零初始值；300-byte生产者、
   412-byte output/28-byte padding、反射/绑定/输出usage、前后seek及RGBA41/67/101/255。
4. 新87类异常：56类回调身份，31类CPU更新/初始数据的身份、owner、存储模式、重复、
   越界/溢出/size及payload不一致。每条30秒超时；必须非零、无信号退出。另合法重复注册/
   删除全部回调metadata，各3-loop CLI通过。旧577类保留，总664类。
5. 最终 **50 captures API/CLI、664类异常、500次lifecycle** 通过，resident growth
   **3,178,496bytes**（门限64MiB）；15份Replay API Metal验证层（上批14份加T49）。
6. commit快照时机变化后，源码triangle、argument buffer、pipeline variants三份独立
   兼容capture新录，每份验证层API和3-loop CLI通过；正式T01–T48不重录。
   已纳入T49一键脚本末尾，最终完整脚本在同一库上通过。
7. bash语法、Python编译和git diff --check通过。最初T49回退失败已修复，不计为最终通过。

## 版本与待验

- 库/app副本：`1cd92c21c774bd5c77749d68ccd3d3804649d1ccb8e76d561197e388c1cd079f`。
- T49：`82771996ea03d587fb5b4473fcc5f9c7d481a935db4a64bd5ce24f290a17bff8`。
- GUI executable未改：`3cc9c3b63507006794f87e219921454aaf50469bebb05f72b10f5c4b98b1a4bc`。

**T34–T49及T10 marker共17份**仍待集中GUI，PHASE35–49及相应批次保持开放。
只新增注册API/资源链接、callback前后参数/结果回退和最终画面差异检查；不要在UI中期待
应用block再次执行或“回调时间线”。统一入口QA_CONSOLIDATED。
