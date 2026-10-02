# BATCH349：帧内 heap alias 与同地址资源身份

2026-10-01，持续目标 active，未提交或推送。

v13 在既有 placement heap 回放机制上补齐逻辑生命周期。仅允许帧内创建的
Shared buffer 在此前所有提交已有显式完成等待、encoder 已结束、引用它的
描述符槽已退役后执行 makeAliasable。后续重叠分配按明确 ResourceId 的退役
记录放行；inline/slot 来源、直接 compute 绑定和 residency 均拒绝旧资源。
回跳先等 GPU、释放帧内子资源、复位范围并清除对应退役状态，重建新一轮对象。

原生 M2 placement buffer 刚创建时 isAliasable 已为 true。这个属性不能证明
应用执行过退役；heap overlap 因而改查实际执行的 makeAliasable 记录，buffer
与 texture 都登记该事件。capture 的 GPU identity 保守资源收集也排除明确
退役对象，防止上一份截帧被已完成 command buffer 保留的 proxy 进入下一份
初始资源集合。退役操作与捕获切换采用已有 transition/submission 锁。

设计对照 D3D12 alias barrier 的 before/after ResourceId；这里只补 Metal 原生
事件状态，不另建 heap 分配器。当前有限路径不支持背景初始对象 alias 或跨队列。

小用例 frame A 的 GPU 地址与 replacement B 完全一致，但值从 41 变成 80。
两份连续捕获各四轮 seek 得到 122→225、186→161 和正确图像/metadata；第二份
初始集合不再包含历史 retired A/B。26 组 alias/等待/旧身份/直接绑定/residency/
范围反例，27 组图形反例通过。整套十九类/560 API+CLI 反例通过，日志
metal-descriptors.kSp9Q8，验证库
24d941c8a84de210268f160b4b218f6b527234dd284677a836d9d91335d9fbda。

全量 T01–T312 首次执行在 T20 ICB action tree 构建崩溃，见下一批修复；这不是
完整回归通过。新增人工 UI 尚待本机解锁，实际 UE 全帧 GPU replay 尚未完成。
