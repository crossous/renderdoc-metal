# B393：submission 所属快照到帧内 Private 索引（定向通过，全量运行中）

v50 在 CPU preflight 预扫快照，以紧随的 commit 关联 command buffer，复用 core
AssignPendingReplayCPUBufferUpdates/ApplyReplayCPUBufferUpdates 的提交所有权规则。
只允许已知 Shared initial/creation 字节加已提交 submission 和当前 producer 的 CPU 更新；
不修改 Native 数据。全部普通 blit 原有范围、birth/alias、预算检查通过后，才把实际复制字节
标记为未来 Private|Tracked index buffer 的已知范围；未复制的 padding 保持 unknown。
跨 command buffer 消费要求 producer 已 commit，依赖已验证单队列/Tracked 顺序；
commit 不当作 GPU 完成，回放同步仍由现有 core 控制。

索引 length<=64KiB，offset/stride/count 必须完全落在已知范围；不能来自 typed table、
GPU-written staging，或随后 GPU shader/write usage 产生的未知数据。
frame index 计入 noteResource 的提交生存期闭包，保持独立 ResourceId。
已有背景 Private 初始规则和 v49 参数检查不变；不使用旧 VA、Private contents() 或猜测数据。

精确库 008ca84b277c4903ef52e76fb1d6e3096a4ba4e27533fdc1ddd20402d0ad514d。
metal-frame-index.RMGES8：Same-submission UInt16 Triangle/负 baseVertex、跨 submission
UInt32 TriangleStrip，各两捕获；256 draws/capture、总4 captures/16 seek cycles，
146 API+CLI反例通过，完整action、descriptor producer/consumer、2×2像素、EID0通过。
未来buffer全部创建在frame内，staging CPU写入后由实际自动commit快照记录。

反例覆盖缺失/截断/错误commit快照、未知source、源/目的/size越界、draw-before-upload、
GPU写后上传、legacy49，以及原graphics/IR/工作量反例。首次GPU写反例放在copy之后，
该顺序本来合法；修正为完整blit encoder块位于compute writer之后后正确拒绝，未放宽实现。
另一次测试按错heap chunk字符串识别future index，修正为实际MTLHeap::newBuffer(offset)。

正在同精确库运行全量308/7786/3080；近期精确v46已通过，不提前归因给v50。
当前UE12230757整帧GPU与人工UI尚未验收，持续目标active，无提交/推送。

精确008ca84b…全量已完成：308 captures/7786 malformed/3080 lifecycle，resident growth12681216B。日志frame-index-combined-regression.log；结束hash仍一致，v51仅编译object未链接。该全量包含v47/v48/v49/v50，不包含v51候选。
