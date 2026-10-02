# BATCH360：tracked heap 异步共享与间接 Shared 内容快照

2026-10-01，持续目标 active，未提交或推送。

实际 UE ef14a1ed…的八个 placement heap 均为 tracked，帧内有 451 render /
142 compute useHeaps。对照本机 SDK MTLHeap.h 77–86/151–160/225–247：tracked
heap 对各子资源读写建立依赖，显式 offset 创建会隐式共享重叠内存。对应
Apple MTLHazardTrackingMode 文档；RenderDoc D3D12/Vulkan 使用原 heap/memory
和 offset 重建资源，未为这种共享创建独立 backing memory。

v22 的首条异步路径仍限制 Shared tracked heap、buffer<=64KiB、既有单队列
命令均已提交、encoder/inline/producer 已关闭、descriptor backing 不被 alias。
允许此前提交尚未 CPU waited/completed，沿用 Native MTLCommandQueue 的
heap hazard tracking；Shared CPU 内容恢复继续等此前提交，未取消 GPU 完成
等待。显式 makeAliasable 的逻辑退役仍保留原完成条件。新 useHeap(s) preflight
检查 encoder、heap identity/type/device、tracked mode、array/stages。

小用例发现 capture 只标记 heap、漏捕间接 Shared 子 buffer 的 CPU 初始化。
原生第一消费122，旧 replay partial 得到161：heap 留着上一轮 GPU 写入的80，
A 的41没有提交快照。修复在 CaptureCmdBufCPUWrites 中按被声明的 heap 保守
扩展当前存活的 Shared buffer 依赖，保存首次提交前 CPU bytes；不猜测 raw VA，
不加入显式已退役对象，不把 GPU-written descriptor table 的 diff 当 CPU 写入。

精确库 SHA256 863f5998b98cba95b5307fe81166420eff03b1603e5d6c58cb28ce1598f431a8。
test_metal_async_alias_macos.sh，metal-async-alias.BOplfi：frame/background ×
retained/unretained 八捕获、32 seek，GPU 写 B[1]=80，再通过存活 A 的不变
descriptor 读取，GPU/pixels122→225、186→161，ordinary metadata不变，200
API+CLI 反例通过。capture只有最后一次 CPU wait；结构化证据验证 A=41 的
快照位于第一次 commit 前、B 是 producer buffer index2、useHeaps 保留。
实际没有插入捕获端中间 CPU wait、显式 retirement 或 CPU 写 B 的捷径。

B359 精确 a9c8e85b…全量 308捕获/7786反例/3080生命周期已通过，resident
growth11501568B。本次新库 frame/MRT319反例、旧同步alias184反例、18个sentinel/相关旧捕获
API+CLI全部通过，未宣称本次精确库全量已通过。完整UE尚未replay、UI未验。
继续用更新的 capture 内容追查 UE frame source、Private/cross-kind aliases、
实际绘制范围；已保存的原帧均保留。

实际 UE 重截完成：0649fac867fa8b98fd114494c4cf4ce0ad709983529e9ba8fbd0434fd97ffdf8，
37563963B，精确 capture 库仍为863f5998…，会话20261001-162622自动退出。
已保留 UE58_heap_sources_0649fac8.rdc，未提交完整GPU replay。1669帧初槽位
全部匹配 initial bytes/sources，1823帧末槽位；110producer、2737inlineVA、
786普通绘制常量，帧内epoch/producer/inline/lifetime错误均0。历史已退役
来源缺失不计为当前帧来源错误。CPU快照205→369条、195→353个buffer，
数据13617646→14751470B；新增158个间接buffer和1133824B提交内容。
Native仅查询1888 placement、340个历史overlap、range错误0，frame texture
提前出现在initial section计数0。overlap为99 BGbuffer→framebuffer、238
framebuffer→framebuffer、3 BGPrivatebuffer→frameTexture（其中texture32×32×16）。
对应出生时未编码/未提交CB均0；其他buffer复用大量发生在CB尚未提交时，
保存CPU上下文审计，下一步沿用tracked heap语义验证，不推断CPU顺序=GPU完成。
当前精确863f5998…全量308/7786/3080已通过、增长13942784B，Private source改动在源码中准备，未构建
以保持本次全量的精确库；人工UI仍待解锁。
