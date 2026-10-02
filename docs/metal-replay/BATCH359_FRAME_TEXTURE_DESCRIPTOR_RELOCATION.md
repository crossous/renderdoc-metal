# BATCH359：帧内 Private texture 的描述符重定位

2026-10-01，持续目标 active，未提交或推送。

真实 UE 新帧 ef14a1ed…已经保留 65 texture/14 view 的正确帧内 birth。v21
接通第一条受控路径：2×2 Private tracked placement texture 作为 MRT 写入
目标，再通过带明确 SourceId 的 UE TextureSRV packet 被下一 pass 采样。

render attachment 的 serialisation 保留原有 ResourceId wire fields，同时在
内存保留 textureId/resolveTextureId。CPU preflight 不需要提前分配 future
texture，就能验证已发生的 birth、heap 对齐/range、格式与 pipeline 匹配。
Native ordinary/parallel pass 显式拒绝未解析的非零附件身份，避免未知 ID
被当成空附件。SourceId 驱动重定位，原始 VA/ID 不作为动态地址猜测依据。
沿用现有 frame placement Native 重建；尚未接受 frame texture aliases/views
或扩大实际 UE 的内存/dispatch/draw 限额。

精确库 SHA256 a9c8e85b54029cd2c45cd6a5002d78fa109ef303e3793e6194ae64372fae134d。
test_metal_descriptor_frame_mrt_macos.sh，metal-frame-mrt.tQQUXw：serial/parallel
× two/five 共八捕获、32 seek，所有颜色附件及下一 pass pixels186/122正确。
每次 seek 的 resolve table 都保留 ordinary metadata 并编码新 Native texture
ID，验证与 captured ID 不同，例如 captured4→replay9/11/13。
319 API+CLI 反例通过：原223附件/pipeline/parent-child组和新增24×4帧内
birth/source/layout/未知颜色、depth、stencil身份拒绝组，均在GPU执行前拒绝。
日志 local-m2-descriptor-replay/frame-mrt-suite.log。

B358 的 ecc44d57…旧21类/674反例已全部通过。本次 a9c8e85b…因涉及所有
render attachment serialisation，T01–T312全量 308捕获/7786畸形输入/3080生命周期打开已通过，resident growth
11501568 B，日志 frame-texture-full-regression.log。完整UE尚未replay；人工UI未验。

下一步：依照 D3D12/Vulkan 的 exact heap/offset 共享和 Native command order，
结合本机 Metal SDK MTLHeap.h 的 tracked-heap hazard 规则，验证无捕获端 CPU
wait 的隐式 placement alias。当前 v17/v18 的 CPU completion 前提偏保守，
不能据此推定 UE 必须加入 CPU wait；先做 GPU 写入/同队列消费的小用例。
