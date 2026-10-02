# BATCH362：编码期间 placement 共享与 partial 提交快照

2026-10-01，持续目标 active，未提交或推送；完整 UE GPU replay 未提交。

Apple SDK MTLHeap.h 明确 placement 的重叠range隐式共享，tracked heap 对所有
子资源建立读写依赖；Apple MTLHazardTrackingMode 文档确认适用于普通
MTLCommandQueue。参考本地RenderDoc D3D12 CreatePlacedResource与Vulkan
vkBindBufferMemory的同heap/offset重建。创建资源本身不会修改或逻辑退役旧
资源，所以v24支持已有CB编码而未提交时创建tracked同区间buffer；所有Native
对象保持到GPU完成后才清理。显式makeAliasable继续保留原退役/完成条件。

Private before-commit：67d8d1df75e3e8ac80d9503ac04e4a7e1a2f8e93cc25a4e1ede6a59bcd304693，
metal-async-alias.nbdBAZ八捕获/32seek/220 API+CLI反例通过。跨kind只增加
tracked Private buffer→受限frame Private texture，旧buffer<=64KiB、texture
仍2px RGBA/BGRA、alignment/range使用Native layout查询，descriptor backing
不允许alias。metal-frame-mrt.HWzF1X serial/parallel×MRT2/5八捕获/32seek/331
反例通过。保存第一次GPU读取到独立16B结果buffer；Native和replay均核对
122/186，后续RT写入186/122、每个MRT像素、frame texture ID重定位和下一pass
采样均匹配。不把buffer线性bytes当成texture物理layout。背景Private直接
readonly compute来源缺少initial内容时，在任何frame GPU提交前拒绝。

Shared before-commit回跳发现：第一CB提交快照包含出生在早期选中事件之后
的B；partial tail未创建B，旧ApplyReplayCPUBufferUpdates返回false，并导致
尚处于Enqueued状态的NativeCB在清理时触发Metal Validation断言。修复按捕获
heap/offset将future Shared snapshot与此时仍存活的Shared buffer逻辑bytes
求交恢复；不创建未来对象，不越过buffer逻辑length，不碰descriptor backing，
恢复仍先等此前已提交GPU完成。standalone future buffer或没有当前物理相交
的部分不影响早期GPU工作。

修复精确SHA57ba6f4a60e34934a96af30207bd7415dec215fea821d3050bcd85829b572bd9，
metal-async-alias.6Kf3ok八捕获/32seek/204反例通过。更严格5l9myz测试让B在
首个提交前CPU写103：第一次NativeGPU读A为184/248，后续GPU写B80再读A/
render为225/161，保存首读值并在seek核对。再去除A的冗余提交快照，只剩
尚未出生的B的CPU快照：新增八份捕获/32seek全部匹配，证明确实恢复共享
bytes而不是忽略future snapshot。原始八份+204反例也通过。

此前863f5998…精确全量308/7786/3080通过、growth13942784B；当前57ba…旧21
类674反例与全量308/7786/3080均已通过，全量驻留增长11943936B；
Private/cross-kind需验证该精确库兼容。
UI仍待解锁。实际0649fac8…有12个descriptor backing相关历史overlap；旧
15284/15357/15492表在相交birth之前slot已全free，15357最后CB28625提交后
29320 free、15492最后CB29174提交后29385 free。新16040/16094表也与旧普通
buffer共享。当前仍拒绝这些table alias；下一步验证slot退役、提交归属和
物理重叠快照的重定位，不通过放开tableBacking保护来掩盖它们。
