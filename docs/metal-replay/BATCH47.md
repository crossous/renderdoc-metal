# BATCH47：同步 Pipeline Options / Reflection / Compute Descriptor

2026-09-26。本批实现3个bridge、3个旧chunk；剩余 **166 bridge / 102 chunk**。
具体范围与限制见PHASE47。没有Computer Use、没有运行qrenderdoc、没有提交；既有改动保留。
T47及旧T34–T46/T10 marker继续待GUI L4，阶段和批次不关闭。

## Test batch

一键：`bash util/buildscripts/scripts/test_metal_capture_batch47_macos.sh`。
只重放：`RENDERDOC_METAL_LAST_TEST=47 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
最终日志：`/tmp/metal-batch47-final.log`。

- 库/CLI/app/demos构建；native验证层5帧、capture8帧；原T01–T46/T10_debug不重录。
- XML核对6个pipeline独立ID、options3/0、descriptor label、maxThreads64、整倍数与mutability；
  3-loop CLI Metal验证层；Replay API核对四dispatch/two-draw、全部buffer/padding、像素和回退。
- T47新增84类异常capture，以及options1/2两种合法重放变体。旧429类仍全部保留。
- 最终 **48份capture API/CLI、513类异常输入、480次lifecycle** 通过；resident growth
  **491,520 bytes**，门限64MiB。48份为T01–T47和T10_debug，T35无draw先开。
- **13份Replay API Metal验证层**：T01/T02/T09/T12/T19/T40/T41/T42/T43/T44/T45/T46/T47。
  最终一键脚本完整通过。早期新增dispatchThreads负例时旧fixture缺该调用的中间日志不算通过。
- bash语法、Python编译和git diff --check。

## 固定版本

- 库与app内嵌库：`16c921313c2f1b8d6fa8b2e1a7e1856710f2c4fa74ac99b5d4ac5728ed4ce8a6`。
- T47：`a28eef0f401b4493cefbabe094a59392042ef8652cabdc73bf0a0466d20242fa`。
- GUI executable未改：`3cc9c3b63507006794f87e219921454aaf50469bebb05f72b10f5c4b98b1a4bc`。

## 后续GUI最小差异

在同一qrenderdoc进程打开T47，选首/末dispatch和两次draw。检查CS/VS/FS shader与buffer
链接、参数元数据、前后事件切换、第一draw只画左半/第二draw完整暗色画面、无错误状态栏。
当前全部待验 **T34–T47 + T10 marker，共15份capture**；以QA_CONSOLIDATED为总入口。
不新增专用reflection窗口要求，不重复所有既有fixture的数值验证。
