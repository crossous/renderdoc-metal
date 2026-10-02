# B423：GPU-authored 表的逻辑槽位退役

UE buffer24 offset3336 generation6459 type4 在 frame chunk35231 退役；该表已被多个 GPU descriptor producer 写入其他槽位。free 事件无 payload，不改变 Native 24B，没有释放对应 Native source；旧 guard 把整表 GPU-written 标志当成了逻辑 free 的禁止条件。

沿用官方 D3D12 CopyDescriptors/Vulkan ReplayDescriptorSetCopy 的 logical shadow 与 Native allocation 分离。coverage65 允许严格匹配 live generation/type 的空 payload free；保留 source 对象和 Native bytes。OverlayDescriptorSlotBuffer 的 frame restore 不清零 GPU-written 表的 dead entry，initial restore 仍清 stale captured identity。再次 value/binding/GPUValue 对 dead entry 仍拒绝；generation 不允许回退，尚未放宽 borrowed 槽位 CPU reuse。

精确9f476f2d：metal-logical-gpu-retirement.7Hcy2O，Shared/Private indirect/parallel 六 captures、24 seeks、108 indirect +24 freshCPU +24 mixed +24 logical-retirement API+CLI negative groups。Native GPU producer 修改 slot0，原始 compute/MRT consumer 后、commit 前 retire slot0，随后 resolve 和 partial seek 正确；追加 helper 在每次 later resolve 核对 retired Native textureID 非零、metadata不变。再次全部六 captures 通过。错误 generation/type/double-free/offset/unknown-live-slot/nonempty-free/post-freeGPUValue/coverage64 均 GPU 前拒绝。

实际 UE pre-submit9f476f2d 已越过所有这些 retire，继续到末段普通 buffer blit stream2779136；没有 initial GPU upload 或整帧 GPU 提交。UI 尚未验收，无提交推送。
