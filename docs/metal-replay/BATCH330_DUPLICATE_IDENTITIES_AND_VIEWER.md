# BATCH330：帧内重复身份误拒绝修复与小帧 Viewer 验证

2026-09-30，HEAD c4be68bb7，工作树保留，未提交/推送。用户解锁后继续。

## 误拒绝与修复

新 UE 帧7cd838c7…的508条帧内身份记录中，164条与帧首同资源的kind/value
完全一致；344条是新身份，分别来自278个MTLHeap::newBuffer(offset)与66个
MTLBuffer::newTextureWithDescriptor。此前的无条件frame拒绝混淆了两者。

最小真实复现：inputB在capture前创建，第一次gpuAddress查询推迟到第二次
compute前。CaptureGPUIdentity把metadata加入resource record与frame副本，
旧库原生两次捕获122/161正常，却在OpenCapture code19拒绝。
记录于 `local-m2-descriptor-replay/late-identity/capture-before-fix.log`、
`open-before-fix.log`。未将完整UE提交到GPU。

ScanDescriptorMetadata现在只接受已在帧首记录过且kind/value完全相同的帧内
副本；新身份仍拒绝，冲突仍拒绝。没有把344个帧内新资源改成帧首资源。
同时核对sampler别名对应的replay native gpuResourceID，相同不可变状态可共享；
不同状态伪造相同捕获ID必须拒绝，而不是任选一个。

CPU审计工具新增identity_timing/new_frame_identities，使用SystemChunk5识别
帧边界，记录真实creation chunk/API/phase。新帧结果508/164/344及分类可复查，
输出 `late-identity/ue-audit.json`。这些是CPU事实，不是重定位执行计划。

## 最终构建和定向结果

最终库/app SHA256：
`78e3cecc338bc1fe023309ef37e21d82362bf6efc2e7f154e9d227e1233331e7`。
脚本仍为 `util/buildscripts/scripts/test_metal_descriptor_relocation_macos.sh`。
最终日志 `build-macos-debug/metal-descriptors.biMurc` / `late-identity/suite-final.log`。

- 两种原生用例、各两份真实注入capture、GPU字节/像素与12/15次seek通过。
  首份table capture含真实帧内重复身份，第二份验证缓存查询与记录跨capture。
- 等价sampler别名API/CLI/GPU字节与seek正例通过。10个table负例加5个GPU
  provenance负例，总计15个API+CLI预期拒绝；含冲突late identity与冲突sampler
  alias，拒绝日志没有frame GPU wait/private initial upload。
- T01/T09/T35/T49/T52/T62最终库API+CLI通过，`late-identity/targeted-final.log`。
- 完整UE最终库API仍code19 GPU前拒绝，`late-identity/ue-api-final.log`；
  344个帧内新身份、临时布局/槽位/provenance和render仍待支持。
- Python CPU审计正常、diff check通过。**全量回归未运行。**

## 人工 UI 结果

用户解锁后通过Computer Use实际操作qrenderdoc。系统文件对话框的AX访问
曾超时；进程sample在NSSavePanel/ViewBridge辅助功能调用，无GPU wait。
保存 `viewer-unlock-sample.txt`，结束本次空Viewer，使用官方filename命令行
直接打开已经终端验证的小帧，不修改Qt全局设置或捕获内容。

旧最终库bd255367…先验证table的EID9/24：Buffer19为0000007A→000000A1→
0000007A（122/161/122）；Texture25 EID32零坐标RGBA为
0.12549/0.25098/0.50196/1，与BGRA字节128/64/32/255一致。
混合表EID15/34/53/15：Buffer22为29/79/A0/29十六进制（41/121/160/41），
DEADBEEF哨兵不变，compute pipeline/buffer绑定正确。

在本批最终78e3cecc…库/app重新检查两份biMurc新capture：table EID9/25/9
再次122/161/122；混合表EID15/34/53/15再次41/121/160/41，哨兵不变。
两个窗口均显示loaded/no problems detected，操作后正常退出。
日志 `late-identity/viewer-final-table.log`、`viewer-final-mixed.log`；UI数值由
Computer Use的AX状态直接读取。**本批两种小帧人工UI通过；完整UE UI未通过；
不能以此清除旧T系列全部待验项。**

下一步在极小用例中接通帧内placement buffer/view的身份、创建时序及回跳
恢复，再处理UE临时表/有效槽位/CPU来源和render。当前不需要用户重截或操作。
