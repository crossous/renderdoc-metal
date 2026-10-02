# B383：较大tracked placement buffer共享（进行中）

实际UE763817d8…Native query-only1704placements/112历史overlap；包含8个65536→131072B、
4个131072→65536B。B381独立范围的大buffer不能证明共享正确；当前runtime implicit alias仍64KiB。

v42候选沿用已有逻辑ResourceId、保留Native两对象、tracked heap依赖、source overlay及
CPU restore前等待。Native/preflight使用同一个DescriptorPlacementAliasLimit，v42最大128KiB，
旧版本64KiB不变；future两buffer重叠的预检增加容量/类型一致性，避免旧v40/41在GPU执行后拒绝。
不把makeAliasable当slot退役，不从相同VA推断逻辑资源身份。

通用参照：D3D12 Serialise_ResourceBarrier保留before/after逻辑ID并unwrap实际资源；
Metal既有v24路径在同一tracked heap下允许两个buffer存储共享，CPU上传仍先等待已提交工作。
Apple官方WWDC21说明tracked heap按heap级别同步，间接写入/写后读须显式useResource：
https://developer.apple.com/videos/play/wwdc2021/10286/（residency/heap dependency部分）。
这里只沿用已建立的Metal API语义，不能用Heap声明代替实际写资源的residency声明。

复用metal_descriptor_alias_capture.mm和graphics replay，新增可选large模式：
12B/128KiB旧对象，128KiB新对象，Shared/Private，background/frame，unretained异步CB。
第一CB只commit不wait；B创建并写入后通过仍live的A descriptor读相同VA；最后等待一次。
Private frame原生初始化只写前三个word；不以未初始化尾部作为正确性证据。
16 captures/64seek候选脚本test_metal_large_placement_alias_macos.sh；旧gate支持大initial字节前缀，
增加旧v41拒绝及正确128KiB/heap范围边界，明确拒绝must occur before replay wait。
静态编译/py/shell检查已通过；等待4322af8全量结束后构建运行，未替换运行库。

实际UE完整coverage/容量/indirect工作仍待适配；完整UE未GPU提交，UI锁屏未验。
持续目标active；未提交或推送。

最终定向库a9ffdb1a8579b051fbab253f3695fd6c89cbc60d4ceea12b374851780e14885c：metal-large-alias.BieJmT，16 captures/64 seeks/208 API+CLI negative groups通过（Shared4×24、Private4×28）。两对象保留、同地址逻辑A读B写结果/每次NativeVA重编码、完整2×2像素、EID0恢复通过。旧v41负例全部在replay wait前拒绝，Private仅GPU初始化无CPU快照。原有21类674兼容运行中；精确v42全量未重复。此前4322af8精确全量308/7786/3080/growth11632640B已通过。

a9ffdb1a…旧21类674 API+CLI兼容完整通过；随后才构建v43，不混记精确全量。
