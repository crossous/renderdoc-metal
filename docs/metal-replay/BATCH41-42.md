# BATCH41–42：Compute 参数/共享内存，Blit descriptor/optimization

2026-09-26。定向自动验证与最终整批脚本全部通过。
GUI L4 延后，PHASE41/42 与本批保持开放。未使用Computer Use、未启动qrenderdoc，未提交。

## 本批功能

- T40接通三个此前隐式forward的compute入口：inline bytes、buffer offset和动态threadgroup
  memory。保留真实/inline绑定身份与长度、offset的事件快照；检查reflection必需资源、
  设备和pipeline线程数、static+dynamic memory总量，并在新encoder重置临时状态。
  新chunk1252–1254追加，不重编号旧capture。
- T41接通blit descriptor创建，以及CPU/GPU各两种texture optimization提示，共五项。
  修复CPU slice/level错误chunk ID与四种hint缺少frame引用的问题；独立hint-only纹理
  验证未被其他copy/draw偶然保活。counter sample attachments明确拒绝，不假装已实现。
- 补充replay侧Metal API Validation暴露共有display参数布局：CPU结构60bytes、MSL要求
  64bytes。已显式补齐尾部padding、添加compile-time大小断言；T01/T02/T09/T40/T41
  replay验证层通过，覆盖2D、array/cube/mip、mesh及新场景。此前普通replay不会报出该问题。
- 剩余bridge/chunk **181/119**（批前182/124；会话基线216/165）。compute新增入口没有
  旧标记，因此不能用标记降幅代替功能覆盖统计。共八个新capture/replay入口。

## 终端测试 batch

完整入口：`bash util/buildscripts/scripts/test_metal_capture_batch41_42_macos.sh`。
只重放已有fixtures：`RENDERDOC_METAL_LAST_TEST=41 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。

1. build app/CLI/demos；T40与T41均启用Metal API Validation跑5帧native，再8帧注入capture。
2. XML核对inline拷贝值1/2/5、offset序列、threadgroup长度/清空、descriptor flag和四个hint
   chunk ID1220/1221/1222/1223；新capture各3-loop CLI。
3. T40五次dispatch完整80-byte精确结果与padding、连续/回退seek、slot0/2的资源身份/
   offset/剩余长度、最终像素。T41复用原T37所有copy/readback/seek断言，再检查只由hint
   引用的13×7纹理初始内容；原T37 capture未重录。
4. T40 **41**、T41 **23** 类新畸形capture无crash拒绝；保留原175类，累计 **239**。
   negative脚本有30秒超时，信号退出不算正常拒绝。T40包括inline/未绑定offset、短常量、
   缺少threadgroup分配、新encoder状态泄漏、总量超限与非法dispatch。
5. 联合 **42 captures**（T01–T41+T10_debug）Replay API/CLI、**420**次lifecycle。显示布局
   修复前两次回归resident growth分别 **1,196,032 / 802,816 bytes**，
   门限64MiB。脚本语法、Python编译、`git diff --check`通过。
6. 共享回归入口已加入上述五份capture的replay验证层；显示布局修复后的最终库上
   **42 captures / 239类异常 / 420次lifecycle全部通过**，resident growth **2,736,128 bytes**。

## 版本与限制

当前库 `b30a0e4cb6ee…`；T40 `73a3398cec48…`；T41 `35745dc5f7ab…`。完整版本与GUI顺序
以 `QA_CONSOLIDATED.md` 为准，旧batch hashes保持历史含义。

正式构建与app内嵌库一致：
`b30a0e4cb6ee250be2833c000bdcf2b2674711bb0bc1a7192f1b51bb46a3e334`。
T40：`73a3398cec4894c82cfcfd4cf062401702ca658be17e9e1bd9fce8b68451e1fc`。
T41：`35745dc5f7ab8459cefd8e258776996ab71770ef1dcdd69ef3b6b673e4cde171`。
完整测试日志 `/tmp/metal-batch41-42.log`；display padding修复后的最终回归日志
`/tmp/metal41-42-final.log`。capture无需因仅replay显示shader修复而重录。

原生验证层发现offset接口要求已有MTLBuffer，不能对inline绑定使用；实现明确拒绝这种
畸形capture。inline原始值可在structured API查看，但没有实现inline常量解码UI或threadgroup
专用UI字段。reflection最小大小不代表任意shader动态索引都安全；原compute布局限制保留。
optimization不是数据copy，也不应伪造copy action或声称性能提升；Managed/计数器未覆盖。

待人工总单扩展到T34–T41 + T10 marker，共九份capture。旧七份的待验项全部保留，
新增只列差异项，共用同一app进程和通用检查。现在不执行、不催促用户，不关闭任何未验批次。
