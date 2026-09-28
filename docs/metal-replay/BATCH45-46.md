# BATCH45–46：Fence、定时 Present 与 Buffer Markers

2026-09-26。完整一键batch通过；GUI L4未执行，PHASE45/46及本批保持开放。
没有Computer Use、没有运行qrenderdoc、没有提交，保留既有未提交代码和UE路线。

## 接通内容与计数

- T44：newFence及Blit/Compute/Render六种update/wait，共七项。补齐对象包装、资源登记、
  序列化和释放；GPU真实执行同步，支持受控同队列Untracked buffers的跨encoder依赖。
- T45/T46：atTime/afterMinimumDuration两种present，buffer添加/清空debug marker两项。
  普通present共享同一capture登记路径；离线replay只保留时间metadata，不做时钟等待。
- 合计 **11个入口，移除7个bridge、接通9个旧chunk分支**。另外两个compute chunk
  1262/1263尾部追加，Max1264。剩余 **169 bridge / 105 chunk**（批前176/114）。
  eResFence尾部追加，已有成功captures编号不变。
- 防御检查：真实对象类型、active encoder、先行fence update、replay epoch；拒绝无效
  依赖，避免GPU等待挂起。marker范围防溢出、后台reset清理过期注释但保留创建数据。

## 完整 test batch

入口：`bash util/buildscripts/scripts/test_metal_capture_batch45_46_macos.sh`。
只重放：`RENDERDOC_METAL_LAST_TEST=46 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
最终完整日志：`/tmp/metal-batch45-46.log`。

1. 库/CLI/app/demos构建；三模式各native Metal验证层5帧、注入capture8帧。
2. XML核对四个fence、十条update/wait、render stage、fence复用、present变体/time、
   帧内marker范围和顺序、后台注释reset压缩；新三份均验证层3-loopCLI。
3. Replay API精确检查304/308-byte数据、末尾padding、GPU依赖写入、VS/CS/Copy usage，
   前进/回退seek及停在每条fence API事件，最终像素15/16/17/255；marker-only172-byte
   buffer全部0x72，不能被漏捕。Present action不能误选End of Capture的合成Present。
4. 新 **87类**畸形capture（T44 56、T45 21、T46 10）：空/未知/错误类型ID、重复创建ID、
   ended encoder、缺producer/future/same-encoder fence、非法stage、range溢出、负数/
   NaN/无穷时间。30秒子命令超时，信号退出不算拒绝；保留原342类，累计 **429类**。
5. 最终 **47 captures API/CLI、470次lifecycle**通过（T01–T46+T10_debug），resident
   growth **1,638,400 bytes**，门限64MiB。历史captures未重录。
6. replay Metal验证层共 **12份**：T01/T02/T09/T12/T19/T40/T41/T42/T43/T44/T45/T46；
   新三份已并入共享回归。bash语法、新Python编译与git diff --check通过。
7. 普通present共用路径另定向复验T00空帧：验证层3-loopCLI及T00/T44/T45/T46×10
   lifecycle通过，resident growth360,448bytes。该40次不混入上方470次主batch统计。

## 版本

库与app内嵌库相同：
`33c4399361f7633975c41d54a831ac9521f78edff8d9a8d15eaad1553e887688`。

- T44：`caa028676f688713f42492bc6935bf5dd40e3103365e0d57d776c94cb917e5cf`。
- T45：`9e71f6b2188370df57f9828635452134d6d28d626eebc73d6108d8a8de62924a`。
- T46：`7b1fbea45e1fe15b291765b7721799b3cde09081fd6bdf6addb28c86d40c0335`。

GUI代码本轮未改动，使用上批VS Storage Buffers页面；新事件/参数仍须用户观察。
当前全部待验 **T34–T46 + T10 marker，14份capture**，详见QA_CONSOLIDATED。
旧11份检查不删不重置；T45/T46只做相对T44的最小差异项。

边界：fence要求当前回放帧内、不同encoder的先行update；捕获前/外部状态、跨队列、
event/shared event、Tile/Object/Mesh未覆盖。marker只是API metadata，无专用范围高亮，
不重建native注释表；present参数保留不代表屏幕调度精度已验证，也不宣称真实UE兼容。
