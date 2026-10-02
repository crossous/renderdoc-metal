# BATCH350：全量回归暴露的动作树与资源生命周期

2026-10-01，持续目标 active，未提交或推送；完整回归仍执行中。

T20 ICB 的 MultiAction|PushMarker 同时进入固定子动作分支与 debug group path，
第一子动作因读取空 child-list 的 back() 崩溃。MultiAction 的标记保持不变，
由既有 BeginMultiAction 管理固定子动作，普通 debug group 才进入后者路径。
T20/T28/T30/T40/T53 及后续全量图像阶段验证通过。

T135 旧捕获的普通 buffer 在帧内创建，但无描述符 coverage。此前 v8 重建路径
只登记带 descriptor 合同的帧内 buffer，回跳因已有对象而拒绝。无 coverage 的
普通帧内 buffer 现在沿同一 owned frame lifecycle 登记、等 GPU、释放、重建；
T135/T136/T137/T138/T140/T149 定向通过，完整 308 份图像+CLI 阶段通过。

Metal MakeDummyDriver 原本返回 NULL，错误之后 ReplayOutput::RefreshOverlay
会解引用空 driver。按 RenderDoc D3D12/Vulkan 使用共享 DummyDriver，复制 Metal
按值存储的 shader reflections，转移 structured file ownership；故意保留
T135 活动回放失败先验证返回 7 而非 SIGSEGV，再修复其 buffer 生命周期。
没有修改通用 replay controller/output 来遮盖 backend 错误。

T49 反例将 commit-time Shared 快照移到首个 command buffer 之前仍被接受。
InternalModifyCPUContents 现要求已存在且未 commit 的所属提交；显式描述符 CPU
事件仍能在编码前执行，不混用两种时间合同。T49 87 组及 callback 元数据
重复/移除正例通过。当前库
93eb26b11dc9bb7f75a913a4969928e1ef1b377a169bbe83d79d016d8a27d2a0。

三项旧反例同步到当前 API/实现：T40 dispatchThreadgroups depth=2 合法，改测
超过1024线程上限；T47/T51 PipelineOption 的 bit4 为已支持的
FailOnBinaryArchiveMiss，改测未知 bit，并追加4/7合法组合。T40 41组、T47 84组
+4正例、T51 104组+4正例、T52 122组+2正例通过。未降低非法参数检查。

续跑发现 T55 重复 function constant arrays 扩容却保留旧 chunk length，导致
后续二进制错位。测试将 length=0 交给既有 WriteStructuredFile scratch writer
重算；81反例与重复写入正例通过。T104/T110/T111、T216/T217 的字符串
扩容也按同一机制构造，避免只测到损坏的外层文件而绕过实际 backend 验证。
T74/T77/T78 的 bit4 旧反例改测未知 bit8。T101/T103 counter 上限4096，
旧值65现合法；以4097作反例，另测65和 informational capture support hint
false 的合法回放。仍由现有 backend 核对 timestamp/Shared/range 和当前设备
native stage-boundary capability，未移除 backend 守卫。该轮剩余回归续跑中。

Launcher 增加可选 UE_METAL_EXIT_AFTER_CAPTURE=1，仅在本次受控捕获保存后关闭
其拥有的 editor process，方便持续循环；默认行为不变，已检查 shell/Python 语法，
实际自动退出验收待下一轮 UE 捕获。最新 UI 待解锁，完整 UE replay 未完成。
