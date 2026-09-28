# BATCH48：Binary Libraries 与 Shader 对象生命周期

2026-09-26。新增 **4 bridge / 4旧chunk**，剩余 **162 bridge / 98 chunk**。
T48实际覆盖file/URL/data/bundle四入口及修复后的原default library路径，详情PHASE48。
未执行qrenderdoc/Computer Use，未提交；旧代码、capture、用户UE路线改动保留。

## Test batch

一键：`bash util/buildscripts/scripts/test_metal_capture_batch48_macos.sh`。
只重放：`RENDERDOC_METAL_LAST_TEST=48 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
正式日志：`/tmp/metal-batch48-final.log`；源码路径兼容复验：`/tmp/metal-batch48-source-compat.log`。

1. 构建库/CLI/app/demos；本机metal/metallib工具编译10765-byte库，生成专用测试app和bundles。
   Native Metal验证层5帧、capture8帧；异常native返回值、library/function提前release已覆盖。
2. 回放前移走整个生成目录，检查原文件路径不可用，五份嵌入payload完全一致；5库7shader，
   失败的source/file/data/bundle/function创建不产生无效chunk。验证层3-loop CLI通过。
3. Replay API逐元素验证676-byte输出/36-byte padding、五dispatch+draw的前后seek，
   distinct shader身份、parent依赖、反射/绑定/usage，以及最终RGBA29/61/97/255。
4. 新64类畸形capture：身份、重复ID、空/短/bad magic/header-only库，以及缺失函数；
   改origin到不存在路径仍成功两轮回放。保留旧513类，累计577类。
5. 最终 **49 captures API/CLI、577类异常、490次lifecycle** 全部通过；resident growth
   **1,228,800bytes**，门限64MiB。14份Replay API Metal验证层：上批13份加T48。
6. 由于Library/Function ownership变化，另对源码编译路径的triangle、argument buffer、
   pipeline variants新录兼容capture，旧正式capture不重录；各自验证层API和3-loopCLI通过。
   `test_metal_source_library_compat_macos.sh` 独立复跑通过并已纳入一键脚本。主batch与此
   补充检查分段完成、使用同一最终库；不将额外3份混入49份主回归统计。
7. bash语法、Python编译和git diff --check通过；生成目录离线检查结束后已恢复。

初次捕获的heap-corruption断点已修复，不能将诊断时GuardMalloc偶然通过视为验收；
以正常allocator上的正式batch结果为准。仅Library/Function所有权本轮修复，其他wrapper
类型仍需独立审计。GUI/跨硬件/metallib源码恢复不算本批已完成。

## 版本与待验

- 库/app副本：`6df633b629eb67b11e60c5ed5000fe77c51c4580781089aa3f9c7f6672b8d83b`。
- T48：`f71eaf76384b211567ca7a4194d1ad1fec3c10110c7de436b2598840a8254b22`。
- GUI executable未改：`3cc9c3b63507006794f87e219921454aaf50469bebb05f72b10f5c4b98b1a4bc`。

全部待GUI **T34–T48 + T10 marker，共16份capture**，PHASE35–48及对应batch保持开放。
T48只新增初始化字节/来源metadata、binary shader反射/资源链接、首末dispatch回退、最终
暗色画面的最小差异项；无原始MSL源文件为预期。总入口QA_CONSOLIDATED保留全部旧待验项。
