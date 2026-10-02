# BATCH355：保留 placement buffer 的物理共享与独立逻辑身份

2026-10-01，持续目标 active，未提交或推送。

实际warm UE帧f744c413…有302处logical buffer range overlap：64次背景旧对象、
238次帧内旧对象。299次没有live descriptor source，3次仍有live descriptor，
47次captured base VA相同。3次均heap22旧48B/new48、224、96B，旧RID14747/
14749/14751仍有有效slot。背景旧对象43个，递归到texture-view/buffer parent、
包括嵌套ResourceId数组后均未发现重用后API引用；这不是GPU退役证明。
新audit_ue_metal_heap_aliases.py保存全部证据和限制，使用logical length，
不冒充Native heapSizeAndAlign、texture范围或GPU完成审计。

D3D12 Serialise_CreateResource按原heap+HeapOffset调用CreatePlacedResource，
alias barrier分别unwrap before/after两个对象；Vulkan Serialise_vkBindBufferMemory
按同memory+offset绑定并记录boundMemoryOffset。Metal官方说明placement offset
allocation隐式共享重叠资源的memory，不要求先makeAliasable：
https://developer.apple.com/documentation/metal/mtlheap/makebuffer%28length%3Aoptions%3Aoffset%3A%29?changes=_7

修正此前只允许显式makeAliasable的限制。v17允许帧内两个Shared tracked
placement buffer共享真实heap范围，两者RID/descriptor source保持活跃；v18
允许旧对象来自帧初。仍要求每个此前CB有captured CPU completion、没有live
encoder/inline/producer pending，不允许descriptor backing被alias、private/
texture隐式alias或范围不合法。Native按捕获offset创建，seek先等GPU再释放
frame object并重置范围、恢复initial内容。没有将Native isAliasable等同逻辑退役，
没有退役旧slot，没有换成独立buffer破坏物理共享，也没有按VA猜SourceID。

## 定向终端

v17库6b4f61aa…：retained/unretained4捕获、16seek、92API+CLI反例通过。
v18库f69220dd2db50733363e3b8fb930ff3a550f67ff14e6b796667f7b4bef453641：
script test_metal_implicit_alias_macos.sh，metal-implicit-alias.5ayyDU，
frame/background × retained/unretained8捕获、32seek、184API+CLI反例通过。
A12B仍存活且其descriptor不变，通过B12/224B写入80，GPU经A读取80；
结果122→225/186→161，pixels和普通metadata正确。初始旧A也按41恢复。

## 全量与人工UI

精确v18全量尚未跑；最近B3535b08e331…全量308/7786/3080通过。人工UI
待解锁；真实UE仍没有captured CPU wait，需要继续补齐有序异步alias、事件
signal、parallel/MRT/间接绘制和实际资源预算，完整replay尚未通过。
