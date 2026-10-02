# B437：Slate 局部回放的未来描述符快照

验收f52全量通过后立即回同一UE原始ef026832的审计副本63c55db1，进一步验证局部事件。2196/2488/3534/3544/3718/3766/3965/4302各两轮EID0重置通过，3534五MRT全部原生读回重复一致；但9520在部分提交完成阶段拒绝。先失败的是16457的CPU快照，随后16459因前者未提交而拒绝前缀，没有Native GPU执行错误。

诊断缩小到16457拥有的未来buffer16481描述符写入，offset0/24B。16481在当前选中事件之后创建，与旧描述符表15930共享Heap22/offset19052544。全帧预检已经证明旧表的全部逻辑槽退休、消费者都在新表出生前提交；局部 replay 尚未创建新对象，旧 `ApplyFutureSharedAliasCPUUpdate` 一律拒绝未来typed table。不能将捕获进程GPU地址直接写入旧原生表，也不能为了通过局部回放提前构造未来身份。

按本地官方D3D12/Vulkan对placed/bound资源的物理别名与逻辑descriptor shadow分离、以及B362既有部分提交快照规则处理：预检保存每个已验证before→after别名的消费者闭包。coverage65仅在Shared/tracked placement、精确别名证明、每个相交旧原生缓冲都有该证明且消费者已经原生Completed且无error时，将未来typed bytes留到原资源出生时恢复。没有匹配证明、旧槽还live、旧消费者未提交/未完成或额外未证明的相交缓冲仍拒绝。普通future共享字节交集恢复、已有资源CPU更新/重定位、GPU完成等待与旧coverage均不变。局部参考为官方源码 `d3d12_device_rescreate_wrap.cpp:694` 的placed heap/offset与独立resource identity、`vk_resource_funcs.cpp:1697` 的buffer↔memory/offset及replay address追踪；它们提供资源身份与物理存储分离的依据，未来Metal提交快照的具体guard复用本后端B362规则。

## 定向终端

极小原生Metal：64KiB heap，24B旧typed table GPU读取17并完成；独立中间GPU工作编码读取34；在中间提交之前创建同地址的新typed table并记录51；最后GPU读取新表。输出包含DEADBEEF标记。f52同一捕获正常全帧打开，但EID11读取17后在EID27局部定位返回6，复现未来快照拒绝。修复库 `ae20688153c6942c42f31e1a6c35804de3b5cc3ec636dc2467a1f814259953ec` 对该原捕获四轮重置/first-middle-future定位，原生结果17/0/0→17/34/0→17/34/51及全部标记通过。

`test_metal_future_descriptor_snapshot_macos.sh` retained/unretained各原生、注入捕获、四轮定位通过；`metal_future_descriptor_snapshot_gate.py` 保持旧槽live或将旧consumer提交挪到新birth之后，两组API/CLI都在帧GPU前拒绝。日志 `future-descriptor-snapshot-targeted.log`，产物 `build-macos-debug/metal-future-descriptor-snapshot.VE4dT1`。不以这些微型测试替代真实UE。

## 立即回同一真实UE

修复后首先返回同一63c55db1 UE文件的9520和9669，各两轮EID0重置通过。9520的16466目标原生900×640/2,304,000B读回，两轮SHA256均为 `1e7f9e791cdabde129856f23e9a13e3463c0d311324dc95e8eb25b8045e74e34`，与此前完整帧相同。日志 `ue-slate9520-alias-fix/event-probe.log`。随后正常OpenCapture、两轮完整帧及官方缩放/jpge90生成JPEG再次与原始捕获缩略图逐字节相等：SHA256 `15d15618c45e04b63bec4f642205c2facad0a048ac3d1af8a5f41630756de6b9`。session `testproj-20261002-194052-512236` / `slate9520-fixed-full-image.log`。

进一步检查实际BasePass indexed draw EID3260（288 indices / 1 instance），五个MRT 14485/14489/14497/14494/14491在两轮EID0重置后原生读回一致；614400B及四个307200B。日志 `ue-basepass3260-alias-fix/event-probe.log`。这些重复一致性检查没有独立捕获的完整浮点MRT golden，不能冒充该比较。

## 全量与UI分开记录

f52验收全量308正常/7786畸形/3080生命周期开启已通过，resident growth11,190,272B/hash起止一致。该结论不冒充ae206881的全量；最新库针对未来typed snapshot的资源恢复改动，验收全量已在冻结ae库上完成（19:51:03–20:08:33）：308正常/7786畸形/3080生命周期开启全部通过，resident growth6,045,696B/hash起止一致；verify-only，不重建Qt或替换主库。日志 `future-typed-alias-frozen-combined-regression.log/json`。UI此前f52已观察到正常打开、64³两个MRT/第63层、3766切换和9669帧末图像；机器随后锁屏，最新库UI及新自动截帧待解锁。未提交或推送，原始UE capture保留。

验收全量后立即回同一63c55db1：1954、3718、3766、9520、9669均两轮EID0重置通过。新增独立Native buffer读回检查，1954的12250/offset16/12B两轮均(8,1,1)，匹配原捕获执行参数，不再是旧(16,34,0)。四个64³ MRT及9520图像重复一致。日志 `ue-ae-acceptance-return/event-probe.log`。随后已启动新自动截帧流程。

最新ae内嵌库UI操作验证通过：正常CLI打开63c55db1，显示loaded/No problems detected；3718两个64³ MRT均切到Slice63，3260五个MRT可见并实际切换第五14491；9520呈现16466/900×640正确编辑器场景，9669通过Texture List打开同一呈现纹理仍正确。CUA截图观察完成，正常退出。不是用户人工确认。新捕获另有CPU预检拒绝，旧帧完整验收不代表新帧已通过。
