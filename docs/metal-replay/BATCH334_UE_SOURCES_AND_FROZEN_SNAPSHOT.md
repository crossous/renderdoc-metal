# BATCH334：UE 明确源资源、inline 布局与冻结的帧首状态

## 实现与依据

按 D3D12 `CopyDescriptorsSimple/GetRefIDs` 的描述符 shadow 思路，在 descriptor
factory 保存已知 native object、成员偏移并穿过异步 RHI 更新队列。捕获中存
RenderDoc ResourceId，不以 raw VA 的多候选列表猜对象。UE 源码仍为官方
5.8.3 tag 的隔离局部副本；没有修改安装版 MetalRHI、类布局或导出列表。

临时更新源只记录 `NumDescriptors * 24` 的实际 upload 子分配，普通 UB/索引
数据保持普通数据。记录租约结束的位置为原有 DeferredDelete 回调；generation
使用 provider 的单调序号。另记录 6 项 static sampler 表。

每次 IRBindResourcesToEncoder 记录当前 stage/index/count 与明确的 CBV/static
sampler backing buffer。顶点流 IRRuntimeVertexBuffer 为 16 字节，只有首 8 字节
为 VA；length/stride 保持普通字段。覆盖 compute/vertex/fragment 的实际调用；
object/mesh 只有 producer hook，尚未做该阶段 GPU 验收。

新增 chunks 1405/1406 为 DescriptorInlineLayout/Binding，独立 CPU export 可用，
UE provider 继续不声明完整 coverage。

## 捕获结束边界修复

第一次完整 stage 记录的 UE 帧发现 9 条 background free 出现在其 frame allocate
之前：Metal EndFrameCapture 先切回 background，再等待 GPU/写文件，UE 延迟删除
回调在此期间给资源 creation record 追加了新历史。其余后端从 descriptor shadow
构造初始状态；这里将诊断历史移到单独所有者，并在 StartFrameCapture 写锁内复制
一个冻结快照，写在 CaptureScope 之前。当前帧更新继续属于 frame record，且进入
下一次捕获的历史。释放、GPU 等待和原始 UE 更新顺序保持原样。

小用例复现 EndFrameCapture 状态切换后的并发 retirement，确认第一张帧不含下一代
历史，第二张帧才在帧首看到它。死 buffer 的历史在下一次 snapshot 清理；活跃 backing
buffer 的历史目前仍累计，后续应压缩为当前 slot shadow。

## 实际 UE 证据

- 明确源对象帧：session 20261001-035721，16,002,719 bytes，SHA256
  `4b5788d558a5fcd4ece78f891bb5585f45636aa5518c15597dfa9493f4fab997`。
  34,405 chunks，帧内所有 821 个非零字段逐次更新匹配；历史已退休源不随帧保留，
  2,861 条历史值缺 identity，仅作不可验证历史记录，不是帧内错误。
- stage/临时源首次帧：session 20261001-042700，15,760,375 bytes，SHA256
  `90ada0261fa10bef67e909f8e59afc7cf50796003c9a5efebd0bd9f39c3fb293`。
  1,337 次已声明 inline、2,603 个非零地址匹配；上面 9 个捕获边界冲突据此定位。
- 冻结快照重截：session 20261001-044243，11,220,555 bytes，SHA256
  `c47bb56d4f6c085aa5f92fe4f7d973792a23112e90369a7f09c8a2b71d7e79da`。
  38,723 chunks；lifetime issues=0，frame value epoch issues=0，inline issues=0。
  compute 209、vertex 584、fragment 285 次声明，2,289 个非零 inline 字段匹配；
  674 个帧内 slot 非零字段逐次匹配。4,019 条历史值的源 identity 未保留。

冻结帧 local 文件 `build-macos-debug/local-m2-descriptor-replay/ue-snapshot.rdc`，
项目 Saved 有 `UE58_NewMap_snapshot_c47bb56d.rdc` 保护副本。
provider dylib SHA256 `d07b8aac4be716465c7ef2fac4ed784c38bc6ed17559efa6a8521fd377a44dca`，
98 个原导出，无缺失/新增。该次 RenderDoc library SHA256
`84c3db6e8fb26957605b41ed26ecfe8648bf59511300c051b4415859f51cc042`。
各新 session capture end=1；桌面再次锁定，已通过 SIGTERM 结束本任务启动的 UE，
launcher exit=0。不是人工 UI 正常退出验收。

CPU 一致性仍不证明动态 heap alias、GPU 时间点、shader 内部嵌套地址或 UE 实际像素
正确；完整 UE replay 未完成。顶点 index4/5 未声明调用来自官方 IR runtime 的普通
draw params/index type，不把它们扫描成指针。安装库原件的恢复证据见 BATCH333。

## 验证边界

六类既有 tiny GPU/seek、56 API+CLI 负例、新 stage/snapshot 协议小帧通过。
日志 `metal-descriptors.CYjp5f`、`metal-descriptors.xPmoI6/race-gate`。
全量回归未跑；新库 Viewer UI 未验。没有提交或推送。
