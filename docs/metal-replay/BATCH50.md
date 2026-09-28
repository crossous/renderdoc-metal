# BATCH50：纹理 CPU 读回记录与子资源同步

2026-09-26。新增 **2 bridge / 3旧chunk**，剩余 **158 bridge / 93 chunk**。
两种getBytes保留原生CPU行为并捕获元数据；synchronizeTexture补齐native replay、
Managed/当前encoder/子资源校验和资源保留。具体支持边界见PHASE50。
未运行qrenderdoc/Computer Use、未提交，未改用户UE路线文件。

## Test batch

一键：`bash util/buildscripts/scripts/test_metal_capture_batch50_macos.sh`。
仅重放：`RENDERDOC_METAL_LAST_TEST=50 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
最终完整日志：`/tmp/metal-batch50-final.log`。

1. 库/CLI/app/demos构建及内嵌库同步通过。Native Metal验证层12帧、注入capture12帧，
   各48次CPU读取，全部有效字节和host行/图像padding/哨兵通过；Replay验证层3-loop CLI通过。
2. XML四次getBytes、三次Managed同步，完整region/pitch/slice/mip与资源ID匹配；
   CPU读回不含指针/输出数据。只经读取或同步引用的纹理被保留；68-byte消费者buffer的
   17-byte更新payload逐字节匹配43/79/113/151/152。
3. Replay API核对七个API事件顺序、五纹理/一个buffer保留、两GPU目标mip/slice及
   未改动子资源、两独立2D资源、CPU派生buffer/padding和事件回退、usage、RGBA43/79/113/255。
   3D的实际两层CPU读取由native/capture验证，不计作3D Replay API/GUI显示通过。
4. 新增176类异常：四次读取的空/未知/错类型资源、region/mip/slice、pitch/溢出，
   同步的错encoder/纹理/子资源、Shared target和结束后的encoder；Private CPU读取拒绝。
   每例30秒超时，必须非零且无信号退出。另删除全部CPU读元数据、合法紧凑pitch两变体
   各3-loop CLI通过。旧664类保留，总 **840类**。
5. 最终 **51 captures API/CLI、840类异常、510次lifecycle** 全通过；resident growth
   **999,424bytes**，门限64MiB。16份API Metal验证层（上批15份加T50）。正式T01–T49不重录。
6. bash语法、Python编译、git diff --check通过。最初Shared synchronizeTexture被原生
   验证层拒绝后已改为Managed并补负例；失败探索不计入通过结果。

## 最终版本 / 后续 QA

- 库/app内嵌副本：`a648ce05f5769f9ce130881f21fddcadb75829298ce1ee2fda66ef507fcd9bc3`。
- T50：`f56406953e8d2dffe9abfa5acf80e3ed95021bc5a24f0265789ed1ee6592e09f`。
- GUI executable未改：`3cc9c3b63507006794f87e219921454aaf50469bebb05f72b10f5c4b98b1a4bc`。

**T34–T50及T10 marker共18份**待集中GUI，PHASE35–50与相应batch保持开放。
新增三项最小QA已合入QA_CONSOLIDATED，不重复旧已验批次；下一编号T51/PHASE51。
